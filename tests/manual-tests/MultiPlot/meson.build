generated_files = qt5.preprocess(
                            moc_headers : 'multiplot.h',
                            dependencies: [qt5widgets])

executable('MultiPlot'
        , 'main.cpp'
        , 'multiplot.cpp'
        , 'multiplot.h'
        , generated_files
        , include_directories : common_inc
        , dependencies : [SciQLopPlots_dep, thread_dep]
        , install : true)
