examples = [
    {'name':'SciQLopGraph', 'source': meson.current_source_dir()+'/SciQLopGraph/main.py'},
    {'name':'SciQLopColorMap', 'source': meson.current_source_dir()+'/SciQLopColorMap/main.py'},
    {'name':'StackedGraphs', 'source': meson.current_source_dir()+'/StackedGraphs/main.py'},
    {'name':'SciQLopVerticalSpan', 'source': meson.current_source_dir()+'/SciQLopVerticalSpan/main.py'},
    {'name':'All', 'source': meson.current_source_dir()+'/All/main.py'},
    {'name':'MicroSciQLop', 'source': meson.current_source_dir()+'/MicroSciQLop/main.py'},
    {'name':'DragNDrop', 'source': meson.current_source_dir()+'/DragNDrop/main.py'},
    {'name':'Products', 'source': meson.current_source_dir()+'/Products/main.py'},
    {'name': 'Projections', 'source': meson.current_source_dir()+'/Projections/main.py'},
    {'name': 'DrawingAPI', 'source': meson.current_source_dir()+'/DrawingAPI/main.py'},
    {'name': 'StressModel', 'source': meson.current_source_dir()+'/StressModel/main.py'},
]

foreach example:examples
    test('examples_'+example['name'],
        python3,
        args: [example['source']],
        env: ['PYTHONPATH='+meson.project_build_root()]
    )
endforeach

sources = []
foreach example:examples
    sources += example['source']
endforeach

if target_machine.system() != 'windows'
    library('SimplePlot_fake',[],
            extra_files:sources)
endif
