"""
setup.py - builds/installs Python extension module

python setup.py build
python setup.py install
"""

import os
import pathlib
import setuptools
import sys

if sys.version_info[0:2] < (3, 7):
  raise RuntimeError('Requires Python 3.7+')

script_path = pathlib.Path(__file__).parent.resolve()
os.chdir(script_path)

module_name = "unvivtool"
module_version = "1.4"
long_description = (script_path / "../README.md").read_text(encoding="utf-8")
extra_compile_args = []

"""
import platform
if platform.system() == "Linux":
    extra_compile_args = [
        "-fexceptions",
        "-fplugin=annobin",
        "-fstack-clash-protection",
        "-fstack-protector-strong",
        "-D_FORTIFY_SOURCE=2",
        "-Wl,-z,defs",
    ]

    if platform.machine() == "x86_64":
        extra_compile_args.append("-mshstk")
        extra_compile_args.append("-fcf-protection")
"""

module = setuptools.Extension(
    module_name,
    extra_compile_args=extra_compile_args,
    sources=["unvivtoolmodule.c"])

setuptools.setup(
    name=module_name,
    version=module_version,
    description="VIV/BIG decoding/encoding",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Benjamin Futasz",
    url="https://github.com/bfut",
    license="GPLv3+",
    python_requires=">=3.7",
    classifiers=[
        "Intended Audience :: Developers",
        "Intended Audience :: End Users/Desktop",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python :: 3",
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        "Topic :: Artistic Software",
        "Topic :: Games/Entertainment",
        "Topic :: Multimedia",
    ],
    ext_modules=[module])