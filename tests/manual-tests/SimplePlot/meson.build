test('main.py', python3,
    env:['PYTHONPATH='+meson.project_build_root()+'/bindings']
)
library('SimplePlot_fake',[],
        extra_files:['main.py'])
