examples = [
    {'name': 'LineGraph', 'source': meson.current_source_dir() + '/basics/line_graph.py'},
    {'name': 'Colormap', 'source': meson.current_source_dir() + '/basics/colormap.py'},
    {'name': 'VerticalSpans', 'source': meson.current_source_dir() + '/basics/vertical_spans.py'},
    {'name': 'ParametricCurve', 'source': meson.current_source_dir() + '/basics/parametric_curve.py'},
    {'name': 'Pipeline', 'source': meson.current_source_dir() + '/advanced/pipeline.py'},
    {'name': 'StackedPlots', 'source': meson.current_source_dir() + '/advanced/stacked_plots.py'},
    {'name': 'DragNDrop', 'source': meson.current_source_dir() + '/advanced/drag_and_drop.py'},
    {'name': 'Products', 'source': meson.current_source_dir() + '/advanced/products.py'},
    {'name': 'StressModel', 'source': meson.current_source_dir() + '/stress/stress_model.py'},
]

foreach example : examples
    test('examples_' + example['name'],
        python3,
        args: [example['source']],
        env: ['PYTHONPATH=' + meson.project_build_root()],
        suite: 'manual',
    )
endforeach

sources = []
foreach example : examples
    sources += example['source']
endforeach

if target_machine.system() != 'windows'
    library('SimplePlot_fake', [],
            extra_files: sources)
endif
