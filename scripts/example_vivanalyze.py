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
import pathlib

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

    ret = None
    retx = None
    retLEN = None
    ret = uvt.get_info(inpath)
    # ret = uvt.get_info(inpath, verbose=True)
    retx = uvt.get_info(inpath, fnhex=True)
    # retx = uvt.get_info(inpath, fnhex=True, verbose=True)
    retLEN = uvt.get_info(inpath, direnlen=80)
    # retLEN = uvt.get_info(inpath, direnlen=80, verbose=True)
    print("ret", "retx", "retLEN")
    print(ret)
    print(retx)
    print(retLEN)
    print("ret", "retx", "retLEN")
    if ret is not None: print(len(ret.get("files", [])))
    if retx is not None: print(len(retx.get("files", [])))
    if retLEN is not None: print(len(retLEN.get("files", [])))
    print("ret", "retx", "retLEN")
    if ret is not None: print(ret["format"])
    if retx is not None: print(retx["format"])
    if retLEN is not None: print(retLEN["format"])


if __name__ == "__main__":
    print(CONFIG)
    main()