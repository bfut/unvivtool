# unvivtool
unvivtool is a VIV/BIG decoder/encoder for uncompressed BIGF, BIGH, and BIG4 archives.
unvivtool is available as command-line interface, and as Python extension module.
It is based on a dependency-free header-only library written in C89.
Python bindings are written in CPython.
Supported on Windows and Linux. Tested on macOS.

Purported VIV/BIG archives can contain faulty or manipulated header information.
unvivtool is designed to validate and recover data as much as possible.

Developers can drop-in and use, the encoder/decoder and some data analysis functions from ``libnfsviv.h``.

## Features
* decode and encode archive
* validate archive
* decode entire archive at once
* drag-and-drop mode for command-line interface
* encode files in specified order
* full UTF8 support
* auto-rename duplicated filenames
* option: retrieve a single file from archive (by index or filename)
* option: list archive contents without writing to disk (dry run)
* option: support non-printable filenames in archive (Base16 representation)
* option: decode/encode with set fixed directory entry length
* compatible with Windows 98 and later
* memory usage typically peaks at ~45 kB; worst case ~25 MB

## File format
The supported formats are called ``BIGF``, ``BIGH``, and ``BIG4`` (equals first 4 bytes).<br/>
Typical file extensions are ``.VIV`` and ``.BIG``.

## Installation / Documentation
Command-line interface: [/doc/unvivtool_cli.md](/doc/unvivtool_cli.md)<br/>
Python extension module: [/python/README.md](/python/README.md)

## References
The canonical BIGF format description was taken from [1].
Fixed directory entry length and all format deviations, own research.

[1] D. Auroux et al. [_The unofficial Need For Speed III file format specifications - Version 1.0_](/references/unofficial_nfs3_file_specs_10.txt) [1998]

## Information
__unvivtool License:__ GNU General Public License v3.0+<br/>
__Website:__ <https://github.com/bfut/unvivtool>

Third party licenses

__UTF-8 Decoder dfa.h:__ MIT License<br/>
__circular buffer libnfsviv.h:__ zlib License (portions copyright)<br/>
__sclpython.h:__ zlib License
