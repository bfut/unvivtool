# unvivtool - command-line interface
This file describes installation and usage of unvivtool as command-line interface.

## Installation
### Windows
Download the latest release and extract ```unvivtool.exe``` to the directory of your choice.

### Linux
```
cd ~
git clone https://github.com/bfut/unvivtool
cd unvivtool
gcc -std=c89 -D_GNU_SOURCE -fPIE -pie -O2 unvivtool.c -o unvivtool
```
### Compiling for Windows
Requires MSVC.
```
git clone https://github.com/bfut/unvivtool
cd unvivtool
cl.exe /Ze /TC unvivtool.c
```
## Usage
Drag-and-drop a VIV/BIG archive onto the executable to decode it.<br>
Drag-and-drop multiple files onto the executable to encode them into a VIV archive.

Alternatively, use the command-line.
```
EXAMPLE 1
   unvivtool d car.viv car_viv

      decodes and extracts all files from archive 'car.viv' to directory
      'car_viv'. If 'car_viv' does not exist, it will be created.

EXAMPLE 2
   unvivtool d car.viv

      decodes and extracts all files from archive 'car.viv' to its parent
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

EXAMPLE 6
      unvivtool d -dnl80 -x -we archive.viv archive_viv

      Real-world example of an archive where the archive header has a fixed
      directory length of 80 bytes (-dnl80). The supposed filenames are
      non-printable characters which are represented in  hexadecimal (base16)
      on extraction to disk (-x). The archive contains a large number of files
      that are expected in a specific order; the complete re-encoding command
      is written 'archive.viv.txt' (-we).
```

## Documentation
```
Usage: unvivtool d [<options>...] <path/to/input.viv> [<path/to/output_directory>]
       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...
       unvivtool <path/to/input.viv>
       unvivtool <paths/to/input_files>...

Commands:
  d            Decode and extract files from VIV/BIG archive
  e            Encode files in new VIV/BIG archive

Options:
  -aot         decoder Overwrite mode: auto rename existing file
  -dnl<N>      decode/encode, set fixed Directory eNtry Length (<N> >= 10)
  -i<N>        decode file at 1-based Index <N>
  -f<name>     decode File <name> (cAse-sEnsitivE) from archive, overrides -i
  -x           decode/encode to/from Filenames in base16/Hexadecimal
  -fmt<format> encode to Format 'BIGF' (default), 'BIGH' or 'BIG4' (w/o quotes)
  -p           Print archive contents, do not write to disk (dry run)
  -we          Write re-Encode command to path/to/input.viv.txt (keep files in order)
  -v           print archive contents, Verbose
```
