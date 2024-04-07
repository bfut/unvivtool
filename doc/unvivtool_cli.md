# unvivtool - command-line interface
This file describes installation and usage of unvivtool as command-line
interface.

## Installation
#### Windows
Download the latest release and extract ```unvivtool.exe``` to the directory of
your choice.

#### Linux

       cd ~
       git clone https://github.com/bfut/unvivtool
       cd unvivtool
       gcc -std=c89 -fPIE -pie -s -O2 unvivtool.c -o unvivtool

## Compiling for Windows
Requires MSVC.

       git clone https://github.com/bfut/unvivtool
       cd unvivtool
       cl.exe /Za /Tc unvivtool.c

## Usage
Drag-and-drop a VIV/BIG archive onto the executable to decode it.<br>
Drag-and-drop multiple files onto the executable to encode them into a VIV archive.

Alternatively, use the command-line.
```
EXAMPLE 1
   unvivtool d car.viv car_viv

      decodes and extracts all files from archive 'car.viv' to directory
      'car_viv'. the output directory must be an existing directory.

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

## Documentation
```
Usage: unvivtool d [<options>...] <path/to/input.viv> <path/to/existing/output_directory>
       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...
       unvivtool <path/to/input.viv>
       unvivtool <paths/to/input_files>...

Commands:
  d             Decode and extract files from VIV/BIG archive
  e             Encode files in new VIV/BIG archive

Options:
  -dnl #        decode/encode, set fixed Directory eNtry Length (>= 10)
  -i #          decode file at 1-based Index #
  -f <name>     decode file <name> (cAse-sEnsitivE) from archive, overrides -i
  -fh           decode/encode to/from Filenames in Hexadecimal
  -fmt <format> encode 'BIGF' (default), 'BIGH' or 'BIG4'
  -p            print archive contents, do not write to disk (dry run)
  -we           write re-Encode command to path/to/input.viv.txt (keep files in order)
  -v            print archive contents, verbose
```
