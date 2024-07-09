/*
  sclpython.h
  SCL Copyright (C) 2022 and later Benjamin Futasz <https://github.com/bfut>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SCLPYTHON_H_
#define SCLPYTHON_H_

#ifdef SCL_PY_PRINTF
#include <stdarg.h>
#include <stdio.h>

#include <Python.h>

#define SCL_PY_PRINTF_BUFSZ 1001  /* sysmodule.c -> sys_write */

#ifdef printf
#undef printf
#endif
int SCL_PY_printf(const char *fmt, ...)
{
  int n = -1;
  char scl_py_printf_buf[SCL_PY_PRINTF_BUFSZ] = {0};
  va_list ap;
  va_start(ap, fmt);

  n = PyOS_vsnprintf(scl_py_printf_buf, sizeof(scl_py_printf_buf), fmt, ap);
  if (n > 0)
  {
    scl_py_printf_buf[n] = '\0';
    PySys_WriteStdout("%s", scl_py_printf_buf);
  }

  va_end(ap);
  return n;
}
#define printf SCL_PY_printf

/*
  fprintf(stderr, ...) and fprintf(stream, ...)

  no support for fprintf(stdout, ...) -> use printf(...) instead
*/
#ifdef fprintf
#undef fprintf
#endif
int SCL_PY_fprintf(FILE *stream, const char *fmt, ...)
{
  int n = -1;
  char scl_py_printf_buf[SCL_PY_PRINTF_BUFSZ] = {0};
  va_list ap;
  va_start(ap, fmt);

  if (stream != stderr)
    n = vfprintf(stream, fmt, ap);
  else if (stream == stderr)
  {
    n = PyOS_vsnprintf(scl_py_printf_buf, sizeof(scl_py_printf_buf), fmt, ap);
    if (n > 0)
    {
      scl_py_printf_buf[n] = '\0';
      PySys_WriteStderr("%s", scl_py_printf_buf);
    }
  }

  va_end(ap);
  return n;
}
#define fprintf SCL_PY_fprintf
#endif  /* SCL_PY_PRINTF */

#endif  /* SCLPYTHON_H_ */
