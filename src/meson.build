SciQLopPlots_moc_files = qtmod.preprocess(
                            moc_headers : ['SyncPanel.hpp','IPlotWidget.hpp', 'QCustomPlotWrapper.hpp'],
                            moc_sources : ['IPlotWidget.cpp', 'QCustomPlotWrapper.cpp'],
                            dependencies: [QCustomPlot_dep, range_dep])


headers = [
    '../include/SciQLopPlots/view.hpp',
    '../include/SciQLopPlots/enums.hpp',
    '../include/SciQLopPlots/axis_range.hpp',
    'Events/Keyboard.hpp',
    'Events/Mouse.hpp',
    'Events/Wheel.hpp',
    'Graph.hpp',
    'SciQLopPlot.hpp',
    'SyncPanel.hpp',
    'IPlotWidget.hpp',
    'QCustomPlotWrapper.hpp'
]

SciQLopPlots = library('SciQLopPlots', 'IPlotWidget.cpp', 'QCustomPlotWrapper.cpp',
                       SciQLopPlots_moc_files, include_directories: ['.', '../include'],
                       extra_files: [headers],
                       dependencies: [QCustomPlot_dep, cpp_utils_dep, range_dep, channels_dep, hedley_dep])

SciQLopPlots_dep = declare_dependency(include_directories: ['.', '../include'], link_with: SciQLopPlots, dependencies: [QCustomPlot_dep, cpp_utils_dep, range_dep, channels_dep, hedley_dep])
