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
    unvivtool_script.py - decode or encode VIV/BIG archives

USAGE
    Run decoder:
    python unvivtool_script.py d </path/to/archive.viv>|</path/to/archive.big>

    Run encoder:
    python unvivtool_script.py e </path/to/folder>

    Print info only:
    python unvivtool_script.py i </path/to/archive.viv>|</path/to/archive.big>
    python unvivtool_script.py </path/to/archive.viv>|</path/to/archive.big>

REQUIRES
    installing unvivtool <https://github.com/bfut/unvivtool>
"""
import argparse
import os
import pathlib
import re

import unvivtool as uvt

def main():
    # Parse command (or print module help)
    parser = argparse.ArgumentParser()
    parser.add_argument("cmd", nargs="+", help="d: unviv(), e: viv(), i: print archive info")
    args = parser.parse_args()

    # Parameters
    opt_requestfmt = "BIGF"  # viv() only
    opt_direnlenfixed = 0
    opt_overwrite = 1

    # Decode
    if args.cmd[0] == "d":
        if len(args.cmd) > 1:
            vivfile = pathlib.Path(args.cmd[1])
        else:
            vivfile = pathlib.Path(__file__).parent / "car.viv"  # all paths can be absolute or relative
        if not vivfile.is_file():
            raise FileExistsError(f"{vivfile}")

        outdir = pathlib.Path(vivfile).parent / (vivfile.stem + "_" + vivfile.suffix[1:])
        try:
            os.mkdir(outdir)
        except FileExistsError:
            print(f"os.mkdir() not necessary, directory exists: {outdir}")
        uvt.unviv(str(vivfile), str(outdir), direnlen=opt_direnlenfixed, overwrite=opt_overwrite)  # extract all files in archive "vivfile"

    # Encode
    elif args.cmd[0] == "e":
        if len(args.cmd) > 1:
            infolder = pathlib.Path(args.cmd[1])
        else:
            infolder = pathlib.Path(__file__).parent / "car_viv/"

        suffix1 = re.compile(r"(\w+)_[vV][iI][vV]$", re.IGNORECASE)
        suffix2 = re.compile(r"(\w+)_[bB][iI][gG]$", re.IGNORECASE)
        if suffix1.match(infolder.stem):
            vivfile = (infolder.parent / infolder.stem[:-4]).with_suffix(".viv")
        elif suffix2.match(infolder.stem):
            vivfile = (infolder.parent / infolder.stem[:-4]).with_suffix(".big")
        else:
            vivfile = (infolder.parent / infolder.stem).with_suffix(".viv")

        infiles = os.listdir(infolder)
        for i in range(len(infiles)):
            infiles[i] = str(infolder / infiles[i])
        infiles = sorted(infiles)
        print(infiles)
        uvt.viv(str(vivfile), infiles, format=opt_requestfmt, direnlen=opt_direnlenfixed)  # encode all files in path/to/infiles

    # Print info (dry run)
    elif args.cmd[0] == "i":
        print(f"#{args.cmd[1:]}#")
        print( " ".join(args.cmd[1:]) ,  pathlib.Path(" ".join(args.cmd[1:]))  )
        if len(args.cmd) > 1:
            vivfile = pathlib.Path(args.cmd[1])
            vivfile = pathlib.Path(" ".join(args.cmd[1:]))
        else:
            vivfile = pathlib.Path(__file__).parent / "car.viv"  # all paths can be absolute or relative
        if not vivfile.is_file():
            raise FileExistsError(f"{vivfile}")
        uvt.unviv(str(vivfile), dir=".", direnlen=opt_direnlenfixed, dry=True, overwrite=opt_overwrite)
    elif pathlib.Path(args.cmd[0]).is_file():
        uvt.unviv(args.cmd[0], dir=".", direnlen=opt_direnlenfixed, dry=True, overwrite=opt_overwrite)

    #
    else:
        print("Invalid command (expects {d, e, i}):", args.cmd[0])
        help(uvt)

if __name__ == "__main__":
    main()
