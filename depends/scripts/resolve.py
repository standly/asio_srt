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
    },
    {
        'pkg_name':'fmt',
        'version': '12.0.0',
        'pkg_file':'fmt-12.0.0.tar.gz',
        'http_download_url': 'https://github.com/fmtlib/fmt/archive/refs/tags/12.0.0.tar.gz',
        'build_command':'''
cd fmt-12.0.0 && mkdir -p cmake_build && cd cmake_build && cmake -DCMAKE_INSTALL_PREFIX=$DST_PATH .. && make -j 8 && make install
''',
        'check_files':["lib/libfmt.a"]
    }


]


for item in libs:
    resolve_utils.resolve(**item)