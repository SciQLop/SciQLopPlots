#!/bin/env python3
# taken from https://github.com/radareorg/cutter/blob/master/src/bindings/src_list.py

import os
import sys
import xml.etree.ElementTree as Et
import argparse

parser = argparse.ArgumentParser(description='PySide/shiboken generated file list predictor')
parser.add_argument('--build-system', default='meson')
parser.add_argument('--bindings')
args = parser.parse_args()

bindings = args.bindings
build_system = args.build_system


def fix_name(name):
    return name.replace('<','_').replace('>','_')

def find_all_objects(node, found=None):
    found = found or []
    found += list(map(lambda t:fix_name(t.attrib['name']), node.findall('object-type')+node.findall('value-type')+node.findall('interface-type')))
    smart_ptrs = node.findall('smart-pointer-type')
    for sp in smart_ptrs:
        classes = sp.attrib.get('instantiations', '').split(',')
        for c in classes:
            found.append(f"{sp.attrib['name']}_{c}")
    for item in node.findall('namespace-type'):
        if item.attrib.get('visible', 'true') in ['true', 'auto']:
            found += [item.attrib['name']]
        found += list(map(lambda name:f"{item.attrib['name']}_{name}",find_all_objects(item)))
    return found


def get_cpp_files_gen(include_package=True):
    with open(bindings,'r') as f:
        xml=Et.fromstring(f.read())
        types = find_all_objects(xml)
        package = xml.attrib['package']

    cpp_files_gen = [f"{package.lower()}_module_wrapper.cpp"]+[f"{typename.lower()}_wrapper.cpp" for typename in types]

    if include_package:
        cpp_files_gen = [os.path.join(package, f) for f in cpp_files_gen]

    return cpp_files_gen


def cmd_cmake():
    sys.stdout.write(";".join(get_cpp_files_gen()))


def cmd_qmake():
    sys.stdout.write("\n".join(get_cpp_files_gen()) + "\n")


def cmd_meson():
    sys.stdout.write(";".join(get_cpp_files_gen(include_package=False)))


cmds = {"cmake": cmd_cmake, "qmake": cmd_qmake, "meson": cmd_meson}

cmds[build_system]()
