generated_files = qt5.preprocess(
                            moc_headers : 'simpleplot.h',
                            ui_files : 'simpleplot.ui',
                            dependencies: [qt5widgets, qt5concurent])
thread_dep = dependency('threads')

executable('SimplePlot'
        , 'main.cpp'
        , 'simpleplot.cpp'
        , generated_files
        , dependencies : [SciQLopPlots_dep, qt5concurent, thread_dep]
        , install : true)
