/*
  unvivtoolmodule.c - VIV/BIG decoder/encoder Python module
  unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>

  Portions copyright, see each source file for more information.

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

#define UVT_UNVIVTOOLMODULE
#define UVTUTF8
#ifndef SCL_DEBUG
#define SCL_DEBUG 0
#endif
#include "../libnfsviv.h"
#define UVT_PY_MaxPathLen LIBNFSVIV_FilenameMaxLen

/* #if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L)
#define SCL_UNUSED [[maybe_unused]]
#else
#define SCL_UNUSED
#endif */

/* util --------------------------------------------------------------------- */

static
char *__UVT_PyBytes_StringToCString(char *dest, PyObject * const src)
{
  if (!src)
  {
    PyErr_SetString(PyExc_ValueError, "Cannot convert None");
    return NULL;
  }
  const char *p = PyBytes_AsString(src);
  if (!p || !memchr(p, '\0', PyBytes_Size(src) + 1))
  {
    PyErr_SetString(PyExc_TypeError, "Argument is not a string");
    return NULL;
  }
  const int len = (int)LIBNFSVIV_clamp(strlen(p) + 1, 1, UVT_PY_MaxPathLen);
  dest = (char *)malloc(len * sizeof(*dest));
  if (!dest)
  {
    PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
    return NULL;
  }
  memcpy(dest, p, len);
  dest[len - 1] = '\0';  /* guarantee nul-termination */
  return dest;
}

/* data analysis ------------------------------------------------------------ */

/* Returns Python dictionary with VivDirectory and list of valid filenames. */
static
PyObject *GetInfo(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv;
  PyObject *retv_obj = NULL;
  PyObject *list_ = NULL;
  char *viv_name = NULL;
  PyObject *viv_name_obj = NULL;
  int opt_verbose = 0;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  VivDirectory vd = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };
  char **filelist = NULL;
  static const char *keywords[] = { "path", "verbose", "direnlen", "fnhex", NULL };

  /* Handle arguments */
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&|$pip:GetInfo",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj,
                                   &opt_verbose, &opt_direnlenfixed, &opt_filenameshex))
  {
    return NULL;
  }

  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  if (!viv_name)
    return NULL;
  Py_DECREF(viv_name_obj);
  if (opt_direnlenfixed != 0)
    opt_direnlenfixed = LIBNFSVIV_Clamp_opt_direnlenfixed(opt_direnlenfixed, opt_verbose);
  const int local_opt_filenameshex = LIBNFSVIV_Fix_opt_filenameshex(opt_filenameshex, opt_direnlenfixed);

  /* Workload */
  SCL_printf("UVT path: %s %p\n", viv_name, viv_name);
  SCL_printf("UVT path sz: %d\n", LIBNFSVIV_GetFilesize(viv_name));
  SCL_printf("UVT opt_verbose: %d\n", opt_verbose);
  SCL_printf("UVT opt_direnlenfixed: %d\n", opt_direnlenfixed);
  SCL_printf("UVT opt_filenameshex: %d\n", opt_filenameshex);
  retv = !!LIBNFSVIV_GetVivDirectory(&vd, viv_name, opt_verbose, opt_direnlenfixed, local_opt_filenameshex);
  SCL_printf("UVT LIBNFSVIV_GetVivDirectory: %d, vd: %p\n", retv, vd);
  SCL_printf("UVT vd.format: %s\n", LIBNFSVIV_GetVivVersionFromFile2(vd.format));
  SCL_printf("UVT LIBNFSVIV_GetVivVersion(viv_name): %d\n", LIBNFSVIV_GetVivVersion(viv_name));

  /* format can be
    BIGF BIGH BIG4: format
    invalid utf8/printable: Py_None, othw. empty dictionary
    invalid non-printable: Py_None, othw. empty dictionary
    null: Py_None, othw. empty dictionary
  */

  /* valid file but is not BIGF BIGH BIG4; returns {format: None|"string" } */
  if (retv == 0 && LIBNFSVIV_GetVivVersion(viv_name) < 0)
  {
    retv_obj = PyDict_New();
    if (!retv_obj)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      return NULL;
    }
#if defined(UVTUTF8)
    const char *fmt_s = LIBNFSVIV_IsUTF8String(vd.format, 4, 0) > 0 ? vd.format : NULL;
#else
    const char *fmt_s = LIBNFSVIV_IsPrintString(vd.format, 4) > 0 ? vd.format : NULL;
#endif
    PyObject *fmt_ = fmt_s ? PyUnicode_FromStringAndSize(fmt_s, 4) : Py_NewRef(Py_None);
    if (0 == PyDict_SetItemString(retv_obj, "format", fmt_))
      return retv_obj;
    return NULL;
  }  /* if */

#if SCL_DEBUG > 0
  LIBNFSVIV_ValidateVivDirectory(&vd);  // debug
  SCL_printf("UVT vd.format: %s\n", LIBNFSVIV_GetVivVersionFromFile2(vd.format));
  fflush(0);
#endif

  if (retv == 1)
  {
    filelist = LIBNFSVIV_VivDirectoryToFileList(&vd, viv_name);
    if (!filelist)
    {
      PyErr_SetString(PyExc_Exception, "Cannot get filelist");
      retv = 0;
    }
    SCL_printf("UVT LIBNFSVIV_VivDirectoryToFileList: %d\n", retv);
  }

#if SCL_DEBUG > 0
  if (retv == 1)  LIBNFSVIV_PrintVivDirEntr(&vd);
#endif

  for (;retv == 1;)
  {
    /* Create dictionary object */
    retv_obj = PyDict_New();
    if (!retv_obj)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      retv = 0;
      break;
    }
    const char *fmt_s = LIBNFSVIV_GetVivVersionFromFile2(vd.format);  /* invalid format is handled above */
    PyObject *fmt_ = fmt_s ? PyUnicode_FromString(fmt_s) : NULL;
    SCL_printf("UVT fmt: %p\n", fmt_);
    if (!fmt_s || !fmt_)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      retv = 0;
      Py_XDECREF(fmt_);
      break;
    }
    retv &= 0 == PyDict_SetItemString(retv_obj, "format", fmt_);
    retv &= 0 == PyDict_SetItemString(retv_obj, "size", Py_BuildValue("i", vd.filesize));
    retv &= 0 == PyDict_SetItemString(retv_obj, "count_dir_entries", Py_BuildValue("i", vd.count_dir_entries));
    retv &= 0 == PyDict_SetItemString(retv_obj, "count_dir_entries_true", Py_BuildValue("i", vd.count_dir_entries_true));
    retv &= 0 == PyDict_SetItemString(retv_obj, "header_size", Py_BuildValue("i", vd.header_size));
    retv &= 0 == PyDict_SetItemString(retv_obj, "header_size_true", Py_BuildValue("i", vd.viv_hdr_size_true));
    if (retv == 0)  Py_XDECREF(fmt_);
    break;
  }

  for (;retv == 1;)
  {
    /* Create list object, add to dictionary object */

    /* From (filelist != NULL) follows (sz > 0), per LIBNFSVIV_VivDirectoryToFileList() */
    int list_len = 0;
    while (filelist[list_len])
      ++list_len;

    list_ = PyList_New(list_len);
    if (!list_)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      retv = 0;
      break;
    }
    for (int i = 0; i < list_len; ++i)  /* do nothing for list of length zero */
    {
      PyObject *item_;
      if (!local_opt_filenameshex)
      {
        item_ = PyUnicode_FromString(filelist[i]);
      }
      else  /* filenames as hex */
      {
        item_ = PyBytes_FromStringAndSize(filelist[i], vd.buffer[i].filename_len_);
      }

      if (!item_)
      {
        PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
        retv = 0;
        break;
      }
      SCL_printf("UVT item_: %p\n", item_);
      PyList_SetItem(list_, i, item_);
    }

    retv &= 0 == PyDict_SetItemString(retv_obj, "files", list_);
    break;
  }  /* for (;;) */

  if (filelist)
  {
    if (*filelist)
      free(*filelist);
    free(filelist);
  }
  LIBNFSVIV_FreeVivDirectory(&vd);
  if (viv_name)
    free(viv_name);

  if (!retv)
  {
    Py_XDECREF(retv_obj);
    Py_XDECREF(list_);
    retv_obj = NULL;
  }
  return retv_obj;
}

/* decoder/encoder ---------------------------------------------------------- */

static
PyObject *Unviv(PyObject *self, PyObject *args, PyObject *kwargs)
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

  /* Handle arguments */
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O&|$iO&ppipi:Unviv",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj, PyUnicode_FSConverter, &outpath_obj,
                                   &request_file_idx, PyUnicode_FSConverter, &request_file_name_obj,
                                   &opt_dryrun, &opt_verbose, &opt_direnlenfixed, &opt_filenameshex,
                                   &opt_overwrite))
  {
    return NULL;
  }
  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  if (!viv_name)
    return NULL;
  Py_DECREF(viv_name_obj);

  for (;;)
  {
    outpath = __UVT_PyBytes_StringToCString(outpath, outpath_obj);
    if (!outpath)
      return NULL;
    Py_XDECREF(outpath_obj);

    if (request_file_name_obj)
    {
      request_file_name = PyBytes_AsString(request_file_name_obj);
      if (!request_file_name)
      {
        PyErr_SetString(PyExc_TypeError, "Cannot convert str");
        break;
      }
    }

    fd = open(viv_name, O_RDONLY);
    if (fd == -1)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot open viv: no such file or directory");
      break;
    }
    close(fd);

    buf_cwd = (char *)malloc(UVT_PY_MaxPathLen + 64);
    if (!buf_cwd)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      break;
    }
    if (!getcwd(buf_cwd, UVT_PY_MaxPathLen + 64))
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot get current working directory");
      break;
    }

    if (opt_direnlenfixed != 0)
    {
      opt_direnlenfixed = LIBNFSVIV_clamp(opt_direnlenfixed, 10, LIBNFSVIV_BufferSize + 16 - 1);
      PySys_WriteStdout("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)\n", opt_direnlenfixed, opt_direnlenfixed, LIBNFSVIV_BufferSize + 16 - 1);
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
      PySys_WriteStdout("Decoder successful.\n");
    else
      PySys_WriteStdout("Decoder failed.\n");

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
PyObject *Viv(PyObject *self, PyObject *args, PyObject *kwargs)
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
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O|$pisip:Viv",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj, &infiles_paths_obj,
                                   &opt_dryrun, &opt_verbose,
                                  //  PyUnicode_FSConverter, &opt_requestfmt_obj,
                                   &opt_requestfmt_ptr, &opt_direnlenfixed, &opt_filenameshex))
  {
    return NULL;
  }

  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  if (!viv_name)
    return NULL;
  Py_DECREF(viv_name_obj);

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
      PySys_WriteStdout("Requested format: %.4s\n", opt_requestfmt);
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

    infiles_paths = (char **)malloc((count_infiles + 1) * sizeof(*infiles_paths));
    if (!infiles_paths)
    {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate memory");
      retv_obj = NULL;
      break;
    }

    ptr = (char *)malloc(length_str * sizeof(**infiles_paths));
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

  if (opt_direnlenfixed != 0)
  {
    opt_direnlenfixed = LIBNFSVIV_clamp(opt_direnlenfixed, 10, LIBNFSVIV_BufferSize + 16 - 1);
    PySys_WriteStdout("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)\n", opt_direnlenfixed, opt_direnlenfixed, LIBNFSVIV_BufferSize + 16 - 1);
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
        PySys_WriteStdout("Encoder successful.\n");
      else
        PySys_WriteStdout("Encoder failed.\n");

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
  "simple BIGF BIGH BIG4 decoder/encoder (commonly known as VIV/BIG)\n"
  "\n"
  "Functions\n"
  "---------\n"
  "GetInfo() -- get archive header and filenames\n"
  "Unviv() -- decode and extract archive\n"
  "Viv() -- encode files in new archive\n"
  "\n"
  "unvivtool "UVTVERS" "UVTCOPYRIGHT"\n"
);

PyDoc_STRVAR(
  GetInfo__doc__,
  " |  GetInfo(path, verbose=False, direnlen=0, fnhex=False)\n"
  " |      Return dictionary of archive header info and list of filenames.\n"
  " |\n"
  " |      Parameters\n"
  " |      ----------\n"
  " |      path : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/archive.viv\n"
  " |      verbose : bool, optional\n"
  " |          Verbose output.\n"
  " |      direnlen : int, optional\n"
  " |          If >= 10, set as fixed archive directory entry length.\n"
  " |      fnhex : bool, optional\n"
  " |          If True, interpret filenames as Base16/hexadecimal.\n"
  " |          Use for non-printable filenames in archive. Keeps\n"
  " |          leading/embedding null bytes.\n"
  " |\n"
  " |      Returns\n"
  " |      -------\n"
  " |      filenames : dictionary\n"
  " |          Filename list will be empty if the directory has zero valid entries.\n"
  " |\n"
  " |      Raises\n"
  " |      ------\n"
  " |      FileNotFoundError\n"
  " |          When 'path' cannot be opened.\n"
);

PyDoc_STRVAR(
  Unviv__doc__,
  " |  Unviv(viv, dir, direnlen=0, fileidx=None, filename=None, fnhex=False, dry=False, verbose=False, overwrite=0)\n"
  " |      Decode and extract archive. Accepts BIGF, BIGH, and BIG4.\n"
  " |\n"
  " |      Parameters\n"
  " |      ----------\n"
  " |      viv : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/archive.viv\n"
  " |      dir : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/output/directory\n"
  " |      direnlen : int, optional\n"
  " |          If >= 10, set as fixed archive directory entry length.\n"
  " |      fileidx : int, optional\n"
  " |          Extract file at given 1-based index.\n"
  " |      filename : str, optional\n"
  " |          Extract file 'filename' (cAse-sEnsitivE) from archive.\n"
  " |          Overrides the fileidx parameter.\n"
  " |      fnhex : bool, optional\n"
  " |          If True, interpret filenames as Base16/hexadecimal.\n"
  " |          Use for non-printable filenames in archive. Keeps\n"
  " |          leading/embedded null bytes.\n"
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
);

PyDoc_STRVAR(
  Viv__doc__,
  " |  Viv(viv, infiles, dry=False, verbose=False, format=\"BIGF\", direnlen=0, fnhex=False)\n"
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
  " |          leading/embedded null bytes.\n"
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
);

/* -------------------------------------------------------------------------- */

static
PyMethodDef m_methods[] = {
  {"GetInfo", (PyCFunction)(void(*)(void))GetInfo, METH_VARARGS | METH_KEYWORDS, GetInfo__doc__},
  {"Unviv",  (PyCFunction)(void(*)(void))Unviv, METH_VARARGS | METH_KEYWORDS, Unviv__doc__},
  {"Viv",    (PyCFunction)(void(*)(void))Viv, METH_VARARGS | METH_KEYWORDS, Viv__doc__},
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
