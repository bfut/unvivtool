# unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>
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
  test_smoketest.py
"""
import pathlib

import pytest

# Look for local build, if not installed
try:
    import unvivtool as uvt
except ModuleNotFoundError:
    import sys
    PATH = pathlib.Path(pathlib.Path(__file__).parent / "../python/build")
    print(PATH)
    for x in PATH.glob("**"):
        sys.path.append(str(x.resolve()))
    del PATH

    import unvivtool as uvt

@pytest.mark.xfail(not hasattr(uvt, "__version__"),
                   reason="missing attribute __version__")
def test_has_attribute_version():
    if hasattr(uvt, "__version__"):
        print(f"uvt.__version__={uvt.__version__}")
    else:
        print(f'hasattr(uvt, "__version__")={hasattr(uvt, "__version__")}')
    assert hasattr(uvt, "__version__")

if __name__ == "__main__":
    print(uvt.__version__)
    test_has_attribute_version()
    help(uvt.unviv)
    print(uvt.unviv)
    print(flush=True)
