# This file is distributed under: CC BY-SA 4.0 <https://creativecommons.org/licenses/by-sa/4.0/>

"""
  tests.py - unit testing unvivtool module with tracemalloc

  NOTE: module should be built with setup_debug.py

  Test decoder:
  python tests.py d

  Test encoder:
  python tests.py e

  Show module help:
  python tests.py help
"""

import argparse
import os
import pathlib
import platform
import sys

# tracemalloc -- BEGIN ---------------------------------------------------------
# Source: https://docs.python.org/3/library/tracemalloc.html
import linecache
import os
import tracemalloc

def display_top(snapshot, key_type='lineno', limit=10):
    snapshot = snapshot.filter_traces((
        tracemalloc.Filter(False, "<frozen importlib._bootstrap>"),
        tracemalloc.Filter(False, "<unknown>"),
    ))
    top_stats = snapshot.statistics(key_type)

    print("Top %s lines" % limit)
    for index, stat in enumerate(top_stats[:limit], 1):
        frame = stat.traceback[0]
        print("#%s: %s:%s: %.1f KiB"
              % (index, frame.filename, frame.lineno, stat.size / 1024),
              "({:d})".format(stat.size))
        line = linecache.getline(frame.filename, frame.lineno).strip()
        if line:
            print('    %s' % line)

    other = top_stats[limit:]
    if other:
        size = sum(stat.size for stat in other)
        print("%s other: %.1f KiB" % (len(other), size / 1024),
              "({:d})".format(size))

    total = sum(stat.size for stat in top_stats)
    print("Total allocated size: %.1f KiB" % (total / 1024), "({:d})".format(total))

# tracemalloc -- END -----------------------------------------------------------


# Look for local build, if not installed
try:
    tracemalloc.start()
    import unvivtool
    tracemalloc.stop()
except ModuleNotFoundError:

    import sys
    p = pathlib.Path(pathlib.Path(__file__).parent / "build")
    for x in p.glob("**"):
        sys.path.append(str(x.resolve()))

    tracemalloc.start()
    import unvivtool
    tracemalloc.stop()


# Parse command: encode or decode (or print module help)
parser = argparse.ArgumentParser()
parser.add_argument("cmd", nargs=1, help="e: viv(), d: unviv()")
args = parser.parse_args()

# Change cwd to script path
script_path = pathlib.Path(__file__).parent.resolve()
os.chdir(script_path)

#
n = "\n"
print(
#    unvivtool.__doc__, n,
    unvivtool, n,
    dir(unvivtool), n,

    flush=True
)


# tracemalloc -- BEGIN ---------------------------------------------------------
# tracemalloc.stop()
first_size, first_peak = tracemalloc.get_traced_memory()

if sys.version_info[0:2] >= (3, 9):
    tracemalloc.reset_peak()

tracemalloc.start()
# tracemalloc -- END -----------------------------------------------------------


count_successful_tests = 0
print("")

# Decode
if args.cmd[0] == "d":
    vivfile = "tests/car.viv"
    outdir = "tests"
    request_fileid = 2            # optional
    request_filename = "LICENSE"  # optional, overrides request_fileid

    print("Test1: unvivtool.unviv(vivfile, outdir)")
    print("Expected result: extract all, return 1", flush=True)
    res = unvivtool.unviv(vivfile, outdir, verbose=1)
    if res == 1:
        print("Test1 success", "\n")
        count_successful_tests += 1
    else:
        print("Test1 failure", "\n")
    res = -1

    print("Test2: unvivtool.unviv(vivfile, outdir, 0, request_filename)")
    print("Expected result: extract LICENSE, return 1", flush=True)
    res = unvivtool.unviv(vivfile, outdir, 0, request_filename, verbose=1, dry=False)
    if res == 1:
        print("Test2 success", "\n")
        count_successful_tests += 1
    else:
        print("Test2 failure", "\n")
    res = -1

    print("Test3: unvivtool.unviv(vivfile, outdir, request_fileid)")
    print("Expected result: extract file at index 2, return 1", flush=True)
    res = unvivtool.unviv(vivfile, outdir, request_fileid, verbose=True)
    if res == 1:
        print("Test3 success", "\n")
        count_successful_tests += 1
    else:
        print("Test3 failure", "\n")
    res = -1

    print("Test4: unvivtool.unviv(vivfile, outdir)")
    print("Expected result: extract LICENSE, return 1", flush=True)
    res = unvivtool.unviv(vivfile, outdir, request_fileid, request_filename, verbose=1)
    if res == 1:
        print("Test4 success", "\n")
        count_successful_tests += 1
    else:
        print("Test4 failure", "\n")
    res = -1

    print("Test5: unvivtool.unviv(vivfile, \"not_a_dir\")")
    print("Expected result: outdir does not exist, return 0", flush=True)
    try:
        res = unvivtool.unviv(vivfile, "not_a_dir", verbose=1)
    except FileNotFoundError as e:
        print("FileNotFoundError:", e)
        res = 0
    if res == 0:
        print("Test5 success", "\n")
        count_successful_tests += 1
    else:
        print("Test5 failure", "\n")
    res = -1

    print("Test6: unvivtool.unviv(vivfile, outdir, keyword=request_filename)")
    print("Expected result: extract LICENSE, return 1", flush=True)
    res = unvivtool.unviv(vivfile, outdir, filename=request_filename, verbose=1)
    if res == 1:
        print("Test6 success", "\n")
        count_successful_tests += 1
    else:
        print("Test6 failure", "\n")
    res = -1

    print("Test7: unvivtool.unviv(vivfile, outdir, keyword=request_fileid)")
    print("Expected result: extract file at index 2, return 1", flush=True)
    res = unvivtool.unviv(vivfile, outdir, fileidx=request_fileid, verbose=1)
    if res == 1:
        print("Test7 success", "\n")
        count_successful_tests += 1
    else:
        print("Test7 failure", "\n")
    res = -1

    if platform.system() == "Windows":
        print("Test8: unvivtool.unviv(\"tests/@二.viv\", outdir, keyword=request_filename)".encode())
        print("Expected result: Linux - decode all, return 1".encode())
        print("                 Windows - \"tests/@二.viv\" raises FileNotFoundError: no unicode support, return NULL".encode(), flush=True)
    else:
        print("Test8: unvivtool.unviv(\"tests/@二.viv\", outdir, keyword=request_filename)")
        print("Expected result: Linux - decode all, return 1")
        print("                 Windows - \"tests/@二.viv\" raises FileNotFoundError: no unicode support, return NULL", flush=True)
    try:
        res = unvivtool.unviv("tests/@二.viv", outdir, verbose=1, filename=request_filename)
    except FileNotFoundError as e:
        print("FileNotFoundError:", e)
        res = 1
    if res == 1:
        print("Test8 success", "\n")
        count_successful_tests += 1
    else:
        print("Test8 failure", "\n")
    res = -1

    if platform.system() == "Windows":
        print("Test9: unvivtool.unviv(vivfile, outdir, keyword=\"ß二\")".encode())
        print("Expected result: cannot find requested file, return 0".encode(), flush=True)
    else:
        print("Test9: unvivtool.unviv(vivfile, outdir, keyword=\"ß二\")")
        print("Expected result: cannot find requested file, return 0", flush=True)
    res = unvivtool.unviv(vivfile, outdir, filename="ß二", verbose=1)
    if res == 0:
        print("Test9 success", "\n")
        count_successful_tests += 1
    else:
        print("Test9 failure", "\n")
    res = -1

    print("Test10: unvivtool.unviv(\"öäü\", None)")
    print("Expected result: TypeError, return NULL", flush=True)
    try:
        res = unvivtool.unviv("öäü", None)
    except TypeError as e:
        print("TypeError:", e)
        res = 1
    if res == 1:
        print("Test10 success", "\n")
        count_successful_tests += 1
    else:
        print("Test10 failure", "\n")
    res = -1

    print("Test11: unvivtool.unviv(\"foo\", None)")
    print("Expected result: TypeError, return NULL", flush=True)
    try:
        res = unvivtool.unviv("foo", None)
    except TypeError as e:
        print("TypeError:", e)
        res = 1
    if res == 1:
        print("Test11 success", "\n")
        count_successful_tests += 1
    else:
        print("Test11 failure", "\n")
    res = -1

    print("Successful tests: {:d}/{:d}".format(count_successful_tests, 11), "\n")

# Encode
elif args.cmd[0] == "e":
    vivfile = "tests/car_out.viv"

    print("Test1: infiles = [\"../LICENSE\", \"pyproject.toml\"]")
    print("Expected result: encode all, return 1", flush=True)
    infiles = ["../LICENSE", "pyproject.toml"]  # (7+1) + (14+1) = 23
    res = unvivtool.viv(vivfile, infiles, verbose=1)
    if res == 1:
        print("Test1 success", "\n")
        count_successful_tests += 1
    else:
        print("Test1 failure", "\n")
    infiles = None
    res = -1

    print("Test2: infiles = [\"../LICENSE\", \"pyproject.toml\", \"not_a_file\"]")
    print("Expected result: skip file that cannot be opened, encode the rest, return 1", flush=True)
    infiles = ["../LICENSE", "pyproject.toml", "not_a_file"]  # (7+1) + (14+1) + (10+1) = 34
    res = unvivtool.viv(vivfile, infiles, verbose=1)
    if res == 1:
        print("Test2 success", "\n")
        count_successful_tests += 1
    else:
        print("Test2 failure", "\n")
    infiles = None
    res = -1

    print("Test3: infiles = []")
    print("Expected result: do nothing, return 1", flush=True)
    infiles = []
    res = unvivtool.viv(vivfile, infiles, verbose=1)
    if res == 1:
        print("Test3 success", "\n")
        count_successful_tests += 1
    else:
        print("Test3 failure", "\n")
    infiles = None
    res = -1

    print("Test4: infiles = [infiles, infiles]")
    print("Expected result: TypeError, return NULL", flush=True)
    infiles = [infiles, infiles]
    try:
        res = unvivtool.viv(vivfile, infiles)
    except TypeError as e:
        print("TypeError:", e)
        res = 1
    if res == 1:
        print("Test4 success", "\n")
        count_successful_tests += 1
    else:
        print("Test4 failure", "\n")
    infiles = None
    res = -1

    if platform.system() == "Windows":
        print("Test5: vivfile = \"tests/@二.viv\"".encode())
        print("Expected result: Linux - encode all, return 1".encode())
        print("                 Windows - FileNotFoundError, return NULL".encode(), flush=True)
    else:
        print("Test5: vivfile = \"tests/@二.viv\"")
        print("Expected result: Linux - encode all, return 1")
        print("                 Windows - FileNotFoundError, return NULL", flush=True)
    infiles = ["../LICENSE", "pyproject.toml"]
    try:
        res = unvivtool.viv("tests/@二.viv", infiles, verbose=True)
    except FileNotFoundError as e:
        print("FileNotFoundError:", e)
        res = 1
    if res == 1:
        print("Test5 success", "\n")
        count_successful_tests += 1
    else:
        print("Test5 failure", "\n")
    infiles = None
    res = -1

    print("Test6: infiles = [\"../LICENSE\", \"pyproject.toml\", \"tests/foo\", \"tests/bar\"")
    print("Expected result: Linux - encode 2 existing, return 1")
    print("                 Windows - skip files that cannot be opened, encode the rest, return 1", flush=True)
    infiles = ["../LICENSE", "pyproject.toml", "tests/foo", "tests/bar"]  #
    res = unvivtool.viv(vivfile, infiles, verbose=True)
    if res == 1:
        print("Test6 success", "\n")
        count_successful_tests += 1
    else:
        print("Test6 failure", "\n")
    infiles = None
    res = -1

    if platform.system() == "Windows":
        print("Test7: infiles = [\"../LICENSE\", \"pyproject.toml\", \"tests/ß二\", \"tests/öäü\"".encode())
        print("Expected result: Linux - encode 2 existing, return 1".encode())
        print("                 Windows - skip files that cannot be opened, encode the rest, return 1".encode(), flush=True)
    else:
        print("Test7: infiles = [\"../LICENSE\", \"pyproject.toml\", \"tests/ß二\", \"tests/öäü\"")
        print("Expected result: Linux - encode 2 existing, return 1")
        print("                 Windows - skip files that cannot be opened, encode the rest, return 1", flush=True)
    infiles = ["../LICENSE", "pyproject.toml", "tests/ß二", "tests/öäü"]  #
    res = unvivtool.viv(vivfile, infiles, verbose=True)
    if res == 1:
        print("Test7 success", "\n")
        count_successful_tests += 1
    else:
        print("Test7 failure", "\n")
    infiles = None
    res = -1

    print("Test8: infiles = (\"not_a_list\", infiles)")
    print("Expected result: expected list, TypeError, return NULL", flush=True)
    infiles = ("not_a_list", infiles)
    try:
        res = unvivtool.viv(vivfile, infiles, dry=True)
    except TypeError as e:
        print("TypeError:", e)
        res = 1
    if res == 1:
        print("Test8 success", "\n")
        count_successful_tests += 1
    else:
        print("Test8 failure", "\n")
    infiles = None
    res = -1

    print("Test9: infiles = None")
    print("Expected result: TypeError, return NULL", flush=True)
    infiles = None
    try:
        res = unvivtool.viv(vivfile, infiles)
    except TypeError as e:
        print("TypeError:", e)
        res = 1
    if res == 1:
        print("Test9 success", "\n")
        count_successful_tests += 1
    else:
        print("Test9 failure", "\n")
    infiles = None
    res = -1

    print("Test10: vivfile = None, infiles = [\"../LICENSE\", \"pyproject.toml\"]")
    print("Expected result: TypeError, return NULL", flush=True)
    infiles = ["../LICENSE", "pyproject.toml"]  # (7+1) + (14+1) = 23
    try:
        res = unvivtool.viv(None, infiles)
    except TypeError as e:
        print("TypeError:", e)
        res = 1
    if res == 1:
        print("Test10 success", "\n")
        count_successful_tests += 1
    else:
        print("Test10 failure", "\n")
    infiles = None
    res = -1

    print("Successful tests: {:d}/{:d}".format(count_successful_tests, 10), "\n")

#
else:
    print("Invalid command (expects {d, e}):", args.cmd[0])
    help(unvivtool)


# tracemalloc -- BEGIN ---------------------------------------------------------
# tracemalloc.stop()
second_size, second_peak = tracemalloc.get_traced_memory()
# tracemalloc.start()

snapshot = tracemalloc.take_snapshot()
display_top(snapshot, limit=40)

print("first_size={:d}".format(first_size), "first_peak={:d}".format(first_peak))
print("second_size={:d}".format(second_size), "second_peak={:d}".format(second_peak))

# tracemalloc -- END -----------------------------------------------------------