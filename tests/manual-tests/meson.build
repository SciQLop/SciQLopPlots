examples = [
    {'name':'plots', 'source': meson.current_source_dir()+'/QCP_Examples/plots/main.py'},
    {'name':'SciQLopGraph', 'source': meson.current_source_dir()+'/SciQLopGraph/main.py'},
    {'name':'SciQLopColorMap', 'source': meson.current_source_dir()+'/SciQLopColorMap/main.py'},
    {'name':'StackedGraphs', 'source': meson.current_source_dir()+'/StackedGraphs/main.py'},
    {'name':'SciQLopVerticalSpan', 'source': meson.current_source_dir()+'/SciQLopVerticalSpan/main.py'},
]

foreach example:examples
    run_target('examples_'+example['name'],
        command:[python3, example['source']],
        env: ['PYTHONPATH='+meson.project_build_root()]
    )
endforeach

sources = []
foreach example:examples
    sources += example['source']
endforeach

library('SimplePlot_fake',[],
        extra_files:sources)
