if qt_sdk == 'qt5'
    qmake_possible_names = ['qmake','qmake-qt5']
    pyside_version = '2'
elif qt_sdk == 'qt6'
    qmake_possible_names = ['qmake','qmake6']
    pyside_version = '6'
else
    error('Only Qt5 or Qt6 are supported')
endif

current_source_dir = meson.current_source_dir()

shiboken = find_program(f'shiboken@pyside_version@')
qmake = find_program(qmake_possible_names)

pymod = import('python')
python3 = pymod.find_installation('python3', modules:[f'PySide@pyside_version@',
                                                      f'shiboken@pyside_version@',
                                                      f'shiboken@pyside_version@_generator',
                                                      'numpy'])
python3_dep = python3.dependency(embed:true)
numpy_inc = run_command(python3, '-c', 'import numpy;print(numpy.get_include())').stdout().strip('\n')

qt_modules = ['QtCore','QtGui','QtWidgets']

generated_srcs  = run_command(python3, 'helper_scripts/src_list.py', '--build-system', 'meson', '--bindings', f'@current_source_dir@/bindings.xml').stdout().split(';')

modules_arg = '--modules=@0@'.format(','.join(qt_modules))

shiboken_build_flags = run_command(python3, 'helper_scripts/shiboken-helper.py', '--qmake', qmake.full_path(), '--pyside_version', pyside_version, '--includes', modules_arg).stdout().strip('\n').split(' ')
shiboken_link_flags  = run_command(python3, 'helper_scripts/shiboken-helper.py', '--qmake', qmake.full_path(), '--pyside_version', pyside_version, '--build-dir', meson.current_build_dir(), '--libs', modules_arg).stdout().strip('\n').split(' ')
shiboken_typesystem  = run_command(python3, 'helper_scripts/shiboken-helper.py', '--qmake', qmake.full_path(), '--pyside_version', pyside_version, '--typesystem').stdout().strip('\n')


sciqlopplots_bindings_src = files('numpy_wrappers.cpp')
sciqlopplots_bindings_headers = files('bindings.h', 'numpy_wrappers.hpp')


subdir('SciQLopPlotsBindings')

shiboken_dep = declare_dependency(compile_args: shiboken_build_flags, link_args: shiboken_link_flags)

sciqlopplots_bindings = python3.extension_module('SciQLopPlotsBindings',
        sciqlopplots_bindings_src, shiboken_generator_out,
        dependencies : [ python3_dep, shiboken_dep, SciQLopPlots_src_dep, cpp_utils_dep],
        install: true,
        include_directories : numpy_inc,
        extra_files: sciqlopplots_bindings_headers + ['helper_scripts/shiboken-gen.py', 'helper_scripts/rpath-helper.py']
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
