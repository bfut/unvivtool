"""
    unvivtool_script.py - decode or encode VIV/BIG archives with unvivtool

HOW TO USE
    Run decoder:
    python example.py d [</path/to/archive.viv>|</path/to/archive.big>]

    Run encoder:
    python example.py e [</path/to/folder>]

    Print info:
    python example.py i [</path/to/archive.viv>|</path/to/archive.big>]

LICENSE
    Copyright (C) 2021 Benjamin Futasz <https://github.com/bfut>
    This file is distributed under: CC BY-NC-SA 4.0
        <https://creativecommons.org/licenses/by-nc-sa/4.0/>
"""
import argparse
import os
import pathlib
import re

import unvivtool

def main():
    # Parse command (or print module help)
    parser = argparse.ArgumentParser()
    parser.add_argument("cmd", nargs="+", help="d: unviv(), e: viv(), i: print viv info")
    args = parser.parse_args()

    # Decode
    if args.cmd[0] == "d":
        if len(args.cmd) > 1:
            vivfile = pathlib.Path(args.cmd[1])
        else:
            vivfile = pathlib.Path(pathlib.Path(__file__).parent / "car.viv")  # all paths can be absolute or relative
        with open(vivfile, mode="rb") as f:
            pass

        outdir = pathlib.Path(pathlib.Path(vivfile).parent / (vivfile.stem + "_" + vivfile.suffix[1:]))
        try:
            os.mkdir(outdir)
        except FileExistsError:
            print(f"os.mkdir() not necessary, directory exists: {outdir}")
        unvivtool.unviv(str(vivfile), str(outdir))  # extract all files in archive "vivfile"

    # Encode
    elif args.cmd[0] == "e":
        if len(args.cmd) > 1:
            infolder = pathlib.Path(args.cmd[1])
        else:
            infolder = pathlib.Path(pathlib.Path(__file__).parent / "car_viv/")

        suffix1 = re.compile(r"(\w+)_[vV][iI][vV]$", re.IGNORECASE)
        suffix2 = re.compile(r"(\w+)_[bB][iI][gG]$", re.IGNORECASE)
        if suffix1.match(infolder.stem):
            vivfile = pathlib.Path(pathlib.Path(infolder).parent, infolder.stem[:-4]).with_suffix(".viv")
        elif suffix2.match(infolder.stem):
            vivfile = pathlib.Path(pathlib.Path(infolder).parent, infolder.stem[:-4]).with_suffix(".big")
        else:
            vivfile = pathlib.Path(pathlib.Path(infolder).parent, infolder.stem).with_suffix(".viv")

        infiles = os.listdir(infolder)
        for i in range(len(infiles)):
            infiles[i] = str(pathlib.Path(infolder / infiles[i]))
        infiles = sorted(infiles)
        print(infiles)
        unvivtool.viv(str(vivfile), infiles)  # encode all files in path/to/infiles

    # Print info
    if args.cmd[0] == "i":
        if len(args.cmd) > 1:
            vivfile = pathlib.Path(args.cmd[1])
        else:
            vivfile = pathlib.Path(pathlib.Path(__file__).parent / "car.viv")  # all paths can be absolute or relative
        with open(vivfile, mode="rb") as f:
            pass
        unvivtool.unviv(str(vivfile), dir="", dry=True, verbose=True)  # dry run

    #
    else:
        print("Invalid command (expects {d, e, i}):", args.cmd[0])
        help(unvivtool)

if __name__ == "__main__":
    main()
