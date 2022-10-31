moc_headers = [
    '../include/SciQLopPlots/plot.hpp',
    '../include/SciQLopPlots/graph.hpp',
    '../include/SciQLopPlots/sync_panel.hpp',
    '../include/SciQLopPlots/time_span.hpp',
    '../include/SciQLopPlots/Interfaces/IPlotWidget.hpp',
    '../include/SciQLopPlots/Interfaces/IGraph.hpp',
    '../include/SciQLopPlots/Interfaces/GraphicObjects/ITimeSpan.hpp',
    '../include/SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp',
    '../include/SciQLopPlots/Qt/QCustomPlot/QCPTimeSpan.hpp'
]

moc_sources = ['plot.cpp', 'graph.cpp', 'sync_panel.cpp', 'time_span.cpp', 'Interfaces/GraphicObjects/ITimeSpan.cpp', 'Interfaces/IPlotWidget.cpp','Interfaces/IGraph.cpp', 'Qt/QCustomPlot/QCustomPlotWrapper.cpp']

SciQLopPlots_moc_files = qtmod.preprocess(
                            moc_headers : moc_headers,
                            moc_sources : moc_sources,
                            include_directories : ['../include'],
                            dependencies: [QCustomPlot_dep])


headers = moc_headers + [
    '../include/SciQLopPlots/view.hpp',
    '../include/SciQLopPlots/enums.hpp',
    '../include/SciQLopPlots/axis_range.hpp',
    '../include/SciQLopPlots/Qt/Events/Keyboard.hpp',
    '../include/SciQLopPlots/Qt/Events/Mouse.hpp',
    '../include/SciQLopPlots/Qt/Events/Wheel.hpp',
    '../include/SciQLopPlots/Interfaces/GraphicObjects/GraphicObject.hpp',
    '../include/SciQLopPlots/Interfaces/GraphicObjects/LayeredGraphicObjectCollection.hpp',
    '../include/SciQLopPlots/Qt/QCustomPlot/SciQLopPlots.hpp'
]

sources = ['Interfaces/GraphicObjects/GraphicObject.cpp']

SciQLopPlots = library('SciQLopPlots', sources, moc_sources,
                       SciQLopPlots_moc_files, include_directories: ['../include'],
                       extra_files: [headers],
                       dependencies: [QCustomPlot_dep, cpp_utils_dep, hedley_dep])

SciQLopPlots_src_dep = declare_dependency(sources:[sources, moc_sources,SciQLopPlots_moc_files] ,include_directories: ['../include'], dependencies: [QCustomPlot_dep, cpp_utils_dep, hedley_dep])
