# unvivtool - Python extension module
This file describes installation, and usage of unvivtool as Python extension
module.

## Usage
A ready to use decoder/encoder script can be found here: [/scripts/unvivtool_script.py](/scripts/unvivtool_script.py)

## Installation
Requires Python 3.8+

       cd ~
       git clone https://github.com/bfut/unvivtool.git
       python -m pip install --upgrade pip setuptools wheel
       python -m pip install unvivtool/python

## Documentation
```
Help on module unvivtool:

NAME
    unvivtool - VIV/BIG decoding/encoding

DESCRIPTION
    Functions
    ---------
    viv() -- encode files in new VIV/BIG archive
    unviv() -- decode and extract VIV/BIG archive
    
    unvivtool 1.11 Copyright (C) 2020-2022 Benjamin Futasz (GPLv3+)

FUNCTIONS
    unviv(...)
        |  unviv(viv, dir, fileidx=None, filename=None, dry=False, verbose=False, strict=False)
        |      Decode and extract files from VIV/BIG archive.
        |
        |      Parameters
        |      ----------
        |      viv : str, os.PathLike object
        |          Absolute or relative, path/to/archive.viv
        |      dir : str, os.PathLike object
        |          Absolute or relative, path/to/existing/output/directory
        |      fileidx : int, optional
        |          Extract file at given 1-based index.
        |      filename : str, optional
        |          Extract file 'filename' (cAse-sEnsitivE) from archive.
        |          Overrides 'fileidx'.
        |      dry : bool
        |          If True, perform dry run: run all format checks and print
        |          archive contents, do not write to disk.
        |      verbose : bool
        |          If True, print archive contents.
        |      strict : bool
        |          If True, run extra format checks and fail on the first
        |          unsuccessful file extraction.
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
        |      to existing subdirectory "car_viv".
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
        |      will print warnings. Up to a certain point, such archives may
        |      still be extracted. Warnings can be turned into errors, forcing
        |      stricter adherence to format specifications. Note, such 'errors'
        |      do not raise Python errors. Instead, unviv() returns 0.
        |
        |      >>> unvivtool.unviv("foo/bar.viv", ".", filename="car00.tga", strict=True)
        |      ...
        |      Strict Format error (Viv directory filesizes do not match archive size)
        |      Decoder failed.
        |      0
    
    viv(...)
        |  viv(viv, infiles, dry=False, verbose=False)
        |      Encode files in new VIV/BIG archive. Skips given input paths
        |      that cannot be opened.
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
    1.11
```