# SciQLopPlots for the browser (Qt for WebAssembly): sciqlopplots.js + .wasm, the embind layer
# in sciqlopplots_js.cpp, wrapped by the ES module sciqlopplots.mjs (the API pages use).
#
# From the repository root:
#   source ~/emsdk/emsdk_env.sh                      # Emscripten 4.0.7 for Qt 6.11
#   printf "[constants]\nqt_wasm = '%s'\n" ~/Qt/6.11.2/wasm_multithread > qt.ini
#   meson setup build-wasm --cross-file qt.ini --cross-file wasm/emscripten.ini --buildtype=minsize
#   meson compile -C build-wasm
#   wasm/build_site.sh build-wasm _site               # library + gallery + tests
#   (cd _site && python3 ../wasm/serve.py 8766)       # http://127.0.0.1:8766/
#   xvfb-run -a -s "-screen 0 1400x1000x24" node wasm/tests/run.mjs _site   # browser tests

python3 = find_program('python3')
qmake = find_program('qmake6', 'qmake')
qt_wasm = run_command(qmake, '-query', 'QT_INSTALL_PREFIX', check: true).stdout().strip()

# Qt is static here, and meson links only its module archives: the bundled libraries, the
# resource objects and the plugins come from Qt's .prl files, as Qt's CMake does.
# Image formats: the embedded icons are SVG and PNG (built into QtGui), nothing else.
qt_static_link = run_command(python3, files('qt_static_link.py'), qt_wasm,
    'Svg', 'PrintSupport', 'Widgets', 'OpenGL', 'Gui', 'Core',
    '--plugins', 'platforms/qwasm', 'imageformats/qsvg', 'iconengines/qsvgicon',
    check: true).stdout().strip().split('\n')

# What qt_add_executable sets for a wasm target: the page's qtloader.js calls
# sciqlopplots_entry, and threads run on a pool of web workers.
emscripten_link_args = [
    '-sMODULARIZE=1', '-sEXPORT_NAME=sciqlopplots_entry',
    '-sEXPORTED_RUNTIME_METHODS=UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS,callMain',
    '-sEXPORTED_FUNCTIONS=_main,__embind_initialize_bindings',
    '-sPTHREAD_POOL_SIZE=4', '-sINITIAL_MEMORY=256MB', '-sMAXIMUM_MEMORY=4GB',
    '-sALLOW_MEMORY_GROWTH', '-sSTACK_SIZE=5MB', '-sMAX_WEBGL_VERSION=2', '-sWASM_BIGINT=1',
    '-sERROR_ON_UNDEFINED_SYMBOLS=1', '-sFETCH', '-lembind',
    # C++ catch blocks run (the cross file compiles them in), and an exception that reaches the
    # page is an Error carrying what(), not a bare pointer.
    '-fexceptions', '-sEXCEPTION_STACK_TRACES=1',
]
# Emscripten optimises the wasm (wasm-opt) and minifies the JavaScript at link time.
if get_option('optimization') not in ['plain', '0']
    emscripten_link_args += '-O' + get_option('optimization')
endif

moc_files = qtmod.compile_moc(headers: sciqlopplots_moc_headers,
                              include_directories: sciqlopplots_includes)
resources_files = qtmod.compile_resources(sources: sciqlopplots_resources)
# The products search runs on it; the Python build gets it through the DSP module.
thread_pool_dep = dependency('bshoshany-thread-pool',
    fallback: ['thread-pool', 'bshoshany_thread_pool_dep'])

executable('sciqlopplots',
    'sciqlopplots_js.cpp', 'python_interface.cpp',
    sciqlopplots_sources, moc_files, resources_files,
    include_directories: sciqlopplots_includes,
    dependencies: [qtdeps, NeoQCP_dep, cpp_utils_dep, hedley_dep, fmt_dep, magic_enum_dep,
                    thread_pool_dep],
    cpp_args: '-DSCIQLOPPLOTS_VERSION="@0@"'.format(meson.project_version()),
    link_args: qt_static_link + emscripten_link_args)

fs = import('fs')
fs.copyfile(qt_wasm / 'plugins/platforms/qtloader.js')
