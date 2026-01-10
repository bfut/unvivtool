# unvivtool - command-line interface
unvivtool is a VIV/BIG decoder/encoder for BIGF, BIGH, and BIG4 archives.

Purported VIV/BIG archives can contain faulty or manipulated information.
unvivtool is designed to validate and recover data wherever possible.

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
Requires MSVC 6.0 or later.
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

      dry run of EXAMPLE 4, does not write to disk

EXAMPLE 6
      unvivtool d -dnl80 -x -we archive.viv archive_viv

      Decodes archive with fixed 80-byte directory entry length header (-dnl80).
      Its filenames contain non-printable characters that are extracted to
      hexadecimal form (base16) (-x). Because the archive contains a large
      number of files, the complete re‑encoding command is written to
      'archive.viv.txt' (-we) for convenience.

EXAMPLE 7
      unvivtool e -alf4 car.viv car.fce car00.tga carp.txt fedata.fsh fedata.eng

      -alf<N>    align file offsets to given power-of-two boundary <N>
      a typical value is 4

EXAMPLE 8
      unvivtool r -alf4 car.viv path/to/file.ext

      Replaces 'file.ext' (names must match) in existing archive, keeping all
      other files intact. With '-alf4', file offsets are aligned to 4-byte
      boundaries.  Without '-alf4', the alignment used in the original archive
      is kept based on a guess from the original file offsets.
```

## Documentation
```
Usage: unvivtool d [<options>...] <path/to/input.viv> [<path/to/output_directory>]
       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...
       unvivtool r [<options>...] <path/to/archive.viv> <path/to/replacement_file>
       unvivtool <path/to/input.viv>
       unvivtool <paths/to/input_files>...

Commands:
  d            Decode and extract files from VIV/BIG archive
  e            Encode files in new VIV/BIG archive
  r            Replace existing file in VIV/BIG archive

Options:
  -aot         decoder Overwrite mode: auto rename existing file
  -dnl<N>      decode/encode, set fixed Directory eNtry Length (<N> >= 10)
  -i<N>        decode file at 1-based Index <N>
  -f<name>     decode File <name> (cAse-sEnsitivE) from archive, overrides -i
  -x           decode/encode to/from filenames in base16/heXadecimal
  -alf<N>      encode/replace, ALign File offsets to <N> (allows 0, 2, 4, 8, 16)
  -fmt<format> encode to Format 'BIGF' (default), 'BIGH', 'BIG4', 'C0FB' or 'wwww' (w/o quotes)
  -p           dry run, Print archive contents (do not write to disk)
  -we          Write re-Encode command to path/to/input.viv.txt (keep files in order)
  -v           Verbose, print archive contents
```
