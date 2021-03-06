# unvivtool - command-line interface

This file describes installation, and usage of unvivtool as command-line
interface. Examples are included. Batch scripts are linked to.

## Installation

#### Windows

Download the latest release and extract ```unvivtool.exe``` to the directory of
your choice.

To uninstall, delete any extracted files.

#### Linux

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       gcc -std=c89 -fPIE -s -O2 unvivtool.c -o unvivtool

## Compiling for Windows

Releases for Windows are cross-compiled on Linux with MinGW:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       i686-w64-mingw32-gcc -std=c89 -fPIE -s -O2 -Xlinker --no-insert-timestamp unvivtool.c -o unvivtool.exe

Compiling with MSVC:

       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       cl.exe /utf-8 unvivtool.c

## Usage

```
Usage: unvivtool d [<options>...] <path/to/input.viv> <path/to/output_directory>
       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...

Commands:
  d             decode and extract files from VIV/BIG archive
  e             encode files in new VIV/BIG archive

Options:
  -i #          decode file at 1-based index #
  -f <name>     decode file <name> (cAse-sEnsitivE) from archive, overrides -i
  -p            print archive contents, do not write to disk (dry run)
  -s            decoder strict mode, extra format checks, fail at first unsuccesful extraction
  -v            verbose
```

## Examples

```
EXAMPLE 1
   unvivtool d car.viv

      decodes and extracts all files from archive 'car.viv' to folder
      CAR_VIV. when folder CAR_VIV already exists, increments
      foldername to CAR_VIV_1

EXAMPLE 2
      unvivtool d -p car.viv

      -p    prints contents of archive 'car.viv', does not write to disk

EXAMPLE 3
      unvivtool e car.viv car.fce car00.tga carp.txt fedata.fsh fedata.eng

      encodes minimum needed files for an :HP car in a new archive file 'car.viv'
```

More examples can be found in the [/batch](/batch) folder.

## File Listing

```
doc/unvivtool_cli.md - this file
libnfsviv.h - implements VIV/BIG decoding/encoding
unvivtool.c - VIV/BIG decoder/encoder command-line interface
README.md
```