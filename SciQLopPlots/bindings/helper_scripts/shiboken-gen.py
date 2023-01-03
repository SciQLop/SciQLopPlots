#!/bin/env python3

import os
import sys
import importlib
import argparse
from glob import glob
import subprocess
import json
import platform

parser = argparse.ArgumentParser(description='PySide/shiboken generator wrapper')
parser.add_argument('--input-header')
parser.add_argument('--input-xml')
parser.add_argument('--shiboken')
parser.add_argument('--typesystem-paths')
parser.add_argument('--output-directory')
parser.add_argument('--build-directory')
parser.add_argument('--ref-build-target')
args = parser.parse_args()


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
    '-std=c++17']

if os.path.exists('/opt/rh/gcc-toolset-11/root/usr/include/c++/11'):
    shiboken_constant_args += [
        '-I/opt/rh/gcc-toolset-11/root/usr/include/c++/11',
        '-I/opt/rh/gcc-toolset-11/root/usr/include/c++/11/x86_64-redhat-linux'
    ]

subprocess.run(
    [args.shiboken, args.input_header, args.input_xml ] +
    shiboken_constant_args +
    cpp_flags(args.build_directory, args.ref_build_target) +
    [
    f'--typesystem-paths={args.typesystem_paths}',
    f'--output-directory={args.output_directory}'
    ],
    check=True
)

