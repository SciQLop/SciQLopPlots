project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++17'], license: 'GPL3', version: '0.0.1')
add_global_arguments(
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


qtmod = import(qt_sdk)
qtcore = dependency(qt_sdk, modules : 'Core')
qtwidgets = dependency(qt_sdk, modules : 'Widgets')
qtgui = dependency(qt_sdk, modules : 'Gui')
qtsvg = dependency(qt_sdk, modules : 'Svg')
qttest = dependency(qt_sdk, modules : 'Test')

QCustomPlot_dep = dependency('QCustomPlot', fallback : ['QCustomPlot', 'QCustomPlot_dep'], default_options: ['qt_version='+qt_sdk])
cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'])
range_dep = dependency('range-v3', fallback : ['range-v3', 'range_dep'])
channels_dep = dependency('channels', fallback : ['channels', 'channels_dep'])
QtGuiTestUtils_dep = dependency('QtGuiTestUtils', fallback : ['QtGuiTestUtils', 'QtGuiTestUtils_dep'], default_options: ['qt_version='+qt_sdk])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])


subdir('src')
subdir('tests')
