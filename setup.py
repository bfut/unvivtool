# unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>
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
import setuptools

if "UVTVERBOSE" in os.environ:
    print(f'UVTVERBOSE={os.environ["UVTVERBOSE"]}')

script_path = pathlib.Path(__file__).parent.resolve()
os.chdir(script_path)

print("try reading VERSION_INFO from libnfsviv.h...")
with open(script_path / "./libnfsviv.h", mode="r", encoding="utf8") as f:
    for _ in range(80 - 1):
        next(f)
    __version__ = f.readline()
    print(f"readline() yields '{__version__}'")
    __version__ = __version__.rstrip().split("\"")[-2]
    print(f"VERSION_INFO={__version__}")
long_description = (script_path / "./python/README.md").read_text(encoding="utf-8")

extra_compile_args = []
if "PYMEM_MALLOC" in os.environ:
    print(f'PYMEM_MALLOC={os.environ["PYMEM_MALLOC"]}')
    extra_compile_args += [ "-DPYMEM_MALLOC" ]
if platform.system() == "Windows":
    extra_compile_args += [
        ("/wd4267"),  # prevents warnings on conversion from size_t to int
        ("/std:c++latest"), ("/Zc:__cplusplus"),  # sets __cplusplus
    ]
else:
    extra_compile_args += [
        # debug
        # ("-g"),
        # ("-pedantic-errors"),  # multi-phase extension gives error
        ("-fvisibility=hidden"),  # sets the default symbol visibility to hidden
        ("-Wformat-security"),
        ("-Wdeprecated-declarations"),
    ]

    if "gcc" in platform.python_compiler().lower():
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
            ("-D_GNU_SOURCE"),
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
    elif "clang" in platform.python_compiler().lower():
        extra_compile_args += [
            # ("-Weverything"),
            ("-Wno-braced-scalar-init"),
            # ("-Wno-newline-eof"),

            ("-D_GNU_SOURCE"),
        ]

ext_modules = [
    setuptools.Extension(
        name="unvivtool",
        sources=sorted(["./python/unvivtoolmodule.c"]),
        define_macros=[
            ("VERSION_INFO", __version__),
            ("UVTVERBOSE", os.environ.get("UVTVERBOSE", 0)),  # 0 if key not set
            ("UVTUTF8", os.environ.get("UVTUTF8", 0)),  # branch: unviv() detects utf8
        ],
        extra_compile_args=extra_compile_args,
    )
]

setuptools.setup(
    name="unvivtool",
    version=__version__,
    author="Benjamin Futasz",
    url="https://github.com/bfut/unvivtool",
    license="GPLv3+",
    description="VIV/BIG decoder/encoder",
    long_description=long_description,
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
    python_requires=">=3.9",
    # extras_require={"test": "pytest"},
    # Currently, build_ext only provides an optional "highest supported C++
    # level" feature, but in the future it may provide more features.
    # cmdclass={"build_ext": build_ext},
    zip_safe=False,
)
