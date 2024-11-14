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
#define chdir _chdir
#define getcwd _getcwd
#else
#include <unistd.h>  /* chdir, getcwd */
#endif

#include <Python.h>

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

/* util --------------------------------------------------------------------- */

/* Returns new reference or NULL. */
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
  const int len = LIBNFSVIV_clamp(UVT_PY_MaxPathLen, 1, (int)strlen(p) + 1);
  dest = (char *)malloc(len * sizeof(*dest));
  if (!dest)
  {
    PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
    return NULL;
  }
  memcpy(dest, p, len);
  dest[len - 1] = '\0';
  return dest;
}

/* data analysis ------------------------------------------------------------ */

/* Returns Python dictionary with VivDirectory and list of valid or all filenames. */
static
PyObject *get_info(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv;
  PyObject *retv_obj = NULL;
  PyObject *list_ = NULL;
  char *viv_name = NULL;
  PyObject *viv_name_obj = NULL;
  int opt_verbose = 0;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  int opt_invalidentries = 0;  /* export info for invalid entries */
  VivDirectory vd = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };
  char **filelist = NULL;
  static const char *keywords[] = { "path", "verbose", "direnlen", "fnhex", "invalid", NULL };

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&|$pipp:get_info",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj,
                                   &opt_verbose, &opt_direnlenfixed, &opt_filenameshex, &opt_invalidentries))
  {
    return NULL;
  }

  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  Py_DECREF(viv_name_obj);
  if (!viv_name)  return NULL;

  if (opt_direnlenfixed != 0)
    opt_direnlenfixed = LIBNFSVIV_Clamp_opt_direnlenfixed(opt_direnlenfixed, opt_verbose);
  const int local_opt_filenameshex = LIBNFSVIV_Fix_opt_filenameshex(opt_filenameshex, opt_direnlenfixed);

  /* Workload */
  SCL_printf("UVT path: %s %p\n", viv_name, viv_name);
  SCL_printf("UVT path sz: %d\n", LIBNFSVIV_GetFilesize(viv_name));
  SCL_printf("UVT opt_verbose: %d\n", opt_verbose);
  SCL_printf("UVT opt_direnlenfixed: %d\n", opt_direnlenfixed);
  SCL_printf("UVT opt_filenameshex: %d\n", opt_filenameshex);
  SCL_printf("UVT opt_invalidentries: %d\n", opt_invalidentries);
  const int viv_format = LIBNFSVIV_GetVivVersion_FromPath(viv_name);
  SCL_printf("UVT viv_format: %d\n", viv_format);
  if (!viv_format)
  {
    PyErr_SetString(PyExc_FileNotFoundError, "Cannot read file");
    return NULL;
  }

  retv = !!LIBNFSVIV_GetVivDirectory(&vd, viv_name, opt_verbose, opt_direnlenfixed, local_opt_filenameshex, 1);

  /* handle formats:

    BIGF BIGH BIG4: "<format>", full dictionary
    refpack'd BIGF BIGH BIG4: "REFPACK_<format>", othw. empty dictionary
    invalid utf8/printable: Py_None, othw. empty dictionary
    invalid non-printable: Py_None, othw. empty dictionary
    null: Py_None, othw. empty dictionary
  */

  /* Handle: Valid file but is not uncompressed BIGF BIGH BIG4; returns {format: None|"string" } */
  if (retv == 0 && viv_format < 0)
  {
    retv_obj = PyDict_New();
    if (!retv_obj)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      return NULL;
    }

    switch (viv_format)
    {
      case -7:
        if (0 == PyDict_SetItemString(retv_obj, "format", PyUnicode_FromString("REFPACK_BIGF")))
          return retv_obj;
        break;
      case -8:
        if (0 == PyDict_SetItemString(retv_obj, "format", PyUnicode_FromString("REFPACK_BIGH")))
          return retv_obj;
        break;
      case -4:
        if (0 == PyDict_SetItemString(retv_obj, "format", PyUnicode_FromString("REFPACK_BIG4")))
          return retv_obj;
        break;
      default:  /* valid file of unknown format */
        {
#if defined(UVTUTF8)
          const char *fmt_s = (LIBNFSVIV_IsUTF8String(vd.format, 4, 0) == 4) ? vd.format : NULL;
#else
          const char *fmt_s = (LIBNFSVIV_IsPrintString(vd.format, 4, 4) == 4) ? vd.format : NULL;
#endif
          PyObject *fmt_ = fmt_s ? PyUnicode_FromStringAndSize(fmt_s, 4) : Py_NewRef(Py_None);
          if (0 == PyDict_SetItemString(retv_obj, "format", fmt_))
            return retv_obj;
        }
        break;
    }

    Py_XDECREF(retv_obj);
    retv_obj = NULL;
    PyErr_SetString(PyExc_Exception, "Cannot get format");
  }  /* Handle: ... */


#if SCL_DEBUG > 0
  LIBNFSVIV_ValidateVivDirectory(&vd);  // debug
  SCL_printf("UVT vd.format: %s\n", LIBNFSVIV_GetVivVersionString(vd.format));
  LIBNFSVIV_PrintVivDirEntr(&vd, opt_invalidentries);
  fflush(0);
#endif

  if (retv == 1)
  {
    filelist = LIBNFSVIV_VivDirectoryToFileList(&vd, viv_name, opt_invalidentries);
    if (!filelist)
    {
      PyErr_SetString(PyExc_Exception, "Cannot get filelist");
      retv = 0;
    }
  }

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
    const char *fmt_s = LIBNFSVIV_GetVivVersionString(vd.format);  /* invalid format is handled above */
    PyObject *fmt_ = fmt_s ? PyUnicode_FromString(fmt_s) : NULL;
    if (!fmt_s || !fmt_)
    {
      PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
      retv = 0;
      Py_XDECREF(fmt_);
      break;
    }
    retv &= 0 == PyDict_SetItemString(retv_obj, "format", fmt_);
    retv &= 0 == PyDict_SetItemString(retv_obj, "__state", Py_BuildValue("b", vd.__padding[0]));
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
        if (LIBNFSVIV_IsUTF8String(filelist[i], vd.buffer[i].filename_len_, 0) == vd.buffer[i].filename_len_)
          item_ = PyUnicode_FromStringAndSize(filelist[i], vd.buffer[i].filename_len_);
        else
          item_ = Py_NewRef(Py_None);
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
      PyList_SetItem(list_, i, item_);
    }  /* for i */

    retv &= 0 == PyDict_SetItemString(retv_obj, "files", list_);

    {
      PyObject *list_offset = PyList_New(list_len);
      PyObject *list_filesize = PyList_New(list_len);
      PyObject *list_fn_len_ = PyList_New(list_len);
      PyObject *list_fn_ofs_ = PyList_New(list_len);
      PyObject *list_validity = PyList_New(list_len);
      if (!list_offset || !list_filesize || !list_fn_len_ || !list_fn_ofs_ || !list_validity)
      {
        PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
        retv = 0;
      }
      for (int i = 0; i < list_len && (retv); ++i)
      {
        PyObject *tmp1 = Py_BuildValue("i", vd.buffer[i].offset);
        PyObject *tmp2 = Py_BuildValue("i", vd.buffer[i].filesize);
        PyObject *tmp3 = Py_BuildValue("i", vd.buffer[i].filename_len_);
        PyObject *tmp4 = Py_BuildValue("i", vd.buffer[i].filename_ofs_);
        PyObject *tmp5 = Py_BuildValue("i", SCL_BITMAP_IsSet(vd.bitmap, i));
        if (!tmp1 || !tmp2 || !tmp3 || !tmp4 || !tmp5)
        {
          PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
          retv = 0;
          break;
        }
        retv &= 0 == PyList_SetItem(list_offset, i, tmp1);
        retv &= 0 == PyList_SetItem(list_filesize, i, tmp2);
        retv &= 0 == PyList_SetItem(list_fn_len_, i, tmp3);
        retv &= 0 == PyList_SetItem(list_fn_ofs_, i, tmp4);
        retv &= 0 == PyList_SetItem(list_validity, i, tmp5);
      }  /* for i */
      retv &= 0 == PyDict_SetItemString(retv_obj, "files_offsets", list_offset);
      retv &= 0 == PyDict_SetItemString(retv_obj, "files_sizes", list_filesize);
      retv &= 0 == PyDict_SetItemString(retv_obj, "files_fn_lens", list_fn_len_);
      retv &= 0 == PyDict_SetItemString(retv_obj, "files_fn_ofs", list_fn_ofs_);
      retv &= 0 == PyDict_SetItemString(retv_obj, "bitmap", list_validity);
      retv &= 0 == PyDict_SetItemString(retv_obj, "validity_bitmap", list_validity);  /* DEPRECATED as of unvivtool 3.2 */

      if (!retv)
      {
        Py_XDECREF(list_offset);
        Py_XDECREF(list_filesize);
        Py_XDECREF(list_fn_len_);
        Py_XDECREF(list_fn_ofs_);
        Py_XDECREF(list_validity);
      }
    }

    break;
  }  /* for (;;) */

  if (filelist)
  {
    if (*filelist)  free(*filelist);
    free(filelist);
  }
  LIBNFSVIV_FreeVivDirectory(&vd);
  if (viv_name)  free(viv_name);
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

  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  Py_DECREF(viv_name_obj);
  if (!viv_name)  return NULL;

  for (;;)
  {
    outpath = __UVT_PyBytes_StringToCString(outpath, outpath_obj);
    Py_XDECREF(outpath_obj);
    if (!outpath)  break;

    if (request_file_name_obj)
    {
      request_file_name = PyBytes_AsString(request_file_name_obj);
      if (!request_file_name)
      {
        PyErr_SetString(PyExc_TypeError, "Cannot convert str");
        break;
      }
      PySys_WriteStdout("Requested file: %s\n", request_file_name);
    }

    if (request_file_idx > 0 && !request_file_name)  PySys_WriteStdout("Requested file at index: %d\n", request_file_idx);

    if (!LIBNFSVIV_IsFile(viv_name))
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot open viv: no such file or directory");
      break;
    }

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

  if (buf_cwd)  free(buf_cwd);
  if (outpath)  free(outpath);
  if (viv_name)  free(viv_name);
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
  char *opt_requestfmt_ptr = NULL;
  int opt_requestendian = 0xe;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  int opt_dryrun = 0;
  int opt_verbose = 0;
  int opt_faithfulencode = 0;
  int count_infiles = 1;
  int i;
  int length_str = 0;
  int ofs = 0;
  PyObject *item = NULL;
  PyObject *bytes = NULL;
  char *ptr = NULL;
  static const char *keywords[] = { "viv", "infiles", "dry", "verbose",
                                    "format", "endian", "direnlen", "fnhex", "faithful", NULL };

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O|$ppsiipp:viv",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj, &infiles_paths_obj,
                                   &opt_dryrun, &opt_verbose,
                                   &opt_requestfmt_ptr, &opt_requestendian,
                                   &opt_direnlenfixed, &opt_filenameshex,
                                   &opt_faithfulencode))
  {
    return NULL;
  }

  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  Py_DECREF(viv_name_obj);
  if (!viv_name)  return NULL;

  for (;;)
  {
    if (opt_requestfmt_ptr)
    {
      int len_ = LIBNFSVIV_min(4, (int)strlen(opt_requestfmt_ptr));
      memcpy(opt_requestfmt, opt_requestfmt_ptr, len_);
      opt_requestfmt[len_] = '\0';
      if (len_ != 4 || LIBNFSVIV_GetVivVersion_FromBuf(opt_requestfmt) <= 0)
      {
        PyErr_SetString(PyExc_ValueError, "Invalid format (expects 'BIGF', 'BIGH' or 'BIG4')");
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
      retv = LIBNFSVIV_Viv(viv_name, infiles_paths, count_infiles,
                           opt_dryrun, opt_verbose, opt_direnlenfixed,
                           opt_filenameshex, opt_requestfmt, opt_requestendian,
                           opt_faithfulencode);

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
    if (*infiles_paths)  free(*infiles_paths);
    free(infiles_paths);
  }
  if (viv_name)  free(viv_name);
  // Py_DECREF(viv_name_obj);  // see above

  return retv_obj;
}

static
PyObject *update(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv = 1;
  PyObject *retv_obj = NULL;

  char *viv_name = NULL;
  PyObject *viv_name_obj;
  char *infile_path = NULL;
  PyObject *infile_path_obj;

  int request_file_idx = 0;
  char *request_file_name = NULL;
  PyObject *request_entry_obj = NULL;

  char *viv_name_out = NULL;
  PyObject *viv_name_out_obj = NULL;
  int opt_insert = 0;
  int opt_replacefilename = 0;
  int opt_dryrun = 0;
  int opt_verbose = 0;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  int opt_faithfulencode = 0;
  static const char *keywords[] = { "inpath", "infile", "entry",
                                    "outpath",
                                    "insert", "replace_filename",
                                    "dry", "verbose", "direnlen", "fnhex", "faithful", NULL };

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O&O|$O&ipppipp:viv",
                                   (char **)keywords,
                                   PyUnicode_FSConverter, &viv_name_obj, PyUnicode_FSConverter, &infile_path_obj,
                                   &request_entry_obj,

                                   PyUnicode_FSConverter, &viv_name_out_obj,

                                   &opt_insert,
                                   &opt_replacefilename,
                                   &opt_dryrun, &opt_verbose,
                                   &opt_direnlenfixed, &opt_filenameshex,
                                   &opt_faithfulencode))
  {
    return NULL;
  }

  viv_name = __UVT_PyBytes_StringToCString(viv_name, viv_name_obj);
  Py_DECREF(viv_name_obj);
  if (!viv_name)  return NULL;

  for (;;)
  {
    infile_path = __UVT_PyBytes_StringToCString(infile_path, infile_path_obj);
    Py_DECREF(infile_path_obj);
    if (!infile_path)  return NULL;

    SCL_printf("UVT viv_name: %s\n", viv_name);
    SCL_printf("UVT infile_path: %s\n", infile_path);

    /* Get str|integer argument */
    if (PyUnicode_CheckExact(request_entry_obj))
    {
      Py_ssize_t len;
      const char *p = PyUnicode_AsUTF8AndSize(request_entry_obj, &len);
      request_file_name = (char *)calloc(LIBNFSVIV_clamp(UVT_PY_MaxPathLen, 1, len + 1) * sizeof(*request_file_name), 1);
      if (!request_file_name)
      {
        PyErr_SetString(PyExc_MemoryError, "Cannot allocate memory");
        return NULL;
      }
      memcpy(request_file_name, p, len);
      request_file_name[len] = '\0';
    }
    else if (PyLong_CheckExact(request_entry_obj))
    {
      request_file_idx = PyLong_AsLong(request_entry_obj);
    }
    else
    {
      PyErr_SetString(PyExc_TypeError, "Expects integer or string");
      return NULL;
    }
    Py_DECREF(request_entry_obj);

    SCL_printf("UVT request_file_name: %s\n", request_file_name);
    SCL_printf("UVT request_file_idx: %d\n", request_file_idx);

    // if (viv_name_out_obj && PyUnicode_CheckExact(viv_name_out_obj))
    if (viv_name_out_obj)
    {
      viv_name_out = __UVT_PyBytes_StringToCString(viv_name_out, viv_name_out_obj);
      Py_DECREF(viv_name_out_obj);
      if (!viv_name_out)  break;
    }
    SCL_printf("UVT viv_name_out: %s\n", viv_name_out);
    SCL_printf("\n");

    retv = LIBNFSVIV_Update(viv_name, viv_name_out,
                            request_file_idx, request_file_name,
                            infile_path,
                            opt_insert, opt_replacefilename,
                            opt_dryrun, opt_verbose,
                            opt_direnlenfixed, opt_filenameshex,
                            opt_faithfulencode);
    if (retv == 1)
      PySys_WriteStdout("Update successful.\n");
    else
      PySys_WriteStdout("Update failed.\n");

    retv_obj = Py_BuildValue("i", retv);
    break;
  }  /* for (;;) */

  if (viv_name)  free(viv_name);
  if (viv_name_out)  free(viv_name_out);
  if (infile_path)  free(infile_path);
  if (request_file_name)  free(request_file_name);

  return retv_obj;
}

/* doc ---------------------------------------------------------------------- */

PyDoc_STRVAR(
  m_doc,
  "simple BIGF BIGH BIG4 decoder/encoder (commonly known as VIV/BIG)\n"
  "\n"
  "Functions\n"
  "---------\n"
  "get_info() -- get archive header and filenames\n"
  "unviv() -- decode and extract archive\n"
  "update() -- replace file in archive\n"
  "viv() -- encode files in new archive\n"
  "\n"
  "unvivtool " UVTVERS " " UVTCOPYRIGHT "\n"
);

PyDoc_STRVAR(
  get_info__doc__,
  " |  get_info(path, verbose=False, direnlen=0, fnhex=False, invalid=False)\n"
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
  " |      invalid : bool, optional\n"
  " |          If True, export all directory entries, even if invalid.\n"
  " |\n"
  " |      Returns\n"
  " |      -------\n"
  " |      header : dictionary\n"
  " |          The only guaranteed entry is \"format\" with a string or None.\n"
  " |          Filenames list will be empty if the directory has zero (valid) entries.\n"
  " |\n"
  " |      Raises\n"
  " |      ------\n"
  " |      FileNotFoundError\n"
  " |      MemoryError\n"
  " |      Exception\n"
);

PyDoc_STRVAR(
  update__doc__,
  " |  update(inpath, infile, entry, outpath=None, insert=0, replace_filename=False, dry=False, verbose=False, direnlen=0, fnhex=False, faithful=False)\n"
  " |      Replace file in archive.\n"
  " |\n"
  " |      Parameters\n"
  " |      ----------\n"
  " |      inpath : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/archive.viv\n"
  " |      infile : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/file.ext\n"
  " |      entry : str, int\n"
  " |          Name of target entry or 1-based index of target entry.\n"
  " |      outpath : str, os.PathLike object, optional\n"
  " |          Absolute or relative, path/to/output_archive.viv\n"
  " |          If empty, overwrite vivpath.\n"
  " |      insert : int, optional\n"
  " |          If  > 0, set as fixed archive directory entry length.\n"
  " |          If == 0, set as fixed archive directory entry length.\n"
  " |          If  < 0, set as fixed archive directory entry length.\n"
  " |      replace_filename : bool, optional\n"
  " |          If True, and infile is a path/to/file.ext, the entry filename will be changed to file.ext\n"
  " |      dry : bool, optional\n"
  " |          If True, perform dry run: run all format checks and print\n"
  " |          archive contents, do not write to disk.\n"
  " |      verbose : bool, optional\n"
  " |          Verbose output.\n"
  " |      direnlen : int, optional\n"
  " |          If >= 10, set as fixed archive directory entry length.\n"
  " |      fnhex : bool, optional\n"
  " |          If True, interpret filenames as Base16/hexadecimal.\n"
  " |          Use for non-printable filenames in archive. Keeps\n"
  " |          leading/embedded null bytes.\n"
  " |      faithful : bool, optional\n"
  " |          If False, ignore invalid entries (default behavior).\n"
  " |          If True, replace any directory entries, even if invalid.\n"
  " |\n"
  " |      Returns\n"
  " |      -------\n"
  " |      {0, 1}\n"
  " |          1 on success.\n"
  " |\n"
  " |      Raises\n"
  " |      ------\n"
  " |      FileNotFoundError\n"
  " |      MemoryError\n"
  " |      TypeError\n"
  " |      Exception\n"
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
  " |      MemoryError\n"
  " |      TypeError\n"
);

PyDoc_STRVAR(
  viv__doc__,
  " |  viv(viv, infiles, dry=False, verbose=False, format=\"BIGF\", endian=0xE, direnlen=0, fnhex=False, faithful=False)\n"
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
  " |      endian : int, char, optional\n"
  " |          Defaults to 0xE for BIGF and BIGH, and 0xC for BIG4.\n"
  " |          Only use for rare occurences where BIGF has to be 0xC.\n"
  " |      direnlen : int, optional\n"
  " |          If >= 10, set as fixed archive directory entry length.\n"
  " |      fnhex : bool, optional\n"
  " |          If True, decode input filenames from Base16/hexadecimal.\n"
  " |          Use for non-printable filenames in archive. Keeps\n"
  " |          leading/embedded null bytes.\n"
  " |      faithful : bool, optional\n"
  " |\n"
  " |      Returns\n"
  " |      -------\n"
  " |      {0, 1}\n"
  " |          1 on success.\n"
  " |\n"
  " |      Raises\n"
  " |      ------\n"
  " |      FileNotFoundError\n"
  " |      MemoryError\n"
  " |      TypeError\n"
  " |      ValueError\n"
);

/* -------------------------------------------------------------------------- */

static
PyMethodDef m_methods[] = {

  {"get_info", (PyCFunction)(void(*)(void))get_info, METH_VARARGS | METH_KEYWORDS, get_info__doc__},
  {"update", (PyCFunction)(void(*)(void))update, METH_VARARGS | METH_KEYWORDS, update__doc__},
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

/*
  https://github.com/python/cpython/blob/main/Include/moduleobject.h
*/
static
PyModuleDef_Slot m_slots[] = {
  {Py_mod_exec, unvivtool_exec},
#ifdef Py_GIL_DISABLED
  // {Py_mod_gil, Py_MOD_GIL_USED},  // default even if not specified
  {Py_mod_gil, Py_MOD_GIL_NOT_USED},  // The module is safe to run without an active GIL.
#endif
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
