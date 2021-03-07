/*
  unvivtoolmodule.c - VIV/BIG decoder/encoder Python module
  unvivtool Copyright (C) 2020 Benjamin Futasz <https://github.com/bfut>

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

/**
  BUILD:
  - to cwd
      python setup.py build
  - install
      python setup.py install
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#define getcwd _getcwd
static const int kUnvivtoolMaxPathLen = 256;
#else
#include <unistd.h>  /* chdir, getcwd */
static const int kUnvivtoolMaxPathLen = 4096;
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
#endif  /* _WIN32 */

#include <Python.h>

/* UVT_MODULE_DEBUG --------------------------------------------------------- */
#ifdef UVT_MODULE_DEBUG
static const unsigned int BINDINGS_DOMAIN = 0x2038;

void *malloc_track(size_t size)
{
  void *ptr = malloc(size);
  PyTraceMalloc_Track(BINDINGS_DOMAIN, (uintptr_t)ptr, size);
  return ptr;
}

void *realloc_track(void *in_ptr, size_t size)
{
  void *ptr = realloc(in_ptr, size);
  if (ptr)
  {
    if (ptr != in_ptr)
    {
      PyTraceMalloc_Untrack(BINDINGS_DOMAIN, (uintptr_t)in_ptr);
    }
    PyTraceMalloc_Track(BINDINGS_DOMAIN, (uintptr_t)ptr, size);
  }
  return ptr;
}

void free_track(void *ptr)
{
  PyTraceMalloc_Untrack(BINDINGS_DOMAIN, (uintptr_t)ptr);
  free(ptr);
}

#define malloc malloc_track
#define realloc realloc_track
#define free free_track
#endif  /* def UVT_MODULE_DEBUG */

#if 0
#define malloc PyMem_Malloc
#define realloc PyMem_Realloc
#define free PyMem_Free
#endif
/* UVT_MODULE_DEBUG --------------------------------------------------------- */

#include "../libnfsviv.h"


/* wrappers ----------------------------------------------------------------- */

static
PyObject *unviv(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv;
  PyObject *retv_obj;
  char *viv_name;
  PyObject *viv_name_obj = NULL;
  char *outpath;
  PyObject *outpath_obj = NULL;
  int request_file_idx = 0;
  char *request_file_name = NULL;
  PyObject *request_file_name_obj = NULL;
  int opt_dryrun = 0;
  int opt_verbose = 0;
  int opt_strictchecks = 0;
  int fd;
  char *buf_cwd = NULL;
  static char *keywords[] = { "viv", "dir", "fileidx", "filename",
                              "dry", "verbose", "strict", NULL };

#ifdef UVT_MODULE_DEBUG
  setbuf(stdout, NULL);
  if (!LIBNFSVIV_SanityTest())
  {
    PyErr_SetString(PyExc_RuntimeError, "failed sanity test");
    return NULL;
  }
#endif

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O&|iO&iii:unviv", keywords,
                                   PyUnicode_FSConverter, &viv_name_obj,
                                   PyUnicode_FSConverter, &outpath_obj,
                                   &request_file_idx,
                                   PyUnicode_FSConverter, &request_file_name_obj,
                                   &opt_dryrun, &opt_verbose, &opt_strictchecks))
  {
    return NULL;
  }

  viv_name = PyBytes_AsString(viv_name_obj);
  if (!viv_name)
  {
    PyErr_SetString(PyExc_TypeError, "cannot convert str");
    return NULL;
  }

  for (;;)
  {
    outpath = PyBytes_AsString(outpath_obj);
    if (!outpath)
    {
      PyErr_SetString(PyExc_TypeError, "cannot convert str");
      retv_obj = NULL;
      break;
    }

    if (request_file_name_obj)
    {
      request_file_name = PyBytes_AsString(request_file_name_obj);
      if (!request_file_name)
      {
        PyErr_SetString(PyExc_TypeError, "cannot convert str");
        retv_obj = NULL;
        break;
      }
    }

#ifndef _WIN32
    else
    {
      fd = open(outpath, O_RDONLY);
      if( fd == -1 )
      {
        printf("Cannot open output directory '%s': no such directory\n", outpath);
        retv_obj = Py_BuildValue("i", 0);
        break;
      }
      close(fd);
    }
#endif  /* not _WIN32 */

    fd = open(viv_name, O_RDONLY);
    if( fd == -1 )
    {
      PyErr_SetString(PyExc_FileNotFoundError, "cannot open viv: no such file or directory");
      retv_obj = NULL;
      break;
    }
    close(fd);

    buf_cwd = /* (char *) */malloc((size_t)(kUnvivtoolMaxPathLen * 4 + 64));
    if (!buf_cwd)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot allocate memory");
      retv_obj = NULL;
      break;
    }
    if (!getcwd(buf_cwd, (size_t)(kUnvivtoolMaxPathLen * 4 + 64)))
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot get current working directory");
      retv_obj = NULL;
      break;
    }

    retv = LIBNFSVIV_Unviv(viv_name, outpath,
                           request_file_idx, request_file_name,
                           opt_dryrun, opt_strictchecks, opt_verbose);

    if (chdir(buf_cwd) != 0)
    {
      PyErr_SetString(PyExc_FileNotFoundError, "Cannot restore working directory");
      retv_obj = NULL;
      break;
    }

    if (retv == 1)
      printf("Decoder successful.\n");
    else
      printf("Decoder failed.\n");

    retv_obj = Py_BuildValue("i", retv);
    break;
  }  /* for (;;) */

  if (buf_cwd)
    free(buf_cwd);
  Py_DECREF(viv_name_obj);
  Py_XDECREF(outpath_obj);
  Py_XDECREF(request_file_name_obj);

  return retv_obj;
}

static
PyObject *viv(PyObject *self, PyObject *args, PyObject *kwargs)
{
  int retv = 1;
  PyObject *retv_obj;
  char *viv_name;
  PyObject *viv_name_obj;
  char **infiles_paths = NULL;
  PyObject *infiles_paths_obj;
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
  static char *keywords[] = { "viv", "infiles", "dry", "verbose", NULL };

#ifdef UVT_MODULE_DEBUG
  setbuf(stdout, NULL);
  if (!LIBNFSVIV_SanityTest())
  {
    PyErr_SetString(PyExc_RuntimeError, "failed sanity test");
    return NULL;
  }
#endif

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O|ii:viv", keywords,
                                   PyUnicode_FSConverter, &viv_name_obj,
                                   &infiles_paths_obj, &opt_dryrun, &opt_verbose))
  {
    return NULL;
  }

  viv_name = PyBytes_AsString(viv_name_obj);
  if (!viv_name)
  {
    PyErr_SetString(PyExc_RuntimeError, "cannot convert str");
    return NULL;
  }

  retv_obj = Py_BuildValue("i", retv);

  for (;;)
  {
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

    infiles_paths = /* (char **) */malloc((size_t)(count_infiles + 1) * (size_t)sizeof(*infiles_paths));
    if (!infiles_paths)
    {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate memory");
      retv_obj = NULL;
      break;
    }

    ptr = /* (char *) */malloc((size_t)length_str * (size_t)sizeof(**infiles_paths));
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

      memcpy(*infiles_paths + ofs, ptr, (size_t)length_str);
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

  if (retv_obj)
  {
    for(;;)
    {
      if (!opt_dryrun)
      {
        fd = open(viv_name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if(fd == -1)
        {
          PyErr_SetString(PyExc_FileNotFoundError, "cannot create archive - no such file or directory");
          retv_obj = NULL;
          break;
        }
        close(fd);
      }

      retv = LIBNFSVIV_Viv(viv_name, infiles_paths, count_infiles,
                           opt_dryrun, opt_verbose);

      if (retv == 1)
        printf("Encoder successful.\n");
      else
        printf("Encoder failed.\n");

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

  Py_DECREF(viv_name_obj);

  return retv_obj;
}


/* doc ---------------------------------------------------------------------- */

PyDoc_STRVAR(
  m_doc,
  "VIV/BIG decoding/encoding\n"
  "\n"
  "Functions\n"
  "---------\n"
  "viv() -- encode files in new VIV/BIG archive\n"
  "unviv() -- decode and extract VIV/BIG archive\n"
  "\n"
  "unvivtool "LIBVERS" Copyright (C) 2020 Benjamin Futasz (GPLv3+)\n"
);

PyDoc_STRVAR(
  unviv__doc__,
  " |  unviv(viv, dir, fileidx=None, filename=None, dry=False, verbose=False, strict=False)\n"
  " |      Decode and extract files from VIV/BIG archive.\n"
  " |\n"
  " |      Parameters\n"
  " |      ----------\n"
  " |      viv : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/archive.viv\n"
  " |      dir : str, os.PathLike object\n"
  " |          Absolute or relative, path/to/existing/output/directory\n"
  " |      fileidx : int, optional\n"
  " |          Extract file at given 1-based index.\n"
  " |      filename : str, optional\n"
  " |          Extract file 'filename' (cAse-sEnsitivE) from archive.\n"
  " |          Overrides 'fileidx'.\n"
  " |      dry : bool\n"
  " |          If True, perform dry run: run all format checks and print\n"
  " |          archive contents, do not write to disk.\n"
  " |      verbose : bool\n"
  " |          If True, print archive contents.\n"
  " |      strict : bool\n"
  " |          If True, run extra format checks and fail on the first\n"
  " |          unsuccessful file extraction.\n"
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
  " |      will print warnings. Up to a certain point, such archives may\n"
  " |      still be extracted. Warnings can be turned into errors, forcing\n"
  " |      stricter adherence to format specifications. Note, such 'errors'\n"
  " |      do not raise Python errors. Instead, unviv() returns 0.\n"
  " |\n"
  " |      >>> unvivtool.unviv(\"foo/bar.viv\", \".\", filename=\"car00.tga\", strict=True)\n"
  " |      ...\n"
  " |      Strict Format error (Viv directory filesizes do not match archive size)\n"
  " |      Decoder failed.\n"
  " |      0\n"
);

PyDoc_STRVAR(
  viv__doc__,
  " |  viv(viv, infiles, dry=False, verbose=False)\n"
  " |      Encode files in new VIV/BIG archive. Skips given input paths\n"
  " |      that cannot be opened.\n"
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


/* -------------------------------------------------------------------------- */

static
PyModuleDef unvivtoolmodule = {
  PyModuleDef_HEAD_INIT,  /* m_base */
  "unvivtool",  /* m_name */
  m_doc,
  -1,  /* m_size */
  m_methods
};


/* -------------------------------------------------------------------------- */

PyMODINIT_FUNC PyInit_unvivtool(void)
{
  return PyModule_Create(&unvivtoolmodule);
}