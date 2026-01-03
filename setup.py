# unvivtool Copyright (C) 2020 and later Benjamin Futasz <https://github.com/bfut>
#
# Portions copyright, see each source file for more information.
#
# You may not redistribute this program without its source code.
# README.md may not be removed or altered from any unvivtool redistribution.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""
setup.py - adapted from https://github.com/pybind/python_example/blob/master/setup.py
"""
import os
import platform
import pathlib
import re
import setuptools
import sys
import sysconfig

SCRIPT_PATH = pathlib.Path(__file__).parent.resolve()
os.chdir(SCRIPT_PATH)

__version__ = re.findall(
    r"#define UVTVERS \"(.*)\"",
    (SCRIPT_PATH / "./libnfsviv.h").read_text("utf-8")
)[0]
print(f"VERSION_INFO={__version__}")

long_description = (SCRIPT_PATH / "./python/README.md").read_text(encoding="utf-8")

if sys.version_info.minor < 14 or sysconfig.get_config_var("Py_GIL_DISABLED") != 1 or sys._is_gil_enabled() == True:
    os.environ["PYMEM_MALLOC"] = ""
elif sys.version_info.minor >= 14:
    if sysconfig.get_config_var("Py_GIL_DISABLED") == 1 and sys._is_gil_enabled() == False:
        print(f'sysconfig.get_config_var("Py_GIL_DISABLED") = {sysconfig.get_config_var("Py_GIL_DISABLED")}')
        print(f'sys._is_gil_enabled() = {sys._is_gil_enabled()}')

extra_compile_args = []
if "PYMEM_MALLOC" in os.environ:
    print(f'PYMEM_MALLOC={os.environ["PYMEM_MALLOC"]}')
    extra_compile_args += [ "-DPYMEM_MALLOC" ]
if platform.system() == "Windows":
    extra_compile_args += [
        # debug
        # ("/Od"),  # disable optimization
        # ("/O2"),
        # ("/guard:cf"),  # Control Flow Guard (CFG) security feature
        # ("/Zi"),  # generate complete debugging information
        # ("/Z7"),  # generate complete debugging information but in .obj files
        # ("/DEBUG"),  # create a .pdb file
        # ("/EHsc"),  # enable C++ exceptions
        # ("/MD"),  # link against MSVCRT DLL
        # ("/bigobj"),  # for large source files
        ("/W4"),  # warning level 4

        ("/wd4267"),  # prevents warnings on conversion from size_t to int
        ("/wd4996"),  # prevents warnings on fopen() and other POSIX functions
        ("/std:c++latest"), ("/Zc:__cplusplus"),  # sets __cplusplus
    ]
else:
    extra_compile_args += [
        # debug
        # ("-std=c99"),
        # ("-std=c11"),
        # ("-std=c2x"),  # C23
        # ("-g"),
        # ("-Og"),
        # ("-O3"),
        # ("-pedantic-errors"),  # multi-phase extension gives error
        # ("-fno-strict-aliasing"),  # -fstrict-aliasing is on by default with -O2

        # ("-fvisibility=hidden"),  # sets the default symbol visibility to hidden
        ("-Wformat-security"),
        ("-Wdeprecated-declarations"),
        ("-Wstrict-aliasing"),

        ("-msse"),
        ("-msse2"),
        # ("-msse3"),
        # ("-mssse3"),
        # ("-msse4"),
        # ("-mavx"),
        # ("-mavx2"),
        # ("-march=native"),  # compiles for the local machine
        # ("-mtune=native"),  # tuned for the local machine, does not generate any code that cannot run on the default machine type
    ]

    # set compiler through environment variables "CC" and "CXX", e.g., CC=gcc|g++|clang|clang++
    print(f"""platform.python_compiler(): {platform.python_compiler()}""")
    print(f"""CC={os.environ.get("CC")}""")

    if "gcc" in platform.python_compiler().lower() and "clang" not in os.environ.get("CC", ""):
        extra_compile_args += [
            # ("-Wno-sign-compare"),  # prevents warnings on conversion from size_t to int
            ("-Wno-unused-parameter"),  # self in PyObject *unviv(PyObject *self, ...)
            ("-Wno-missing-field-initializers"),  # {NULL, NULL} in struct PyMethodDef{}

            ("-fasynchronous-unwind-tables"),
            ("-Wall"),
            ("-Wextra"),
            ("-Wstack-protector"),
            ("-Woverlength-strings"),
            ("-Wpointer-arith"),
            ("-Wunused-local-typedefs"),
            ("-Wunused-result"),
            ("-Wvarargs"),
            ("-Wvla"),
            ("-Wwrite-strings"),  # ("-Wno-discarded-qualifiers"),

            # # https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc
            ("-D_GNU_SOURCE"),  # realpath()
            ("-D_FORTIFY_SOURCE=2"),
            # ("-D_GLIBCXX_ASSERTIONS"),
            # ("-fexceptions"),  # C++ exceptions
            # ("-fplugin=annobin"),
            ("-fstack-clash-protection"),
            ("-fstack-protector-strong"),
            # ("-mcet"), ("-fcf-protection"),  # (x86 only)
            ("-Werror=format-security"),
            ("-Werror=implicit-function-declaration"),
            # ("-Wl,-z,defs"),
            # ("-Wl,-z,now"),
            # ("-Wl,-z,relro"),
        ]
    elif "clang" in platform.python_compiler().lower() or "clang" in os.environ.get("CC", ""):
        extra_compile_args += [
            # ("-fsanitize=address"), ("-fno-omit-frame-pointer"),

            # ("-Weverything"),
            ("-Wno-braced-scalar-init"),
            ("-Wno-embedded-directive"),
            # ("-Wno-newline-eof"),

            ("-D_GNU_SOURCE"),
        ]

define_macros = [
    ("VERSION_INFO", __version__),
    # ("SCL_DEBUG", os.environ.get("SCL_DEBUG", 0)),  # 0 if key not set
]
# extra_compile_args += [("-DUVTUTF8")]  # defined in unvivtoolmodule.c
# print(f'UVTUTF8={os.environ.get("UVTUTF8", "")}')
print(f'SCL_DEBUG={os.environ.get("SCL_DEBUG", "")}')
if "SCL_DEBUG" in os.environ:
    define_macros += [("SCL_DEBUG", os.environ.get("SCL_DEBUG"))]
ext_modules = [
    setuptools.Extension(
        name="unvivtool",
        sources=sorted(["./python/unvivtoolmodule.c"]),
        define_macros=define_macros,
        extra_compile_args=extra_compile_args,
    )
]

setuptools.setup(
    name="unvivtool",
    version=__version__,
    author="Benjamin Futasz",
    url="https://github.com/bfut/unvivtool",
    license="GPLv3+",
    description="simple BIGF BIGH BIG4 decoder/encoder (commonly known as VIV/BIG)",
    long_description=long_description,
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
    python_requires=">=3.10",
    # extras_require={"test": "pytest"},
    # Currently, build_ext only provides an optional "highest supported C++
    # level" feature, but in the future it may provide more features.
    # cmdclass={"build_ext": build_ext},
    zip_safe=False,
)
