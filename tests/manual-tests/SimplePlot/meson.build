generated_files = qtmod.preprocess(
                            moc_headers : 'simpleplot.h',
                            ui_files : 'simpleplot.ui',
                            dependencies: [qtwidgets])

executable('SimplePlot'
        , 'main.cpp'
        , 'simpleplot.cpp'
        , 'simpleplot.h'
        , generated_files
        , include_directories : common_inc
        , dependencies : [SciQLopPlots_dep, thread_dep]
        , install : true)
