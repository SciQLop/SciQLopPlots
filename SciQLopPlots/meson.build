qtmod = import(qt_sdk)
qtdeps = dependency(qt_sdk, modules : ['Core','Widgets','Gui','Svg','PrintSupport', 'OpenGL'], method:'qmake')

cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'], default_options: [f'with_tests=false'])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])
fmt_dep = dependency('fmt', static:true)
gl_dep = dependency('gl')

qmake_possible_names = ['qmake-qt6','qmake6','qmake']
pyside_version = '6'


project_source_root = meson.project_source_root()
current_source_dir = meson.current_source_dir()

src_list = find_program('bindings/helper_scripts/src_list.py')
shiboken_helper = find_program('bindings/helper_scripts/shiboken-helper.py')
shiboken_gen = find_program('bindings/helper_scripts/shiboken-gen.py')


shiboken = find_program(f'shiboken@pyside_version@')
qmake = find_program(qmake_possible_names)

pymod = import('python')
python3 = pymod.find_installation('python3', modules:[f'PySide@pyside_version@',
                                                      f'shiboken@pyside_version@',
                                                      f'shiboken@pyside_version@_generator',
                                                      'numpy'])
python3_dep = python3.dependency(embed:false)

qt_modules = ['QtCore','QtGui','QtWidgets']

generated_srcs  = run_command(python3, src_list, '--build-system', 'meson', '--bindings', f'@current_source_dir@/bindings/bindings.xml').stdout().split(';')

modules_arg = '--modules=@0@'.format(','.join(qt_modules))

shiboken_build_flags = run_command(python3, shiboken_helper, '--qmake', qmake.full_path(), '--includes', modules_arg).stdout().strip('\n').split(' ')
shiboken_link_flags_out = run_command(python3, shiboken_helper, '--qmake', qmake.full_path(), '--build-dir', meson.current_build_dir(), '--libs', modules_arg)
shiboken_link_flags  = shiboken_link_flags_out.stdout().strip('\n').split(' ')
message('shiboken_build_flags: @0@'.format(shiboken_link_flags_out.stderr()))
shiboken_typesystem  = run_command(python3, shiboken_helper, '--qmake', qmake.full_path(), '--typesystem').stdout().strip('\n')


sciqlopplots_bindings_src = []
sciqlopplots_bindings_headers = files('bindings/bindings.h')

sciqlopplots_python_sources = ['__init__.py']
configure_file(input:'__init__.py',output:'__init__.py', copy:true)

subdir('bindings/SciQLopPlotsBindings')

shiboken_dep = declare_dependency(compile_args: shiboken_build_flags, link_args: shiboken_link_flags)

moc_headers = [
    'bindings/_QCustomPlot.hpp',
    project_source_root + '/include/SciQLopPlots/QCPAbstractPlottableWrapper.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopGraph.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopCurve.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopColorMap.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopVerticalSpan.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlotItem.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlot.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopGraphResampler.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopCurveResampler.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopColorMapResampler.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopTracer.hpp',
    project_source_root + '/include/SciQLopPlots/QCPItemRichText.hpp',
    project_source_root + '/qcustomplot-source/qcustomplot.h'
]

moc_sources = []
headers = moc_headers + \
         [project_source_root+'/include/SciQLopPlots/BufferProtocol.hpp',
         project_source_root+'/include/SciQLopPlots/constants.hpp']

includes = include_directories(['../include', '../qcustomplot-source'])

moc_files = qtmod.preprocess(
                            moc_headers : moc_headers,
                            moc_sources : moc_sources,
                            include_directories : includes)


sources = moc_files \
        + sciqlopplots_bindings_src \
        + shiboken_generator_out \
        + [ '../src/SciQLopPlot.cpp',
            '../src/QCPAbstractPlottableWrapper.cpp',
            '../src/SciQLopPlotItem.cpp',
            '../src/SciQLopVerticalSpan.cpp',
            '../src/SciQLopGraph.cpp',
            '../src/SciQLopGraphResampler.cpp',
            '../src/SciQLopCurve.cpp',
            '../src/SciQLopCurveResampler.cpp',
            '../src/SciQLopColorMap.cpp',
            '../src/SciQLopColorMapResampler.cpp',
            '../src/BufferProtocol.cpp',
            '../src/SciQLopTracer.cpp',
            '../src/QCPItemRichText.cpp',
            '../qcustomplot-source/qcustomplot.cpp'
        ]

sciqlopplots_bindings = python3.extension_module('SciQLopPlotsBindings',
        sources,
        dependencies : [ python3_dep, shiboken_dep, qtdeps, cpp_utils_dep, gl_dep, fmt_dep],
        cpp_args:['-DQCUSTOMPLOT_USE_OPENGL'],
        subdir : 'SciQLopPlots',
        install: true,
        include_directories : includes,
        extra_files: extra_files + sciqlopplots_python_sources +
                     sciqlopplots_bindings_headers + headers +
                     ['bindings/snippets.cpp', 'bindings/helper_scripts/shiboken-gen.py']
)

python3.install_sources(sciqlopplots_python_sources, pure: false, subdir: 'SciQLopPlots')

summary(
    {
        'QMake': qmake.full_path(),
        'Pyside Version': pyside_version,
        'shiboken path': shiboken.full_path(),
        'shiboken_build_flags':shiboken_build_flags,
        'shiboken_link_flags':shiboken_link_flags,
        'shiboken_typesystem':shiboken_typesystem
    },
    section:'Pyside'
)

