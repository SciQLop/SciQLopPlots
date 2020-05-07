project('SciQLopPlots', 'cpp',default_options : ['cpp_std=c++17'])

qt5 = import('qt5')
qt5core = dependency('qt5', modules : 'Core')
qt5widgets = dependency('qt5', modules : 'Widgets')
qt5gui = dependency('qt5', modules : 'Gui')
qt5svg = dependency('qt5', modules : 'Svg')
qt5test = dependency('qt5', modules : 'Test')

QCustomPlot_dep = dependency('QCustomPlot', fallback : ['QCustomPlot', 'QCustomPlot_dep'])
cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'])

SciQLopPlots_moc_files = qt5.preprocess(
                            moc_headers : 'src/SciQLopPlot.hpp',
                            moc_sources : 'src/SciQLopPlot.cpp',
                            dependencies: [QCustomPlot_dep])


SciQLopPlots = library('SciQLopPlots', 'src/SciQLopPlot.cpp',
                       SciQLopPlots_moc_files, include_directories: 'src',
                       dependencies: [QCustomPlot_dep, cpp_utils_dep])

SciQLopPlots_dep = declare_dependency(include_directories: 'src', link_with: SciQLopPlots, dependencies: [QCustomPlot_dep, cpp_utils_dep])

subdir('src')
subdir('tests')
subdir('manual-tests')
