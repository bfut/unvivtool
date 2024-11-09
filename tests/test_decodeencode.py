# unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>

# Portions copyright, see each source file for more information.

# You may not redistribute this program without its source code.
# README.md may not be removed or altered from any unvivtool redistribution.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""
  test_decodeencode.py
"""
import os
import pathlib
import platform
import shutil
import sys

import pytest

UVT_USE_TRACEMALLOC = True

# --------------------------------------
script_path = pathlib.Path(__file__).parent
invivfile = script_path / "in/car.viv"
outdir = script_path / ".out"
request_fileid = 2            # optional
request_filename = "LICENSE"  # optional, overrides request_fileid

if outdir.is_dir():
    shutil.rmtree(outdir)
elif outdir.exists():
    raise FileExistsError(outdir)
try:
    os.mkdir(outdir)
except FileExistsError:
    pass

if UVT_USE_TRACEMALLOC and platform.python_implementation() != "PyPy" and __name__ != "__main__":
    import tracemalloc
    # tracemalloc -- BEGIN ------------------------------------
    # tracemalloc.stop()

    first_size, first_peak = tracemalloc.get_traced_memory()

    if sys.version_info[0:2] >= (3, 9):
        tracemalloc.reset_peak()

    tracemalloc.start(2)
    # tracemalloc -- END --------------------------------------

import unvivtool as uvt

if UVT_USE_TRACEMALLOC and platform.python_implementation() != "PyPy" and __name__ != "__main__":
    # tracemalloc -- BEGIN ------------------------------------
    # Source: https://docs.python.org/3/library/tracemalloc.html
    import linecache
    import os
    import tracemalloc

    # https://docs.python.org/3/library/tracemalloc.html#filter
    def display_top(snapshot, key_type="lineno", limit=10):
        snapshot = snapshot.filter_traces((
            tracemalloc.Filter(True, os.__file__, all_frames=True),
            tracemalloc.Filter(True, pathlib.__file__, all_frames=True),
            tracemalloc.Filter(False, platform.__file__, all_frames=True),
            tracemalloc.Filter(False, shutil.__file__, all_frames=True),
            tracemalloc.Filter(False, tracemalloc.__file__, all_frames=True),
            tracemalloc.Filter(False, "<frozen importlib._bootstrap>"),
            # tracemalloc.Filter(True, "<unknown>", all_frames=True),

            tracemalloc.Filter(True, pytest.__file__, all_frames=True),
            tracemalloc.Filter(True, uvt.__file__, all_frames=True),
        ))
        top_stats = snapshot.statistics(key_type)

        # assert len(top_stats) < limit
        print(tracemalloc.get_traceback_limit())


        print(f"Top {limit} lines")
        for index, stat in enumerate(top_stats[:limit], 1):
            for i in range(len(stat.traceback)):
                frame = stat.traceback[0]
                print(f"#{index}: {frame.filename}:{frame.lineno}: "
                      f"{(stat.size / 1024):.1f} KiB "
                      f"({stat.size}) "
                      f"{i}")
                line = linecache.getline(frame.filename, frame.lineno).strip()
                if line:
                    print(f"    {line}")

        other = top_stats[limit:]
        if other:
            size = sum(stat.size for stat in other)
            print(f"{len(other)} other: {(size / 1024):.1f} KiB" ,
                f"({size})")

        total = sum(stat.size for stat in top_stats)
        print(f"Total allocated size: {(total / 1024):.1f} KiB ({total})")
    # tracemalloc -- END --------------------------------------

retv = 1

def test_decode1():
    print("Expected result: extract all, return 1")
    retv = uvt.unviv(invivfile, outdir, verbose=1)
    assert retv == 1

def test_decode2():
    print("Expected result: extract LICENSE, return 1")
    retv = uvt.unviv(invivfile, outdir, fileidx=0, filename=request_filename, verbose=1, dry=False)
    assert retv == 1

def test_decode3():
    print("Expected result: extract file at index 2, return 1")
    retv = uvt.unviv(invivfile, outdir, fileidx=request_fileid, verbose=True)
    assert retv == 1

def test_decode4():
    print("Expected result: extract LICENSE, return 1")
    retv = uvt.unviv(invivfile, outdir, fileidx=request_fileid, filename=request_filename, verbose=1)
    assert retv == 1

def test_decode5():
    print("Expected result: outdir does not exist and is created, return 1")
    try:
        os.rmdir(outdir / "not_a_dir")
    except FileNotFoundError:
        pass
    retv = uvt.unviv(invivfile, outdir / "not_a_dir", verbose=1)
    assert retv == 1

def test_decode6():
    print("Expected result: extract LICENSE, return 1")
    retv = uvt.unviv(invivfile, outdir, filename=request_filename, verbose=1)
    assert retv == 1

def test_decode7():
    print("Expected result: extract file at index 2, return 1")
    retv = uvt.unviv(invivfile, outdir, fileidx=request_fileid, verbose=1)
    assert retv == 1

@pytest.mark.xfail(sys.platform.startswith("win"),
                   reason="encoding issues on Windows...")
def test_decode8():
    retv = uvt.unviv(script_path / "in/@二.viv", outdir, verbose=1, filename=request_filename)
    assert retv == 1

def test_decode9():
    print('Test9: uvt.unviv(invivfile, outdir, keyword="ß二")'.encode())
    print("Expected result: cannot find requested file, return 0")
    retv = uvt.unviv(invivfile, outdir, filename="ß二", verbose=1)
    assert retv == 0

def test_decode10():
    print('Test10: uvt.unviv("öäü", None)')
    print("Expected result: TypeError")
    try:
        uvt.unviv("öäü", None)
        assert False
    except TypeError as e:
        print("TypeError:", e)

def test_decode11():
    print('Test11: uvt.unviv("foo", None)')
    print("Expected result: TypeError")
    try:
        uvt.unviv("foo", None)
        assert False
    except TypeError as e:
        print("TypeError:", e)


def test_encode1():
    vivfile = script_path / ".out/car_out_encode1.viv"
    vivfile.unlink(True)
    infiles = [ str(script_path / "in/LICENSE"), str(script_path / "in/pyproject.toml") ]  # (7+1) + (14+1) = 23
    for path in infiles:
        assert pathlib.Path(path).is_file()
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()
    viv_info = uvt.get_info(vivfile)
    # print(viv_info)
    assert viv_info.get("format") == "BIGF" and viv_info.get("count_dir_entries") == viv_info.get("count_dir_entries_true")
    assert viv_info.get("count_dir_entries") == len(infiles)

def test_encode2():
    print('Test2: infiles = [ "in/LICENSE", "invalid_path", "in/pyproject.toml" ]')
    print("Expected result: skip path that does not exist, encode the rest, return 1", flush=True)
    vivfile = script_path / ".out/car_out_encode2.viv"
    vivfile.unlink(True)
    infiles = [ "in/LICENSE", "invalid_path", "in/pyproject.toml" ]  # (7+1) + (12+1) + (14+1) = 36
    infiles = [str(script_path / path) for path in infiles]
    for path in infiles:
        assert pathlib.Path(path).exists() or pathlib.Path(path).stem == "invalid_path"
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()
    viv_info = uvt.get_info(vivfile)
    # print(viv_info)
    assert viv_info.get("format") == "BIGF" and viv_info.get("count_dir_entries") == viv_info.get("count_dir_entries_true")
    assert viv_info.get("count_dir_entries") == len(infiles) - 1


def test_encode3():
    print("Test3: infiles = []")
    vivfile = script_path / ".out/car_out_encode3.viv"
    vivfile.unlink(True)
    infiles = []
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and len(vivfile.read_bytes()) == 16

def test_encode4():
    print("Test4: infiles = [infiles, infiles]")
    print("Expected result: TypeError", flush=True)
    vivfile = script_path / ".out/car_out_encode4.viv"
    vivfile.unlink(True)
    infiles = []
    infiles = [infiles, infiles]
    assert isinstance(infiles, list)
    try:
        uvt.viv(vivfile, infiles)
        assert False
    except TypeError as e:
        print("TypeError:", e)
        assert str(e) == "expected list of str"
    assert not vivfile.exists()

@pytest.mark.xfail(sys.platform.startswith("win"),
                   reason="encoding issues on Windows...")
def test_encode5():
    print('Test5: vivfile = "tests/@二.viv"'.encode())
    print("Expected result: Linux - encode all, return 1")
    vivfile = script_path / ".out/@二_encode5.viv"
    vivfile.unlink(True)
    infiles = [ str(script_path / "in/LICENSE"), str(script_path / "in/pyproject.toml") ]
    for path in infiles:
        assert pathlib.Path(path).is_file()
    retv = uvt.viv(vivfile, infiles, verbose=True)
    assert retv == 1 and vivfile.exists()

def test_encode6():
    print('Test6: infiles = ["in/LICENSE", "in/pyproject.toml", "in"]')
    print("Expected result: encode existing files, skip existing directory, return 1")
    vivfile = script_path / ".out/car_out_encode6.viv"
    vivfile.unlink(True)
    infiles = [ str(script_path / "in/LICENSE"), str(script_path / "in/pyproject.toml"), str(script_path / "in/") ]
    for path in infiles:
        assert pathlib.Path(path).is_file() or pathlib.Path(path).is_dir()
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()

def test_encode7():
    print('Test7: infiles = ["in/LICENSE", "in/pyproject.toml", "."]')
    print("Expected result: encode existing files, skip existing directory, return 1")
    vivfile = script_path / ".out/car_out_encode7.viv"
    vivfile.unlink(True)
    infiles = [ str(script_path / "in/LICENSE"), str(script_path / "in/pyproject.toml"), str(script_path / "./") ]
    for path in infiles:
        assert pathlib.Path(path).is_file() or pathlib.Path(path).is_dir()
    retv = uvt.viv(vivfile, infiles, verbose=True)
    assert retv == 1 and vivfile.exists()

def test_encode8():
    print('Test8: infiles = ["in/LICENSE", "in/pyproject.toml", "in/ß二", "in/öäü"]'.encode())
    print("Expected result: encode existing files, return 1")
    vivfile = script_path / ".out/car_out_encode8.viv"
    vivfile.unlink(True)
    infiles = [ "in/LICENSE", "in/pyproject.toml", "in/ß二" ]
    infiles = [str(script_path / path) for path in infiles]
    for path in infiles:
        assert pathlib.Path(path).is_file()
    retv = uvt.viv(vivfile, infiles, verbose=True)
    assert retv == 1 and vivfile.exists()

def test_encode9():
    vivfile = script_path / ".out/car_out_encode9.viv"
    vivfile.unlink(True)
    infiles = ( "in/LICENSE", "in/pyproject.toml" )
    assert isinstance(infiles, tuple)
    try:
        uvt.viv(vivfile, infiles)
        assert False
    except TypeError as e:
        print("TypeError:", e)
        assert str(e) == "expected list"
    assert not vivfile.exists()

def test_encode10():
    vivfile = script_path / ".out/car_out_encode10.viv"
    vivfile.unlink(True)
    infiles = None
    try:
        uvt.viv(vivfile, infiles)
        assert False
    except TypeError as e:
        print("TypeError:", e)
        assert str(e) == "expected list"
    assert not vivfile.exists()

def test_encode11():
    vivfile = None
    infiles = [ "in/LICENSE", "in/pyproject.toml" ]
    try:
        uvt.viv(vivfile, infiles)
        assert False
    except TypeError as e:
        print("TypeError:", e)
        assert str(e) == "expected str, bytes or os.PathLike object, not NoneType"

def test_encode12():
    vivfile = script_path / ".out/car_out_encode12.viv"
    vivfile.unlink(True)
    infiles = [ "invalid_path", "invalid_path", "invalid_path" ]
    infiles = [str(script_path / path) for path in infiles]
    print(infiles)
    for path in infiles:
        assert not pathlib.Path(path).exists()
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()

def test_encode13():
    vivfile = script_path / ".out/car_out_encode13.viv"
    vivfile.unlink(True)
    infiles = [ "foo/invalid_path1", "bar/invalid_path2", "in/invalid_path3" ]  # notice no file-ending
    print(infiles)
    for path in infiles:
        assert not pathlib.Path(path).exists()
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()

def test_encode14():
    vivfile = script_path / ".out/car_out_encode14.viv"
    vivfile.unlink(True)
    infiles = [ "invalid_path", "invalid_path", "invalid_path" ]
    print(script_path, infiles)
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()

def test_encode15():
    vivfile = script_path / ".out/car_out_encode15.viv"
    vivfile.unlink(True)
    infiles = [ "in/LICENSE", "in/pyproject.toml", "invalid_path" ]  # notice file-ending
    print(script_path, infiles)
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists()

def test_encode16():
    vivfile = script_path / ".out/car_out_encode16.viv"
    vivfile.unlink(True)
    infiles = [ "in/LICENSE", "in/pyproject.toml", ]
    print(script_path, infiles)
    retv = uvt.viv(vivfile, infiles, verbose=1, dry=0)
    assert retv == 1 and vivfile.exists()

def test_encode17():
    """uvt.viv() overwrite existing file"""
    vivfile = script_path / ".out/car_out_encode17.viv"
    vivfile.unlink(True)
    infiles = [ "invalid" ]
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists() and len(vivfile.read_bytes()) == 16
    viv_info = uvt.get_info(vivfile)
    print(viv_info)
    assert viv_info["size"] == os.path.getsize(vivfile)

    infiles = [ str(script_path / "in/LICENSE"), str(script_path / "in/pyproject.toml") ]
    for path in infiles:
        assert pathlib.Path(path).is_file()
    retv = uvt.viv(vivfile, infiles, verbose=1)
    assert retv == 1 and vivfile.exists() and len(vivfile.read_bytes()) > 16
    viv_info = uvt.get_info(vivfile)
    print(viv_info)
    assert viv_info["size"] == os.path.getsize(vivfile)
    assert viv_info["__state"] == 14
    assert viv_info.get("format") == "BIGF" and viv_info.get("count_dir_entries") == viv_info.get("count_dir_entries_true")
    assert viv_info.get("count_dir_entries") == len(infiles)


def test_update1():
    print('update1: infiles = ["in/LICENSE", "in/pyproject.toml", "in/ß二", "in/öäü"]'.encode())
    print("Expected result: encode existing files, update idx=2, return 1")
    vivfile = script_path / ".out/car_out_update1.viv"
    vivfile.unlink(True)
    infiles = [ "in/LICENSE", "in/pyproject.toml", "in/ß二" ]
    infiles = [str(script_path / path) for path in infiles]
    for path in infiles:
        assert pathlib.Path(path).is_file()
    retv = uvt.viv(vivfile, infiles, verbose=True)
    assert retv == 1 and vivfile.exists() and len(vivfile.read_bytes()) > 16
    viv_info = uvt.get_info(vivfile)
    print(viv_info)

    # update
    newfile = script_path / "in/car_out.viv"
    assert newfile.exists()
    idx = 2
    retv = uvt.update(vivfile, newfile, idx, replace_filename=False, verbose=True)
    assert retv == 1 and vivfile.exists()
    viv_info = uvt.get_info(vivfile)
    print(viv_info)

@pytest.mark.xfail(sys.platform.startswith("win"),
                   reason="encoding issues on Windows...")
def test_update2():
    print("Expected result: encode existing files, update idx=1, return 1")
    vivfile = script_path / ".out/car_out_update2.viv"
    vivfile.unlink(True)
    infiles = [ "in/LICENSE", "in/pyproject.toml", "in/ß二" ]
    infiles = [str(script_path / path) for path in infiles]
    for path in infiles:
        assert pathlib.Path(path).is_file()
    retv = uvt.viv(vivfile, infiles, verbose=True)
    assert retv == 1 and vivfile.exists() and len(vivfile.read_bytes()) > 16
    viv_info = uvt.get_info(vivfile)
    print(viv_info)

    # update
    newfile = script_path / "in/öäü"
    assert newfile.exists()
    idx = 1
    retv = uvt.update(vivfile, newfile, idx, replace_filename=True, verbose=True)
    assert retv == 1 and vivfile.exists()
    viv_info = uvt.get_info(vivfile)
    print(viv_info)

@pytest.mark.skipif(platform.python_implementation() == "PyPy",
                   reason="'pypy-3.8' no tracemalloc")
@pytest.mark.xfail(sys.platform != "linux",
                   reason="encoding issues on Windows...")
def test_tracemalloc1():
    if not UVT_USE_TRACEMALLOC:  return
    # tracemalloc -- BEGIN ------------------------------------
    # tracemalloc.stop()
    second_size, second_peak = tracemalloc.get_traced_memory()
    # tracemalloc.start()

    snapshot = tracemalloc.take_snapshot()
    display_top(snapshot, limit=40)

    print(f"first_size={first_size}", f"first_peak={first_peak}")
    print(f"second_size={second_size}", f"second_peak={second_peak}")
    print(flush = True)
    # tracemalloc -- END --------------------------------------

    # macos   second_size=164595 second_peak=205471
    # ubuntu  second_size=245752 second_peak=284290
    # windows second_size=615734 second_peak=1901992 Python 3.10
    # windows second_size=425980 second_peak=1859260 Python 3.11
    # windows second_size=333462 second_peak=1827646 Python 3.12
    assert second_size < 350000 and second_peak < 400000

if __name__ == "__main__":
    print(uvt.__version__)
    test_encode1()
    test_encode2()
    test_encode3()
    test_encode4()
    test_encode5()
    test_encode6()
    test_encode7()
    test_encode8()
    test_encode9()
    test_encode10()
    test_encode11()
    test_encode12()
    test_encode13()
    test_encode14()
    test_encode15()
    test_encode16()
    test_encode17()
    test_update1()
