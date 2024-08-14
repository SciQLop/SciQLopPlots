project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++17', 'buildtype=release'], license: 'GPL3', version: '0.8.1')
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
    'tests/sanity/signal_and_slots.sh',
    'tests/manual-tests/common.py'
)

subdir('SciQLopPlots')

subdir('tests')

