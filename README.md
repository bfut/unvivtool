# unvivtool
unvivtool is a VIV/BIG decoder/encoder for uncompressed BIGF, BIGH, and BIG4 archives.
unvivtool is available as command-line interface, and as Python extension module.
It is based on a header-only library written in C89.
Python bindings are written in CPython.
Supported on Windows and Linux. Tested on macOS.

Purported VIV/BIG archives sometimes contain faulty or manipulated header information.
unvivtool is designed to validate and recover data as much as possible.

## Features
* decode and encode archive
* validate archive
* decode entire archive at once
* retrieve a single file from archive (by index or filename)
* list archive contents without writing to disk (dry run)
* encode files in set order
* fully support UTF8-filenames in archive
* fully support non-printable filenames in archive (represent as Base16)
* decode/encode with set fixed directory entry length
* low memory usage (typically ~30 kB; worst case ~25 MB)
* experimental BIG4 support

## File format
The formats are called ``BIGF``, ``BIGH``, and ``BIG4`` (equals first 4 bytes). Typical file extensions are ``.VIV`` and ``.BIG``.

## Installation / Documentation / Examples
Command-line interface: [/doc/unvivtool_cli.md](/doc/unvivtool_cli.md)<br/>
Python extension module: [/python/README.md](/python/README.md)

## References
The canonical BIGF format description was taken from [1].

[1] D. Auroux et al. [_The unofficial Need For Speed III file format specifications - Version 1.0_](/references/unofficial_nfs3_file_specs_10.txt) [1998]

## Information
__unvivtool License:__ GNU General Public License v3.0+<br/>
__Website:__ <https://github.com/bfut/unvivtool>

Third party licenses

__sclpython.h:__ zlib License<br/>
__UTF-8 Decoder dfa.h:__ MIT License
