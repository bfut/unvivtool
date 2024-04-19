# unvivtool - Python extension module
unvivtool is a VIV/BIG decoder/encoder for uncompressed BIGF, BIGH, and BIG4 archives.

Purported VIV/BIG archives sometimes contain faulty or manipulated header information.
unvivtool is designed to validate and recover data as much as possible.

This file describes installation and usage of unvivtool as Python extension module.

## Usage
A ready to use decoder/encoder script can be found here: [https://github.com/bfut/unvivtool/blob/main/scripts/unvivtool_script.py](https://github.com/bfut/unvivtool/blob/main/scripts/unvivtool_script.py)

## Installation
Requires Python 3.9+
```
python -m pip install unvivtool
```

## Documentation
```
Help on module unvivtool:

NAME
    unvivtool - VIV/BIG decoding/encoding

DESCRIPTION
    Functions
    ---------
    unviv() -- decode and extract VIV/BIG archive
    viv() -- encode files in new VIV/BIG archive

    unvivtool 1.20 Copyright (C) 2020-2024 Benjamin Futasz (GPLv3+)

FUNCTIONS
    unviv(...)
        |  unviv(viv, dir, direnlen=0, fileidx=None, filename=None, fnhex=False, dry=False, verbose=False, overwrite=0)
        |      Decode and extract archive. Accepts BIGF, BIGH, and BIG4.
        |
        |      Parameters
        |      ----------
        |      viv : str, os.PathLike object
        |          Absolute or relative, path/to/archive.viv
        |      dir : str, os.PathLike object
        |          Absolute or relative, path/to/output/directory
        |      direnlen : int, optional
        |          If >= 10, set as fixed archive directory entry length.
        |      fileidx : int, optional
        |          Extract file at given 1-based index.
        |      filename : str, optional
        |          Extract file 'filename' (cAse-sEnsitivE) from archive.
        |          Overrides the fileidx parameter.
        |      fnhex : bool, optional
        |          If True, encode filenames to Base16/hexadecimal.
        |          Use for non-printable filenames in archive. Keeps
        |          leading/embedding null bytes.
        |      dry : bool, optional
        |          If True, perform dry run: run all format checks and print
        |          archive contents, do not write to disk.
        |      verbose : bool, optional
        |          Verbose output.
        |      overwrite : int, optional
        |          If == 0, warns and attempts overwriting existing files. (default)
        |          If == 1, attempts renaming existing files, skips on failure.
        |
        |      Returns
        |      -------
        |      {0, 1}
        |          1 on success.
        |
        |      Raises
        |      ------
        |      FileNotFoundError
        |          When 'viv' cannot be opened.
        |
        |      Examples
        |      --------
        |      Extract all files in "car.viv" in the current working directory
        |      to subdirectory "car_viv".
        |
        |      >>> unvivtool.unviv("car.viv", "car_viv")
        |      ...
        |      1
        |
        |      Before extracting, check the archive contents and whether it
        |      passes format checks.
        |
        |      >>> unvivtool.unviv("car.viv", "car_viv", dry=True)
        |      Begin dry run
        |      ...
        |
        |      Now that archive contents have been printed, extract file at
        |      1-based index 2. Again print archive contents while extracting.
        |
        |      >>> unvivtool.unviv("car.viv", "car_viv", fileidx=2, verbose=True)
        |      ...
        |
        |      Next, extract file "car00.tga" from another archive. File
        |      "bar.viv" sits in subdirectory "foo" of the current working
        |      directory. This time, contents should be extracted to the current
        |      working directory.
        |
        |      >>> unvivtool.unviv("foo/bar.viv", ".", filename="car00.tga")
        |      ...
        |      Strict Format warning (Viv directory filesizes do not match archive size)
        |      ...
        |      Decoder successful.
        |      1
        |
        |      Some archives may have broken headers. When detected, unvivtool
        |      will print warnings. Up to a certain point, such archives can
        |      still be extracted.

    viv(...)
        |  viv(viv, infiles, dry=False, verbose=False, format="BIGF", direnlen=0, fnhex=False)
        |      Encode files to new archive in BIGF, BIGH or BIG4 format.
        |      Skips given input paths that cannot be opened.
        |
        |      Parameters
        |      ----------
        |      viv : str, os.PathLike object
        |          Absolute or relative, path/to/output.viv
        |      infiles : list of str, list of os.PathLike objects
        |          List of absolute or relative, paths/to/input/files.ext
        |      dry : bool
        |          If True, perform dry run: run all format checks and print
        |          archive contents, do not write to disk.
        |      verbose : bool
        |          If True, print archive contents.
        |      format : str, optional
        |          Expects "BIGF", "BIGH" or "BIG4".
        |      direnlen : int, optional
        |          If >= 10, set as fixed archive directory entry length.
        |      fnhex : bool, optional
        |          If True, decode input filenames from Base16/hexadecimal.
        |          Use for non-printable filenames in archive. Keeps
        |          leading/embedding null bytes.
        |
        |      Returns
        |      -------
        |      {0, 1}
        |          1 on success.
        |
        |      Raises
        |      ------
        |      FileNotFoundError
        |          When 'viv' cannot be created.
        |
        |      Examples
        |      --------
        |      Encode all files in the list 'infiles_paths' in a new archive
        |      "out.viv". The archive is to be created in a subdirectory
        |      "foo", relative to the current working directory. Both input
        |      files are in the current parent directory.
        |
        |      >>> viv = "foo/out.viv"
        |      >>> infiles_paths = ["../LICENSE", "../README.md"]
        |      >>> unvivtool.viv(viv, infiles_paths)
        |      ...
        |      Encoder successful.
        |      1
        |
        |      The dry run functionality may be used to test parameters without
        |      writing to disk.
        |
        |      >>> unvivtool.viv(viv, infiles_paths, dry=True)
        |      Begin dry run
        |      ...
        |
        |      Supposing, the dry run has been successful. Encode the listed
        |      files, again printing archive contents.
        |
        |      >>> unvivtool.viv(viv, infiles_paths, verbose=True)
        |      ...

VERSION
    1.20
