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

sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py', 'widget_plot.py']
foreach py_src:sciqlopplots_python_sources
  configure_file(input:py_src,output:py_src, copy:true)
endforeach

subdir('bindings/SciQLopPlotsBindings')

shiboken_dep = declare_dependency(compile_args: shiboken_build_flags, link_args: shiboken_link_flags)

moc_headers = ['bindings/_QCustomPlot.hpp'] + sciqlopplots_moc_headers

moc_sources = []
headers = moc_headers + sciqlopplots_headers

includes = include_directories(['../include'])

moc_files = qtmod.compile_moc(
                            headers : moc_headers,
                            sources : moc_sources,
                            include_directories : includes)

resources_files = qtmod.compile_resources(sources : sciqlopplots_resources + sciqlopplots_logos)
# Every compiled .qrc defines qt_resource_data: two of them in one unity chunk collide.
resources_lib = static_library('sciqlopplots_resources', resources_files,
        dependencies : qtdeps,
        override_options: ['unity=off'],
)

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

sources = moc_files + sciqlopplots_sources

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
        link_whole: [bindings_lib, resources_lib],
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

