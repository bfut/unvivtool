# unvivtool Copyright (C) 2021-2022 Benjamin Futasz <https://github.com/bfut>
#
# You may not redistribute this program without its source code.
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
  test_smoketest.py - smoke-testing unvivtool Python extension module
"""
import os
import pathlib
import sys

import pytest

# Look for local build, if not installed
try:
    import unvivtool
except ModuleNotFoundError:
    import sys
    PATH = pathlib.Path(pathlib.Path(__file__).parent / "../python/build")
    print(PATH)
    for x in PATH.glob("**"):
        sys.path.append(str(x.resolve()))
    del PATH

    import unvivtool

@pytest.mark.xfail(not hasattr(unvivtool, "__version__"),
                   reason="missing attribute __version__")
def test_version():
    script_path = pathlib.Path(__file__).parent.parent.resolve()
    os.chdir(script_path)
    with open(script_path / "../libnfsviv.h", mode="r", encoding="utf8") as f:
        for _ in range(36):
            next(f)
        __version__ = f.readline().rstrip().split("\"")[-2]
        print(f"VERSION_INFO={__version__}")
    if hasattr(unvivtool, "__version__"):
        print(f"unvivtool.__version__={unvivtool.__version__}")
    else:
        print(f'hasattr(unvivtool, "__version__")={hasattr(unvivtool, "__version__")}')
    assert hasattr(unvivtool, "__version__") and unvivtool.__version__ == __version__
