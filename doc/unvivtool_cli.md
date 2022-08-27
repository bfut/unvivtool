# unvivtool - command-line interface
This file describes installation, and usage of unvivtool as command-line
interface.

## Usage
Ready to use scripts can be found in [/scripts](/scripts)

```
EXAMPLE 1
   unvivtool d car.viv car_viv

      decodes and extracts all files from archive 'car.viv' to directory
      'car_viv'. the output directory must be a valid path.

EXAMPLE 2
   unvivtool d car.viv .

      decodes and extracts all files from archive 'car.viv' to current working
      directory

EXAMPLE 3
      unvivtool d -p car.viv car_viv

      -p    prints contents of archive 'car.viv', does not write to disk

EXAMPLE 4
      unvivtool e car.viv car.fce car00.tga carp.txt fedata.fsh fedata.eng

      encodes minimum needed files for an :HP car in a new archive file 'car.viv'

EXAMPLE 5
      unvivtool e -p car.viv car.fce car00.tga carp.txt fedata.fsh fedata.eng

      Dry run of EXAMPLE 4, does not write to disk
```

## Installation
#### Windows
Download the latest release and extract ```unvivtool.exe``` to the directory of
your choice.

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
       cl.exe /utf-8 /O2 unvivtool.c

## Documentation
```
Usage: unvivtool d [<options>...] <path/to/input.viv> <path/to/existing/output_directory>
       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...

Commands:
  d             decode and extract files from VIV/BIG archive
  e             encode files in new VIV/BIG archive

Options:
  -i #          decode file at 1-based index #
  -f <name>     decode file <name> (cAse-sEnsitivE) from archive, overrides -i
  -p            print archive contents, do not write to disk (dry run)
  -s            decoder strict mode, extra format checks, fail at first unsuccessful extraction
  -v            verbose
```