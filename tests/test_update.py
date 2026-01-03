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
  test_update.py
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

if UVT_USE_TRACEMALLOC and __name__ != "__main__":
    import tracemalloc
    # tracemalloc -- BEGIN ------------------------------------
    # tracemalloc.stop()

    first_size, first_peak = tracemalloc.get_traced_memory()

    if sys.version_info[0:2] >= (3, 9):
        tracemalloc.reset_peak()

    tracemalloc.start(2)
    # tracemalloc -- END --------------------------------------

import unvivtool as uvt

if UVT_USE_TRACEMALLOC and __name__ != "__main__":
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

@pytest.mark.xfail(sys.platform.startswith("win") or 'microsoft' in platform.release().lower(),
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

    #debug
    shutil.copy2(vivfile, vivfile.with_suffix(".bak"))

    # update
    newfile = script_path / "in/öäü"
    assert newfile.exists()
    idx = 1
    retv = uvt.update(vivfile, newfile, idx, replace_filename=True, verbose=True)
    assert retv == 1 and vivfile.exists()
    viv_info = uvt.get_info(vivfile)
    print(viv_info)

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
    # windows second_size=333462 second_peak=1827646 Python 3.12
    assert second_size < 350000*10 and second_peak < 400000 * 10
