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
    unvivtool_script.py - decode or encode VIV/BIG archives

USAGE
    Run decoder:
    python unvivtool_script.py </path/to/archive.viv>|</path/to/archive.big> [</path/to/output_folder>]

    Run encoder:
    python unvivtool_script.py </path/to/folder> [</path/to/archive.viv>|</path/to/archive.big>]

    Print info only:
    python unvivtool_script.py i </path/to/archive.viv>|</path/to/archive.big>

REQUIRES
    python -m pip install -U unvivtool
        this installs unvivtool <https://github.com/bfut/unvivtool>
"""
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
    "opt_alignfofs" : 4,  # viv() only
    "verbose_encode" : 1,
}


def main():
    # Parse command (or print module help)
    parser = argparse.ArgumentParser()
    parser.add_argument("cmd", nargs="+", help= "<path/to/file>, <path/to/folder>, i <path/to/file>")
    args = parser.parse_args()
    inpath = pathlib.Path(args.cmd[0])


    # Decode
    if inpath.is_file():
        print(f"args: {args.cmd[:]}")
        outdir = None
        if len(args.cmd) > 1:
            outdir = pathlib.Path(args.cmd[1])
        if not isinstance(outdir, pathlib.Path):
            outdir = inpath.parent / (inpath.stem + "_" + inpath.suffix[1:])

        ptn = time.process_time_ns()
        if uvt.unviv(str(inpath), str(outdir), direnlen=CONFIG["opt_direnlenfixed"], overwrite=CONFIG["opt_overwrite"]):  # extract all files in archive "inpath"
            print(f"unvivtool took {(float(time.process_time_ns() - ptn) / 1e6):.2f} ms")


    # Encode
    elif inpath.is_dir():
        print(f"args: {args.cmd[:]}")
        if len(args.cmd) > 1:
            vivfile = pathlib.Path(args.cmd[1])
        else:
            suffix1 = re.compile(r"(\w+)_[vV][iI][vV]$", re.IGNORECASE)
            suffix2 = re.compile(r"(\w+)_[bB][iI][gG]$", re.IGNORECASE)
            if suffix1.match(inpath.stem):
                vivfile = (inpath.parent / inpath.stem[:-4]).with_suffix(".viv")
            elif suffix2.match(inpath.stem):
                vivfile = (inpath.parent / inpath.stem[:-4]).with_suffix(".big")
            else:
                vivfile = (inpath.parent / inpath.stem).with_suffix(".viv")

        infiles = os.listdir(inpath)
        for i in range(len(infiles)):
            infiles[i] = str(inpath / infiles[i])
        infiles = sorted(infiles)
        print(infiles)
        ptn = time.process_time_ns()
        if uvt.viv(str(vivfile), infiles, verbose=CONFIG["verbose_encode"], format=CONFIG["opt_requestfmt"], direnlen=CONFIG["opt_direnlenfixed"], alignfofs=CONFIG["opt_alignfofs"]):  # encode all files in path/to/infiles
            print(f"unvivtool took {(float(time.process_time_ns() - ptn) / 1e6):.2f} ms")


    # Print archive info (dry run)
    elif args.cmd[0] == "i" and len(args.cmd) > 1:
        print(f"args: {args.cmd[:]}")
        print( " ".join(args.cmd[1:]) ,  pathlib.Path(" ".join(args.cmd[1:]))  )
        if len(args.cmd) > 1:
            vivfile = pathlib.Path(" ".join(args.cmd[1:]))
        else:
            vivfile = pathlib.Path(__file__).parent / "car.viv"  # all paths can be absolute or relative
        if not vivfile.is_file():
            raise FileExistsError(f"{vivfile}")
        ptn = time.process_time_ns()
        if uvt.unviv(str(vivfile), dir=".", direnlen=CONFIG["opt_direnlenfixed"], dry=True, overwrite=CONFIG["opt_overwrite"]):
            print(f"unvivtool took {(float(time.process_time_ns() - ptn) / 1e6):.2f} ms")


    #
    else:
        print("Invalid command (expects {<path/to/file>, <path/to/folder>, i <path/to/file>}):", args.cmd[0])

if __name__ == "__main__":
    main()
