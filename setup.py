import os
import re
import subprocess
import sys
from pathlib import Path
import tarfile
from tempfile import TemporaryDirectory
from glob import glob
import json

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.sdist import sdist
from setuptools.command.install_lib import install_lib
from setuptools.command.install import install


class MesonExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.fspath(Path(sourcedir).resolve())
        print(f"Source dir {self.sourcedir}")


class MesonBuild(build_ext):
    def build_extension(self, ext):
        ext_fullpath = Path.cwd() / self.get_ext_fullpath(ext.name)
        extdir = ext_fullpath.parent.resolve()
        meson_args = []
        build_args = []
        build_temp = Path(self.build_temp) / ext.name
        if not build_temp.exists():
            build_temp.mkdir(parents=True)

        print(f"Source dir {ext.sourcedir}, build_temp {build_temp}")
        subprocess.run(
            ["meson", ext.sourcedir, '.'] + meson_args, cwd=build_temp, check=True
        )
        subprocess.run(
            ["meson", "compile"] + build_args, cwd=build_temp, check=True
        )
        self._libs = list(json.loads(open(f"{build_temp}/meson-info/intro-install_plan.json").read())['targets'].keys())

    def get_libraries(self, ext):
        return self._libs + build_ext.get_libraries(self, ext)

class MesonSDist(sdist):
    def _copy_source_files(self, dest_dir):
        with TemporaryDirectory() as cfg_dir:
            print(f"Configuring meson project for sdist in {cfg_dir}")
            subprocess.run(
                ["meson", cfg_dir, '.'] , check=True
            )
            print(f"Calling meson dist")
            subprocess.run(
                ["meson", "dist", '--include-subprojects', '--allow-dirty', '--no-tests'] , cwd=cfg_dir, check=True
            )

            with tarfile.open(glob(f"{cfg_dir}/meson-dist/SciQLopPlots-*.tar.xz")[0]) as tar:
                print(f"Extracting dist archive and copying content to {dest_dir}")
                with TemporaryDirectory() as extract_dir:
                    tar.extractall(extract_dir)
                    self.copy_tree(glob(f"{extract_dir}/SciQLopPlots-*")[0], dest_dir)

    def make_release_tree(self, base_dir, files):
        sdist.make_release_tree(self, base_dir, files)
        self._copy_source_files(base_dir)

class MesonInstall(install):
    def run(self):
        print("MesonInstall")
        print(f"self.install_platlib={self.install_platlib}, self.install_lib={self.install_lib}, self.prefix={self.prefix}")
        install.run(self)

class MesonInstall_lib(install_lib):
    def run(self):
        install_lib.run(self)
        current_path=str(Path.cwd())
        print("MesonInstall_lib")
        print(f"install_dir={self.install_dir}, build_dir={self.build_dir}, current_path ={current_path}")
        print(f"{os.listdir(current_path)}")
        print(f"{os.listdir(current_path+'/build')}")
        subprocess.run(
            ["meson", "install", '--destdir', self.install_dir] , cwd=self.build_dir, check=True
        )


setup(
    name="SciQLopPlots",
    version="0.0.1",
    author="Alexis Jeandet",
    author_email="alexis.jeandet@member.fsf.org",
    description="SciQLopPlots python bindings",
    long_description="",
    ext_modules=[MesonExtension("SciQLopPlots")],
    cmdclass={"build_ext": MesonBuild, "sdist": MesonSDist, "install_lib": MesonInstall_lib, "install": MesonInstall},
    zip_safe=False,
    extras_require={"test": ["pytest>=6.0"]},
    python_requires=">=3.7",
)
