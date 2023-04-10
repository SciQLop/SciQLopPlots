project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++17', 'buildtype=release'], license: 'GPL3', version: '0.2.2')
add_project_arguments(
            '-DCATCH_CONFIG_NO_POSIX_SIGNALS', # workaround for this https://github.com/catchorg/Catch2/issues/2192
            language: 'cpp',
            native: true
)
qt_sdk='qt6'

extra_files=files(
    'pyproject.toml',
    'setup.cfg',
    '.github/workflows/pythonpublish-linux.yml',
    '.github/workflows/pythonpublish-win.yml',
    '.github/workflows/pythonpublish-osx.yml',
    'README.md', 'COPYING'
)

subdir('SciQLopPlots')

subdir('tests')

