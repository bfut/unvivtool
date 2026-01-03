# unvivtool
unvivtool is a VIV/BIG decoder/encoder for BIGF, BIGH, and BIG4 archives.
Available as command-line interface and Python extension module.
Based on a dependency-free header-only library written in C89.
Python bindings are written in CPython.
Supported on Windows and Linux. Tested on macOS.

Purported VIV/BIG archives can contain faulty or manipulated header information.
unvivtool is designed to validate and recover data as much as possible.
The decoder performs a single pass buffered read of the archive header only; content extraction is optional.

C/C++ developers can drop-in and use, the encoder/decoder and some data analysis functions from ``libnfsviv.h``.

## Features
* memory usage typically peaks below 20 kB even for large archives; worst case ~25 MB
* decode and encode archive
* validate archive
* decode entire archive
* encode files in specified order
* support UTF8 names in archive
* drag-and-drop mode for command-line interface
* option: auto-rename duplicated filenames on decode
* option: retrieve a single file from archive (by index or filename)
* option: list archive contents without writing to disk (dry run)
* option: encode to align file offsets to given power-of-two boundary
* option: support non-printable filenames in archive (Base16 representation)
* option: decode/encode with set fixed directory entry length
* Win98 compatibility

## File format
The supported archive formats are called ``BIGF``, ``BIGH``, ``BIG4``, ``0x8000FBC0``, and ``wwww`` (equals first 4 bytes).<br/>
The first four are variants of each other, the last is a precursor.
All are derived from ``EA IFF 85`` (see [2]).<br/>
Archives can be arbitrarily large and can contain arbitrarily many entries.<br/>
Typical file extensions are ``.VIV`` and ``.BIG``.<br/>

## Installation / Documentation
Command-line interface: [/cli/README.md](/cli/README.md)<br/>
Python extension module: [/python/README.md](/python/README.md)

## References
The canonical ``BIGF`` format description was taken from [1].
Unofficial descriptions of fixed directory entry length, format deviations and ``0x8000FBC0`` (see [3]), own work.

[1] D. Auroux et al. (1998) [_The unofficial Need For Speed III file format specifications - Version 1.0_](/references/unofficial_nfs3_file_specs_10.txt)<br/>
[2] J. Morrison (1985) _"EA IFF 85" Standard for Interchange Format Files_<br/>
[3] B. F. (2024) [_Unofficial 0xFBC0 File Format Specification (.viv)_](/references/viv_C0FB_specs.md)

## Information
__unvivtool License:__ GNU General Public License v3.0+<br/>
__Website:__ <https://github.com/bfut/unvivtool>

Portions copyright, see each source file for more information.
