import resolve_utils

libs=[
    {
        'pkg_name':'srt',
        'version': '1.5.4',
        'pkg_file':'srt-1.5.4.tar.gz',
        'http_download_url': 'https://github.com/Haivision/srt/archive/refs/tags/v1.5.4.tar.gz',
        'build_command':'''
cd srt-1.5.4
mkdir -p cmake_build && cd cmake_build &&
cmake -DCMAKE_INSTALL_PREFIX=$DST_PATH .. && make -j 8 && make install
''',
        'check_files':["lib/libsrt.a"]
    }

]


for item in libs:
    resolve_utils.resolve(**item)