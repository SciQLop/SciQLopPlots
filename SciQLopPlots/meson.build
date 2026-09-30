qtmod = import(qt_sdk)
qtdeps = dependency(qt_sdk, modules : ['Core','Widgets','Gui','Svg','PrintSupport', 'OpenGL'])

cpp_utils_dep = dependency('cpp_utils', fallback : ['cpp_utils', 'cpp_utils_dep'], default_options: [f'with_tests=false'])
hedley_dep = dependency('hedley', main : true, fallback : ['hedley', 'hedley_dep'])
fmt_dep = dependency('fmt', static:true)
gl_dep = dependency('gl')
# NeoQCP's draw/resample zones feed the SciQLopPlots tracer (and Tracy when enabled).
meson.override_dependency('neoqcp-profiling-hook', declare_dependency(
    include_directories: include_directories('../include'),
    compile_args: ['-DNEOQCP_PROFILING_HOOK=<SciQLopPlots/Profiling.hpp>']))
NeoQCP_dep = dependency('NeoQCP', default_options: ['tracy_enable=@0@'.format(get_option('tracy_enable')),
                                                    'unity=on',
                                                    'with_tests=false',
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
# One abi3 wheel per platform: the buffer protocol we read arrays with joined the
# stable ABI in Python 3.11. Everything linked into the modules must agree on it.
py_limited_api = '3.11'
py_limited_api_args = ['-DPy_LIMITED_API=0x030B0000']
# Shiboken's headers need two typedefs that CPython 3.11 keeps out of the limited
# API: supply them when building with 3.11 (see the header).
if python3.language_version().version_compare('<3.12')
    limited_api_compat = meson.current_source_dir() / 'bindings' / 'limited_api_311_compat.h'
    if meson.get_compiler('cpp').get_argument_syntax() == 'msvc'
        py_limited_api_args += ['/FI' + limited_api_compat]
    else
        py_limited_api_args += ['-include', limited_api_compat]
    endif
endif

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

sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py']
foreach py_src:sciqlopplots_python_sources
  configure_file(input:py_src,output:py_src, copy:true)
endforeach

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
    project_source_root + '/include/SciQLopPlots/MultiPlots/CrosshairSynchronizer.hpp',
    project_source_root + '/include/SciQLopPlots/MultiPlots/VPlotsAlign.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp',
    project_source_root + '/include/SciQLopPlots/ColorScaleController.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopCurve.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopColorMap.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp',
    project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/SciQLopCurveResampler.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopPlotItem.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopCrosshair.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopPixmapItem.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopStraightLines.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopShapesItems.hpp',
    project_source_root + '/include/SciQLopPlots/Items/SciQLopTextItem.hpp',
    project_source_root + '/include/SciQLopPlots/DataProducer/DataProducer.hpp',
    project_source_root + '/include/SciQLopPlots/DragNDrop/PlotDragNDropCallback.hpp',
    project_source_root + '/include/SciQLopPlots/DragNDrop/PlaceHolderManager.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Model/Model.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/Model/Node.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/TreeView.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/InspectorView.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertyDelegateBase.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/InspectorExtension.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopMultiPlotPanelDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/InspectorExtensionDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/PropertiesPanel.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/View/PropertiesView.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LineDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LegendDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorGradientDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/MarkerDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsModel.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsNode.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsView.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp',
    project_source_root + '/include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp',
    project_source_root + '/include/SciQLopPlots/Products/QueryHighlighter.hpp',
    project_source_root + '/include/SciQLopPlots/Products/QueryLineEdit.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopOverlay.hpp',
    project_source_root + '/include/SciQLopPlots/SciQLopTheme.hpp',
    project_source_root + '/include/SciQLopPlots/Icons/icons.hpp'
]

moc_sources = []
headers = moc_headers + \
         [project_source_root+'/include/SciQLopPlots/Python/PythonInterface.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Views.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Validation.hpp',
         project_source_root+'/include/SciQLopPlots/Python/MatchedBuffers.hpp',
         project_source_root+'/include/SciQLopPlots/Python/SafeSlot.hpp',
         project_source_root+'/include/SciQLopPlots/constants.hpp',
         project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
         project_source_root+'/include/SciQLopPlots/Products/ScoreMerge.hpp',
         project_source_root+'/include/SciQLopPlots/Products/QueryParser.hpp',
         project_source_root+'/include/SciQLopPlots/Products/ScoreSignalRegistry.hpp']

includes = include_directories(['../include'])

moc_files = qtmod.compile_moc(
                            headers : moc_headers,
                            sources : moc_sources,
                            include_directories : includes)

resources_files = qtmod.compile_resources(sources : ['../resources/resources.qrc'])

# Shiboken-generated sources and PythonInterface.cpp must be compiled
# without unity: PySide6 headers reference protected enums that error
# when merged with other TUs, and PythonInterface.cpp has slots macro conflicts.
bindings_lib = static_library('sciqlopplots_bindings_impl',
        sciqlopplots_bindings_src + shiboken_generator_out + ['../src/PythonInterface.cpp'],
        dependencies : [python3_dep, shiboken_dep, qtdeps, NeoQCP_dep, cpp_utils_dep, fmt_dep, magic_enum_dep],
        include_directories : includes,
        cpp_args: py_limited_api_args,
        # Only PyInit_ must be exported (PyMODINIT_FUNC marks it); abi3audit fails
        # the wheel on shiboken's exported "Python*" converter functions.
        gnu_symbol_visibility: 'inlineshidden',
        override_options: ['unity=off'],
)

sources = moc_files \
        + resources_files \
        + [
            '../src/Profiling.cpp',
            '../src/Tracing.cpp',
            '../src/SciQLopPlotInterface.cpp',
            '../src/SciQLopPlot.cpp',
            '../src/SciQLopPlotLegend.cpp',
            '../src/SciQLopTimeSeriesPlot.cpp',
            '../src/SciQLopNDProjectionPlot.cpp',
            '../src/SciQLopPlotAxis.cpp',
            '../src/SciQLopTheme.cpp',
            '../src/SciQLopPlotCollection.cpp',
            '../src/AxisSynchronizer.cpp',
            '../src/XAxisSynchronizer.cpp',
            '../src/TimeAxisSynchronizer.cpp',
            '../src/CrosshairSynchronizer.cpp',
            '../src/VPlotsAlign.cpp',
            '../src/SciQLopMultiPlotObject.cpp',
            '../src/SciQLopMultiPlotPanel.cpp',
            '../src/SciQLopPlotContainer.cpp',
            '../src/Export/SciQLopExportable.cpp',
            '../src/MultiPlotsVSpan.cpp',
            '../src/SciQLopPixmapItem.cpp',
            '../src/SciQLopShapesItems.cpp',
            '../src/SciQLopTextItem.cpp',
            '../src/SciQLopGraphInterface.cpp',
            '../src/SciQLopGraphComponentInterface.cpp',
            '../src/SciQLopGraphComponent.cpp',
            '../src/QCPAbstractPlottableWrapper.cpp',
            '../src/SciQLopPlotItem.cpp',
            '../src/SpanBase.cpp',
            '../src/SciQLopSpanBase.cpp',
            '../src/SciQLopVerticalSpan.cpp',
            '../src/SciQLopHorizontalSpan.cpp',
            '../src/SciQLopRectangularSpan.cpp',
            '../src/SciQLopLineGraph.cpp',
            '../src/SciQLopMultiGraphBase.cpp',
            '../src/SciQLopWaterfallGraph.cpp',
            '../src/SciQLopSingleLineGraph.cpp',
            '../src/SciQLopTimeColoredCurve.cpp',
            '../src/ColorScaleController.cpp',
            '../src/SciQLopCurve.cpp',
            '../src/SciQLopNDProjectionCurves.cpp',
            '../src/SciQLopColorMapBase.cpp',
            '../src/SciQLopColorMap.cpp',
            '../src/SciQLopHistogram2D.cpp',
            '../src/AbstractResampler.cpp',
            '../src/SciQLopCurveResampler.cpp',
            '../src/SciQLopCrosshair.cpp',
            '../src/SciQLopStraightLines.cpp',
            '../src/DataProducer.cpp',
            '../src/Model.cpp',
            '../src/Node.cpp',
            '../src/TypeRegistry.cpp',
            '../src/DelegateRegistry.cpp',
            '../src/Registrations.cpp',
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
            '../src/SciQLopColorMapBaseDelegate.cpp',
            '../src/SciQLopColorMapDelegate.cpp',
            '../src/SciQLopHistogram2DDelegate.cpp',
            '../src/SciQLopWaterfallDelegate.cpp',
            '../src/SciQLopGraphComponentDelegate.cpp',
            '../src/SciQLopPlotAxisDelegate.cpp',
            '../src/SciQLopTextItemDelegate.cpp',
            '../src/InspectorExtensionDelegate.cpp',
            '../src/ColorDelegate.cpp',
            '../src/FontDelegate.cpp',
            '../src/LineDelegate.cpp',
            '../src/MarkerDelegate.cpp',
            '../src/ColorGradientDelegate.cpp',
            '../src/ProductsModel.cpp',
            '../src/ProductsNode.cpp',
            '../src/ScoreSignalRegistry.cpp',
            '../src/QueryParser.cpp',
            '../src/ProductsTreeFilterModel.cpp',
            '../src/ProductsFlatFilterModel.cpp',
            '../src/QueryHighlighter.cpp',
            '../src/QueryLineEdit.cpp',
            '../src/ProductsView.cpp',
            '../src/ProductsScoreDelegate.cpp',
            '../src/icons.cpp'
        ]

cpp_args = []

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
        dependencies : [ python3_dep, shiboken_dep, qtdeps, NeoQCP_dep, cpp_utils_dep, gl_dep, fmt_dep, magic_enum_dep, dsp_dep] + optional_deps,
        link_whole: [bindings_lib],
        cpp_args: cpp_args,
        limited_api: py_limited_api,
        subdir : 'SciQLopPlots',
        install: true,
        include_directories : includes,
        extra_files: extra_files + sciqlopplots_python_sources +
                     sciqlopplots_bindings_headers + headers +
                     ['bindings/helper_scripts/shiboken-gen.py']
)

python3.install_sources(sciqlopplots_python_sources, pure: false, subdir: 'SciQLopPlots')

# DSP Python extension module (no Qt/Shiboken dependency — pure computation)
numpy_inc = run_command(python3, ['-c', 'import numpy; print(numpy.get_include())'], check: true).stdout().strip()

# Pass numpy's include via -I rather than include_directories(): the latter
# rejects an absolute path that lives inside the source tree, which happens when
# the build venv (hence numpy) sits in-tree (e.g. ./.venv).
sciqlop_dsp_ext = python3.extension_module('_sciqlop_dsp',
    '../src/DSP/python_module.cpp',
    include_directories: [includes],
    cpp_args: ['-I' + numpy_inc],
    dependencies: [python3_dep, dsp_dep],
    limited_api: py_limited_api,
    subdir: 'SciQLopPlots',
    install: true,
)

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

