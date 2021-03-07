"""
setup_tests.py - builds/installs Python module, with -DBINDINGS_DEBUG

python setup_tests.py build
python setup_tests.py install
"""

import os
import pathlib
import platform
import setuptools

script_path = pathlib.Path(__file__).parent.resolve()
os.chdir(script_path)

module_name = "unvivtool"
module_version = "1.6"
long_description = (script_path / "../README.md").read_text(encoding="utf-8")
extra_compile_args = [
    "-DUVT_MODULE_DEBUG",
]
if platform.system() != "Windows":
    extra_compile_args.extend([
        "-pedantic-errors",
        "-g",
        "-Wall",
        "-Wextra",
        "-Wno-newline-eof",  # clang
        "-Wstack-protector",
        "-fasynchronous-unwind-tables",

        #"-fanalyzer",  # GCC 10
    ])
extra_link_args = extra_compile_args

module = setuptools.Extension(
    module_name,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
    sources=["unvivtoolmodule.c"])

setuptools.setup(
    name=module_name,
    version=module_version,
    description="VIV/BIG decoding/encoding",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Benjamin Futasz",
    url="https://github.com/bfut/unvivtool",
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