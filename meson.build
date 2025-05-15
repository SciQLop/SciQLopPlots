project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++20', 'buildtype=release'], license: 'GPL3', version: '0.15.1')
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

# enables fast-math optimizations
#if meson.get_compiler('c').get_id() == 'msvc'
#    add_project_arguments('/fp:fast', language: 'cpp')
#else
#    add_project_arguments('-ffast-math', language: 'cpp')
#endif


subdir('SciQLopPlots')

subdir('tests')

