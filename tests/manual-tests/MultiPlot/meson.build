generated_files = qtmod.preprocess(
                            moc_headers : 'multiplot.h',
                            dependencies: [qtwidgets])

executable('MultiPlot'
        , 'main.cpp'
        , 'multiplot.cpp'
        , 'multiplot.h'
        , generated_files
        , include_directories : common_inc
        , dependencies : [SciQLopPlots_dep, thread_dep]
        , install : true)
