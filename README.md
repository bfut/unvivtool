[![ci-cli](https://github.com/bfut/unvivtool/actions/workflows/ci-cli.yml/badge.svg)](https://github.com/bfut/unvivtool/actions/workflows/ci-cli.yml)
[![python](https://github.com/bfut/unvivtool/actions/workflows/ci-python.yml/badge.svg)](https://github.com/bfut/unvivtool/actions/workflows/ci-python.yml)

# unvivtool
unvivtool is a VIV/BIG decoder/encoder. VIV/BIG is an uncompressed archive format.
It is available as command-line interface, and as Python extension module.
unvivtool is based on a header-only library written in C89. Python bindings are
written in CPython. Supported on Windows and Linux.
Tested on macOS.

## Features
* validates VIV/BIG archive
* decodes entire archive at once
* retrieves a single file from archive (by index or by filename)
* lists archive contents without writing to disk

## Installation / Documentation
Command-line interface: [/doc/unvivtool_cli.md](/doc/unvivtool_cli.md)<br/>
Python extension module: [/python/README.md](/python/README.md)

## References
D. Auroux et al. [_The unofficial Need For Speed III file format specifications - Version 1.0_](/references/unofficial_nfs3_file_specs_10.txt) [1998]

## Information
__unvivtool License:__ GNU General Public License v3.0+<br/>
__Website:__ <https://github.com/bfut/unvivtool>
