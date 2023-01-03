#!/bin/env python3

import importlib
import argparse
import subprocess
import site
import os
import platform
from tempfile import TemporaryDirectory
from glob import glob

parser = argparse.ArgumentParser(description='PySide/shiboken wrapper rpath fix')
parser.add_argument('--wheel-file')
parser.add_argument('--pyside_version', default='2')
args = parser.parse_args()

pyside_ver = args.pyside_version

shiboken = importlib.import_module(f'shiboken{pyside_ver}')
PySide = importlib.import_module(f'PySide{pyside_ver}')


site_packages_path = site.getsitepackages()[0]

PySide_mod_rpath = '$ORIGIN/'+os.path.relpath(PySide.__path__[0], site_packages_path)
bundled_qt_rpath = f"{PySide_mod_rpath}/Qt/lib"
shiboken_mod_rpath = '$ORIGIN/'+os.path.relpath(shiboken.__path__[0], site_packages_path)

def fix_rpath_macos(extracted_wheel_folder):
    lib_file=glob(f'{extracted_wheel_folder}/SciQLopPlotsBindings.*.so')[0]
    subprocess.run(
        ['install_name_tool', '-add_rpath', '@loader_path/PySide6/Qt/lib/', lib_file],
        check=True
    )
    subprocess.run(
        ['install_name_tool', '-add_rpath', '@loader_path/PySide6/', lib_file],
        check=True
    )
    subprocess.run(
        ['install_name_tool', '-add_rpath', '@loader_path/shiboken6/', lib_file],
        check=True
    )


def fix_rpath_linux(extracted_wheel_folder):
    so_file=glob(f'{extracted_wheel_folder}/SciQLopPlotsBindings.*.so')[0]
    subprocess.run(
        ['patchelf', '--remove-rpath', so_file],
        check=True
    )

    subprocess.run(
        ['patchelf', '--set-rpath', ":".join([PySide_mod_rpath, bundled_qt_rpath, shiboken_mod_rpath]), so_file],
        check=True
    )


with TemporaryDirectory() as tmpdir:
    subprocess.run(
        ['wheel', 'unpack','-d', tmpdir,  args.wheel_file],
        check=True
    )
    extracted_wheel_folder = glob(f'{tmpdir}/sciqlopplots-*')[0]
    if platform.system().lower() == 'darwin':
        fix_rpath_macos(extracted_wheel_folder)
    else:
        fix_rpath_linux(extracted_wheel_folder)
    subprocess.run(
        ['wheel', 'pack','-d', 'dist', glob(f'{tmpdir}/sciqlopplots-*')[0]],
        check=True
    )
