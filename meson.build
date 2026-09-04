project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++20', 'buildtype=release'], license: 'GPL3', version: '0.35.3')
add_project_arguments(
            '-DCATCH_CONFIG_NO_POSIX_SIGNALS', # workaround for this https://github.com/catchorg/Catch2/issues/2192
            language: 'cpp',
            native: true
)
qt_sdk='qt6'

extra_files=files(
    'pyproject.toml',
    'setup.cfg',
    '.github/workflows/build_wheels.yml',
    '.github/workflows/test.yml',
    'Docker/Dockerfile',
    'Docker/build.sh',
    'README.md', 'COPYING',
    'tests/manual-tests/common.py',
    'resources/resources.qrc',
    'resources/README.md'
)

if get_option('buildtype').startswith('release')
    add_project_arguments('-DNDEBUG', language : ['cpp'])
else
    add_project_arguments('-DDEBUG', language : ['cpp'])
endif

cpp = meson.get_compiler('cpp')

subdir('src/DSP')

subdir('SciQLopPlots')

# Off for wheel builds (see pyproject.toml): nothing under tests/ is installed,
# and test_resampler_locking is an executable, so it links libpython through
# python3.dependency(embed: true) -- which manylinux images don't ship, failing
# the wheel build at link time even though the wheel itself needs none of this.
if get_option('with_tests')
    subdir('tests')
endif

