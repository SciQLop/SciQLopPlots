#!/bin/env python3

import os
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
parser.add_argument('--pyside_version', default='2')
args = parser.parse_args()

pyside_ver = args.pyside_version

shiboken_generator = importlib.import_module(f'shiboken{pyside_ver}_generator')
shiboken = importlib.import_module(f'shiboken{pyside_ver}')
PySide = importlib.import_module(f'PySide{pyside_ver}')

PySide_mod_path = PySide.__path__[0]
shiboken_generator_mod_path = shiboken_generator.__path__[0]
shiboken_mod_path = shiboken.__path__[0]

build_dir = args.build_dir

if platform.system().lower() == 'darwin':
    ext_sufix = re.escape('.dylib')
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
                return f'{folder}/{found[0]}'

def make_link_flag(lib_path):
    basename = os.path.basename(lib_path)
    if basename.startswith('lib'):
        return f"-l{basename[3:].split('.so')[0]}"
    return lib_path


def make_link_flags(libs_paths):
    if platform.system().lower() == 'linux':
        link_flag="-Wl,-rpath="
    else:
        link_flag="-L"
    folders = list(set(list(map(lambda l: f"{link_flag}{os.path.relpath(os.path.dirname(l), build_dir)}",libs_paths))))
    #libs = list(map(make_link_flag, libs_paths))
    libs = libs_paths
    return folders + libs


if shiboken.__file__ and shiboken_generator.__file__ and PySide.__file__:
    PySide_inc = first_existing_path([f'{PySide_mod_path}/include',f'/usr/include/PySide{pyside_ver}'])
    PySide_typesys = first_existing_path([f'{PySide_mod_path}/typesystems','/usr/share/PySide{pyside_ver}/typesystems'])
    shiboken_includes = first_existing_path([f'{shiboken_mod_path}/include',f'{shiboken_generator_mod_path}/include',f'/usr/include/shiboken{pyside_ver}'])
    
    if args.typesystem:
        print(PySide_typesys)

    modules = args.modules.split(',')

    if args.libs:
        main_lib = [find_lib(f'libshiboken{pyside_ver}.*{ext_sufix}', [f'{shiboken_mod_path}', '/usr/lib64/'])]
        main_lib += [find_lib(f'lib[Pp]y[sS]ide{pyside_ver}\..*{ext_sufix}.*', [f'{PySide_mod_path}', '/usr/lib64/'])]
        if platform.system().lower() == 'darwin':
            modules_libs = []
        else:
            modules_libs = [importlib.import_module(f'PySide{pyside_ver}.{module}').__file__ for module in modules]
        print(" ".join(make_link_flags(main_lib + modules_libs)))

    if args.includes:
        modules_incs = [f"-I{PySide_inc}/{module}" for module in modules]
        print(" ".join([f"-I{PySide_inc} -I{shiboken_includes}"]+ modules_incs))
    
    
