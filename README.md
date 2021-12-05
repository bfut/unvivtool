# unvivtool
unvivtool is a VIV/BIG decoder/encoder. It is available as a command-line
interface, and as a Python module. VIV/BIG is an uncompressed archive format.

unvivtool is based on a header-only library written in C. Python bindings are
written in CPython.

## Features
* numerous format checks
* decodes an entire archive at once or retrieves a single file either by index
  or by filename
* optionally lists archive contents without writing to disk

## Installation / Documentation
Command-line interface: [/doc/unvivtool_cli.md](/doc/unvivtool_cli.md)<br/>
Python extension module: [/doc/unvivtool_py.md](/doc/unvivtool_py.md)

## References
D. Auroux et al. [_The unofficial Need For Speed III file format specifications - Version 1.0_](/references/unofficial_nfs3_file_specs_10.txt) [1998]

## Information
__unvivtool License:__ GNU General Public License v3.0+<br/>
__Website:__ <https://github.com/bfut/unvivtool>