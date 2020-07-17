generated_files = qt5.preprocess(
                            moc_headers : 'simpleplot.h',
                            ui_files : 'simpleplot.ui',
                            dependencies: [qt5widgets])

executable('SimplePlot'
        , 'main.cpp'
        , 'simpleplot.cpp'
        , 'simpleplot.h'
        , generated_files
        , include_directories : common_inc
        , dependencies : [SciQLopPlots_dep, thread_dep]
        , install : true)
