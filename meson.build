project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++17'], license: 'GPL3', version: '0.0.1')
add_global_arguments(
            '-DCATCH_CONFIG_NO_POSIX_SIGNALS', # workaround for this https://github.com/catchorg/Catch2/issues/2192
            language: 'cpp',
            native: true
)

qt5 = import('qt5')
qt5core = dependency('qt5', modules : 'Core')
qt5widgets = dependency('qt5', modules : 'Widgets')
qt5gui = dependency('qt5', modules : 'Gui')
qt5svg = dependency('qt5', modules : 'Svg')
qt5test = dependency('qt5', modules : 'Test')

QCustomPlot_dep = dependency('QCustomPlot', fallback : ['QCustomPlot', 'QCustomPlot_dep'])
cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'])
range_dep = dependency('range-v3', fallback : ['range-v3', 'range_dep'])
channels_dep = dependency('channels', fallback : ['channels', 'channels_dep'])
QtGuiTestUtils_dep = dependency('QtGuiTestUtils', fallback : ['QtGuiTestUtils', 'QtGuiTestUtils_dep'])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])


subdir('src')
subdir('tests')
