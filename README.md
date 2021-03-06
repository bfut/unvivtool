# unvivtool - VIV/BIG decoder/encoder
unvivtool extracts and creates VIV/BIG archives. It is available as a
command-line interface, and as a Python module. Developed for Windows and
Linux.

### Features

* numerous format checks
* decodes an entire archive at once or retrieves a single file either by index
  or by filename
* optionally lists archive contents without writing to disk
* low peak memory usage

## Documentation

Command-line interface: [/doc/unvivtool_cli.md](/doc/unvivtool_cli.md)<br/>
Python extension module: [/doc/unvivtool_py.md](/doc/unvivtool_py.md)

## Installation (CLI)

#### Windows

Download the latest release and extract ```unvivtool.exe``` to the directory of
your choice.

To uninstall, delete any extracted files.

#### Linux

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       gcc -std=c89 -s -O2 unvivtool.c -o unvivtool

## Installation (Python)

Requires Python 3.7+

       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool/python
       python setup.py install

## References

D. Auroux et al. [_The unofficial Need For Speed III file format specifications - Version 1.0_](/references/unofficial_nfs3_file_specs_10.txt) [1998]

## Information

__License:__ GNU General Public License v3.0+<br/>
__Website:__ <https://github.com/bfut/unvivtool>