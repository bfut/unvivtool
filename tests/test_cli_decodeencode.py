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
  test_cli_decodeencode.py
"""
import os
import pathlib
import platform
import re
import shutil
import subprocess
import struct

import pytest

def get_subprocess_ret(command, shell=False):
    ret = subprocess.run(command, shell=shell, capture_output=True)
    print("args =", ret.args)
    print("stdout =", ret.stdout.decode("utf-8"))
    print("stderr =", ret.stderr.decode("utf-8"))
#    print("Exit code", ret.returncode);
    return ret

SCRIPT_PATH = pathlib.Path(__file__).parent.resolve()
OUTDIR = SCRIPT_PATH / ".out_cli"
if platform.system() == "Windows":
    if struct.calcsize("P") * 8 == 64:
        EXECUTABLE_PATH = SCRIPT_PATH / ".." / "unvivtool_x64.exe"
    else:
        EXECUTABLE_PATH = SCRIPT_PATH / ".." / "unvivtool_x86.exe"
elif platform.system() == "Linux":
    EXECUTABLE_PATH = SCRIPT_PATH / ".." / "unvivtool"
else:
    EXECUTABLE_PATH = SCRIPT_PATH / ".." / "unvivtool_macOS"
VALGRIND_EXISTS = get_subprocess_ret(" ".join([ "valgrind", "--version" ]), True).returncode == 0
if VALGRIND_EXISTS:
    VALGRIND_LOG = "valgrind.log"
    EXECUTABLE_PATH = f"valgrind -s --log-file=\"{str(OUTDIR / VALGRIND_LOG)}\" " + str(EXECUTABLE_PATH)

if OUTDIR.is_dir():
    shutil.rmtree(OUTDIR)
try:
    os.mkdir(OUTDIR)
except FileExistsError:
    pass
assert OUTDIR.is_dir()

def delete_whitespace(string):
    return re.sub(r"\s+", "", string, flags=re.MULTILINE)

def get_valgrind_results(string):
    m1 = re.search(r"^.*?in use at exit:.*?blocks\n", string, flags=re.MULTILINE)
    m2 = re.search(r"^.*?total heap usage:.*?allocated\n", string, flags=re.MULTILINE)
    m3 = re.search(r"^.*?ERROR SUMMARY:.*?\n", string, flags=re.MULTILINE)
    return m1.group(0)[:-1], m2.group(0)[:-1], m3.group(0)[:-1]

def delete_file(path):
    try:
        os.remove(path)
    except FileNotFoundError:
        pass

# ------------------------------------------------------------------------------
def test_dir():
    print(OUTDIR)
    print(EXECUTABLE_PATH)
    if platform.system() == "Windows":
        ret = get_subprocess_ret(" ".join([
            "dir",
        ]), True)
    else:
        ret = get_subprocess_ret(" ".join([
            "ls", "-lg",
        ]), True)
    assert OUTDIR.exists()
    assert True
    return ret

@pytest.mark.skipif(0, reason="")
def test_cli_smoketest():
    err = ""

    # capture stdout of Usage(void) from unvivtool.c
    void_Usage = re.findall(
        r"void Usage\(void\)(.*){1}fflush\(stdout\);",
        (SCRIPT_PATH / "../unvivtool.c").read_text("utf-8"),
        re.DOTALL
    )[0]
    void_Usage_strings = re.findall(
        r"(?!#include )\"(.*)\\n\"",
        void_Usage,
        re.MULTILINE
    )
    out = "\n".join([s for s in void_Usage_strings])
#     out = \
# """Usage: unvivtool d [<options>...] <path/to/input.viv> [<path/to/output_directory>]
#        unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...
#        unvivtool <path/to/input.viv>
#        unvivtool <paths/to/input_files>...
# """

    ret = get_subprocess_ret(" ".join([f"{EXECUTABLE_PATH}"]), True)
    stdout = delete_whitespace(ret.stdout.decode("utf-8"))
    assert ret.returncode == 0
    if "valgrind" not in str(EXECUTABLE_PATH):
        assert ret.stderr.decode("utf-8") == err
    else:
        log = (OUTDIR / VALGRIND_LOG).read_text(encoding="utf-8")
        a, b, c = get_valgrind_results(log)
        print(a)
        print(b)
        print(c)
        # print(log)
    assert re.sub(r"^.*?Usage", "Usage", stdout) == delete_whitespace(out)



@pytest.mark.skipif(0, reason="")
@pytest.mark.parametrize("verbose, dry",
    [ ("", ""),
      ("-v", ""),
      ("", "-p"),
      ("-v", "-p"), ])
def test_cli_decode_all_existing_dir(verbose, dry):
    err = ""
    out = \
"""File format (header) = BIGF
Archive Size (header) = 35307 (0x89eb)
Directory Entries (header) = 2
Header Size (header) = 55 (0x37)
Archive Size (parsed) = 35307 (0x89eb)
Header Size (parsed) = 55 (0x37)
Directory Entries (parsed) = 2
Endianness (parsed) = 0xe
Number extracted: 2
Decoder successful.
"""
    outv = \
"""Printing archive directory:

   id Valid       Offset          Gap         Size Len FnOf  Name
 ---- ----- ------------ ------------ ------------ --- ----  -----------------------
                       0                        55           header
 ---- ----- ------------ ------------ ------------ --- ----  -----------------------
    1     1           55            0        35149   7   18  LICENSE
    2     1        35204            0          103  14   28  pyproject.toml
 ---- ----- ------------ ------------ ------------ --- ----  -----------------------
                   35307                     35252           2 files
Number extracted: 2
Decoder successful.
"""
    poutv = \
"""Printing archive directory:

   id Valid       Offset          Gap         Size Len FnOf  Name
 ---- ----- ------------ ------------ ------------ --- ----  -----------------------
                       0                        55           header
 ---- ----- ------------ ------------ ------------ --- ----  -----------------------
    1     1           55            0        35149   7   18  LICENSE
    2     1        35204            0          103  14   28  pyproject.toml
 ---- ----- ------------ ------------ ------------ --- ----  -----------------------
                   35307                     35252           2 files
End dry run
Decoder successful.
"""
    delete_file(OUTDIR / "LICENSE")
    delete_file(OUTDIR / "pyproject.toml")
    ret = get_subprocess_ret(" ".join([
      f"{EXECUTABLE_PATH}",
      "d",
      verbose, dry,
      f"{SCRIPT_PATH / 'in/car.viv'}",
      f"{OUTDIR}",  # _{verbose}
    ]), True)
    stdout = delete_whitespace(ret.stdout.decode("utf-8"))
    assert ret.returncode == 0
    if "valgrind" not in str(EXECUTABLE_PATH):
        assert ret.stderr.decode("utf-8") == err
    else:
        log = (OUTDIR / VALGRIND_LOG).read_text(encoding="utf-8")
        a, b, c = get_valgrind_results(log)
        print(a)
        print(b)
        print(c)
        # print(log)
    if dry != "-p":
        if verbose != "-v":
            assert re.sub(r"^.*?File", "File", stdout) == delete_whitespace(out)
        else:
            assert re.sub(r"^.*?Printing", "Printing", stdout) == delete_whitespace(outv)
    else:
        assert re.sub(r"^.*?Printing", "Printing", stdout) == delete_whitespace(poutv)

# @pytest.mark.skipif(0, reason="")
# @pytest.mark.parametrize("verbose, dry",
#     [ ("", "", ""),
#       ("-v", "", ""),
#       ("", "", "-p"),
#       ("-v", "", "-p"), ])
# def test_cli_decode_existing_file_existing_dir(verbose, dry):
#     err = ""
#     out = \
# """Archive Size (parsed) = 35307 (0x89eb)
# Directory Entries (header) = 2
# Decoder successful.
# """
#     outv = \
# """Archive Size (parsed) = 35307 (0x89eb)
# Archive Size (header) = 35307 (0x89eb)
# Header Size (header) = 55 (0x37)
# File format (parsed) = BIGF
# Directory Entries (header) = 2
# Directory Entries (parsed) = 2
# Requested file idx = 2
# Requested file = pyproject.toml
# Buffer = 4096
# Header Size (parsed) = 55 (0x37)

# Printing VIV directory:

#    id       Offset Gap         Size Len  Name
#  ---- ------------ --- ------------ ---  -----------------------
#     1           55   0        35149   8  LICENSE
#     2        35204   0          103  15  pyproject.toml
#  ---- ------------ --- ------------ ---  -----------------------
#              35307            35252      2 files
# Decoder successful.
# """
#     poutv = \
# """Archive Size (parsed) = 35307
# Directory Entries (header) = 2
# Archive Size (header) = 35307
# Header Size (header) = 55 (0x37)
# File format (parsed) = BIGF
# Directory Entries (parsed) = 2
# Requested file idx = 2
# Requested file = pyproject.toml
# Buffer = 4096
# Header Size (parsed) = 55

# Printing VIV directory:

#    id       Offset Gap         Size Len  Name
#  ---- ------------ --- ------------ ---  -----------------------
#     1           55   0        35149   8  LICENSE
#     2        35204   0          103  15  pyproject.toml
#  ---- ------------ --- ------------ ---  -----------------------
#              35307            35252      2 files
# End dry run
# Decoder successful.
# """
#     ret = get_subprocess_ret(" ".join([
#       f"{EXECUTABLE_PATH}",
#       "d",
#       verbose, dry,
#       "-fn pyproject.toml",
#       f"{SCRIPT_PATH / 'in/car.viv'}",
#       f"{OUTDIR}",
#     ]), True)
#     assert ret.returncode == 0
#     assert ret.stderr.decode("utf-8") == err
#     if dry != "-p":
#         if verbose != "-v":
#             assert re.sub(r"^.*?\n+.*?\n+.*?\n+.*?\n+Archive", "Archive", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#         else:
#             assert re.sub(r"^.*?\n+.*?\n+.*?\n+.*?\n+Archive", "Archive", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     else:
#         assert ret.stderr.decode("utf-8") == "" #err
#         assert re.sub(r"^.*?\n+.*?\n.*?\n+.*?\n+.*?\n+Archive", "Archive", ret.stdout.decode("utf-8")) == delete_whitespace(poutv)


# @pytest.mark.skipif(0, reason="")
# @pytest.mark.parametrize("verbose, dry",
#     [ ("", "", ""),
#       ("-v", "", ""),
#       ("", "", "-p"),
#       ("-v", "", "-p"), ])
# def test_cli_decode_not_existing_file_existing_dir(verbose, dry):
#     err = \
# """Cannot find requested file in archive (filename is cAse-sEnsitivE)
# """
#     out = \
# """Archive Size (parsed) = 35307
# Directory Entries (header) = 2
# Decoder failed.
# """
#     outv = out  # ends on error, hence no more infp
#     poutv = outv  # ends on error, hence no "Endr dry run" message
#     ret = get_subprocess_ret(" ".join([
#       f"{EXECUTABLE_PATH}",
#       "d",
#       verbose, dry,
#       "-fn not_in_archive",
#       f"{SCRIPT_PATH / 'in/car.viv'}",
#       f"{OUTDIR}",
#     ]), True)
#     assert ret.returncode != 0
#     assert ret.stderr.decode("utf-8") == err
#     if dry != "-p":
#         if verbose != "-v":
#             assert re.sub(r"^.*?\n+.*?\n+.*?\n+.*?\n+Archive", "Archive", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#         else:
#             assert re.sub(r"^.*?\n+.*?\n+.*?\n+.*?\n+Archive", "Archive", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     else:
#         assert re.sub(r"^.*?\n+.*?\n+.*?\n+.*?\n+.*?\n+Archive", "Archive", ret.stdout.decode("utf-8")) == delete_whitespace(poutv)


# @pytest.mark.skipif(0, reason="")
# @pytest.mark.parametrize("verbose, strict, dry",
#     [ ("", "", ""),
#       ("-v", "", ""),
#       ("", "-strict", ""),
#       ("-v", "-strict", ""),# ])
#       ("", "", "-p"),
#       ("-v", "", "-p"),
#       ("", "-strict", "-p"),
#       ("-v", "-strict", "-p"), ])
# def test_cli_encode_with_existing(verbose, strict, dry):
#     err = ""
#     out = \
# """Number of files to encode = 2
# Encoder successful.
# """
#     outv = \
# """Number of files to encode = 2
# Buffer = 4096
# Header Size = 55
# Directory Entries = 2
# Archive Size = 35307

#    id       Offset         Size Len  Name
#  ---- ------------ ------------ ---  -----------------------
#     1           55        35149   8  LICENSE
#     2        35204          103  15  pyproject.toml
#  ---- ------------ ------------ ---  -----------------------
#              35307        35252      2 files
# Encoder successful.
# """
#     poutv = \
# """Number of files to encode = 2
# Buffer = 4096
# Header Size = 55
# Directory Entries = 2
# Archive Size = 35307

#    id       Offset         Size Len  Name
#  ---- ------------ ------------ ---  -----------------------
#     1           55        35149   8  LICENSE
#     2        35204          103  15  pyproject.toml
#  ---- ------------ ------------ ---  -----------------------
#              35307        35252      2 files
# End dry run
# Encoder successful.
# """
#     ret = get_subprocess_ret(" ".join([
#       f"{EXECUTABLE_PATH}",
#       "e",
#       verbose, strict, dry,
#       f"{OUTDIR / 'car.viv'}",
#       f"{SCRIPT_PATH / 'in/LICENSE'}", f"{SCRIPT_PATH / 'in/pyproject.toml'}",
#     ]), True)
#     assert ret.returncode == 0
#     assert ret.stderr.decode("utf-8") == err
#     assert ret.returncode == 0
#     if dry != "-p":
#       assert ret.stderr.decode("utf-8") == err
#       if verbose != "-v":
#           assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#       else:
#           assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     else:
#       assert ret.stderr.decode("utf-8") == "" # no error
#       assert re.sub(r"^.*?\n+.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(poutv)
#     """
#     if verbose != "-v":
#         assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#     else:
#         assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     """


# @pytest.mark.skipif(0, reason="")
# @pytest.mark.parametrize("verbose, strict, dry",
#     [ ("", "", ""),
#       ("-v", "", ""),
#       ("", "-strict", ""),
#       ("-v", "-strict", ""),# ])
#       ("", "", "-p"),
#       ("-v", "", "-p"),
#       ("", "-strict", "-p"),
#       ("-v", "-strict", "-p"), ])
# def test_cli_encode_with_skip(verbose, strict, dry):
#     err = ""
#     out = \
# """Number of files to encode = 3
# Cannot open file. Skipping 'not_a_file'
# Encoder successful.
# """
#     outv = \
# """Number of files to encode = 3
# Cannot open file. Skipping 'not_a_file'
# Buffer = 4096
# Header Size = 55
# Directory Entries = 2
# Archive Size = 35307

#    id       Offset         Size Len  Name
#  ---- ------------ ------------ ---  -----------------------
#     1           55        35149   8  LICENSE
#     2        35204          103  15  pyproject.toml
#  ---- ------------ ------------ ---  -----------------------
#              35307        35252      2 files
# Encoder successful.
# """
#     poutv = \
# """Number of files to encode = 3
# Cannot open file. Skipping 'not_a_file'
# Buffer = 4096
# Header Size = 55
# Directory Entries = 2
# Archive Size = 35307

#    id       Offset         Size Len  Name
#  ---- ------------ ------------ ---  -----------------------
#     1           55        35149   8  LICENSE
#     2        35204          103  15  pyproject.toml
#  ---- ------------ ------------ ---  -----------------------
#              35307        35252      2 files
# End dry run
# Encoder successful.
# """
#     ret = get_subprocess_ret(" ".join([
#       f"{EXECUTABLE_PATH}",
#       "e",
#       verbose, strict, dry,
#       f"{OUTDIR / 'car.viv'}",
#       f"{SCRIPT_PATH / 'in/LICENSE'}", "not_a_file", f"{SCRIPT_PATH / 'in/pyproject.toml'}",
#     ]), True)
#     assert ret.returncode == 0
#     assert ret.stderr.decode("utf-8") == err
#     """
#         if verbose != "-v":
#             assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#         else:
#             assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     """
#     assert ret.returncode == 0
#     if dry != "-p":
#       assert ret.stderr.decode("utf-8") == err
#       if verbose != "-v":
#           assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#       else:
#           assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     else:
#       assert ret.stderr.decode("utf-8") == "" #err
#       assert re.sub(r"^.*?\n+.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(poutv)


# @pytest.mark.skipif(0, reason="")
# @pytest.mark.parametrize("verbose, strict, dry",
#     [ ("", "", ""),
#       ("-v", "", ""),
#       ("", "-strict", ""),
#       ("-v", "-strict", ""),# ])
#       ("", "", "-p"),
#       ("-v", "", "-p"),
#       ("", "-strict", "-p"),
#       ("-v", "-strict", "-p"), ])
# def test_cli_encode_to_invalid_with_existing(verbose, strict, dry):
#     err = \
# """viv: Cannot create output file 'not_a_dir/car.viv'
# """
#     out = \
# """Number of files to encode = 2
# Encoder failed.
# """
#     outv = \
# """Number of files to encode = 2
# Buffer = 4096
# Header Size = 55
# Directory Entries = 2
# Archive Size = 35307

#    id       Offset         Size Len  Name
#  ---- ------------ ------------ ---  -----------------------
#     1           55        35149   8  LICENSE
#     2        35204          103  15  pyproject.toml
#  ---- ------------ ------------ ---  -----------------------
#              35307        35252      2 files
# Encoder failed.
# """
#     poutv = \
# """Number of files to encode = 2
# Buffer = 4096
# Header Size = 55
# Directory Entries = 2
# Archive Size = 35307

#    id       Offset         Size Len  Name
#  ---- ------------ ------------ ---  -----------------------
#     1           55        35149   8  LICENSE
#     2        35204          103  15  pyproject.toml
#  ---- ------------ ------------ ---  -----------------------
#              35307        35252      2 files
# End dry run
# Encoder successful.
# """
#     ret = get_subprocess_ret(" ".join([
#       f"{EXECUTABLE_PATH}",
#       "e",
#       verbose, strict, dry,
#       f"{'not_a_dir/' + 'car.viv'}",
#       f"{SCRIPT_PATH / 'in/LICENSE'}", f"{SCRIPT_PATH / 'in/pyproject.toml'}",
#     ]), True)
#     if dry != "-p":
#       assert ret.returncode != 0
#       assert ret.stderr.decode("utf-8") == err
#       if verbose != "-v":
#           assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(out)
#       else:
#           assert re.sub(r"^.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(outv)
#     else:
#       assert ret.returncode == 0
#       assert ret.stderr.decode("utf-8") == "" #err
#       assert re.sub(r"^.*?\n+.*?\n+.*?\n+Number", "Number", ret.stdout.decode("utf-8")) == delete_whitespace(poutv)

