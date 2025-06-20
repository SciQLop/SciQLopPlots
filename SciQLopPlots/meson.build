qtmod = import(qt_sdk)
qtdeps = dependency(qt_sdk, modules : ['Core','Widgets','Gui','Svg','PrintSupport', 'OpenGL'], method:'qmake')

cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'], default_options: [f'with_tests=false'])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])
fmt_dep = dependency('fmt', static:true)
gl_dep = dependency('gl')
NeoQCP_dep = dependency('NeoQCP', default_options: ['tracy_enable=@0@'.format(get_option('tracy_enable')),
                                                    'with_opengl=@0@'.format(get_option('with_opengl'))
                                                    ])
magic_enum_dep = dependency('magic_enum', static:true)

qmake_possible_names = ['qmake-qt6','qmake6','qmake']
pyside_version = '6'


project_source_root = meson.project_source_root()
current_source_dir = meson.current_source_dir()

src_list = find_program('bindings/helper_scripts/src_list.py')
shiboken_helper = find_program('bindings/helper_scripts/shiboken-helper.py')
shiboken_gen = find_program('bindings/helper_scripts/shiboken-gen.py')

shiboken = find_program(f'shiboken@pyside_version@')
qmake = find_program(qmake_possible_names)

message(find_program('python3', required:true).full_path())


pymod = import('python')
python3 = pymod.find_installation('python3', modules:[f'PySide@pyside_version@',
                                                      f'shiboken@pyside_version@',
                                                      f'shiboken@pyside_version@_generator',
                                                      'numpy'])
python3_dep = python3.dependency(embed:false)

qt_modules = ['QtCore','QtGui','QtWidgets']

generated_srcs  = run_command(python3, src_list, '--build-system', 'meson', '--bindings', f'@current_source_dir@/bindings/bindings.xml').stdout().split(';')

modules_arg = '--modules=@0@'.format(','.join(qt_modules))

shiboken_build_flags = run_command(python3, shiboken_helper, '--qmake', qmake.full_path(), '--includes', modules_arg).stdout().strip('\n').split(';')
shiboken_link_flags_out = run_command(python3, shiboken_helper, '--qmake', qmake.full_path(), '--build-dir', meson.current_build_dir(), '--libs', modules_arg)
shiboken_link_flags  = shiboken_link_flags_out.stdout().strip('\n').split(';')
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
    project_source_root + '/include/SciQLopPlots/helpers.hpp',
    project_source_root + '/include/SciQLopPlots/Debug.hpp',
    project_source_root + '/include/SciQLopPlots/Profiling.hpp',
    project_source_root + '/include/SciQLopPlots/unique_names_factory.hpp',
    project_source_root + '/include/SciQLopPlots/enums.hpp',
    project_source_root + '/include/SciQLopPlots/qcp_enums.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlotRange.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlotInterface.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlotLegendInterface.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlotLegend.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlot.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopTimeSeriesPlot.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopNDProjectionPlot.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopPlotAxis.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/SciQLopMultiPlotObject.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/AxisSynchronizer.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/XAxisSynchronizer.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/TimeAxisSynchronizer.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/VPlotsAlign.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopCurve.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopColorMap.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/SciQLopLineGraphResampler.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/SciQLopCurveResampler.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/SciQLopColorMapResampler.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopPlotItem.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopTracer.hpp',
    project_source_root + '/include/SciQLopPlots/Items/QCPItemRichText.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopPixmapItem.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopStraightLines.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopShapesItems.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopTextItem.hpp',
    project_source_root + '/include/SciQLopPlots/DataProducer/DataProducer.hpp',
    project_source_root + '/include/SciQLopPlots/DragNDrop/PlotDragNDropCallback.hpp',
    project_source_root + '/include/SciQLopPlots/DragNDrop/PlaceHolderManager.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/InspectorBase.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Model/Model.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Model/Node.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopMultiPlotPanelInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopPlotInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopNDProjectionPlotInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopNDProjectionCurvesInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopGraphInterfaceInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopGraphComponentInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopColorMapInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopPlotAxisInspector.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/TreeView.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/InspectorView.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertyDelegateBase.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertyDelegates.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopMultiPlotPanelDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/PropertiesPanel.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/PropertiesView.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LineDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LegendDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorGradientDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/MarkerDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsModel.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsNode.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsView.hpp',
    project_source_root + '/include/SciQLopPlots/Icons/icons.hpp'
]

moc_sources = []
headers = moc_headers + \
         [project_source_root+'/include/SciQLopPlots/Python/PythonInterface.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Views.hpp',
         project_source_root+'/include/SciQLopPlots/constants.hpp']

includes = include_directories(['../include'])

moc_files = qtmod.compile_moc(
                            headers : moc_headers,
                            sources : moc_sources,
                            include_directories : includes)

resources_files = qtmod.compile_resources(sources : ['../resources/resources.qrc'])

sources = moc_files \
        + resources_files \
        + sciqlopplots_bindings_src \
        + shiboken_generator_out \
        + [
            '../src/Profiling.cpp',
            '../src/PythonInterface.cpp',
            '../src/SciQLopPlotInterface.cpp',
            '../src/SciQLopPlot.cpp',
            '../src/SciQLopPlotLegend.cpp',
            '../src/SciQLopTimeSeriesPlot.cpp',
            '../src/SciQLopNDProjectionPlot.cpp',
            '../src/SciQLopPlotAxis.cpp',
            '../src/SciQLopPlotCollection.cpp',
            '../src/AxisSynchronizer.cpp',
            '../src/XAxisSynchronizer.cpp',
            '../src/TimeAxisSynchronizer.cpp',
            '../src/VPlotsAlign.cpp',
            '../src/SciQLopMultiPlotObject.cpp',
            '../src/SciQLopMultiPlotPanel.cpp',
            '../src/SciQLopPlotContainer.cpp',
            '../src/MultiPlotsVSpan.cpp',
            '../src/SciQLopPixmapItem.cpp',
            '../src/SciQLopShapesItems.cpp',
            '../src/SciQLopTextItem.cpp',
            '../src/SciQLopGraphInterface.cpp',
            '../src/SciQLopGraphComponentInterface.cpp',
            '../src/SciQLopGraphComponent.cpp',
            '../src/QCPAbstractPlottableWrapper.cpp',
            '../src/SciQLopPlotItem.cpp',
            '../src/SciQLopVerticalSpan.cpp',
            '../src/SciQLopLineGraph.cpp',
            '../src/SciQLopCurve.cpp',
            '../src/SciQLopNDProjectionCurves.cpp',
            '../src/SciQLopColorMap.cpp',
            '../src/AbstractResampler.cpp',
            '../src/SciQLopLineGraphResampler.cpp',
            '../src/SciQLopCurveResampler.cpp',
            '../src/SciQLopColorMapResampler.cpp',
            '../src/SciQLopTracer.cpp',
            '../src/QCPItemRichText.cpp',
            '../src/SciQLopStraightLines.cpp',
            '../src/DataProducer.cpp',
            '../src/InspectorBase.cpp',
            '../src/Inspectors.cpp',
            '../src/Model.cpp',
            '../src/Node.cpp',
            '../src/SciQLopMultiPlotPanelInspector.cpp',
            '../src/SciQLopPlotInspector.cpp',
            '../src/SciQLopNDProjectionPlotInspector.cpp',
            '../src/SciQLopNDProjectionCurvesInspector.cpp',
            '../src/SciQLopGraphInterfaceInspector.cpp',
            '../src/SciQLopGraphComponentInspector.cpp',
            '../src/SciQLopColorMapInspector.cpp',
            '../src/SciQLopPlotAxisInspector.cpp',
            '../src/TreeView.cpp',
            '../src/InspectorView.cpp',
            '../src/PropertyDelegateBase.cpp',
            '../src/SciQLopMultiPlotPanelDelegate.cpp',
            '../src/PropertiesPanel.cpp',
            '../src/PropertiesView.cpp',
            '../src/BooleanDelegate.cpp',
            '../src/SciQLopPlotDelegate.cpp',
            '../src/LegendDelegate.cpp',
            '../src/SciQLopGraphDelegate.cpp',
            '../src/SciQLopColorMapDelegate.cpp',
            '../src/SciQLopGraphComponentDelegate.cpp',
            '../src/SciQLopPlotAxisDelegate.cpp',
            '../src/ColorDelegate.cpp',
            '../src/LineDelegate.cpp',
            '../src/MarkerDelegate.cpp',
            '../src/ColorGradientDelegate.cpp',
            '../src/ProductsModel.cpp',
            '../src/ProductsNode.cpp',
            '../src/ProductsView.cpp',
            '../src/icons.cpp'
           # '../qcustomplot-source/qcustomplot.cpp'
        ]

cpp_args = []

#if target_machine.system() != 'darwin'
#    cpp_args += ['-DQCUSTOMPLOT_USE_OPENGL']
#endif

if get_option('trace_refcount') == true
    cpp_args += ['-D_TRACE_REF_COUNT']
endif

optional_deps = []

if get_option ( 'tracy_enable')
    if get_option ( 'buildtype') != 'debugoptimized'
        warning ( 'Profiling builds should set -- buildtype = debugoptimized')
    endif
    tracy = dependency ( 'tracy', static : true )
    optional_deps += [ tracy ]
endif

sciqlopplots_bindings = python3.extension_module('SciQLopPlotsBindings',
        sources,
        dependencies : [ python3_dep, shiboken_dep, qtdeps, NeoQCP_dep, cpp_utils_dep, gl_dep, fmt_dep, magic_enum_dep] + optional_deps,
        cpp_args: cpp_args,
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

