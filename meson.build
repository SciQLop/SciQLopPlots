project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++17'], license: 'GPL3', version: '0.0.3')
add_project_arguments(
            '-DCATCH_CONFIG_NO_POSIX_SIGNALS', # workaround for this https://github.com/catchorg/Catch2/issues/2192
            language: 'cpp',
            native: true
)

qt_sdk=get_option('qt_version')
if qt_sdk=='auto'
    qt_version=run_command(find_program('qmake'), '-query', 'QT_VERSION').stdout()
    if qt_version.startswith('5.')
        qt_sdk='qt5'
    elif qt_version.startswith('6.')
        qt_sdk='qt6'
    endif
endif

with_tests = get_option('with_tests')


qtmod = import(qt_sdk)
qtcore = dependency(qt_sdk, modules : 'Core')
qtwidgets = dependency(qt_sdk, modules : 'Widgets')
qtgui = dependency(qt_sdk, modules : 'Gui')
qtsvg = dependency(qt_sdk, modules : 'Svg')

QCustomPlot_dep = dependency('QCustomPlot_src', default_options: [f'qt_version=@qt_sdk@', f'with_tests=@with_tests@', 'build_libs=false'])
cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'], default_options: [f'with_tests=@with_tests@'])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])


subdir('src')
subdir('bindings')

if build_machine.system() == 'linux'
    library('python_packaging',[],
            extra_files:['pyproject.toml',
            '.github/workflows/pythonpublish-linux.yml',
            '.github/workflows/pythonpublish-win.yml',
            '.github/workflows/pythonpublish-osx.yml',
            'README.md', 'COPYING'])
endif

if with_tests
    qttest = dependency(qt_sdk, modules : 'Test')
    QtGuiTestUtils_dep = dependency('QtGuiTestUtils', fallback : ['QtGuiTestUtils', 'QtGuiTestUtils_dep'], default_options: ['qt_version='+qt_sdk])
    subdir('tests')
endif
