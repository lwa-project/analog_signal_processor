import os
import numpy
import tempfile
import subprocess
from setuptools import setup, Extension, find_packages
from distutils import log
from distutils.command.install import install


class dummy_install(install):
    """Dummay install method that doesn't let you install."""
    def finalize_options(self, *args, **kwargs):
        raise RuntimeError("This is a dummy package that cannot be installed")


ExtensionModules = [Extension('sub20Config', ['sub20Config.cpp'], include_dirs=['libsub'], libraries=['m', 'usb-1.0', 'sub'], extra_compile_args=['-std=c++11'], extra_link_args=['-Llibsub'])]


setup(
    cmdclass = {'install': dummy_install}, 
    name = 'dummy_package',
    version = '0.0',
    description = 'This is a dummy package to help build the pulsar extensions',
    ext_modules = ExtensionModules
)
