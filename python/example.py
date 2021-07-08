# This file is distributed under: CC BY-SA 4.0 <https://creativecommons.org/licenses/by-sa/4.0/>

"""
  example.py

  Run decoder:
  python example.py d

  Run encoder:
  python example.py e
"""

import argparse
import pathlib

import unvivtool

# Parse command: encode or decode (or print module help)
parser = argparse.ArgumentParser()
parser.add_argument("cmd", nargs=1, help="e: viv(), d: unviv()")
args = parser.parse_args()

# Decode
if args.cmd[0] == "d":
    vivfile = "tests/car.viv"                   # all paths can be absolute or relative
    outdir = "tests"
    unvivtool.unviv(vivfile, outdir)            # extract all files in archive 'vivfile'

# Encode
elif args.cmd[0] == "e":
    vivfile = "tests/car_out.viv"
    infiles = ["LICENSE", "pyproject.toml"]
    unvivtool.viv(vivfile, infiles)             # encode all files in 'infiles'

#
else:
    print("Invalid command (expects {d, e}):", args.cmd[0])
    help(unvivtool)