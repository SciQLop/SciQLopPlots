SciQLopPlots_moc_files = qt5.preprocess(
                            moc_headers : ['SyncPanel.hpp','IPlotWidget.hpp', 'QCustomPlotWrapper.hpp'],
                            moc_sources : ['IPlotWidget.cpp', 'QCustomPlotWrapper.cpp'],
                            dependencies: [QCustomPlot_dep, range_dep])


SciQLopPlots = library('SciQLopPlots', 'IPlotWidget.cpp', 'QCustomPlotWrapper.cpp',
                       SciQLopPlots_moc_files, include_directories: '.',
                       dependencies: [QCustomPlot_dep, cpp_utils_dep, range_dep, channels_dep])

SciQLopPlots_dep = declare_dependency(include_directories: '.', link_with: SciQLopPlots, dependencies: [QCustomPlot_dep, cpp_utils_dep, range_dep, channels_dep])
