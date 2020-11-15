# VIV/BIG decoder/encoder
unvivtool - command-line tool for Windows, and Linux (GPLv3)  
libnfsviv.h - implements viv/big decoding/encoding (MIT License)

Should compile without warnings in C89, and C++.

## Installation

#### Linux:

Run the following commands from a terminal app:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       gcc -std=c89 -s -O2 unvivtool.c -o unvivtool

#### How to build for Windows

With MinGW installed on your system, run the following commands to cross-compile a win32 executable on Linux:

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       cd unvivtool
       i686-w64-mingw32-gcc -std=c89 -fstack-clash-protection -s -O2 unvivtool.c -o unvivtool.exe


## Usage

```
Usage: unvivtool e [<options>...] <output.viv> [<input_files>...]
       unvivtool d [<options>...] <input.viv> [<output_directory>]

Commands:
  e        encode files in new archive
  d        decode and extract archive

Options:
  -o       overwrite existing
  -p       print archive contents, do not write to disk
  -v       verbose
  -strict  extra format checks
```
A batch script can be found in ```./stuff```

## Examples

```
EXAMPLE 1
      unvivtool d CAR.VIV
      
            decodes and extracts all files from archive "CAR.VIV" to folder 
            "CAR_VIV". when folder "CAR_VIV" already exists, increments 
            foldername to "CAR_VIV_1"
```
```
EXAMPLE 2
      unvivtool d -p CAR.VIV
      
      -p    prints contents of archive "CAR.VIV", does not write to disk
```
```
EXAMPLE 3
      unvivtool d -v -o CAR.VIV FOO
      
            decodes and extracts all files from archive "CAR.VIV" to folder 
            "FOO"
            
      -v    verbose
      
      -o    overwrites files in folder "FOO", if they exist. "FOO" is relative
            to current working directory.
```
```
EXAMPLE 4
      unvivtool e CAR.VIV car.fce car00.tga carp.txt fedata.fsh fedata.eng

            encodes minimum needed files for an NFS3 car in a new archive file 
            "CAR.VIV"
```
