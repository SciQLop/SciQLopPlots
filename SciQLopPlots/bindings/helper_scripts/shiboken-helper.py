#!/bin/env python3

import os, sys
import platform
import importlib
import importlib.machinery
import argparse
from glob import glob, escape
import re
import platform

parser = argparse.ArgumentParser(description='PySide/shiboken ')
group = parser.add_mutually_exclusive_group()
group.add_argument('--libs', action='store_true')
group.add_argument('--includes',action='store_true')
group.add_argument('--typesystem', action='store_true')
parser.add_argument('--modules')
parser.add_argument('--qmake')
parser.add_argument('--build-dir', nargs='?')
args = parser.parse_args()

pyside_ver = 6

shiboken_generator = importlib.import_module(f'shiboken{pyside_ver}_generator')
shiboken = importlib.import_module(f'shiboken{pyside_ver}')
PySide = importlib.import_module(f'PySide{pyside_ver}')

PySide_mod_path = PySide.__path__[0]
shiboken_generator_mod_path = shiboken_generator.__path__[0]
shiboken_mod_path = shiboken.__path__[0]

build_dir = args.build_dir

if platform.system().lower() == 'darwin':
    ext_sufix = re.escape('.dylib')
elif platform.system().lower() == 'windows':
    ext_sufix = re.escape('.lib')
else:
    ext_sufix = f"[{'|'.join(map(re.escape, importlib.machinery.EXTENSION_SUFFIXES))}]"


def first_existing_path(path_list):
    for path in path_list:
        if path is not None and os.path.exists(path):
            return path

def find_lib(name, search_folders):
    name_regex = re.compile(name)
    for folder in search_folders:
        if os.path.exists(folder):
            files=os.listdir(folder)
            found = list(filter(name_regex.match, files))
            if len(found):
                return f'{folder}{os.path.sep}{found[0]}'

def make_link_flag(lib_path):
    basename = os.path.basename(lib_path)
    if basename.startswith('lib'):
        return f"-l{basename[3:].split('.so')[0]}"
    return lib_path


def make_link_flags(libs_paths):
    libs_paths = list(filter(lambda l: l is not None, libs_paths))
    print(f"platform: {platform.system().lower()}", file=sys.stderr)
    print(f"libs_paths: {libs_paths}", file=sys.stderr)
    if platform.system().lower() == 'linux':
        link_flag="-Wl,-rpath="
    else:
        link_flag="-L"
    if platform.system().lower() == 'windows':
        folders = list(set(list(map(lambda l: f"{link_flag}{os.path.dirname(l)}",libs_paths))))
    else:
        folders = list(set(list(map(lambda l: f"{link_flag}{os.path.relpath(os.path.dirname(l), build_dir)}",libs_paths))))
    #libs = list(map(make_link_flag, libs_paths))
    libs = libs_paths
    return folders + libs


if shiboken.__file__ and shiboken_generator.__file__ and PySide.__file__:
    PySide_inc = first_existing_path([f'{PySide_mod_path}{os.path.sep}include',f'/usr/include/PySide{pyside_ver}'])
    PySide_typesys = first_existing_path([f'{PySide_mod_path}{os.path.sep}typesystems','/usr/share/PySide{pyside_ver}/typesystems'])
    shiboken_includes = first_existing_path([f'{shiboken_mod_path}{os.path.sep}include',f'{shiboken_generator_mod_path}{os.path.sep}include',f'/usr/include/shiboken{pyside_ver}'])
    
    if args.typesystem:
        print(PySide_typesys)

    modules = args.modules.split(',')

    if args.libs:
        main_lib = [find_lib(f'(lib)?shiboken{pyside_ver}.*{ext_sufix}', [f'{shiboken_mod_path}', '/usr/lib64/'])]   
        print(f"main_lib: {main_lib}", file=sys.stderr)
        main_lib += [find_lib(f'(lib)?[Pp]y[sS]ide{pyside_ver}\..*{ext_sufix}.*', [f'{PySide_mod_path}', '/usr/lib64/'])]
        print(f"main_lib: {main_lib}", file=sys.stderr)
        if platform.system().lower() == 'windows':
            main_lib += [find_lib('python3.lib', [f'{os.path.dirname(sys.executable)}{os.path.sep}libs'])]
        if platform.system().lower() == 'darwin' or platform.system().lower() == 'windows':
            modules_libs = []
        else:
            modules_libs = [importlib.import_module(f'PySide{pyside_ver}.{module}').__file__ for module in modules]
        print(f"modules_libs: {modules_libs}", file=sys.stderr)
        print(";".join(make_link_flags(main_lib + modules_libs)))

    if args.includes:
        modules_incs = [f"-I{PySide_inc}{os.path.sep}{module}" for module in modules]
        print(";".join([f"-I{PySide_inc};-I{shiboken_includes}"]+ modules_incs))
    
    
