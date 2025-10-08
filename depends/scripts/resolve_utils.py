import os
import subprocess
import tarfile
import zipfile
from urllib import request

g_script_path = os.path.dirname(__file__)
g_project_path = os.path.dirname(os.path.dirname(g_script_path))
g_pkg_path = os.path.join(g_project_path, "depends/pkgs")
g_resolved_path = os.path.join(g_project_path, "depends/resolved")
g_build_path = os.path.join(g_project_path, "depends/build")

print(
    f" project_path={g_project_path} \n"
    f" script_path={g_script_path} \n"
    f" pkg_path={g_pkg_path}\n"
    f" resolved_path={g_resolved_path}\n"
    f" build_path={g_build_path}\n"
)


class Dependency:
    def __init__(self, pkg_name, build_command, version=None, pkg_file=None, check_files=None, http_download_url=None,
                 git_source_url=None):
        self.pkg_name = pkg_name
        self.version = version
        self.pkg_file = pkg_file
        self.build_command = build_command
        self.check_files = check_files
        self.http_download_url = http_download_url
        self.git_source_url = git_source_url

        self.src_dir = os.path.join(g_build_path, self.pkg_name)
        self.dest_dir = os.path.join(g_resolved_path, self.pkg_name)
        self.pkg_path = os.path.join(g_pkg_path, self.pkg_file) if self.pkg_file else None

    def _uncompress(self):
        print(f"[{self.pkg_name}] Uncompressing {self.pkg_path} to {self.src_dir}")

        if not os.path.exists(self.pkg_path):
            print(f"[{self.pkg_name}] Source file not found: {self.pkg_path}")
            return False

        os.makedirs(self.src_dir, exist_ok=True)

        try:
            if self.pkg_path.endswith(('.tar.gz', '.tgz')):
                with tarfile.open(self.pkg_path, 'r:gz') as tar:
                    tar.extractall(path=self.src_dir)
            elif self.pkg_path.endswith('.tar.xz'):
                with tarfile.open(self.pkg_path, 'r:xz') as tar:
                    tar.extractall(path=self.src_dir)
            elif self.pkg_path.endswith('.zip'):
                with zipfile.ZipFile(self.pkg_path, 'r') as zip_ref:
                    zip_ref.extractall(self.src_dir)
            elif self.pkg_path.endswith(('.tar.bz2', '.tbz2')):
                with tarfile.open(self.pkg_path, 'r:bz2') as tar:
                    tar.extractall(path=self.src_dir)
            else:
                print(f"[{self.pkg_name}] Unsupported compression format for {self.pkg_path}")
                return False
            return True
        except (tarfile.TarError, zipfile.BadZipFile, Exception) as e:
            print(f"[{self.pkg_name}] Failed to uncompress {self.pkg_path}: {e}")
            return False

    def _can_skip(self):
        if not self.check_files:
            return False

        if not os.path.exists(self.dest_dir):
            return False

        for f in self.check_files:
            check_path = os.path.join(self.dest_dir, f)
            if not os.path.exists(check_path):
                return False
            else:
                print(f"[{self.pkg_name}] Check file exists: {check_path}")
        print(f"[{self.pkg_name}] Skip building: all check files exist.")
        return True

    def _build(self):
        os.makedirs(self.dest_dir, exist_ok=True)
        env = os.environ.copy()
        env.update({
            "DST_PATH": self.dest_dir,
            "SRC_FILE_PATH": self.src_dir,
            "PKG_NAME": self.pkg_name,
            "RESOLVED_PATH": g_resolved_path,
        })

        print(f"[{self.pkg_name}] Building in {self.src_dir}...")
        print(f"[{self.pkg_name}] Command: {self.build_command}")

        try:
            subprocess.run(self.build_command, cwd=self.src_dir, env=env, shell=True, check=True)
            print(f"[{self.pkg_name}] Build successful.")
        except subprocess.CalledProcessError as e:
            print(f"[{self.pkg_name}] Build failed with exit code {e.returncode}")
            return False
        return True

    def _download(self):
        if not self.http_download_url:
            return True
        print(f"[{self.pkg_name}] Downloading from {self.http_download_url} to {self.pkg_path}")
        os.makedirs(g_pkg_path, exist_ok=True)
        try:
            request.urlretrieve(self.http_download_url, self.pkg_path)
            return True
        except Exception as e:
            print(f"[{self.pkg_name}] Download failed: {e}")
            return False

    def _git_clone(self):
        if not self.git_source_url:
            return True
        print(f"[{self.pkg_name}] Cloning from {self.git_source_url} to {self.src_dir}")
        os.makedirs(g_build_path, exist_ok=True)
        clone_cmd = f"git clone {self.git_source_url} {self.src_dir}"
        try:
            subprocess.run(clone_cmd, cwd=g_build_path, shell=True, check=True)
            print(f"[{self.pkg_name}] Git clone successful.")
        except subprocess.CalledProcessError as e:
            print(f"[{self.pkg_name}] Git clone failed with exit code {e.returncode}")
            return False
        return True

    def resolve(self):
        print(f"----------------- Resolving {self.pkg_name} -----------------")
        try:
            if self._can_skip():
                return

            if self.git_source_url:
                if not self._git_clone():
                    return
            else:
                if not os.path.exists(self.pkg_path):
                    if not self._download():
                        return
                if not self._uncompress():
                    return

            self._build()
        finally:
            print(f"----------------- Finished resolving {self.pkg_name} -----------------\n")


def resolve(**kwargs):
    dep = Dependency(**kwargs)
    dep.resolve()
