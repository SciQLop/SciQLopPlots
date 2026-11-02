#!/bin/env python3

import os
import sys
import importlib
import argparse
from glob import glob
import subprocess
import json
import platform
import re

parser = argparse.ArgumentParser(description='PySide/shiboken generator wrapper')
parser.add_argument('--input-header')
parser.add_argument('--input-xml')
parser.add_argument('--shiboken')
parser.add_argument('--typesystem-paths')
parser.add_argument('--output-directory')
parser.add_argument('--build-directory')
parser.add_argument('--ref-build-target')
args = parser.parse_args()

env = os.environ.copy()

def cpp_flags(build_dir, ref_build_target):
    with open(f"{build_dir}/meson-info/intro-targets.json", 'r') as targets_f:
        targets = json.load(targets_f)
        # this is extra fragile!
        ref_target = list(filter(lambda k: k['name'].startswith(ref_build_target), targets))[0]
        params = ref_target['target_sources'][0]['parameters']
        return list(filter(lambda p: p.upper().startswith('-I') or p.startswith('-F'), params))


shiboken_constant_args=['--generator-set=shiboken',
    '--enable-parent-ctor-heuristic',
    '--enable-return-value-heuristic',
    '--use-isnull-as-nb_nonzero',
    '--avoid-protected-hack',
    '--enable-pyside-extensions',
    #'--debug-level=full',
    '-std=c++20',
    '--generator-set=shiboken']

if 'linux' in platform.system().lower():
    #if os.environ.get('AUDITWHEEL_PLAT', '') == 'manylinux_2_28_x86_64':
    #    gcc_found = False
    #    for v in reversed(range(11,14)):
    #        prefix = f'/opt/rh/gcc-toolset-{v}/root/usr'
    #        if os.path.exists(prefix):
    #            shiboken_constant_args += [
    #                f"-I/opt/rh/gcc-toolset-{v}/root/usr/lib/gcc/x86_64-redhat-linux/{v}/../../../../include/c++/{v}",
    #                f"-I/opt/rh/gcc-toolset-{v}/root/usr/lib/gcc/x86_64-redhat-linux/{v}/../../../../include/c++/{v}/x86_64-redhat-linux",
    #                f"-I/opt/rh/gcc-toolset-{v}/root/usr/lib/gcc/x86_64-redhat-linux/{v}/../../../../include/c++/{v}/backward",
    #                f"-I/opt/rh/gcc-toolset-{v}/root/usr/lib/gcc/x86_64-redhat-linux/{v}/include",
    #                f"-I/opt/rh/gcc-toolset-13/root/usr/include",
    #                f"-I/usr/local/include",
    #                f"-I/usr/include",
    #            ]
    #            gcc_found = True
    #            shiboken_constant_args += ['--compiler=g++']
    #            break
    # An explicit LLVM_INSTALL_DIR wins: some distros (Fedora) ship llvm-config's
    # prefix apart from the clang resource headers shiboken needs (stddef.h).
    if 'LLVM_INSTALL_DIR' not in env:
        try:
            llvm_dir = subprocess.run(['llvm-config', '--prefix'], check=True, stdout=subprocess.PIPE)
            env['LLVM_INSTALL_DIR'] = llvm_dir.stdout.decode().strip()
        except FileNotFoundError:
            pass

cmd = [args.shiboken, args.input_header, args.input_xml ] + shiboken_constant_args + cpp_flags(args.build_directory, args.ref_build_target) + [ f'--typesystem-paths={args.typesystem_paths}', f'--output-directory={args.output_directory}']

try:
    r = subprocess.run(
        cmd,
        check=True,
        env=env,
        capture_output=True
    )
except subprocess.CalledProcessError as e:
    print(e)
    print(e.stdout.decode())
    print(e.stderr.decode())
    sys.exit(e.returncode)

# Shiboken's exception-handling turns every C++ exception into RuntimeError. Ask
# sqp::python_error_type (ErrorTranslation.hpp) instead, so a bad value is a ValueError.
# The hand-written catch ladders in bindings.xml call it too.
MODULE_INCLUDE = '#include "sciqlopplotsbindings_python.h"\n'
TRANSLATION_INCLUDE = '#include <SciQLopPlots/Python/ErrorTranslation.hpp>\n'
GENERIC_CATCH = re.compile(r'(catch \(const std::exception &e\) \{\s*errorType = )PyExc_RuntimeError;')


# Shiboken's sequence-to-container converters iterate without checking PyObject_GetIter: a 0-d numpy
# array passes the sequence check, GetIter fails, and PyIter_Next(nullptr) segfaults. Stop instead: the
# TypeError GetIter set ("iteration over a 0-d array") then aborts the call.
UNCHECKED_ITER = 'Shiboken::AutoDecRef it(PyObject_GetIter(pyIn));\n'
CHECKED_ITER = UNCHECKED_ITER + '    if (it.isNull())\n        return;\n'


# libshiboken's integer conversion raises OverflowError without a message: name it right after the
# argument conversions, where each generated function checks for a conversion error.
CONVERSION_CHECK = re.compile(r'^(\s*)if \(Shiboken::Errors::occurred\(\) == nullptr\) \{', re.M)
FUNCTION_HEADER = re.compile(r'^static (?:PyObject \*|int )Sbk_(\w+?)(?:Func_(\w+)|_Init)\(', re.M)


# Shiboken converts every argument before checking for an error: with one already pending, the next
# conversion calls into the C API and Python turns that into SystemError. Convert only while none is.
ARGUMENT_CONVERSION = re.compile(r'^(\s*)(pythonToCpp\[\d+\]\(pyArgs\[\d+\], &[^;]+\);)$', re.M)


# A complex numpy array passes the sequence check for a list of doubles, and each element then converts
# with a ComplexWarning, its imaginary part dropped: refuse it, so the call raises TypeError instead.
REAL_LIST_CHECK = re.compile(r'(static PythonToCppFunc is_PySequence_PythonToCpp__Q\w+_(?:double|float)__Convertible\(PyObject \*pyIn\)\n\{\n)')


# Shiboken's argument errors that don't say why (an unknown keyword as AttributeError, "(missing
# signature)" or a constructor's "wrong argument values" when keywords were given): sqp::wrong_arguments
# makes them TypeErrors naming the types. _MinusOne is the constructors' variant.
WRONG_ARGUMENTS = re.compile(r'return (Shiboken::returnWrongArguments(?:_MinusOne)?\((\w+), [^;]*\));')


def wrong_arguments(source):
    def wrap(m):
        # The enclosing function's header: its first two lines ("static int" alone on the first, for
        # a constructor).
        start = source.rfind('\nstatic ', 0, m.start()) + 1
        header = source[start:source.find('\n', source.find('\n', start) + 1)]
        keywords = 'kwds' if 'PyObject *kwds' in header else 'nullptr'
        positional = 'true' if m.group(2) == 'args' else 'false'  # args: a tuple; pyArg: the only one
        return f'return sqp::wrong_arguments({m.group(1)}, {m.group(2)}, {positional}, {keywords});'
    return WRONG_ARGUMENTS.sub(wrap, source)


def refuse_complex_arrays(source):
    return REAL_LIST_CHECK.sub(r'\1    if (sqp::is_complex_array(pyIn))\n        return {};\n', source)


def guard_argument_conversions(source):
    # In braces: some conversions are the body of an if/else without braces, whose else must not move.
    source = ARGUMENT_CONVERSION.sub(r'\1{ if (!PyErr_Occurred()) \2 }', source)
    return SINGLE_ARGUMENT_CONVERSION.sub(r'\1{ if (!PyErr_Occurred()) \2 }', source)


# PySide's QColor and QFont conversions pass bad input on silently (an unparsable colour name) or with a
# printed error per item (non-str font families): check this module's colour and font arguments first.
CHECKED_ARGUMENT = re.compile(r'^(\s*)::(QColor|QFont|QList<QColor>) cppArg(\d+)(?:_local)?\b[^;*]*;\n', re.M)
SINGLE_ARGUMENT_CONVERSION = re.compile(r'^(\s*)(pythonToCpp\(pyArg, &[^;]+\);)$', re.M)


def check_arguments(body, name):
    single = 'PyObject *pyArg)' in body.split('\n', 1)[0]
    check = {"QColor": "check_color_arg", "QFont": "check_font_arg", "QList<QColor>": "check_color_list_arg"}
    return CHECKED_ARGUMENT.sub(
        lambda m: f'{m.group(0)}{m.group(1)}sqp::{check[m.group(2)]}('
                  f'{"pyArg" if single else f"pyArgs[{m.group(3)}]"}, "{name}");\n', body)


def name_conversion_errors(source):
    out, last = [], 0
    headers = list(FUNCTION_HEADER.finditer(source))
    for header, end in zip(headers, headers[1:] + [None]):
        stop = end.start() if end else len(source)
        name = f"{header.group(1)}.{header.group(2) or '__init__'}"
        body = check_arguments(source[header.start():stop], name)
        body = CONVERSION_CHECK.sub(lambda m: f'{m.group(1)}sqp::name_conversion_error("{name}");\n{m.group(0)}',
                                    body)
        out += [source[last:header.start()], body]
        last = stop
    return ''.join(out) + source[last:]


# What each patch leaves in the output. Patches match generated text: a shiboken update that changes it
# turns a patch into a silent no-op, so the build fails instead when one left no trace at all.
PATCH_MARKERS = {
    "GENERIC_CATCH": "sqp::python_error_type(e);",
    "UNCHECKED_ITER": CHECKED_ITER,
    "CONVERSION_CHECK": "sqp::name_conversion_error(",
    "ARGUMENT_CONVERSION": "{ if (!PyErr_Occurred()) pythonToCpp[",
    "SINGLE_ARGUMENT_CONVERSION": "{ if (!PyErr_Occurred()) pythonToCpp(pyArg,",
    "REAL_LIST_CHECK": "sqp::is_complex_array(pyIn)",
    "CHECKED_ARGUMENT QColor": "sqp::check_color_arg(",
    "CHECKED_ARGUMENT QFont": "sqp::check_font_arg(",
    "CHECKED_ARGUMENT QList<QColor>": "sqp::check_color_list_arg(",
    "WRONG_ARGUMENTS": "sqp::wrong_arguments(",
}


def require_every_patch(patched_sources):
    missing = [name for name, marker in PATCH_MARKERS.items() if not any(marker in s for s in patched_sources)]
    if missing:
        sys.exit(f"shiboken-gen.py: these patches matched nothing in the generated code: {', '.join(missing)}")


def patch_generated(output_directory):
    patched_sources = []
    for path in glob(f'{output_directory}/**/*.cpp', recursive=True):
        with open(path) as f:
            source = f.read()
        translated = GENERIC_CATCH.sub(r'\1sqp::python_error_type(e);', source)
        translated = translated.replace(UNCHECKED_ITER, CHECKED_ITER)
        translated = name_conversion_errors(translated)
        translated = guard_argument_conversions(translated)
        translated = refuse_complex_arrays(translated)
        translated = wrong_arguments(translated)
        if 'sqp::' in translated:
            # Not in bindings.h: shiboken's own parser fails on Python.h.
            translated = translated.replace(MODULE_INCLUDE, MODULE_INCLUDE + TRANSLATION_INCLUDE, 1)
        patched_sources.append(translated)
        if translated != source:
            with open(path, 'w') as f:
                f.write(translated)
    require_every_patch(patched_sources)


patch_generated(args.output_directory)
print(r.stdout.decode())
print(r.stderr.decode())
sys.exit(r.returncode)
