/*
  unvivtoolmodule.c - VIV/BIG decoder/encoder Python module
  unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>

  You may not redistribute this program without its source code.
  README.md may not be removed or altered from any unvivtool redistribution.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
/* #define chdir _chdir */  /* defined in libnfsviv.h */
#define getcwd _getcwd
#else
#include <unistd.h>  /* chdir, getcwd */
#endif

#include <fcntl.h>  /* open() */

#ifdef _WIN32
#include <io.h>
#define close _close
#define open _open
#define O_RDONLdY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_CREAT _O_CREAT
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#endif

#include <Python.h>

/* https://github.com/pybind/python_example/blob/master/src/main.cpp */
#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

#ifdef PYMEM_MALLOC
#define malloc PyMem_Malloc
#define realloc PyMem_Realloc
#define free PyMem_Free
#endif

#define SCL_PY_PRINTF  /* native printf */
#include "../include/SCL/sclpython.h"

#include "../libnfsviv.h"
#define kUnvivtoolMaxPathLen kLibnfsvivFilenameMaxLen

/* wrappers ----------------------------------------------------------------- */

static
PyObject *unviv(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv;
  PyObject *retv_obj = NULL;
  char *viv_name = NULL;
  PyObject *viv_name_obj = NULL;
  char *outpath = NULL;
  PyObject *outpath_obj = NULL;
  int request_file_idx = 0;
  char *request_file_name = NULL;
  PyObject *request_file_name_obj = NULL;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  int opt_dryrun = 0;
  int opt_verbose = 0;
  int opt_overwrite = 0;
  int fd;
  char *buf_cwd = NULL;
  static const char *keywords[] = { "viv", "dir",
                                    "fileidx", "filename",
                                    "dry", "verbose", "direnlen", "fnhex",
                                    "overwrite", NULL };

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O&|$iO&ppipi:unviv",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj, PyUnicode_FSConverter, &outpath_obj,
                                   &request_file_idx, PyUnicode_FSConverter, &request_file_name_obj,
                                   &opt_dryrun, &opt_verbose, &opt_direnlenfixed, &opt_filenameshex,
                                   &opt_overwrite))
  {
    return NULL;
  }

  {
    const char *viv_name_ = PyBytes_AsString(viv_name_obj);
    if (!viv_name_)
    {
      PyErr_SetString(PyExc_TypeError, "cannot convert str");
      return NULL;
    }
    viv_name = /* (char *) */malloc(LIBNFSVIV_max(strlen(viv_name_) + 1, kUnvivtoolMaxPathLen) * sizeof(*viv_name));
    if (!viv_name)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot allocate memory");
      return NULL;
    }
    memcpy(viv_name, viv_name_, strlen(viv_name_) + 1);
    Py_DECREF(viv_name_obj);
  }

  for (;;)
  {
    {
      const char *outpath_ = PyBytes_AsString(outpath_obj);
      if (!outpath_)
      {
        PyErr_SetString(PyExc_TypeError, "cannot convert str");
        break;
      }
      outpath = /* (char *) */malloc(LIBNFSVIV_max(strlen(outpath_) + 1, kUnvivtoolMaxPathLen) * sizeof(*outpath));
      if (!outpath)
      {
        PyErr_SetString(PyExc_FileNotFoundError, "Cannot allocate memory");
        break;
      }
      memcpy(outpath, outpath_, strlen(outpath_) + 1);
      Py_XDECREF(outpath_obj);
    }

    if (request_file_name_obj)
    {
      request_file_name = PyBytes_AsString(request_file_name_obj);
      if (!request_file_name)
      {
        PyErr_SetString(PyExc_TypeError, "cannot convert str");
        break;
      }
    }

    fd = open(viv_name, O_RDONLY);
    if (fd == -1)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "cannot open viv: no such file or directory");
      break;
    }
    close(fd);

    buf_cwd = /* (char *) */malloc(kUnvivtoolMaxPathLen + 64);
    if (!buf_cwd)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot allocate memory");
      break;
    }
    if (!getcwd(buf_cwd, kUnvivtoolMaxPathLen + 64))
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot get current working directory");
      break;
    }

    opt_direnlenfixed = LIBNFSVIV_max(opt_direnlenfixed, 0);
    if (opt_direnlenfixed > 0)
    {
      opt_direnlenfixed = LIBNFSVIV_max(opt_direnlenfixed, 0);
      opt_direnlenfixed = LIBNFSVIV_min(opt_direnlenfixed, INT_MAX / 2);
      PySys_WriteStdout("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)", opt_direnlenfixed, opt_direnlenfixed, INT_MAX / 2);
    }

    if (opt_dryrun)
      opt_verbose = 1;

    retv = LIBNFSVIV_Unviv(viv_name, outpath,
                           request_file_idx, request_file_name,
                           opt_dryrun, opt_verbose, opt_direnlenfixed,
                           opt_filenameshex, 0, opt_overwrite);

    if (chdir(buf_cwd) != 0)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot restore working directory");
      break;
    }

    if (retv == 1)
      PySys_WriteStdout("Decoder successful.");
    else
      PySys_WriteStdout("Decoder failed.");

    retv_obj = Py_BuildValue("i", retv);
    break;
  }  /* for (;;) */

  if (buf_cwd)
    free(buf_cwd);
  if (outpath)
    free(outpath);
  if (viv_name)
    free(viv_name);
  Py_XDECREF(request_file_name_obj);
  // Py_XDECREF(outpath_obj);  // see above
  // Py_DECREF(viv_name_obj);  // see above

  return retv_obj;
}

static
PyObject *viv(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv = 1;
  PyObject *retv_obj = NULL;
  char *viv_name = NULL;
  PyObject *viv_name_obj;
  char **infiles_paths = NULL;
  PyObject *infiles_paths_obj;
  char opt_requestfmt[5] = "BIGF";
  // PyObject *opt_requestfmt_obj = NULL;
  char *opt_requestfmt_ptr = NULL;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  int opt_dryrun = 0;
  int opt_verbose = 0;
  int count_infiles = 1;
  int i;
  int length_str = 0;
  int ofs = 0;
  int fd;
  PyObject *item = NULL;
  PyObject *bytes = NULL;
  char *ptr = NULL;
  static const char *keywords[] = { "viv", "infiles", "dry", "verbose",
                                    "format", "direnlen", "fnhex", NULL };

  // if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O|piO&ip:viv",
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O|$pisip:viv",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj, &infiles_paths_obj,
                                   &opt_dryrun, &opt_verbose,
                                  //  PyUnicode_FSConverter, &opt_requestfmt_obj,
                                   &opt_requestfmt_ptr, &opt_direnlenfixed, &opt_filenameshex))
  {
    return NULL;
  }

  {
    const char *viv_name_ = PyBytes_AsString(viv_name_obj);
    if (!viv_name_)
    {
      PyErr_SetString(PyExc_TypeError, "cannot convert str");
      return NULL;
    }
    viv_name = /* (char *) */malloc(LIBNFSVIV_max(strlen(viv_name_) + 1, kUnvivtoolMaxPathLen) * sizeof(*viv_name));
    if (!viv_name)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot allocate memory");
      return NULL;
    }
    memcpy(viv_name, viv_name_, strlen(viv_name_) + 1);
    Py_DECREF(viv_name_obj);
  }

  for (;;)
  {
    if (opt_requestfmt_ptr)
    {
      memcpy(opt_requestfmt, opt_requestfmt_ptr, LIBNFSVIV_min(4, strlen(opt_requestfmt_ptr)) + 1);
      if (strncmp(opt_requestfmt, "BIGF", 5) &&
          strncmp(opt_requestfmt, "BIGH", 5) &&
          strncmp(opt_requestfmt, "BIG4", 5))
      {
        PyErr_SetString(PyExc_ValueError, "expects format parameter 'BIGF', 'BIGH' or 'BIG4'");
        break;
      }
      PySys_WriteStdout("Requested format: %.4s", opt_requestfmt);
    }

    retv_obj = Py_BuildValue("i", retv);

    count_infiles = (int)PyList_Size(infiles_paths_obj);
    if (count_infiles < 0)
    {
      PyErr_SetString(PyExc_TypeError, "expected list");
      retv_obj = NULL;
      break;
    }

    for (i = 0; i < count_infiles; ++i)
    {
      item = PyList_GetItem(infiles_paths_obj, i);
      if (!item)
      {
        PyErr_SetString(PyExc_MemoryError, "cannot get item");
        retv_obj = NULL;
        break;
      }
      if (!PyUnicode_CheckExact(item))
      {
        PyErr_SetString(PyExc_TypeError, "expected list of str");
        retv_obj = NULL;
        break;
      }
      Py_INCREF(item);

      bytes = PyUnicode_AsEncodedString(item, "utf-8", "strict");
      if (!bytes)
      {
        Py_DECREF(item);
        PyErr_SetString(PyExc_MemoryError, "cannot get item as unicode string");
        retv_obj = NULL;
        break;
      }

      ptr = PyBytes_AsString(bytes);
      if (!ptr)
      {
        Py_DECREF(bytes);
        Py_DECREF(item);
        PyErr_SetString(PyExc_MemoryError, "cannot get item as string");
        retv_obj = NULL;
        break;
      }

      length_str += (int)strlen(ptr) + 1;

      ptr = NULL;
      Py_DECREF(bytes);
      bytes = NULL;
      Py_DECREF(item);
      item = NULL;
    }  /* for i */

    if (!retv_obj)
      break;  /* for (;;) */

    infiles_paths = /* (char **) */malloc((count_infiles + 1) * sizeof(*infiles_paths));
    if (!infiles_paths)
    {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate memory");
      retv_obj = NULL;
      break;
    }

    ptr = /* (char *) */malloc(length_str * sizeof(**infiles_paths));
    if (!ptr)
    {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate memory");
      retv_obj = NULL;
      break;
    }
    *infiles_paths = ptr;
    ptr = NULL;

    for (i = 0; i < count_infiles; ++i)
    {
      item = PyList_GetItem(infiles_paths_obj, i);
      if (!item)
      {
        PyErr_SetString(PyExc_MemoryError, "cannot get item");
        retv_obj = NULL;
        break;
      }
      Py_INCREF(item);

      bytes = PyUnicode_AsEncodedString(item, "utf-8", "strict");
      if (!bytes)
      {
        Py_DECREF(item);
        PyErr_SetString(PyExc_MemoryError, "cannot get item as unicode string");
        retv_obj = NULL;
        break;
      }

      ptr = PyBytes_AsString(bytes);
      if (!ptr)
      {
        Py_DECREF(bytes);
        Py_DECREF(item);
        PyErr_SetString(PyExc_MemoryError, "cannot get item as string");
        retv_obj = NULL;
        break;
      }

      length_str = (int)strlen(ptr) + 1;

      memcpy(*infiles_paths + ofs, ptr, length_str);
      infiles_paths[i] = *infiles_paths + ofs;

      ofs += length_str;

      ptr = NULL;
      Py_DECREF(bytes);
      bytes = NULL;
      Py_DECREF(item);
      item = NULL;
    }  /* for i */

    break;
  }  /* for (;;) */

  opt_direnlenfixed = LIBNFSVIV_max(opt_direnlenfixed, 0);
  if (opt_direnlenfixed > 0)
  {
    opt_direnlenfixed = LIBNFSVIV_max(opt_direnlenfixed, 10);
    opt_direnlenfixed = LIBNFSVIV_min(opt_direnlenfixed, INT_MAX / 2);
    PySys_WriteStdout("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)", opt_direnlenfixed, opt_direnlenfixed, INT_MAX / 2);
  }

  if (retv_obj)
  {
    for (;;)
    {
      if (!opt_dryrun)
      {
        fd = open(viv_name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
          PyErr_SetString(PyExc_FileNotFoundError, "cannot create archive - no such file or directory");
          retv_obj = NULL;
          break;
        }
        close(fd);
      }

      retv = LIBNFSVIV_Viv(viv_name, infiles_paths, count_infiles,
                           opt_dryrun, opt_verbose, opt_direnlenfixed,
                           opt_filenameshex, opt_requestfmt);

      if (retv == 1)
        PySys_WriteStdout("Encoder successful.");
      else
        PySys_WriteStdout("Encoder failed.");

      retv_obj = Py_BuildValue("i", retv);
      break;
    }  /* for (;;) */
  }  /* if (retv_obj) */

  if (infiles_paths)
  {
    if (*infiles_paths)
      free(*infiles_paths);
  }
  if (infiles_paths)
    free(infiles_paths);

  if (viv_name)
    free(viv_name);
  // Py_DECREF(viv_name_obj);  // see above

  return retv_obj;
}

/* doc ---------------------------------------------------------------------- */

PyDoc_STRVAR(
  m_doc,
  "VIV/BIG decoding/encoding\n"
  "\n"
  "Functions\n"
  "---------\n"
  "unviv() -- decode and extract VIV/BIG archive\n"
  "viv() -- encode files in new VIV/BIG archive\n"
  "\n"
  "unvivtool "UVTVERS" "UVTCOPYRIGHT"\n"
);

PyDoc_STRVAR(
  unviv__doc__,
  " |  unviv(viv, dir, direnlen=0, fileidx=None, filename=None, fnhex=False, dry=False, verbose=False, overwrite=0)\n"
  " |      Decode and extract archive. Accepts BIGF, BIGH, and BIG4.\n"
  " |\n"
  " |      Parameters\n"
  " |      ----------\n"
  " |      viv : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/archive.viv\n"
  " |      dir : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/existing/output/directory\n"
  " |      direnlen : int, optional\n"
  " |          If >= 10, set as fixed archive directory entry length.\n"
  " |      fileidx : int, optional\n"
  " |          Extract file at given 1-based index.\n"
  " |      filename : str, optional\n"
  " |          Extract file 'filename' (cAse-sEnsitivE) from archive.\n"
  " |          Overrides the fileidx parameter.\n"
  " |      fnhex : bool, optional\n"
  " |          If True, encode filenames to Base16/hexadecimal.\n"
  " |          Use for non-printable filenames in archive. Keeps\n"
  " |          leading/embedding null bytes.\n"
  " |      dry : bool, optional\n"
  " |          If True, perform dry run: run all format checks and print\n"
  " |          archive contents, do not write to disk.\n"
  " |      verbose : bool, optional\n"
  " |          Verbose output.\n"
  " |      overwrite : int, optional\n"
  " |          If == 0, warns and attempts overwriting existing files. (default)\n"
  " |          If == 1, attempts renaming existing files, skips on failure.\n"
  " |\n"
  " |      Returns\n"
  " |      -------\n"
  " |      {0, 1}\n"
  " |          1 on success.\n"
  " |\n"
  " |      Raises\n"
  " |      ------\n"
  " |      FileNotFoundError\n"
  " |          When 'viv' cannot be opened.\n"
  " |\n"
  " |      Examples\n"
  " |      --------\n"
  " |      Extract all files in \"car.viv\" in the current working directory\n"
  " |      to existing subdirectory \"car_viv\".\n"
  " |\n"
  " |      >>> unvivtool.unviv(\"car.viv\", \"car_viv\")\n"
  " |      ...\n"
  " |      1\n"
  " |\n"
  " |      Before extracting, check the archive contents and whether it\n"
  " |      passes format checks.\n"
  " |\n"
  " |      >>> unvivtool.unviv(\"car.viv\", \"car_viv\", dry=True)\n"
  " |      Begin dry run\n"
  " |      ...\n"
  " |\n"
  " |      Now that archive contents have been printed, extract file at\n"
  " |      1-based index 2. Again print archive contents while extracting.\n"
  " |\n"
  " |      >>> unvivtool.unviv(\"car.viv\", \"car_viv\", fileidx=2, verbose=True)\n"
  " |      ...\n"
  " |\n"
  " |      Next, extract file \"car00.tga\" from another archive. File\n"
  " |      \"bar.viv\" sits in subdirectory \"foo\" of the current working\n"
  " |      directory. This time, contents should be extracted to the current\n"
  " |      working directory.\n"
  " |\n"
  " |      >>> unvivtool.unviv(\"foo/bar.viv\", \".\", filename=\"car00.tga\")\n"
  " |      ...\n"
  " |      Strict Format warning (Viv directory filesizes do not match archive size)\n"
  " |      ...\n"
  " |      Decoder successful.\n"
  " |      1\n"
  " |\n"
  " |      Some archives may have broken headers. When detected, unvivtool\n"
  " |      will print warnings. Up to a certain point, such archives can\n"
  " |      still be extracted.\n"
);

PyDoc_STRVAR(
  viv__doc__,
  " |  viv(viv, infiles, dry=False, verbose=False, format=\"BIGF\", direnlen=0, fnhex=False)\n"
  " |      Encode files to new archive in BIGF, BIGH or BIG4 format.\n"
  " |      Skips given input paths that cannot be opened.\n"
  " |\n"
  " |      Parameters\n"
  " |      ----------\n"
  " |      viv : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/output.viv\n"
  " |      infiles : list of str, list of os.PathLike objects\n"
  " |          List of absolute or relative, paths/to/input/files.ext\n"
  " |      dry : bool\n"
  " |          If True, perform dry run: run all format checks and print\n"
  " |          archive contents, do not write to disk.\n"
  " |      verbose : bool\n"
  " |          If True, print archive contents.\n"
  " |      format : str, optional\n"
  " |          Expects \"BIGF\", \"BIGH\" or \"BIG4\".\n"
  " |      direnlen : int, optional\n"
  " |          If >= 10, set as fixed archive directory entry length.\n"
  " |      fnhex : bool, optional\n"
  " |          If True, decode input filenames from Base16/hexadecimal.\n"
  " |          Use for non-printable filenames in archive. Keeps\n"
  " |          leading/embedding null bytes.\n"
  " |\n"
  " |      Returns\n"
  " |      -------\n"
  " |      {0, 1}\n"
  " |          1 on success.\n"
  " |\n"
  " |      Raises\n"
  " |      ------\n"
  " |      FileNotFoundError\n"
  " |          When 'viv' cannot be created.\n"
  " |\n"
  " |      Examples\n"
  " |      --------\n"
  " |      Encode all files in the list 'infiles_paths' in a new archive\n"
  " |      \"out.viv\". The archive is to be created in a subdirectory\n"
  " |      \"foo\", relative to the current working directory. Both input\n"
  " |      files are in the current parent directory.\n"
  " |\n"
  " |      >>> viv = \"foo/out.viv\"\n"
  " |      >>> infiles_paths = [\"../LICENSE\", \"../README.md\"]\n"
  " |      >>> unvivtool.viv(viv, infiles_paths)\n"
  " |      ...\n"
  " |      Encoder successful.\n"
  " |      1\n"
  " |\n"
  " |      The dry run functionality may be used to test parameters without\n"
  " |      writing to disk.\n"
  " |\n"
  " |      >>> unvivtool.viv(viv, infiles_paths, dry=True)\n"
  " |      Begin dry run\n"
  " |      ...\n"
  " |\n"
  " |      Supposing, the dry run has been successful. Encode the listed\n"
  " |      files, again printing archive contents.\n"
  " |\n"
  " |      >>> unvivtool.viv(viv, infiles_paths, verbose=True)\n"
  " |      ...\n"
);

/* -------------------------------------------------------------------------- */

static
PyMethodDef m_methods[] = {
  {"unviv",  (PyCFunction)(void(*)(void))unviv, METH_VARARGS | METH_KEYWORDS, unviv__doc__},
  {"viv",    (PyCFunction)(void(*)(void))viv, METH_VARARGS | METH_KEYWORDS, viv__doc__},
  {NULL,     NULL}
};

static
int unvivtool_exec(PyObject *mod)
{
  if (PyModule_AddStringConstant(mod, "__version__", MACRO_STRINGIFY(VERSION_INFO)) < 0)
  {
    return -1;
  }
  return 0;
}

static
PyModuleDef_Slot m_slots[] = {
  {Py_mod_exec, unvivtool_exec},
  {0, NULL}
};

static
PyModuleDef unvivtoolmodule = {
  PyModuleDef_HEAD_INIT,  /* m_base */
  "unvivtool",  /* m_name */
  m_doc,  /* m_doc */
  0,  /* m_size */
  m_methods,  /* m_methods */
  m_slots,  /* m_slots */
  NULL,  /* m_traverse */
  NULL,  /* m_clear */
  NULL  /* m_free */
};

PyMODINIT_FUNC PyInit_unvivtool(void)
{
  return PyModuleDef_Init(&unvivtoolmodule);
}
