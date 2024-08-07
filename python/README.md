# unvivtool - Python extension module
unvivtool is a VIV/BIG decoder/encoder for uncompressed BIGF, BIGH, and BIG4 archives.

Purported VIV/BIG archives can contain faulty or manipulated header information.
unvivtool is designed to validate and recover data as much as possible.

## Usage
A ready to use decoder/encoder script can be found here: [https://github.com/bfut/unvivtool/blob/main/scripts/unvivtool_script.py](https://github.com/bfut/unvivtool/blob/main/scripts/unvivtool_script.py)

## Installation
Requires Python 3.10+
```
python -m pip install unvivtool
```

## Documentation
```
Help on module unvivtool:

NAME
    unvivtool - simple BIGF BIGH BIG4 decoder/encoder (commonly known as VIV/BIG)

DESCRIPTION
    Functions
    ---------
    get_info() -- get archive header and filenames
    unviv() -- decode and extract archive
    update() -- replace file in archive
    viv() -- encode files in new archive

    unvivtool 3.0 Copyright (C) 2020-2024 Benjamin Futasz (GPLv3+)

FUNCTIONS
    get_info(...)
        |  get_info(path, verbose=False, direnlen=0, fnhex=False, invalid=False)
        |      Return dictionary of archive header info and list of filenames.
        |
        |      Parameters
        |      ----------
        |      path : str, os.PathLike object
        |          Absolute or relative, path/to/archive.viv
        |      verbose : bool, optional
        |          Verbose output.
        |      direnlen : int, optional
        |          If >= 10, set as fixed archive directory entry length.
        |      fnhex : bool, optional
        |          If True, interpret filenames as Base16/hexadecimal.
        |          Use for non-printable filenames in archive. Keeps
        |          leading/embedding null bytes.
        |      invalid : bool, optional
        |          If True, export all directory entries, even if invalid.
        |
        |      Returns
        |      -------
        |      header : dictionary
        |          The only guaranteed entry is "format" with a string or None.
        |          Filenames list will be empty if the directory has zero (valid) entries.
        |
        |      Raises
        |      ------
        |      FileNotFoundError
        |      MemoryError
        |      Exception

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
        |          If True, interpret filenames as Base16/hexadecimal.
        |          Use for non-printable filenames in archive. Keeps
        |          leading/embedded null bytes.
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
        |      MemoryError
        |      TypeError

    update(...)
        |  update(inpath, infile, entry, outpath=None, insert=0, replace_filename=False, dry=False, verbose=False, direnlen=0, fnhex=False, faithful=False)
        |      Replace file in archive.
        |
        |      Parameters
        |      ----------
        |      inpath : str, os.PathLike object
        |          Absolute or relative, path/to/archive.viv
        |      infile : str, os.PathLike object
        |          Absolute or relative, path/to/file.ext
        |      entry : str, int
        |          Name of target entry or 1-based index of target entry.
        |      outpath : str, os.PathLike object, optional
        |          Absolute or relative, path/to/output_archive.viv
        |          If empty, overwrite vivpath.
        |      insert : int, optional
        |          If  > 0, set as fixed archive directory entry length.
        |          If == 0, set as fixed archive directory entry length.
        |          If  < 0, set as fixed archive directory entry length.
        |      replace_filename : bool, optional
        |          If True, and infile is a path/to/file.ext, the entry filename will be changed to file.ext
        |      dry : bool, optional
        |          If True, perform dry run: run all format checks and print
        |          archive contents, do not write to disk.
        |      verbose : bool, optional
        |          Verbose output.
        |      direnlen : int, optional
        |          If >= 10, set as fixed archive directory entry length.
        |      fnhex : bool, optional
        |          If True, interpret filenames as Base16/hexadecimal.
        |          Use for non-printable filenames in archive. Keeps
        |          leading/embedded null bytes.
        |      faithful : bool, optional
        |          If False, ignore invalid entries (default behavior).
        |          If True, replace any directory entries, even if invalid.
        |
        |      Returns
        |      -------
        |      {0, 1}
        |          1 on success.
        |
        |      Raises
        |      ------
        |      FileNotFoundError
        |      MemoryError
        |      TypeError
        |      Exception

    viv(...)
        |  viv(viv, infiles, dry=False, verbose=False, format="BIGF", endian=0xE, direnlen=0, fnhex=False, faithful=False)
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
        |      endian : int, char, optional
        |          Defaults to 0xE for BIGF and BIGH, and 0xC for BIG4.
        |          Only use for rare occurences where BIGF has to be 0xC.
        |      direnlen : int, optional
        |          If >= 10, set as fixed archive directory entry length.
        |      fnhex : bool, optional
        |          If True, decode input filenames from Base16/hexadecimal.
        |          Use for non-printable filenames in archive. Keeps
        |          leading/embedded null bytes.
        |      faithful : bool, optional
        |
        |      Returns
        |      -------
        |      {0, 1}
        |          1 on success.
        |
        |      Raises
        |      ------
        |      FileNotFoundError
        |      MemoryError
        |      TypeError
        |      ValueError

VERSION
    3.0
