# unvivtool - portable VIV/BIG decoder/encoder
unvivtool - command-line tool for Windows, and Linux (GPLv3)  
libnfsviv.h - implements VIV/BIG decoding/encoding (zlib License)

A portable, open-source approach with permissive license.

Features:

 * decodes/encodes dozens of archives in a tenth of a second via batch script
 * dry run option lists archive's contents
 * decodes an entire archive at once or removes a single file either by index or
   by filename
 * surgically decodes a single file, from archive with broken header, by offset
   and filesize
   
## Installation

#### Windows:

Download the latest release and extract ```unvivtool.exe``` to the directory of
your choice.

```
sha256sum:  <hash>  unvivtool
md5sum:     <hash>  unvivtool
```

#### Linux:

Run the following commands from a terminal app:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       gcc -std=c89 -s -O2 unvivtool.c -o unvivtool

#### Build for Windows

The released win32 executables are cross-compiled on Linux with MinGW by running
the following commands:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       i686-w64-mingw32-gcc -std=c89 -fstack-clash-protection -s -O2 unvivtool.c -o unvivtool.exe

## Usage

```
Usage: unvivtool e [<options>...] <output.viv> [<input_files>...]
       unvivtool d [<options>...] <input.viv> [<output_directory>]

Commands:
  e             encode files in new archive
  d             decode and extract archive

Options:
  -o            overwrite existing
  -p            print archive contents, do not write to disk
  -v            verbose
  -strict       extra format checks
  -fn <name>    decode file <name> (cAse-sEnsitivE) from archive (overrides -id)
  -id #         decode file at index (1-based)
  -fs #         decode single file with filesize (requires either -fn or -id)
  -fofs #       decode file at offset (requires either -fn or -id)
```
Batch scripts are provided in the ```batch``` folder. Recommended for encoding.

## Examples

```
EXAMPLE 1
      unvivtool d CAR.VIV

            decodes and extracts all files from archive CAR.VIV to folder
            CAR_VIV. when folder CAR_VIV already exists, increments
            foldername to CAR_VIV_1
```
```
EXAMPLE 2
      unvivtool d -p CAR.VIV

      -p    prints contents of archive CAR.VIV, does not write to disk
```
```
EXAMPLE 3
      unvivtool d -v -o -fn dash.qfs -fofs 744666 -fs 367738 car.viv dir1

            decodes and extracts file dash.qfs from archive car.viv to folder
            dir1. this example recovers dash.qfs from the partially broken
            official DLC NFS3 car /walm/car.viv

      -v    verbose

      -o    overwrites files in folder dir1, if they exist. dir1 is created
            relative to current working directory.

      -fn   only extract dash.qfs

      -fofs extract file from offset 744666 (overrides VIV directory entry)

      -fs   extract 367738 bytes (overrides VIV directory entry)
```
```
EXAMPLE 4
      unvivtool e CAR.VIV car.fce car00.tga carp.txt fedata.fsh fedata.eng

            encodes minimum needed files for an NFS3 car in a new archive file
            CAR.VIV. for bash terminals, find/xargs is recommended instead.
```

## References

McMullen et al. [1998]: [_The unofficial Need For Speed III file format specifications - Version 1.0_](/stuff/unofficial_nfs3_file_specs_10%2Bbf1.txt)

## Website

<https://github.com/bfut/unvivtool>

## Note

This README.md may not be removed or altered from any unvivtool redistribution.
