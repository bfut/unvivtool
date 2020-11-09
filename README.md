# VIV/BIG decoder/encoder
unvivtool - command-line tool for Windows, and Linux  
libnfsviv.h - implements viv/big decoding/encoding

Compiles in C89, and C++.

## Installation

#### Linux:

Run the following from a terminal app:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       gcc -std=c89 -s -O2 unvivtool.c -o unvivtool

#### How to build for Windows

With MinGW installed on your system, run the following commands to cross-compile a win32 executable on Linux:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       i686-w64-mingw32-gcc -std=c89 -s -O2 unvivtool.c -o unvivtool.exe


## Usage

```
Usage: unvivtool e [<options>...] <output.viv> [<input_files>...]
       unvivtool d [<options>...] <input.viv> [<output_directory>]
       unvivtool p <input.viv>

Commands:
  e        encode files in new archive
  d        decode archive, extract to directory
  p        print archive contents

Options:
  -o       overwrite existing output directory or file
  -v       verbose
```

## Examples

```
EXAMPLE 1
      unvivtool.exe d CAR.VIV
      
            decodes and extracts all files from archive "CAR.VIV" to folder "CAR_VIV".
            when folder "CAR_VIV" already exists, increments foldername to "CAR_VIV_1"
```
```
EXAMPLE 2
      unvivtool.exe d -v -o CAR.VIV FOO
      
            decodes and extracts all files from archive "CAR.VIV" to folder "FOO"
            
      -v    prints a message for each step
      
      -o    overwrites files in folder "FOO", if it exists
```
```
EXAMPLE 3
      unvivtool.exe p CAR.VIV
      
            prints contents of archive "CAR.VIV", does not write disk
```
```
EXAMPLE 4
      unvivtool.exe e CAR.VIV car.fce car00.tga carp.txt fedata.fsh fedata.eng

            encodes minimum needed files for NFS3 car in a new archive file "CAR.VIV"
```
