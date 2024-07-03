# unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>
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
import argparse
import os
import pathlib
import re
import time

import unvivtool as uvt

CONFIG = {
    "opt_requestfmt" : "BIGF",  # viv() only
    "opt_direnlenfixed" : 0,
    "opt_overwrite" : 1,
}


def main():
    # Parse command (or print module help)
    parser = argparse.ArgumentParser()
    parser.add_argument("cmd", nargs="+", help= "<path/to/file>, <path/to/folder>, i <path/to/file>")
    args = parser.parse_args()
    inpath = pathlib.Path(args.cmd[0])

    print(f"inpath: '{inpath}'")

    ret = uvt.GetVivDirectory_path(inpath, verbose=True)
    retx = uvt.GetVivDirectory_path(inpath, verbose=True, fnhex=True)
    print(ret)
    print(retx)

    ret = uvt.GetVivDirectory_path(inpath, direnlen=80)
    print(ret)


if __name__ == "__main__":
    print(CONFIG)
    main()
