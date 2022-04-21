moc_headers = [
    '../include/SciQLopPlots/Qt/SyncPanel.hpp',
    '../include/SciQLopPlots/Interfaces/IPlotWidget.hpp',
    '../include/SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp'
]

moc_sources = ['IPlotWidget.cpp', 'Qt/QCustomPlot/QCustomPlotWrapper.cpp']

SciQLopPlots_moc_files = qtmod.preprocess(
                            moc_headers : moc_headers,
                            moc_sources : moc_sources,
                            include_directories : ['../include'],
                            dependencies: [QCustomPlot_dep, range_dep])


headers = moc_headers + [
    '../include/SciQLopPlots/view.hpp',
    '../include/SciQLopPlots/enums.hpp',
    '../include/SciQLopPlots/axis_range.hpp',
    '../include/SciQLopPlots/Qt/Events/Keyboard.hpp',
    '../include/SciQLopPlots/Qt/Events/Mouse.hpp',
    '../include/SciQLopPlots/Qt/Events/Wheel.hpp',
    '../include/SciQLopPlots/Qt/Graph.hpp',
    '../include/SciQLopPlots/Interfaces/PlotWidget.hpp',
    '../include/SciQLopPlots/Qt/SyncPanel.hpp'
]

SciQLopPlots = library('SciQLopPlots', moc_sources,
                       SciQLopPlots_moc_files, include_directories: ['../include'],
                       extra_files: [headers],
                       dependencies: [QCustomPlot_dep, cpp_utils_dep, range_dep, channels_dep, hedley_dep])

SciQLopPlots_dep = declare_dependency(include_directories: ['../include'], link_with: SciQLopPlots, dependencies: [QCustomPlot_dep, cpp_utils_dep, range_dep, channels_dep, hedley_dep])
