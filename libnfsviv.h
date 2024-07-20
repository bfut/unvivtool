/*
  libnfsviv.h - simple BIGF BIGH BIG4 decoder/encoder (commonly known as VIV/BIG)
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

/*
  The API in this header-only library is composed of two parts:

  1. LIBNFSVIV_Unviv() and LIBNFSVIV_Viv() are one-and-done functions
  2. Data anlysis via
    LIBNFSVIV_GetVivVersion*()
    LIBNFSVIV_GetVivDirectory*() - returns struct *VivDirectory, the archive header
    LIBNFSVIV_VivDirectoryToFileList*() - returns char** of filenames listed in the archive header

  The decoder performs a single pass buffered read of the archive header.
  The encoder allows archiving in user-determined order.
  All functions are designed to be safe (and fast).
  A known BIGF variation with fixed directory entry length and non-printable
  filenames is supported in a first.

  Compiling:
    * little-endian, 32-bit|64-bit
    * Win98+ (MSVC 6.0+), Linux, macOS
    * non-Win32 requires _GNU_SOURCE #define'd for realpath()
    * optionally #define UVTUTF8 for the UVTUTF8 branch (decoder supports utf8-filenames within archive header), forces dfa.h dependency

  BIGF theoretical limits, assuming signed int:
    min header len:          16         0x10
    max header len:          2147483631 0x7fffffef
    min directory entry len: 10         0xa
    min dir entries count:   0
    max dir entries count:   214748363  0x0ccccccb

  BIGF BIGH headers usually contain big-endian numeric values.

  Special cases:
    archive header can have filesize encoded in little endian
    BIGF header can have a fixed directory entry length (e.g., 80 bytes). This allows names with embedded nul's.

  LIBNFSVIV_unviv() handles the following format deviations {with strategy}:
    * Archive header has incorrect filesize {value unused}
    * Archive header has incorrect number of directory entries {check endianness and/or assume large enough value}
    * Archive header has incorrect number directory length {value unused}
    * Archive header has incorrect offset {value unused}
    * At least 1 directory entry has illegal offset or length {skip file}
    * Two directory entries have the same file name (use opt_overwrite == 1) {overwrite or rename existing}
    * Directory entry would overwrite archive on extraction {skip file}
    * Directory entry file name contains non-ASCII UTF8 characters {native support in UVTUTF8-branch}
    * Directory entry file name contains non-printable characters (use opt_filenameshex == 1) {skip file or represent filename in base16}
    * Directory entry file name is too long {skips file}
    * Directory entry has fixed length and filename string is followed by large number of nul's (use opt_direnlenfixed == sz) {native support via option opt_direnlenfixed}
*/

#ifndef LIBNFSVIV_H_
#define LIBNFSVIV_H_

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>  /* GetFullPathNameA, GetLongPathNameA */
#include <direct.h>
#ifndef chdir
#define chdir _chdir
#endif
#ifndef mkdir
#define mkdir(a,b) _mkdir(a)
#endif
#ifndef stat
#define stat _stat
#endif
#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#ifndef S_IFMT
#define S_IFMT _S_IFMT
#endif
#else
#include <unistd.h>
#if defined(__STDC__)
#include <dirent.h>
#endif
#endif

#ifndef SCL_DEVMODE
#define SCL_DEVMODE 0  /* 0: release, 1: development, 2: experimental */
#endif
#ifndef SCL_DEBUG
#define SCL_DEBUG 0  /* 1: dev console output */
#endif

#if defined(SCL_DEBUG) && SCL_DEBUG > 0
#include <assert.h>
#define SCL_printf printf
#define SCL_assert assert
#else
/* #define SCL_printf(format, ...) { } */  /* C89-incompatible */
static void SCL_printf(const char *format, ...) { (void)format; }
#define SCL_assert(x)
#endif

#define UVTVERS "3.0"
#define UVTCOPYRIGHT "Copyright (C) 2020-2024 Benjamin Futasz (GPLv3+)"

#ifdef UVTUTF8  /* optional branch: unviv() utf8-filename support */
#include "./include/dfa.h"
#endif

#ifndef LIBNFSVIV_max
#define LIBNFSVIV_max(x,y) ((x)<(y)?(y):(x))
#define LIBNFSVIV_min(x,y) ((x)<(y)?(x):(y))
#define LIBNFSVIV_clamp(x,minv,maxv) ((maxv)<(minv)||(x)<(minv)?(minv):((x)>(maxv)?(maxv):(x)))
#define LIBNFSVIV_ceil(x,y) ((x)/(y)+((x)%(y)))  /* ceil(x/y) for x>=0,y>0 */
#endif

#if !defined(PATH_MAX) && !defined(_WIN32)
#define PATH_MAX 4096  /* for realpath() */
#endif

#define LIBNFSVIV_BufferSize 4096
#if !defined(_WIN32)
#define LIBNFSVIV_FilenameMaxLen PATH_MAX
#else
#define LIBNFSVIV_FilenameMaxLen 256 * 4  /* utf8 */
#endif
#define LIBNFSVIV_DirEntrMax 1572864  /* ceil(((1024 << 10) * 24) / 16) */

#define LIBNFSVIV_CircBuf_Size LIBNFSVIV_BufferSize + 16
/* #define LIBNFSVIV_CircBuf_Size 0x10 + 16 */


#define LIBNFSVIV_WENCFileEnding ".txt"

#ifndef __cplusplus
typedef struct VivDirEntr VivDirEntr;
typedef struct VivDirectory VivDirectory;
#define LIBNFSVIV_extern
#else
#define LIBNFSVIV_extern extern "C"
#endif

struct VivDirEntr {
  int offset;
  int filesize;
  int filename_ofs_;
  int filename_len_;  /* string length without nul */
};

#if defined(_WIN64) || defined(__LP64__) || defined(_M_X64) || defined(__x86_64__)
#define LIBNFSVIV_VivDirectoryPaddingSize 12
#else
#define LIBNFSVIV_VivDirectoryPaddingSize 20
#endif
struct VivDirectory {
  char format[4];  /* BIGF, BIGH or BIG4 */
  int filesize;
  int count_dir_entries;  /* per header */
  int header_size;  /* per header. includes VIV directory*/

  int count_dir_entries_true;  /* parsed valid entries count */
  int viv_hdr_size_true;  /* parsed unpadded. includes VIV directory. filename lengths include nul */

  int length;  /* length of buffer */
  int null_count;
  char *validity_bitmap;  /* len == ceil(length/64) * 64, i.e., multiples of 64 byte */
  VivDirEntr *buffer;

  /*
    keep 64 byte aligned;
    char[0] bitfield: 0 unused, 1-3 True for big-endianness
    char[>0], used for validity_bitmap if length sufficiently small
  */
  char __padding[LIBNFSVIV_VivDirectoryPaddingSize];
};

/* util --------------------------------------------------------------------- */
LIBNFSVIV_extern int LIBNFSVIV_GetVivVersion_FromBuf(const char * const buf);

/* fixed length entries with all-printable names are not known to exist */
static
int LIBNFSVIV_Fix_opt_filenameshex(const int opt_filenameshex, const int opt_direnlenfixed)
{
  return (opt_filenameshex || (opt_direnlenfixed >= 10));
}

static
int LIBNFSVIV_Clamp_opt_direnlenfixed(int opt_direnlenfixed, const int opt_verbose)
{
  const int temp_ = opt_direnlenfixed;
  opt_direnlenfixed = LIBNFSVIV_clamp(opt_direnlenfixed, 10, LIBNFSVIV_BufferSize + 16 - 1);
  if (opt_verbose && temp_ != opt_direnlenfixed)
    printf("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)\n", opt_direnlenfixed, opt_direnlenfixed, LIBNFSVIV_BufferSize + 16 - 1);
  return opt_direnlenfixed;
}

/* Assumes bitmap len divisible by 8 */
static
void LIBNFSVIV_SetBitmapTrue(char *bitmap, const int idx)
{
  bitmap[idx >> 3] |= 1 << (idx & 7);
}

/* Assumes bitmap len divisible by 8 */
static
void LIBNFSVIV_SetBitmapFalse(char *bitmap, const int idx)
{
  bitmap[idx >> 3] &= ~(1 << (idx & 7));
}

/* Assumes bitmap len divisible by 8 */
static
char LIBNFSVIV_GetBitmapValue(const char *bitmap, const int idx)
{
  return (bitmap[idx >> 3] >> (idx & 7)) & 1;
}

static
void *LIBNFSVIV_CallocVivDirectoryValidityBitmap(VivDirectory *vd)
{
  SCL_printf("CallocVivDirectoryValidityBitmap: length %d (case2: %d)\n", vd->length, vd->length >= 0 && vd->length <= (LIBNFSVIV_VivDirectoryPaddingSize - 1) * 8);
  if (vd->length < 0)
    return NULL;
  else if (vd->length <= (LIBNFSVIV_VivDirectoryPaddingSize - 1) * 8)
  {
    char *p = vd->__padding + 1;
    SCL_printf("LIBNFSVIV_CallocVivDirectoryValidityBitmap %p %p\n", p, vd->__padding);
    memset(p, 0, sizeof(vd->__padding) - 1);
    return p;
  }
  else
    return calloc(LIBNFSVIV_ceil(vd->length, 64) * 64 * sizeof(*vd->validity_bitmap), 1);
}

static
void LIBNFSVIV_FreeVivDirectoryValidityBitmap(VivDirectory *vd)
{
  if (vd->length > (LIBNFSVIV_VivDirectoryPaddingSize - 1) * 8)
    free(vd->validity_bitmap);
}

/* free's buffer and validity_bitmap */
static
void LIBNFSVIV_FreeVivDirectory(VivDirectory *vd)
{
  if (vd->buffer)
    free(vd->buffer);
  if (vd->validity_bitmap)
    LIBNFSVIV_FreeVivDirectoryValidityBitmap(vd);
}

static
VivDirectory *LIBNFSVIV_VivDirectory_Init(VivDirectory *vd, const int len)
{
  /* if (!vd)  return NULL; */
  vd->length = len;
  vd->validity_bitmap = (char *)LIBNFSVIV_CallocVivDirectoryValidityBitmap(vd);
  if (!vd->validity_bitmap)  return NULL;
  vd->buffer = (VivDirEntr *)calloc(vd->length * sizeof(*vd->buffer), 1);
  if (!vd->buffer)
  {
    free(vd->validity_bitmap);
    return NULL;
  }
  return vd;
}

#if defined(UVTUTF8)
/* Returns UTF8 length without nul. If __s is not nul-terminated, call with nul_terminate=0 */
static
int LIBNFSVIV_IsUTF8String(void *__s, const size_t max_len, const char nul_terminate)
{
  unsigned char *s = (unsigned char *)__s;
  size_t pos = 0;
  unsigned int codepoint, state = 0;
  if (!__s)  return 0;
  while (!(state == UTF8_REJECT) && (pos < max_len) && *s)
  {
    DFA_decode(&state, &codepoint, *s++);
    ++pos;
  }
  SCL_printf("    IsUTF8String: pos = %d, max_len = %d, state = %d (UTF8_ACCEPT %d); return=%d\n", (int)pos, (int)max_len, (int)state, UTF8_ACCEPT, (int)(pos * (!nul_terminate || (pos < max_len)) * (state == UTF8_ACCEPT)));
  return (int)pos * (!nul_terminate || (pos < max_len)) * (state == UTF8_ACCEPT);
}
#else
/* Returns isprint length without nul. If __s is not nul-terminated, set nul_terminate=0 */
static
int LIBNFSVIV_IsPrintString(void *__s, const size_t max_len, const char nul_terminate)
{
  unsigned char *s = (unsigned char *)__s;
  size_t pos = 0;
  if (!__s)  return 0;
  while ((pos < max_len) && *s)
  {
    if (!isprint(*s++))
      pos = max_len;
    ++pos;
  }
  return pos * (!nul_terminate || (pos < max_len));
}
#endif

static
int LIBNFSVIV_SwapEndian(const int y)
{
  unsigned int x;
  int z;
  memcpy(&x, &y, sizeof(x));
  x = ((x >> 24) & 0x000000ff) | ((x << 24) & 0xff000000) |
      ((x << 8) & 0xff0000) | ((x >> 8) & 0x00ff00);
  memcpy(&z, &x, sizeof(x));
  return z;
}

/* Assumes (buf) and (bufsz > 1). Returns len == strlen(buf) on success, otherwise -1.*/
static
int LIBNFSVIV_FreadToStr(char *buf, const int bufsz, const int ofs, int len, FILE *src)
{
  len = LIBNFSVIV_min(len, bufsz - 1);
  if (len >= 0 && !fseek(src, ofs, SEEK_SET) && (int)fread(buf, 1, len, src) == len)
  {
    buf[len] = '\0';
    return len;
  }
  buf[0] = '\0';
  return -1;
}

/* Assumes src_len is length without nul. */
static
void *LIBNFSVIV_memcpyToString(void *dest, const void *src,
                               const int src_len, const int dest_len)
{
  unsigned char *p = (unsigned char *)dest;
#if SCL_DEBUG > 0
  if (dest_len > 0)
  {
#endif
  memcpy(dest, src, src_len);
  p[LIBNFSVIV_min(src_len, dest_len - 1)] = '\0';
  return dest;
#if SCL_DEBUG > 0
  }
  return NULL;
#endif
}

/* Assumes input \in 0123456ABCDEFabcdef */
static
int LIBNFSVIV_hextoint(const char in)
{
  if (in >= '0' && in <= '9')
    return in - '0';
  if (in >= 'a' && in <= 'f')
    return in - 'a' + 10;
  if (in >= 'A' && in <= 'F')
    return in - 'A' + 10;
  return 0;
}

/* Assumes (0x0 <= in <= 0xF). Returns upper-case hexadecimal. */
static
char LIBNFSVIV_inttohex(const int in)
{
  if (in >= 0 && in < 0xA)
    return in + '0';
  if (in >= 0xA && in <= 0xF)
    /* return in + 'a' - 10; */
    return in + 'A' - 10;
  return '0';
}

/*
  Decodes Base16 string to binary string. Clips overlong. Returns strlen()

  Examples:
    "666F6F" -> "foo"
    "0066006F6F" -> "\0f\0oo" (i.e., keeps leading/embedded null)
*/
static
int LIBNFSVIV_DecBase16(char *str)
{
  const char *ptr = str;
  int i = 0;
  char buf[LIBNFSVIV_FilenameMaxLen];
  while (*ptr && *ptr + 1 && i < LIBNFSVIV_FilenameMaxLen - 2)  /* buf always ends on nul */
  {
    buf[i] = LIBNFSVIV_hextoint(*ptr) << 4;
    buf[i] += LIBNFSVIV_hextoint(*(ptr + 1));
    ptr += 2;
    ++i;
  }
  buf[i] = '\0';
  memcpy(str, buf, i + 1);
  return i;
}

/*
  Encodes binary string to Base16 string, clips overlong
  use min_len == -1 for strings, positive value if string has leading nul

  Example:
  e.g., "foo" -> "666F6F"
*/
static
void LIBNFSVIV_EncBase16(char *str, const int min_len)
{
  const char *ptr = str;
  int i = 0;
  char buf[LIBNFSVIV_FilenameMaxLen];
  while ((*ptr || i < 2*min_len) && i < LIBNFSVIV_FilenameMaxLen - 2 - 1)  /* buf always ends on nul */
  {
    buf[i] = LIBNFSVIV_inttohex((*ptr & 0xF0) >> 4);
    buf[i + 1] = LIBNFSVIV_inttohex(*ptr & 0xF);
    ++ptr;
    i += 2;
  }
  buf[i] = '\0';
  memcpy(str, buf, i + 1);
}

#ifdef _WIN32
static
void LIBNFSVIV_BkwdToFwdSlash(char *filename)
{
  char *ptr;
  while ((ptr = strrchr(filename, '\\')))
    ptr[0] = '/';
}
#endif

/* 'path/to/file.ext' returns pointer to 'file.ext'
  filename is not changed; except on Windows, converts '\\' to '/'. */
static
char *LIBNFSVIV_GetPathBasename(char *filename)
{
  char *ptr;
#ifdef _WIN32
  LIBNFSVIV_BkwdToFwdSlash(filename);
#endif
  if ((ptr = strrchr(filename, '/')))
    return ptr + 1;
  else
    return filename;
}

/* Returns -1 on error. Allows (path==NULL). */
static
int LIBNFSVIV_GetFilesize(const char *path)
{
  struct stat sb;
  SCL_printf("LIBNFSVIV_GetFilesize: %d\n", !!path && !stat(path, &sb) ? (int)sb.st_size : 0 );
  return !!path && !stat(path, &sb) ? (int)sb.st_size : -1;
}

static
int LIBNFSVIV_IsFile(const char *path)
{
  FILE *file = fopen(path, "rb");
  if (!file)  return 0;
  fclose(file);
  return 1;
}

static
int LIBNFSVIV_IsDir(const char *path)
{
  struct stat sb;
#if !defined(S_IFDIR)
  SCL_printf("LIBNFSVIV_IsDir: %d\n", !stat(path, &sb) && S_ISDIR(sb.st_mode));
  return (!stat(path, &sb) && S_ISDIR(sb.st_mode)) ? 1 : 0;
#else
  SCL_printf("LIBNFSVIV_IsDir: %d\n", (!stat(path, &sb) && (sb.st_mode & S_IFMT) == S_IFDIR));
  return (!stat(path, &sb) && (sb.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
#endif
}

/* Assumes !buf. Removes trailing '/'. Returned path never ends on '/' */
static
void LIBNFSVIV_GetParentDir(char *buf)
{
  char *ptr = buf + strlen(buf) - 1;
  if (ptr[0] == '/')
    ptr[0] = '\0';
  ptr = strrchr(buf, '/');
  if (ptr)
    ptr[0] = '\0';
  else
  {
    buf[0] = '.';
    buf[1] = '\0';
  }
}

#ifdef _WIN32
/* Wrapper for GetFullPathName()
  If (!dst): updates src, returns updated src, keeps (!dst) unchanged and ignores nBufferLength.
  Else: updates dst, returns updated dst, keeps src unchanged.

 [out] lpFilePart
A pointer to a buffer that receives the address (within lpBuffer) of the final file name component in the path.
This parameter can be NULL.
If lpBuffer refers to a directory and not a file, lpFilePart receives zero. */
static
char *LIBNFSVIV_GetFullPathName(char *src, char *dst, const size_t nBufferLength, char **lpFilePart)
{
  size_t len;
  if (!dst)
  {
    char buf[LIBNFSVIV_FilenameMaxLen];
    len = (size_t)GetFullPathName(src, LIBNFSVIV_FilenameMaxLen, buf, lpFilePart);  /* returns length without nul */
    if (len == 0 || len >= LIBNFSVIV_FilenameMaxLen)
      src[0] = '\0';
    else
    {
      memcpy(src, buf, len + 1);
      LIBNFSVIV_BkwdToFwdSlash(src);
    }
    return src;
  }
  else
  {
    len = (size_t)GetFullPathName(src, nBufferLength, dst, lpFilePart);  /* returns length without nul */
    if (len == 0 || len >= nBufferLength)
      dst[0] = '\0';
    else
      LIBNFSVIV_BkwdToFwdSlash(dst);
    return dst;
  }
}
#else
/* Wrapper for realpath(const char *src, char *dst)
  If (!dst): updates src, returns updated src, keeps (!dst) unchanged.
  Else: updates dst, returns updated dst, keeps src unchanged.

  Assumes (src) and sizeof(src) >= 4096 (PATH_MAX) and src is string.
  gcc -std=c89 requires sizeof(dst) >= 4096 to avoid buffer overflow */
static
char *LIBNFSVIV_GetFullPathName(char *src, char *dst)
{
  char *ptr;
  if (!dst)
  {
    char buf[LIBNFSVIV_FilenameMaxLen];
    ptr = realpath(src, buf);
    if (!ptr)
      src[0] = '\0';
    else
      memcpy(src, ptr, strlen(ptr) + 1);
  }
  else
  {
    ptr = realpath(src, dst);
    if (!ptr)
      dst[0] = '\0';
  }
  return ptr;
}
#endif

/*
  Write len bytes from infile to outfile. Returns 1 on success, 0 on failure.
  Assumes (dst) and (src) and (buf).
*/
static
int LIBNFSVIV_FileCopy(FILE *dest, FILE *src, int len, char *buf, const int bufsz)
{
  int err = 1;
  int chunk;
#if SCL_DEBUG > 0
  if (bufsz > 0)
  {
#endif
  while (len > 0)
  {
    chunk = LIBNFSVIV_min(bufsz, len);
    err &= chunk == (int)fread(buf, 1, chunk, src);
    err &= chunk == (int)fwrite(buf, 1, chunk, dest);
    len -= chunk;
  }
#if SCL_DEBUG > 0
  }
#endif
  return len == 0 && err == 1;
}

/*
  Invalidates entries whose output path is identical to the archive.
  Assumes (vd) and both, viv_name and outpath are strings.
*/
static
void LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths(VivDirectory *vd, char *viv_name, const char *outpath, FILE *viv, const size_t viv_sz)
{
  char buf[LIBNFSVIV_FilenameMaxLen];

  /** Case: viv parentdir != outpath -> return */
  memcpy(buf, viv_name, LIBNFSVIV_min(strlen(viv_name), LIBNFSVIV_FilenameMaxLen - 1));
  buf[LIBNFSVIV_min(strlen(viv_name), LIBNFSVIV_FilenameMaxLen - 1)] = '\0';
  LIBNFSVIV_GetParentDir(buf);
  if (strcmp(buf, outpath))
    return;

  /** Case: viv parentdir == outpath
   if for all i: viv filename != vivdirentry[i]->filename  -> return
   else: vivdirentry[i] is invalid
  */
  {
    char *viv_basename = LIBNFSVIV_GetPathBasename(viv_name);
    int chunk_size;
    int i;
    for (i = 0; i < vd->count_dir_entries_true; ++i)
    {
      fseek(viv, vd->buffer[i].filename_ofs_, SEEK_SET);
      chunk_size = LIBNFSVIV_min(viv_sz - vd->buffer[i].filename_ofs_, LIBNFSVIV_FilenameMaxLen);
      if (fread(buf, 1, chunk_size, viv) != (size_t)chunk_size)  { fprintf(stderr, "EnsureVivPathNotInVivDirWritePaths: File read error (strcmp)\n"); break; }
      if (LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) == 1 && !strcmp(buf, viv_basename))
      {
        LIBNFSVIV_SetBitmapFalse(vd->validity_bitmap, i);
        ++vd->null_count;
        printf("Warning:EnsureVivPathNotInVivDirWritePaths: Skip file '%s' (%d) (would overwrite this archive)\n", buf, i);
      }
    }
  }
}

/*
  Rename "path/to/existing/file.ext" to "path/to/existing/file_N.ext" with N in 0..999

  Returns 0 on failure, !0 on success.
  Assumes path is string, sz is size of path buffer.
*/
static
int LIBNFSVIV_IncrementFile(const char * const path, int sz, const int verbose)
{
  int retv = 0;
  if (!path)
    sz = strlen(path);
  else
    sz = -1;
  if (0 < sz && sz < LIBNFSVIV_FilenameMaxLen - 32 && !LIBNFSVIV_IsDir(path))
  {
    char buf[LIBNFSVIV_FilenameMaxLen];
    memcpy(buf, path, sz + 1);
    {
      int i;
      const char *ext = strrchr(path, '.');
      char *p = strrchr(buf, '.');  /* end of stem */
      if (p)
        p[0] = '\0';
      else
      {
        p = buf + sz;
        ext = path + sz;  /* no extension, hence we need '\0' */
      }
      for (i = 0; i < 1000; ++i)
      {
        sprintf(p, "_%d%s", i, ext);
        if (!LIBNFSVIV_IsFile(buf))
        {
          if (!rename(path, buf))
          {
            if (verbose)  printf("IncrementFile: Incremented existing file '%s' to '%s'\n", path, buf);
            retv = 1;
            break;
          }
        }
      }  /* for i */
    }
  }  /* if */
  if (verbose && !retv)  printf("IncrementFile: Cannot increment existing file '%s'\n", path);
  return retv;
}

/* CircBuf ------------------------------------------------------------------ */

/*
  struct LIBNFSVIV_CircBuf and LIBNFSVIV_CircBuf_* functions are derived from
    WDL - circbuf.h
    Copyright (C) 2005 Cockos Incorporated (zlib License)
*/

#ifndef __cplusplus
typedef struct LIBNFSVIV_CircBuf LIBNFSVIV_CircBuf;
#endif

struct LIBNFSVIV_CircBuf {  /* treat as private members */
  unsigned char *buf;
  int sz;
  int rd;
  int wr;
};

static
int LIBNFSVIV_CircBuf_lefttoread(const LIBNFSVIV_CircBuf * const cb)
{
  const int d = cb->wr - cb->rd;
  if (d >= 0)  return d;
  return d + cb->sz;
}

/* ignores wr */
static
int LIBNFSVIV_CircBuf_readtoend(const LIBNFSVIV_CircBuf * const cb)
{
  return cb->sz - cb->rd;
}

/* len is NOT upper-bounded by EOF */
static
int LIBNFSVIV_CircBuf_addFromFile(LIBNFSVIV_CircBuf *cb, FILE *file, const int filesz, int len)
{
  int written = 0;
  int wrlen1 = cb->sz - cb->wr;
  if (len < 0 || !cb->buf)  return -1;
  if (len > filesz)  len = filesz/*  - (int)ftell(file) */;
  if (len > cb->sz)  len = cb->sz;
  if (wrlen1 < len)
  {
    written += (int)fread(cb->buf + cb->wr, 1, wrlen1, file);
    written += (int)fread(cb->buf, 1, len - wrlen1, file);
    SCL_printf("    circbuf_addFromFile() stats: len: %d, written: %d, cb->wr: %d\n", len, written, cb->wr);
    if (written != len)  return -1;
    cb->wr = len - wrlen1;
  }
  else
  {
    written += (int)fread(cb->buf + cb->wr, 1, len, file);
    SCL_printf("    circbuf_addFromFile() stats: len: %d, written: %d, cb->wr: %d\n", len, written, cb->wr);
    if (written != len)  return -1;
    cb->wr += len;
  }
  cb->wr %= cb->sz;
  SCL_printf("!   circbuf_addFromFile() stats: len: %d, written: %d, cb->wr: %d\n", len, written, cb->wr);
  return written;
}

/* Forwards rd, ignores wr */
static
void LIBNFSVIV_CircBuf_Fwd(LIBNFSVIV_CircBuf *cb, int len)
{
  cb->rd += len;
  cb->rd %= cb->sz;
}

static
int LIBNFSVIV_CircBuf_Peek(LIBNFSVIV_CircBuf *cb, void *dest, const int ofs, int len)
{
  int rdlen1 = cb->sz - cb->rd - ofs;
  if (len < 0 || ofs < 0 || !cb->buf)  return 0;
  if (len > cb->sz)  len = cb->sz - ofs;
#if SCL_DEBUG > 0
  if (rdlen1 < 0)  SCL_printf("    circbuf_Peek(): rdlen1: %d, len: %d\n", rdlen1, len);
  SCL_assert(rdlen1 >= 0);
  if (rdlen1 < 0)  return 0;  /* Wstringop-overflow */
#endif
  if (rdlen1 < len)
  {
    memcpy(dest, cb->buf + cb->rd + ofs, rdlen1);
    memcpy((unsigned char *)dest + rdlen1, cb->buf, len - rdlen1);
  }
  else
  {
    memcpy(dest, cb->buf + cb->rd + ofs, len);
  }
  return len;
}

#if SCL_DEBUG > 0
void SCL_debug_printbuf(void *buf, int sz, int readat, int writeat)
{
  int l = sz;
  unsigned char *p = (unsigned char *)buf;
  int pos = 0;
  SCL_printf("  readat: %d, writeat: %d\n", readat, writeat);
  if (l < 0 || !buf)  return;
  while (l-- > 0)
  {
    if (pos == readat && pos == writeat)  SCL_printf("#%02x ", *p++);
    else if (pos == readat)  SCL_printf("r%02x ", *p++);
    else if (pos == writeat)  SCL_printf("w%02x ", *p++);
    else  SCL_printf(" %02x ", *p++);

    if (++pos % 0x10 == 0)  SCL_printf("\n");
  }
  if (pos % 0x10 != 0)  SCL_printf("\n");
}
#else
#define SCL_debug_printbuf(a,b,c,d)
#endif

static
int LIBNFSVIV_CircBuf_Get(LIBNFSVIV_CircBuf *cb, void *buf, const int ofs, int len)
{
  const int read = LIBNFSVIV_CircBuf_Peek(cb, buf, ofs, len);
  cb->rd += read;
  cb->rd %= cb->sz;
  return read;
}

#if SCL_DEBUG > 0
static
unsigned char *LIBNFSVIV_CircBuf_PeekPtr(const LIBNFSVIV_CircBuf * const cb, int ofs)
{
  if (ofs < 0 || !cb->buf)  return NULL;
  if (ofs > cb->sz)  return NULL;
  {
    const int r2e = LIBNFSVIV_CircBuf_readtoend(cb);
    ofs -= (r2e < ofs) ? r2e : 0;
    return cb->buf + cb->rd + ofs;
  }
}
#endif

/* ofs reduces len. unbounded by wr. */
static
void *LIBNFSVIV_CircBuf_memchr(const LIBNFSVIV_CircBuf * const cb, int c, int ofs, int len)
{
#if 1
  if (len <= 0)  return NULL;
  if (!cb->buf || cb->sz <= 0) return NULL;
  if (ofs > cb->sz)  { fprintf(stderr, "warning ofs\n"); return NULL; } /* ofs %= cb->sz; */  /* really an error */
  if (len > cb->sz)  { fprintf(stderr, "warning len\n"); return NULL; } /* len %= cb->sz; */  /* really an error */

  {
    int rdlen1;
    int rdofs = cb->rd + ofs;
    if (rdofs > cb->sz)  rdofs -= cb->sz;
    len -= ofs;
    rdlen1 = cb->sz - rdofs;
    SCL_printf("    circbuf_memchr(): rdofs: %d, ofs: %d, rdlen1: %d, len: %d\n", rdofs, ofs, rdlen1, len);
    if (rdlen1 < len)  /* r2e */
    {
      void *p = memchr(cb->buf + rdofs, c, rdlen1);
      SCL_printf("    circbuf_memchr(): rdofs: %d, ofs: %d, rdlen1: %d, len: %d,  p:'%p'\n", rdofs, ofs, rdlen1, len, p);
      return p ? p : memchr(cb->buf, c, len - rdlen1);
    }
    return memchr(cb->buf + rdofs, c, len);
  }
#elif 0
  {
    int rdlen1 = cb->rd + ofs + len;
    if (rdlen1 > cb->sz)  rdlen1 -= cb->sz;
    ofs -= cb->sz - cb->rd;
    len -= ofs;
    SCL_printf("    circbuf_memchr(): ofs: %d, rdlen1: %d, len: %d\n", ofs, rdlen1, len);
    if (rdlen1 < len)
    {
      void *p = memchr(cb->buf + cb->rd + ofs, c, rdlen1);
      SCL_printf("    circbuf_memchr(): p:'%p', ofs: %d, rdlen1: %d, len: %d\n", p, ofs, rdlen1, len);
      return p ? p : memchr(cb->buf /* + ofs - rdlen1 */, c, len - rdlen1);
    }
    return memchr(cb->buf + cb->rd + ofs, c, len);
  }
#endif
}

static
int __LIBNFSVIV_CircBuf_strnlen(const unsigned char * const str, const int strsz)
{
  const unsigned char * const p = (const unsigned char *)memchr(str, '\0', strsz);
  return p ? (int)(p - str) : strsz;
}

static
int LIBNFSVIV_CircBuf_PeekStrlen(LIBNFSVIV_CircBuf *cb, const int ofs, int len)
{
  int rdlen1 = cb->sz - cb->rd - ofs;
  if (rdlen1 < 0)  rdlen1 = 0;
  if (len > cb->sz)  len = cb->sz;
  len -= ofs;
  if (len < 0 || !cb->buf)  return -1;
  if (rdlen1 < len)
  {
    const int ret = __LIBNFSVIV_CircBuf_strnlen(cb->buf + cb->rd + ofs, rdlen1);
#if 0
    SCL_debug_printbuf(cb->buf, cb->sz, cb->rd, cb->wr);
    SCL_printf("    circbuf_PeekStrlen(): rdlen1: %d, len: %d\n", rdlen1, len);
    SCL_printf("    circbuf_PeekStrlen(): stats: len: %d, cb->rd: %d, cb->wr: %d\n", len, cb->rd, cb->wr);
#endif
    return (ret < rdlen1) ? ret : ret + __LIBNFSVIV_CircBuf_strnlen(cb->buf, len - rdlen1);
  }
  return __LIBNFSVIV_CircBuf_strnlen(cb->buf + cb->rd + ofs, len);
}

#if defined(UVTUTF8)
/* Input optionally with terminating nul. Returns length without nul. */
static
int LIBNFSVIV_CircBuf_PeekIsUTF8(LIBNFSVIV_CircBuf *cb, const int ofs, int len)
{
  int rdlen1 = cb->sz - cb->rd - ofs;
  unsigned char *s;
  int pos = 0;
  unsigned int codepoint, state = 0;
  if (ofs < 0 || !cb->buf)  return 0;
  if (len > cb->sz)  len = cb->sz;
  len -= ofs;
  if (len < 0)  return 0;
  s = cb->buf + cb->rd + ofs;
  SCL_printf("    CircBuf_PeekIsUTF8(): pos: %d,, rdlen1: %d, len: %d\n", pos, rdlen1, len);
  if (rdlen1 < len)
  {
    while (!(state == UTF8_REJECT) && (pos < rdlen1) && *s)
    {
      DFA_decode(&state, &codepoint, *s++);
      ++pos;
    }
    SCL_printf(":   CircBuf_PeekIsUTF8(): pos: %d,, rdlen1: %d, len: %d\n", pos, rdlen1, len);
    if (pos < rdlen1)  return pos * (state == UTF8_ACCEPT);
    s = cb->buf;
    while (!(state == UTF8_REJECT) && (pos < len - rdlen1) && *s)
    {
      DFA_decode(&state, &codepoint, *s++);
      ++pos;
    }
  }
  else
  {
    while (!(state == UTF8_REJECT) && (pos < len) && *s)
    {
      DFA_decode(&state, &codepoint, *s++);
      ++pos;
    }
  }
  SCL_printf("    CircBuf_PeekIsUTF8(): pos: %d,, rdlen1: %d, len: %d\n", pos, rdlen1, len);
  return pos * (state == UTF8_ACCEPT);
}
#else
/* Input optionally with terminating nul. Returns length without nul. */
static
int LIBNFSVIV_CircBuf_PeekIsPrint(LIBNFSVIV_CircBuf *cb, const int ofs, int len)
{
  int rdlen1 = cb->sz - cb->rd - ofs;
  unsigned char *s;
  int pos = 0;
  if (ofs < 0 || !cb->buf)  return 0;
  if (len > cb->sz)  len = cb->sz;
  len -= ofs;
  if (len < 0)  return 0;
  s = cb->buf + cb->rd + ofs;
  if (rdlen1 < len)
  {
    while ((pos < rdlen1) && *s)
    {
      if (!isprint(*s++))  pos = len;
      ++pos;
    }
    if (pos < rdlen1)  return pos;
    s = cb->buf;
    while ((pos < len - rdlen1) && *s)
    {
      if (!isprint(*s++))  pos = len;
      ++pos;
    }
  }
  else
  {
    while ((pos < len) && *s)
    {
      if (!isprint(*s++))  pos = len;
      ++pos;
    }
  }
  return pos;
}
#endif

/* stats -------------------------------------------------------------------- */

/*
  Sums clamped filename sizes plus nul's.
*/
static
int LIBNFSVIV_SumVivDirectoryFilenameSizes(const VivDirectory * const vd, const int opt_invalidentries)
{
  int sz = 0;
  int i;
  for (i = 0; i < vd->count_dir_entries; ++i)
  {
    if (!opt_invalidentries && LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) == 0)
      continue;
    sz += LIBNFSVIV_clamp(vd->buffer[i].filename_len_, 0, LIBNFSVIV_FilenameMaxLen - 1);
    ++sz;  /* nul */
  }
  SCL_printf("LIBNFSVIV_SumVivDirectoryFilenameSizes: %d  (opt_invalidentries: %d)\n", sz, opt_invalidentries);
  return sz;
}

static
void LIBNFSVIV_PrintVivDirEntr(const VivDirectory * const vd, const int opt_invalidentries)
{
  int i;
  int cnt;
  printf("PrintVivDirEntr\n");

  printf("vd->count_dir_entries: %d\n", vd->count_dir_entries);
  printf("vd->count_dir_entries_true: %d\n", vd->count_dir_entries_true);
  printf("vd->length: %d\n", vd->length);
  printf("vd->null_count: %d\n", vd->null_count);
  printf("vd->header_size: %d\n", vd->header_size);
  printf("vd->viv_hdr_size_true: %d\n", vd->viv_hdr_size_true);
  printf("vd->filesize: %d\n", vd->filesize);
  printf("vd valid filenames strings size: %d\n", LIBNFSVIV_SumVivDirectoryFilenameSizes(vd, 0));
  printf("vd filenames strings size: %d\n", LIBNFSVIV_SumVivDirectoryFilenameSizes(vd, opt_invalidentries));
  printf("i     valid? offset          filesize        filename_ofs_        filename_len_\n");
  for (i = 0, cnt = 0; i < LIBNFSVIV_min(vd->length, 4096) && cnt < vd->count_dir_entries; ++i)
  {
    cnt += LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i);
    if (!LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
      continue;
    printf("%2d     %d     %d (0x%x)   %d (0x%x)       %d (0x%x)       %d (nul: 0x%x)\n",
           i, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i),
           vd->buffer[i].offset, vd->buffer[i].offset,
           vd->buffer[i].filesize, vd->buffer[i].filesize,
           vd->buffer[i].filename_ofs_, vd->buffer[i].filename_ofs_,
           vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_ + vd->buffer[i].filename_len_ - 1);
  }
}

static
void __LIBNFSVIV_PrintVivDirectoryStats_Header(VivDirectory * const vd)
{
  #if defined(UVTUTF8)
    printf("File format (header) = %.4s\n", LIBNFSVIV_IsUTF8String(vd->format, 4, 0) == 4 ? vd->format : "....");
  #else
    printf("File format (header) = %.4s\n", LIBNFSVIV_IsPrintString(vd->format, 4, 0) == 4 ? vd->format : "....");
  #endif
  printf("Archive Size (header) = %d (0x%x)\n", vd->filesize, vd->filesize);
  printf("Directory Entries (header) = %d\n", vd->count_dir_entries);
  printf("Header Size (header) = %d (0x%x)\n", vd->header_size, vd->header_size);
}

static
void __LIBNFSVIV_PrintVivDirectoryStats_Parsed(VivDirectory * const vd)
{
  {
    int fsz = vd->viv_hdr_size_true;
    int i;
    for (i = 0; i < vd->count_dir_entries; ++i)
    {
      if (LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
      {
        fsz += vd->buffer[i].filesize;

      }
    }
    printf("Archive Size (parsed) = %d (0x%x)\n", fsz, fsz);
  }
  printf("Header Size (parsed) = %d (0x%x)\n", vd->viv_hdr_size_true, vd->viv_hdr_size_true);
  printf("Directory Entries (parsed) = %d\n", vd->count_dir_entries_true);
  printf("Endianness (parsed) = 0x%x\n", vd->__padding[0]);
}

/* Upon return, file always points to end of stream. */
static
void LIBNFSVIV_PrintStatsDec(VivDirectory * const vd, FILE *file,
                             const int request_file_idx, const char * const request_file_name,
                             const int opt_direnlenfixed, const int opt_filenameshex)
{
  int i;
  int gap;
  int contents_size = 0;
  int bufsize;
  unsigned char *buf;
  char filename[LIBNFSVIV_FilenameMaxLen] = {0};
  int filenamemaxlen = LIBNFSVIV_FilenameMaxLen;
  size_t sz;
  int viv_filesize;
  fseek(file, 0, SEEK_END);
  viv_filesize = (int)ftell(file);

  if (opt_direnlenfixed >= 10)
    filenamemaxlen = LIBNFSVIV_min(filenamemaxlen, opt_direnlenfixed - 0x08);

  if (vd->count_dir_entries_true > 0)
    bufsize = LIBNFSVIV_min(viv_filesize, vd->viv_hdr_size_true);
  else
    bufsize = LIBNFSVIV_min(viv_filesize, (1 << 22) + 1);;
  printf("bufsize = %d (0x%x)\n", bufsize, bufsize);

  if (bufsize > (1 << 22))
  {
    printf("Header purports to be greater than 4MB\n");
    return;
  }
  else if (bufsize < 16)
  {
    printf("Empty file\n");
    return;
  }

  SCL_printf("Buffer = %d\n", LIBNFSVIV_BufferSize);
  printf("Filenames as hex: %d\n", opt_filenameshex);
  if (request_file_idx)
    printf("Requested file idx = %d\n", request_file_idx);
  if ((request_file_name) && (request_file_name[0] != '\0'))
    printf("Requested file = %.*s\n", LIBNFSVIV_FilenameMaxLen - 1, request_file_name);

  if (vd->count_dir_entries_true > 0)
  {
    buf = (unsigned char *)malloc(bufsize * sizeof(*buf));
    if (!buf)
    {
      fprintf(stderr, "Cannot allocate memory\n");
      return;
    }

    for (i = 0; i < vd->count_dir_entries_true; ++i)
    {
      if (LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
        contents_size += vd->buffer[i].filesize;
    }

    /* Parse entire header */
    fseek(file, 0, SEEK_SET);
    if ((int)fread(buf, 1, bufsize, file) != bufsize)
    {
      fprintf(stderr, "File read error (print stats)\n");
      free(buf);
      fseek(file, 0, SEEK_END);
      return;
    }

#if SCL_DEBUG > 0
    if (opt_direnlenfixed >= 10)  assert(vd->viv_hdr_size_true == 0x10 + vd->count_dir_entries_true * opt_direnlenfixed);
#endif

    printf("\nPrinting archive directory:\n"
           "\n"
           "   id Valid       Offset          Gap         Size Len FnOf  Name\n"
           " ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n");
    printf("                       0                %10d           header\n"
           " ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n", vd->viv_hdr_size_true);

    for (i = 0; i < vd->count_dir_entries_true; ++i)
    {
      if (i > 0)  gap = vd->buffer[i].offset - vd->buffer[i - 1].offset - vd->buffer[i - 1].filesize;
      else  gap = vd->buffer[0].offset - vd->viv_hdr_size_true;

      /* integrity check... */
      if (vd->buffer[i].filename_ofs_ < 0 || vd->buffer[i].filename_len_ < 0 || vd->buffer[i].filename_ofs_ + vd->buffer[i].filename_len_ > bufsize)
      {
        fprintf(stderr, "invalid VivDirectory (entry %d)\n", i);
        break;
      }

      LIBNFSVIV_memcpyToString(filename, buf + vd->buffer[i].filename_ofs_, vd->buffer[i].filename_len_, LIBNFSVIV_FilenameMaxLen);
      if (opt_filenameshex)
        LIBNFSVIV_EncBase16(filename, vd->buffer[i].filename_len_);
      /* avoid printing non-UTF8 / non-printable string */
      sz = strlen(filename) + 1;
#ifdef UVTUTF8
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s \n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsUTF8String(buf + vd->buffer[i].filename_ofs_, sz, 1) > 0) ? filename : "<non-UTF8>");
#else
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s \n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsPrintString(buf + vd->buffer[i].filename_ofs_, sz, 1) > 0) ? filename : "<non-printable>");
#endif
    }

    printf(" ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n"
           "              %10d                %10d           %d files\n", vd->buffer[vd->count_dir_entries_true - 1].offset + vd->buffer[vd->count_dir_entries_true - 1].filesize, contents_size, vd->count_dir_entries_true);

    free(buf);
  }  /* if */

  fseek(file, 0, SEEK_END);
}

static
void LIBNFSVIV_PrintStatsEnc(VivDirectory * const vd,
                             char **infiles_paths, const int count_infiles,
                             const int opt_filenameshex, const int opt_faithfulencode)
{
  int gap;
  int i;
  int j;
  size_t sz;
  char *ptr;
#ifdef _WIN32
  char buf[LIBNFSVIV_FilenameMaxLen] = {0};
#endif

  SCL_printf("Buffer = %d\n", LIBNFSVIV_BufferSize);
  __LIBNFSVIV_PrintVivDirectoryStats_Header(vd);
  /* __LIBNFSVIV_PrintVivDirectoryStats_Parsed(vd); */  /* file not yet written */
  printf("Filenames as hex: %d\n", opt_filenameshex);
  if (opt_faithfulencode)
    printf("Faithful encoder: %d\n", opt_faithfulencode);

  if (vd->count_dir_entries > 0)
  {
    printf("\nPrinting archive directory:\n"
           "\n"
           "   id Valid       Offset          Gap         Size Len FnOf  Name\n"
           " ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n");
    printf("                       0                %10d           header\n"
           " ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n", vd->viv_hdr_size_true);

    for (i = 0, j = 0; i < count_infiles; ++j)
    {
      if (!opt_faithfulencode && !LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, j))
        continue;

      if (i > 0)  gap = vd->buffer[i].offset - vd->buffer[i - 1].offset - vd->buffer[i - 1].filesize;
      else  gap = vd->buffer[0].offset - vd->viv_hdr_size_true;

      /* avoid printing non-UTF8 / non-printable string */
#ifdef _WIN32
      sz = GetLongPathName(infiles_paths[i], buf, LIBNFSVIV_FilenameMaxLen);  /* transform short paths that contain tilde (~) */
      if (!sz || sz > sizeof(buf))  buf[0] = '\0';
      ++sz;
      ptr = buf;
#else
      sz = strlen(infiles_paths[i]) + 1;
      ptr = infiles_paths[i];
#endif
#ifdef UVTUTF8
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s \n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsUTF8String(ptr, sz, 1) > 0) ? LIBNFSVIV_GetPathBasename(ptr) : "<non-UTF8>");
#else
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s \n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsPrintString(ptr, sz, 1) > 0) ? LIBNFSVIV_GetPathBasename(ptr) : "<non-printable>");
#endif
      ++i;
    }
    printf(" ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n"
           "              %10d                %10d           %d files\n", vd->filesize, vd->filesize - vd->header_size, vd->count_dir_entries);
  }
}

/* internal: validate ------------------------------------------------------- */

static
int LIBNFSVIV_GetVivFileValidMinOffset(const VivDirectory *vd, const int filesize)
{
  int i;
  int min_ = filesize;
  for (i = 0; i < vd->count_dir_entries; ++i)
  {
    if (!LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
      continue;
    min_ = LIBNFSVIV_min(min_, vd->buffer[i].offset);
  }
  return min_;
}

static
int LIBNFSVIV_CheckVivHeader(const VivDirectory *vd, const int viv_filesize)
{
  int retv = 1;

  if(LIBNFSVIV_GetVivVersion_FromBuf(vd->format) <= 0)
  {
    fprintf(stderr, "CheckVivHeader: Format error (expects BIGF, BIGH, BIG4)\n");
    retv = 0;
  }

  if (vd->count_dir_entries < 0)
  {
    fprintf(stderr, "CheckVivHeader: Format error (number of directory entries < 0) %d\n", vd->count_dir_entries);
    retv = 0;
  }

  if (vd->count_dir_entries > LIBNFSVIV_DirEntrMax)
  {
    fprintf(stderr, "CheckVivHeader: Number of purported directory entries not supported and likely invalid (%d > %d)\n", vd->count_dir_entries, LIBNFSVIV_DirEntrMax);
    retv = 0;
  }

  if (vd->header_size > viv_filesize)
    fprintf(stderr, "Warning:CheckVivHeader: Format (headersize > filesize)\n");

  if (vd->header_size > vd->count_dir_entries * (8 + LIBNFSVIV_FilenameMaxLen) + 16)
    fprintf(stderr, "Warning:CheckVivHeader: Format (invalid headersize) (%d) %d\n", vd->header_size, vd->count_dir_entries);

  return retv;
}

static
int LIBNFSVIV_CheckVivDirectory(VivDirectory *vd, const int viv_filesize)
{
  int retv = 1;
  int contents_size = 0;
  int i;
  int minimal_ofs;

  if (vd->count_dir_entries != vd->count_dir_entries_true)
  {
    printf("Warning:CheckVivDirectory: incorrect number of archive directory entries in header (%d files listed, %d files found)\n", vd->count_dir_entries, vd->count_dir_entries_true);
  }

  /* :HS, :PU allow values greater than true value */
  if ((vd->count_dir_entries < 1) || (vd->count_dir_entries_true < 1))
  {
    printf("Warning:CheckVivDirectory: empty archive (%d files listed, %d files found)\n", vd->count_dir_entries, vd->count_dir_entries_true);
    return 1;
  }

  /* Validate file offsets, sum filesizes */
  for (i = 0; i < vd->count_dir_entries; ++i)
  {
    int ofs_now = vd->buffer[i].offset;

    if (!LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
      continue;

    if ((vd->buffer[i].filesize >= viv_filesize) ||
        (vd->buffer[i].filesize < 0))
    {
      printf("CheckVivDirectory: file %d invalid (filesize out of bounds) (%d ? %d)\n", i, vd->buffer[i].filesize, viv_filesize);
      LIBNFSVIV_SetBitmapFalse(vd->validity_bitmap, i);
    }
    if ((ofs_now < vd->viv_hdr_size_true) ||
        (ofs_now < vd->header_size) ||
        (ofs_now >= viv_filesize))
    {
      printf("CheckVivDirectory: file %d invalid (offset out of bounds) %d\n", i, ofs_now);
      LIBNFSVIV_SetBitmapFalse(vd->validity_bitmap, i);
    }
    if (ofs_now >= INT_MAX - vd->buffer[i].filesize)
    {
      printf("CheckVivDirectory: file %d invalid (offset overflow) %d\n", i, ofs_now);
      LIBNFSVIV_SetBitmapFalse(vd->validity_bitmap, i);
    }
    if ((ofs_now + vd->buffer[i].filesize > viv_filesize))
    {
      printf("CheckVivDirectory: file %d invalid (filesize from offset out of bounds) (%d+%d) > %d\n", i, ofs_now, vd->buffer[i].filesize, viv_filesize);
      LIBNFSVIV_SetBitmapFalse(vd->validity_bitmap, i);
    }

    if (LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) == 1)
      contents_size += vd->buffer[i].filesize;
    else
    {
      --vd->count_dir_entries_true;
      ++vd->null_count;
    }
  }  /* for i */

  minimal_ofs = LIBNFSVIV_GetVivFileValidMinOffset(vd, viv_filesize);
  if (vd->buffer[0].offset != minimal_ofs)
  {
    printf("Warning:CheckVivDirectory: smallest offset (%d) is not file 0\n", minimal_ofs);
  }

  /* Typically, should be equal. Smaller is allowed, as archives may have null-byte padding "gaps" between files.
     example: official DLC walm/car.viv  */
  if (minimal_ofs + contents_size > viv_filesize)
  {
    printf("Warning:CheckVivDirectory (valid archive directory filesizes sum too large: overlapping content?)\n");
  }

  /* :HS, :PU allow value greater than true value */
  if (vd->count_dir_entries != vd->count_dir_entries_true)
    printf("Warning:CheckVivDirectory (archive header has incorrect number of directory entries)\n");

  return retv;
}

/* decode ------------------------------------------------------------------- */

/*
  Clamp number of viv directory entries to be parsed to 0,max
  Check and potentially swap filesize endianness s.t. vd->filesize > 0
*/
static
void LIBNFSVIV_FixVivHeader(VivDirectory *vd, const int filesz)
{
  if (vd->count_dir_entries < 0)
  {
    printf("Warning:FixVivHeader: Format (invalid number of purported directory entries) (%d)(0x%x),\n", vd->count_dir_entries, vd->count_dir_entries);
    SCL_printf("FixVivHeader: 32 bit (%d)(0x%x) bitmask,\n", vd->count_dir_entries & 0x7FFFFFFF, vd->count_dir_entries & 0x7FFFFFFF);
    vd->count_dir_entries = LIBNFSVIV_min(vd->count_dir_entries & 0x7FFFFFFF, LIBNFSVIV_DirEntrMax);
    printf("Warning:FixVivHeader: assume %d entries\n", vd->count_dir_entries);
  }
  else if (vd->count_dir_entries > LIBNFSVIV_DirEntrMax)
  {
    printf("Warning:FixVivHeader: Format (unsupported number of purported directory entries) (%d)(0x%x),\n", vd->count_dir_entries, vd->count_dir_entries);
    vd->count_dir_entries = LIBNFSVIV_DirEntrMax;
    printf("assume %d entries\n", vd->count_dir_entries);
  }
  if (LIBNFSVIV_SwapEndian(vd->filesize) == filesz)
  {
    vd->filesize = filesz;
    vd->__padding[0] ^= 0x2;
  }
}

/* Assumes ftell(file) == 0
   Returns 1, if Viv header can be read. Else 0. */
static
int LIBNFSVIV_ReadVivHeader(VivDirectory *vd, FILE *file)
{
  int sz = 0;

  sz += (int)fread(vd->format, 1, 4, file);
  sz += (int)fread(&vd->filesize, 1, 4, file);
  sz += (int)fread(&vd->count_dir_entries, 1, 4, file);
  sz += (int)fread(&vd->header_size, 1, 4, file);

  if (sz != 16)
  {
    fprintf(stderr, "ReadVivHeader: File read error\n");
    return 0;
  }

  vd->__padding[0] = 0xC;
  if (strncmp(vd->format, "BIG4", 4))  /* BIG4 encodes filesize in little endian */
  {
    vd->filesize = LIBNFSVIV_SwapEndian(vd->filesize);
    vd->__padding[0] += 0x2;
  }
  vd->count_dir_entries = LIBNFSVIV_SwapEndian(vd->count_dir_entries);
  vd->header_size = LIBNFSVIV_SwapEndian(vd->header_size);

  return 1;
}

/* Assumes (vd).
Assumes (vd->count_dir_entries >= true value).
Assumes (vd->length == 0) && !(vd->buffer) && !(vd->validity_bitmap)
Returns boolean.

If (opt_direnlenfixed < 10) assumes variable length directory entries,
else assumes fixed length directory entries.

vd->count_dir_entries_true will be the number of entries parsed.
vd->viv_hdr_size_true will be the true unpadded header size.
*/
static
int LIBNFSVIV_ReadVivDirectory(VivDirectory *vd,
                        const int viv_filesize, FILE *file,
                        const int opt_verbose, const int opt_direnlenfixed,
                        const int opt_filenameshex)
{
  unsigned char buf[LIBNFSVIV_CircBuf_Size] = {0};  /* initialize in case (viv_filesize < buffer size) */
  int len = 0;
  int i;
  LIBNFSVIV_CircBuf cbuf;

  cbuf.buf = buf;
  cbuf.sz = sizeof(buf);
  cbuf.rd = 0;
  cbuf.wr = 0;

  vd->count_dir_entries_true = vd->count_dir_entries;
  vd->length =  LIBNFSVIV_ceil(vd->count_dir_entries, 2) * 2;  /* 2*sizeof(VivDirEntr) == 32 */
  if (!LIBNFSVIV_VivDirectory_Init(vd, vd->length))  /* 2*sizeof(VivDirEntr)==32 */
  {
    fprintf(stderr, "ReadVivDirectory: Cannot allocate memory\n");
    return 0;
  }
  vd->null_count = vd->count_dir_entries;
  vd->viv_hdr_size_true = 0x10;
  /* if (opt_verbose >= 1)
  {
    printf("Directory Entries (malloc'd): %d (ceil(x/64)=%d), Bitmap (malloc'd): %d, Padding: %d\n", vd->length, LIBNFSVIV_ceil(vd->length, 64), vd->length > LIBNFSVIV_VivDirectoryPaddingSize * 8, LIBNFSVIV_VivDirectoryPaddingSize);
  } */

  /*
    The following cases may occur when reading filenames from the directory:
      1. filename is printable string (isprint() == true)
      2. filename is string of UTF8 characters
      3. filename is string of length 1
      4. filename can contain any character (especially all nul's), can have embedded nul's, can lack nul-termination

    Handle cases:
      * variable length directory entries
        - allow cases 1. and 2. and 3.
      * fixed length directory entries (>= 10 bytes)
        - allow cases 3. and 4.

    Supported filename length is upper-bounded by (buffer size - 8).
    We are using a buffer size of 4096 + 16 to ensure that any valid
    UTF8-encoded filename can be handled.

    Directory size can be arbitrarily large. However, typical archive
    directories are well below 1024 bytes in length.
  */
  if (opt_direnlenfixed < 10)  /* variable length entry */
  {
    SCL_printf("  Read initial chunk\n");
    if (viv_filesize - (int)ftell(file) >= 10 && LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - 4) < 9)
    {
      fprintf(stderr, "ReadVivDirectory: File read error at %d\n", vd->viv_hdr_size_true);
      return 0;
    }

    for (i = 0; i < vd->count_dir_entries_true; ++i)
    {
      char valid = 1;
      int lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);

      SCL_printf("\n");
      SCL_printf("i: %d\n", i);
      SCL_printf("ftell(file): %d 0x%x\n", (int)ftell(file), (int)ftell(file));
      SCL_printf("vd->viv_hdr_size_true: %d\n", vd->viv_hdr_size_true);
      SCL_printf("cbuf stats: buf %p, !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", cbuf.buf, !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
      SCL_printf("lefttoread: %d\n", lefttoread);
      SCL_printf("readtoend: %d\n", LIBNFSVIV_CircBuf_readtoend(&cbuf));
      SCL_printf("memchr: %p\n", lefttoread > 0 ? LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread) : NULL);
      SCL_printf("memchr2: %p\n", LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread));
      /* SCL_debug_printbuf(cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr); */

      /**
        Read next chunk if:
          - less than 9 bytes left to read
          - no nul-termination found where filename is expected

        Read length is bounded by EOF (i.e., reading 0 bytes is valid)
      */
      if (lefttoread < 9 || !LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread))
      {
        SCL_printf("  Read next chunk\n");
        /* Returns 0 bytes if (ftell(file) == EOF) */
        if (LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - lefttoread) < 0)
        {
          fprintf(stderr, "ReadVivDirectory: File read error at %d\n", vd->viv_hdr_size_true);
          return 0;
        }

        lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);  /* zero if (cb->rd == cb->wr) */
        lefttoread = lefttoread > 0 ? lefttoread : (int)sizeof(buf);
        SCL_printf("  cbuf stats: buf %p, !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", cbuf.buf, !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_printf("  memchr: %p\n", LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread));
        /* SCL_debug_printbuf(cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr); */
      }

      /* Get next entry */

      /*
        Expect a string here (filename). If no string can be found, offset
        has progressed past the directory. Then the previous entry ended the
        directory, and FOR is ended.

        UVTUTF8-branch:
          check for UTF8-string
        Default mode:
          check for printable string
        opt_filenameshex mode:
          Output filenames will be Base16 (hexadecimal) encoded: check for string
      */

      /* Ensure nul-terminated */
      if (!LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread))
      {
        if (opt_verbose >= 1)
          fprintf(stderr, "Warning:ReadVivDirectory: Filename at %d not a string. Not a directory entry. Stop parsing directory.\n", vd->viv_hdr_size_true);

        vd->count_dir_entries_true = i;  /* breaks FOR loop */
        break;
      }

      vd->buffer[i].filename_len_ = 0;

      valid &= 4 == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].offset, 0, 4);
      SCL_printf("  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
      valid &= 4 == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].filesize, 0, 4);
      SCL_printf("  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
      vd->buffer[i].offset   = LIBNFSVIV_SwapEndian(vd->buffer[i].offset);
      vd->buffer[i].filesize = LIBNFSVIV_SwapEndian(vd->buffer[i].filesize);
      SCL_printf("valid: %d\n", valid);

      vd->viv_hdr_size_true += 0x08;
      vd->buffer[i].filename_ofs_ = vd->viv_hdr_size_true;

#if 0 && SCL_DEBUG > 0
      {
        unsigned char * const ptr = LIBNFSVIV_CircBuf_PeekPtr(&cbuf, 0);
        SCL_printf("ptr: %p\n", ptr);
        {
          int _readtoend = LIBNFSVIV_CircBuf_readtoend(&cbuf);
          unsigned char *_p = ptr;
          SCL_printf("ptr: '");
          while (!!_p && *_p != '\0' && _readtoend-- > 0)  SCL_printf("%c", *_p++);
          SCL_printf("'\n");
        }
      }
#endif

      SCL_printf("(!opt_filenameshex) %d\n", (!opt_filenameshex));
      if (!opt_filenameshex)
      {
#if defined(UVTUTF8)
        /* End if filename is not UTF8-string */
        char tmp_UTF8 = 0;
        LIBNFSVIV_CircBuf_Peek(&cbuf, &tmp_UTF8, 0, 1);
        SCL_printf("tmp_UTF8: %c\n", tmp_UTF8);
        len = LIBNFSVIV_CircBuf_PeekIsUTF8(&cbuf, 0, LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        vd->buffer[i].filename_len_ = len;
        ++len;
        LIBNFSVIV_CircBuf_Fwd(&cbuf, len);
        SCL_printf("len: %d (0x%x)\n", len, len);
        SCL_printf(":  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_printf("vd: offset: 0x%x\n", vd->buffer[i].offset);
        SCL_printf("vd: filesize: 0x%x\n", vd->buffer[i].filesize);
        SCL_printf("vd->buffer[i] stats: filename_ofs_ 0x%x, filename_len_ 0x%x (next 0x%x)\n", vd->buffer[i].filename_ofs_, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_ + vd->buffer[i].filename_len_);
        if (!isprint(tmp_UTF8) && (len < 2))
#else
        /* End if filename is not printable string
           very crude check as, e.g., CJK characters end the loop */
        len = LIBNFSVIV_CircBuf_PeekIsPrint(&cbuf, 0, LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        ++len;
        LIBNFSVIV_CircBuf_Fwd(&cbuf, len);
        vd->buffer[i].filename_len_ = len;
        if (len < 2)
#endif
        {
          vd->viv_hdr_size_true -= 0x08;
          vd->count_dir_entries_true = i;  /* breaks while-loop */
          break;
        }
      }
      else  /* filenames as hex */
      {
        len = LIBNFSVIV_CircBuf_PeekStrlen(&cbuf, 0, LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        vd->buffer[i].filename_len_ = len;
        ++len;
        LIBNFSVIV_CircBuf_Fwd(&cbuf, len);
        SCL_printf("len: %d (0x%x)\n", len, len);
        SCL_printf(":  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_printf("vd: offset: 0x%x\n", vd->buffer[i].offset);
        SCL_printf("vd: filesize: 0x%x\n", vd->buffer[i].filesize);
        SCL_printf("vd->buffer[i] stats: filename_ofs_ 0x%x, filename_len_ 0x%x (next 0x%x)\n", vd->buffer[i].filename_ofs_, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_ + vd->buffer[i].filename_len_);
      }

      vd->viv_hdr_size_true += len;
      valid &= (len <= LIBNFSVIV_FilenameMaxLen);

      if (valid == 1)
      {
        LIBNFSVIV_SetBitmapTrue(vd->validity_bitmap, i);
        --vd->null_count;
      }
    }  /* for i */
  }
  else  /* fixed length entry */
  {
    if (opt_direnlenfixed >= (int)sizeof(buf))
    {
      fprintf(stderr, "ReadVivDirectory: fixed directory entry length too large for buffer size (%d > %d)\n", opt_direnlenfixed, (int)sizeof(buf));
      return 0;
    }

    SCL_printf("  Read initial chunk\n");
    if (viv_filesize - (int)ftell(file) >= 10 && LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - 4) < opt_direnlenfixed)
    {
      fprintf(stderr, "ReadVivDirectory: File read error at %d\n", vd->viv_hdr_size_true);
      return 0;
    }
    SCL_printf("\n");

    for (i = 0; i < vd->count_dir_entries_true; ++i)
    {
      char valid = 1;
      int lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);

      /* update buffer */
      if (lefttoread < opt_direnlenfixed)
      {
        SCL_printf("  Read next chunk\n");
        /* Returns 0 bytes if (ftell(file) == EOF) */
        if (LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - lefttoread) < 0)
        {
          fprintf(stderr, "ReadVivDirectory: File read error at %d\n", vd->viv_hdr_size_true);
          return 0;
        }

        lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);
        lefttoread = lefttoread > 0 ? lefttoread : (int)sizeof(buf);
      }

      /* Get next entry */

      /* Ensure fixed length (int)opt_direnlenfixed is available */
      if (lefttoread < opt_direnlenfixed)
      {
        if (opt_verbose >= 1)
          fprintf(stderr, "Warning:ReadVivDirectory: Filename at %d not a string. Not a directory entry. Stop parsing directory.\n", vd->viv_hdr_size_true);

        vd->count_dir_entries_true = i;  /* breaks FOR loop */
        break;
      }

      vd->buffer[i].filename_len_ = 0;

      valid &= 4 == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].offset, 0, 4);
      valid &= 4 == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].filesize, 0, 4);
      vd->buffer[i].offset   = LIBNFSVIV_SwapEndian(vd->buffer[i].offset);
      vd->buffer[i].filesize = LIBNFSVIV_SwapEndian(vd->buffer[i].filesize);

      vd->viv_hdr_size_true += 0x08;
      vd->buffer[i].filename_ofs_ = vd->viv_hdr_size_true;

      if (opt_filenameshex)  /* filenames as hex */
      {
        /*
          Find last non-nul byte for name.
          Accepts embedded/leading nul's and missing terminating nul.
        */
        {
          int len_ = opt_direnlenfixed - 0x08;
          unsigned char buf_[sizeof(buf)];
          const unsigned char *p_;
          LIBNFSVIV_CircBuf_Peek(&cbuf, buf_, 0, len_);
          p_ = buf_ + len_ - 1;  /* last byte */
          while (*p_-- == '\0' && len_ > 0)
            --len_;
          vd->buffer[i].filename_len_ = len_;
        }
      }
      else
      {
        /** fixed length entries with printable filenames are not known to exist */
        fprintf(stderr, "ReadVivDirectory: Not implemented. Try with filenames as hex.\n");
        return 0;
      }

      vd->viv_hdr_size_true += opt_direnlenfixed - 0x08;
      LIBNFSVIV_CircBuf_Fwd(&cbuf, opt_direnlenfixed - 0x08);
      valid &= (len <= LIBNFSVIV_FilenameMaxLen);

      if (valid == 1)
      {
        LIBNFSVIV_SetBitmapTrue(vd->validity_bitmap, i);
        --vd->null_count;
      }
    }  /* for i */
  }

  #if SCL_DEBUG > 0
    LIBNFSVIV_PrintVivDirEntr(vd, 0);
  #endif

  return 1;
}

/*
  Extracts the described file to disk. Returns boolean.
  Assumes (vde), (infile). If (wenc_file), assumes (wenc_outpath).
*/
static
int LIBNFSVIV_VivExtractFile(const VivDirEntr * const vde, FILE *infile,
                             const int opt_filenameshex, const int opt_overwrite,
                             FILE *wenc_file, const char * const wenc_outpath)
{
  char buf[LIBNFSVIV_BufferSize] = {0};
  int retv = 1;
  FILE *outfile;

  /* Read outfilename to buf */
  if (LIBNFSVIV_FreadToStr(buf, sizeof(buf), vde->filename_ofs_, vde->filename_len_, infile) < 0)
  {
    fprintf(stderr, "VivExtractFile: File read error at %d (extract outfilename)\n", vde->filename_ofs_);
    return 0;
  }

  if (opt_filenameshex)  /* Option: Encode outfilename to Base16 */
    LIBNFSVIV_EncBase16(buf, vde->filename_len_);

  /* Create outfile */
  if (LIBNFSVIV_IsFile(buf))  /* overwrite mode: for existing files and duplicated filenames in archive */
  {
    if (opt_overwrite == 1)  /* attempt renaming existing file, return on failure */
    {
      if (!LIBNFSVIV_IncrementFile(buf, (int)sizeof(buf), 1))
        return 0;
    }
    else
    {
      fprintf(stderr, "Warning:VivExtractFile: Attempt overwriting existing '%s' (duplicated filename?)\n", buf);
    }
  }
  outfile = fopen(buf, "wb");
  if (!outfile)
  {
    fprintf(stderr, "VivExtractFile: Cannot create output file '%s'\n", buf);
    return 0;
  }

  if (wenc_file)  /* Option: Write re-Encode command to file */
  {
    fprintf(wenc_file, " \"%s/%s\"", wenc_outpath, buf);
    fflush(wenc_file);
  }

  /* Extract */
  memset(buf, 0, sizeof(buf));
  fseek(infile, vde->offset, SEEK_SET);
  retv &= LIBNFSVIV_FileCopy(outfile, infile, vde->filesize, buf, (int)sizeof(buf));
  fclose(outfile);
  return retv;
}

/** Assumes (request_file_name) and request_file_name is string.
    Returns 1-based directory entry index of given filename,
    -1 if it does not exist, 0 on error. **/
static
int LIBNFSVIV_GetIdxFromFname(const VivDirectory *vd, FILE* infile, const char *request_file_name)
{
  char buf[LIBNFSVIV_FilenameMaxLen];
  const int len = (int)strlen(request_file_name);
  int i;

  if (len + 1 > LIBNFSVIV_FilenameMaxLen)
  {
    fprintf(stderr, "GetIdxFromFname: Requested filename is too long\n");
    return 0;
  }

  for (i = 0; i < vd->count_dir_entries; ++i)
  {
    if (len == vd->buffer[i].filename_len_)
    {
      if (len != LIBNFSVIV_FreadToStr(buf, len + 1, vd->buffer[i].filename_ofs_, vd->buffer[i].filename_len_, infile))
        fprintf(stderr, "GetIdxFromFname: File read error at 0x%x\n", vd->buffer[i].filename_ofs_);
      if (!strncmp(buf, request_file_name, len))
        return i + 1;
    }
  }

  fprintf(stderr, "GetIdxFromFname: Cannot find requested file in archive\n");
  return -1;
}

/* internal: encode --------------------------------------------------------- */

/* Assumes length of opt_requestfmt >= 4 */
static
int LIBNFSVIV_SetVivDirHeader(VivDirectory *vd,
                              char **infiles_paths, const int count_infiles,
                              const char *opt_requestfmt, const int opt_requestendian,
                              const int opt_direnlenfixed,
                              const int opt_filenameshex,
                              const int opt_faithfulencode)
{
  int retv = 1;
  int curr_offset;
  int len_filename;  /* string length including nul */
  int i;
#ifdef _WIN32
  char buf[LIBNFSVIV_FilenameMaxLen];
#endif

  if (!opt_requestfmt || LIBNFSVIV_GetVivVersion_FromBuf(opt_requestfmt) <= 0)
  {
    fprintf(stderr, "SetVivDirHeader: Invalid format (expects 'BIGF', 'BIGH' or 'BIG4')\n");
    return 0;
  }

  if (!LIBNFSVIV_VivDirectory_Init(vd, LIBNFSVIV_ceil(count_infiles, 2) * 2))  /* 2*sizeof(VivDirEntr)==32 */
  {
    fprintf(stderr, "SetVivDirHeader: Cannot allocate memory\n");
    return 0;
  }

  curr_offset = 16;
  vd->filesize = 0;

#if SCL_DEBUG >= 1
  assert(vd->length >= count_infiles);
#endif

  /**
    Create VivDirectory based on filelist, even for invalid files.

    opt_faithfulencode == 0:  directory entries for invalid files do not count towards offsets
    opt_faithfulencode != 0:  directory entries for invalid files do count towards offsets (pretend all is well, set filesize 0)
  */
  for (i = 0; i < count_infiles; ++i)
  {
    SCL_printf("i: %d\n", i);
    /* If invalid, pretend that the file is there but has length 0. */
    if (!LIBNFSVIV_IsFile(infiles_paths[i]) || LIBNFSVIV_IsDir(infiles_paths[i]))
    {
      LIBNFSVIV_SetBitmapFalse(vd->validity_bitmap, i);
      ++vd->null_count;
      vd->buffer[i].filesize = 0;
      if (!opt_faithfulencode)
      {
        fprintf(stderr, "SetVivDirHeader: Invalid file. Skipping '%s'\n", infiles_paths[i]);
        continue;
      }
      else
        printf("Warning:SetVivDirHeader: Invalid file. Skipping '%s'\n", infiles_paths[i]);
    }
    else
    {
      LIBNFSVIV_SetBitmapTrue(vd->validity_bitmap, i);
      ++vd->count_dir_entries_true;
      vd->buffer[i].filesize = LIBNFSVIV_GetFilesize(infiles_paths[i]);
      vd->filesize += vd->buffer[i].filesize;
    }

#ifdef _WIN32
    len_filename = (int)GetLongPathName(infiles_paths[i], buf, LIBNFSVIV_FilenameMaxLen) + 1;  /* transform short paths that contain tilde (~) */
    if (len_filename < 2 || len_filename > LIBNFSVIV_FilenameMaxLen)
      printf("Warning:SetVivDirHeader: Cannot get long path name for file '%s' (len_filename=%d)\n", infiles_paths[i], (int)strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1);
    else
      len_filename = (int)strlen(LIBNFSVIV_GetPathBasename(buf)) + 1;
#else
    len_filename = (int)strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1;
#endif

    len_filename = LIBNFSVIV_clamp(len_filename, 1, LIBNFSVIV_FilenameMaxLen);
    if (opt_filenameshex)
      len_filename = len_filename / 2 + len_filename % 2;
    vd->buffer[i].filename_len_ = len_filename - 1;

    curr_offset += 0x8;
    vd->buffer[i].filename_ofs_ = curr_offset;
    curr_offset += len_filename;

    SCL_printf("curr_offset: %d\n", curr_offset);
    SCL_printf("vd offset: %d\n", vd->buffer[i].offset);
    SCL_printf("vd filesize: %d\n", vd->buffer[i].filesize);
    SCL_printf("vd filename_ofs_: 0x%x\n", vd->buffer[i].filename_ofs_);
    SCL_printf("vd filename_len_: %d\n", vd->buffer[i].filename_len_);

    if (opt_direnlenfixed > 10 && len_filename <= opt_direnlenfixed)
      curr_offset += opt_direnlenfixed - len_filename - 0x8;
  }

  vd->buffer[0].offset = curr_offset;
  for (i = 1; i < vd->length; ++i)
    vd->buffer[i].offset = vd->buffer[i - 1].offset + vd->buffer[i - 1].filesize;

  memcpy(vd->format, opt_requestfmt, 4);
  vd->__padding[0] |= opt_requestendian & 0xf;  /* archive header endianess */
  vd->filesize += curr_offset;
  vd->count_dir_entries = vd->count_dir_entries_true;
  vd->header_size = curr_offset;
  vd->viv_hdr_size_true = vd->header_size;

#if SCL_DEBUG > 0
    printf("debug:LIBNFSVIV_SetVivDirHeader: offsets ");
    for (i = 0; i < count_infiles; ++i)
      printf("%d ", vd->buffer[i].offset);
    printf("/%d (curr_offset: %d)\n", vd->count_dir_entries_true, curr_offset);
#endif

#if SCL_DEBUG > 0
    printf("debug:LIBNFSVIV_SetVivDirHeader: input paths validity ");
    for (i = 0; i < count_infiles; ++i)
      printf("%d ", LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i));
    printf("/%d (null count: %d)\n", vd->count_dir_entries_true, vd->null_count);
#endif

  return retv;
}

static
int LIBNFSVIV_WriteVivHeader(VivDirectory *vd, FILE *file)
{
  size_t err = 0;
  int buf[3];

  if (LIBNFSVIV_GetBitmapValue(&vd->__padding[0], 1))
    buf[0] = LIBNFSVIV_SwapEndian(vd->filesize);
  if (LIBNFSVIV_GetBitmapValue(&vd->__padding[0], 2))
    buf[1] = LIBNFSVIV_SwapEndian(vd->count_dir_entries);
  if (LIBNFSVIV_GetBitmapValue(&vd->__padding[0], 3))
    buf[2] = LIBNFSVIV_SwapEndian(vd->header_size);

  err += fwrite(vd->format, 1, 4, file);
  err += fwrite(&buf[0], 1, 4, file);
  err += fwrite(&buf[1], 1, 4, file);
  err += fwrite(&buf[2], 1, 4, file);

  return err == 16;
}

/*
  Assumes (ftell(file) == 16).
  Updates vd->viv_hdr_size_true to ftell(file).
  Allows writing broken headers with (opt_faithfulencode != 0)
*/
static
int LIBNFSVIV_WriteVivDirectory(VivDirectory *vd, FILE *file,
                                char **infiles_paths, const int count_infiles,
                                const int opt_direnlenfixed, const int opt_filenameshex,
                                const int opt_faithfulencode)
{
  int val;
  char buf[LIBNFSVIV_FilenameMaxLen] = {0};
  int len;  /* strlen() of viv directory entry filename  */
  int i;
  size_t err = 0x10;  /* track VivDirectory written length */

  for (i = 0; i < count_infiles; ++i)
  {
    if (!LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) && !opt_faithfulencode)
      continue;

    val = LIBNFSVIV_SwapEndian(vd->buffer[i].offset);
    err += fwrite(&val, 1, 4, file);

    val = LIBNFSVIV_SwapEndian(vd->buffer[i].filesize);
    err += fwrite(&val, 1, 4, file);

#ifdef _WIN32
    /*
      transform short paths that contain tilde (~)
      GetLongPathName() returns 0 on error, otherwise strlen(). For the moment,
    */
    len = (int)GetLongPathName(infiles_paths[i], buf, LIBNFSVIV_FilenameMaxLen);
    if (strlen(infiles_paths[i]) > 0 && (len == 0 || len > LIBNFSVIV_FilenameMaxLen))
    {
      printf("Warning:WriteVivDirectory: Cannot get long path name for file '%s' (len=%d)\n", infiles_paths[i], (int)strlen(infiles_paths[i]));
      break;
    }
    buf[len] = '\0';
    {
      char *ptr = LIBNFSVIV_GetPathBasename(buf);
      len = (int)strlen(ptr);
      memmove(buf, ptr, len + 1);
    }
#else
    len = (int)strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i]));
    if (len > LIBNFSVIV_FilenameMaxLen - 1)
    {
      fprintf(stderr, "WriteVivDirectory: infile basename length incompatible (%d)\n", len);
      return 0;
    }
    memcpy(buf, LIBNFSVIV_GetPathBasename(infiles_paths[i]), len + 1);
#endif

    if (opt_filenameshex)
    {
      len = LIBNFSVIV_DecBase16(buf);
      if (len != vd->buffer[i].filename_len_)
        printf("Warning:WriteVivDirectory: Base16 conversion mishap (%d!=%d)\n", len, vd->buffer[i].filename_len_);
    }

    if (opt_direnlenfixed >= 10 && len > opt_direnlenfixed - 0x8)
    {
      printf("Warning:WriteVivDirectory: Filename too long. Trim to fixed directory entry length (%d > %d).\n", len, opt_direnlenfixed);
      len = opt_direnlenfixed - 0x8;
    }

    err += len * (len == (int)fwrite(buf, 1, len, file));

    if (opt_direnlenfixed < 10)
      err += '\0' == fputc('\0', file);
    else
    {
      while (len++ < opt_direnlenfixed)
        err += '\0' == fputc('\0', file);
    }
  }  /* for i */

  vd->viv_hdr_size_true = (int)ftell(file);
  return (size_t)ftell(file) == err;
}

/*
  Copies LEN bytes from offset INFILE_OFS, of SRC to DEST.
  Returns ftell(DEST) on success, -1 on failure.

  If (!SRC), opens/closes SRC at infile_path.
*/
static
int LIBNFSVIV_VivWriteFile(FILE *dest, FILE *src, const char * const infile_path, const int infile_ofs, const int len)
{
  int retv = 1;
  char buf[LIBNFSVIV_BufferSize];
  if (src && infile_path)  return -1;
  else if (!src && infile_path)
    src = fopen(infile_path, "rb");
  if (!src)
  {
    fprintf(stderr, "VivWriteFile: Cannot open file '%s' (src)\n", infile_path);
    return -1;
  }
  fseek(src, LIBNFSVIV_max(0, infile_ofs), SEEK_SET);
  retv &= LIBNFSVIV_FileCopy(dest, src, len, buf, (int)sizeof(buf));
  if (infile_path)  fclose(src);
  return (retv) ? (int)ftell(dest) : -1;
}


/* api ---------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* Assumes (sz >= 4), need not nul-terminated. Returns string (char[5]) or NULL. */
const char *LIBNFSVIV_GetVivVersionString(const char *format)
{
  if (!format)
    return NULL;
  if (strncmp(format, "BIGF", 4) == 0)
    return "BIGF";
  if (strncmp(format, "BIGH", 4) == 0)
    return "BIGH";
  if (strncmp(format, "BIG4", 4) == 0)
    return "BIG4";
  return NULL;
}

/*
  Returns 7 (BIGF), 8 (BIGH), 4 (BIG4), 0 (refpack), -1 (unknown format)
  Assumes (sz >= 4).

  Attn: Return values differ from LIBNFSVIV_GetVivVersion_FromPath().
*/
int LIBNFSVIV_GetVivVersion_FromBuf(const char * const buf)
{
  const unsigned char rp[] = { 0x10, 0xfb };
  if (strncmp(buf, "BIGF", 4) == 0)
    return 7;
  if (strncmp(buf, "BIGH", 4) == 0)
    return 8;
  if (strncmp(buf, "BIG4", 4) == 0)
    return 4;
  if (memcmp(buf, rp, 2) == 0)
    return 0;
  return -1;
}

/*
  Returns 7 (BIGF), 8 (BIGH), 4 (BIG4), -7|-8|-4 (refpack'd BIGF|BIGH|BIG4), -1 (unknown format), 0 (fread error)

  Attn: Return values differ from LIBNFSVIV_GetVivVersion_FromBuf().
*/
int LIBNFSVIV_GetVivVersion_FromPath(const char * const path)
{
  int retv = 0;
  FILE *file = fopen(path, "rb");
  if (file)
  {
    char buf[16];
    if (LIBNFSVIV_GetFilesize(path) < 16)
      retv = -1;
    else if (fread(&buf, 1, 16, file) == 16)
    {
      retv = LIBNFSVIV_GetVivVersion_FromBuf(buf);
      if (retv == 0)  /* refpack */
      {
        retv = LIBNFSVIV_GetVivVersion_FromBuf(buf + 6);
        retv = retv > 0 ? -retv : -1;
      }
    }
    fclose(file);
  }
  return retv;
}

/* Assumes (dst) && (ending). Returns 1 if dst + ending fits in dst, 0 otherwise. */
int LIBNFSVIV_AppendFileEnding(char *dst, const size_t dst_sz, const char *ending)
{
  const size_t ofs = strlen(dst);
  const size_t ending_sz = strlen(ending);
  if (ofs + ending_sz < dst_sz)
  {
    memcpy(dst + ofs, ending, ending_sz + 1);
    return 1;
  }
  return 0;
}

/*
  The following functions can be used for data analysis.
  These functions are made available in the Python module, but can be used anywhere else.

  Usage:
    1. Get the VIV directory either by calling LIBNFSVIV_GetVivDirectoryFromFile() or LIBNFSVIV_GetVivDirectoryFromPath()
    2. To get the list of files in the VIV archive,
      call LIBNFSVIV_VivDirectoryToFileList_FromFile() or LIBNFSVIV_VivDirectoryToFileList_FromPath()
*/

/* Assumes (ftell(file) == 0) */
VivDirectory *LIBNFSVIV_GetVivDirectory_FromFile(VivDirectory *vd, FILE *file, const int filesz,
                                                 const int opt_verbose, const int opt_direnlenfixed,
                                                 const int opt_filenameshex,
                                                 const int __print_stats_if_verbose)
{
  VivDirectory *retv = NULL;
  for (;;)
  {
    if (!vd || !file)
    {
      break;
    }
    if (filesz < 16)
    {
      fprintf(stderr, "Format error (invalid filesize) %d\n", filesz);
      break;
    }
    if (!LIBNFSVIV_ReadVivHeader(vd, file))
      break;
#ifdef UVT_UNVIVTOOLMODULE
    if (opt_verbose)
    {
#endif
    __LIBNFSVIV_PrintVivDirectoryStats_Header(vd);
#ifdef UVT_UNVIVTOOLMODULE
    }
#endif
    LIBNFSVIV_FixVivHeader(vd, filesz);
    if (!LIBNFSVIV_CheckVivHeader(vd, filesz))
      break;
    if (!LIBNFSVIV_ReadVivDirectory(vd, filesz, file, opt_verbose, opt_direnlenfixed, opt_filenameshex))
      break;
    if (!LIBNFSVIV_CheckVivDirectory(vd, filesz))
    {
      LIBNFSVIV_PrintVivDirEntr(vd, 0);
      break;
    }
#ifdef UVT_UNVIVTOOLMODULE
    if (opt_verbose)
    {
#endif
    __LIBNFSVIV_PrintVivDirectoryStats_Parsed(vd);
#ifdef UVT_UNVIVTOOLMODULE
    }
#endif
    if (__print_stats_if_verbose && opt_verbose)
    {
      LIBNFSVIV_PrintStatsDec(vd, file, 0, NULL, opt_direnlenfixed, opt_filenameshex);
    }

    retv = vd;
    break;
  }
  return retv;
}

/* Wrapper for LIBNFSVIV_GetVivDirectory_FromFile() */
VivDirectory *LIBNFSVIV_GetVivDirectory(VivDirectory *vd, char * const path,
                                        const int opt_verbose, const int opt_direnlenfixed,
                                        const int opt_filenameshex,
                                        const int __print_stats_if_verbose)
{
  const int filesz = LIBNFSVIV_GetFilesize(path);
  FILE *file = path ? fopen(path, "rb") : NULL;
  VivDirectory *ret = LIBNFSVIV_GetVivDirectory_FromFile(vd, file, filesz, opt_verbose, opt_direnlenfixed, opt_filenameshex, __print_stats_if_verbose);
  if (file)  fclose(file);
  return ret;
}


/* Returns 0 on success. */
static
int LIBNFSVIV_ValidateVivDirectory(const VivDirectory * const vd)
{
  int err = 0;
  if (!vd)  err |= 1 << 0;
  if (vd->count_dir_entries < 0 || vd->count_dir_entries > vd->length)  err |= 1 << 1;
  if (vd->count_dir_entries_true < 0 || vd->count_dir_entries_true > vd->length)  err |= 1 << 2;
  /* if (vd->filesize < 0)  err |= 1 << 3; */  /* irrelevant */
  /* if (vd->header_size < 16)  err |= 1 << 4; */  /* irrelevant */
  if (vd->viv_hdr_size_true < 16)  err |= 1 << 5;
  /* if (vd->viv_hdr_size_true > vd->filesize)  err |= 1 << 6; */  /* irrelevant */
  /* if (vd->viv_hdr_size_true > vd->header_size)  err |= 1 << 7; */  /* irrelevant */
  if (vd->count_dir_entries - vd->count_dir_entries_true != vd->null_count)  err |= 1 << 8;
  SCL_printf("LIBNFSVIV_ValidateVivDirectory: 0x%x\n", err);
  return err;
}

/*
  Returns NULL on unsuccesful malloc.
  Returns char **filelist, an array of char* arrays. Returned filenames may be non-printable and may have embedded nul's. Consult VivDirectory for the filenames lengths.

  The first element is a contiguous block of all filenames. The rest are pointers to the start of each filename.
  Consumers must call free(*filelist), then free(filelist).

  From (filelist != NULL) follows (sz > 0).
  The number of list elements is (sz-1), the last array element filelist[sz-1] is NULL.

  NB: filelist and *filelist malloc'd sizes are upper-bounded, see libnfsviv.h header
*/
char **LIBNFSVIV_VivDirectoryToFileList_FromFile(VivDirectory *vd, FILE *file, const int opt_invalidentries)
{
  char **ret = NULL;
  if (LIBNFSVIV_ValidateVivDirectory(vd))  return NULL;

  if (!file)  return NULL;

  for (;;)
  {
    char **filelist;
    int filelist_len = !opt_invalidentries ? vd->count_dir_entries_true : vd->count_dir_entries;
    filelist = (char **)malloc((filelist_len + 1) * sizeof(*filelist));
    if (!filelist)
    {
      fprintf(stderr, "VivDirectoryToFileList: Cannot allocate memory\n");
      break;
    }
    filelist[filelist_len] = NULL;
    SCL_printf("VivDirectoryToFileList_FromFile: filelist_len %d\n", filelist_len);

    /* Create list of all filenames, even invalid ones.
       All values are clamped s.t. they do not exceed file size.

       Expects strings of length 1 at least
    */
    if (vd->count_dir_entries_true > 0 || (opt_invalidentries && vd->count_dir_entries > 0))
    {
      int filenames_sz;
      filenames_sz = LIBNFSVIV_SumVivDirectoryFilenameSizes(vd, opt_invalidentries);  /* guarantees (filenames_sz >= vd->count_dir_entries_true) */
      *filelist = (char *)calloc(filenames_sz * sizeof(**filelist), 1);
      if (!*filelist)
      {
        fprintf(stderr, "VivDirectoryToFileList: Cannot allocate memory\n");
        free(filelist);
        break;
      }
      {
        int i;
        int cnt_entries;
        char *p = *filelist;
        SCL_printf("!  p: %p, *filelist: %p, last: %p\n", p, *filelist, *filelist + (filenames_sz-1));
        for (i = 0, cnt_entries = 0; i < vd->count_dir_entries; ++i)
        {
          if (!opt_invalidentries && LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) == 0)
            continue;
          SCL_assert(p <= *filelist + filenames_sz);
          if (p > *filelist + filenames_sz)
          {
            fprintf(stderr, "VivDirectoryToFileList: buffer overflow\n");
            free(*filelist);
            free(filelist);
            return NULL;
          }
          filelist[cnt_entries] = p;
          if (LIBNFSVIV_FreadToStr(p, vd->buffer[i].filename_len_ + 1, vd->buffer[i].filename_ofs_, vd->buffer[i].filename_len_, file) != vd->buffer[i].filename_len_)
          {
            fprintf(stderr, "VivDirectoryToFileList: File read error at %d\n", vd->buffer[i].filename_ofs_);
            free(*filelist);
            free(filelist);
            return NULL;
          }
          p += vd->buffer[i].filename_len_ + 1;
          ++cnt_entries;
        }
        SCL_assert(opt_invalidentries || cnt_entries == vd->count_dir_entries_true);
        SCL_assert(p == *filelist + filenames_sz);
        SCL_printf(".cnt_entries %d, vd->count_dir_entries_true %d, opt_invalidentries: %d\n", cnt_entries, vd->count_dir_entries_true, opt_invalidentries);
        SCL_printf("filenames_sz %d\n", filenames_sz);
        SCL_printf(".p: %p, *filelist: %p, last: %p\n", p, *filelist, *filelist + filenames_sz);
#ifdef UVT_UNVIVTOOLMODULE
        if ((!opt_invalidentries && cnt_entries != vd->count_dir_entries_true) || p != *filelist + filenames_sz)
        {
          fprintf(stderr, "VivDirectoryToFileList: buffer overflow or incorrect count\n");
          fflush(0);
          free(*filelist);
          free(filelist);
          return NULL;
        }
#endif
      }
    }  /* if (vd->count_dir_entries_true > 0) */

    ret = filelist;
    break;
  }  /* for (;;) */

  return ret;
}

/* Wrapper for LIBNFSVIV_VivDirectoryToFileList_FromFile() */
char **LIBNFSVIV_VivDirectoryToFileList(VivDirectory *vd, char *path, const int opt_invalidentries)
{
  FILE *file = path ? fopen(path, "rb") : NULL;
  char **ret = LIBNFSVIV_VivDirectoryToFileList_FromFile(vd, file, opt_invalidentries);
  if (file)  fclose(file);
  return ret;
}


/* LIBNFSVIV_Unviv() and LIBNFSVIV_Viv() are one-and-done functions */

/*
  Assumes viv_name and outpath are NOT const's and have size >= LIBNFSVIV_FilenameMaxLen
  Assumes (viv_name). Assumes (outpath). Overwrites directory 'outpath'.
  Changes working directory to 'outpath'.

  (request_file_idx == 0), extract all
  (request_file_idx > 0), extract file at given 1-based index.
  (!request_file_name), extract file with given name. Overrides 'request_file_idx'.
  (opt_direnlenfixed < 10), do not impose fixed directory length
  (opt_filenameshex != 0), for nonprintable character in filenames, decode/encode to/from Filenames in Hexadecimal
  (opt_wenccommand != 0), write re-Encode command to path/to/input.viv.txt (keep files in order)
  (opt_overwrite < 1), overwrite existing files
  (opt_overwrite == 1), rename existing files
*/
int LIBNFSVIV_Unviv(char *viv_name, char *outpath,
                    int request_file_idx, const char *request_file_name,
                    const int opt_dryrun, const int opt_verbose,
                    const int opt_direnlenfixed, const int opt_filenameshex,
                    const int opt_wenccommand, const int opt_overwrite)
{
  int retv = 0;
  FILE *file = NULL;
  int viv_filesize;
  VivDirectory vd = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };
  int i = 0;
  int count_extracted = 0;
  char *wenc_buf = NULL;
  FILE *wenc_f = NULL;
  const int local_opt_filenameshex = (opt_filenameshex || (opt_direnlenfixed >= 10));  /* fixed length entries with all-printable names are not known to exist */

  SCL_printf(
    "request_file_idx %d\n"
    "opt_dryrun %d\n"
    "opt_verbose %d\n"
    "opt_direnlenfixed %d\n"
    "opt_filenameshex %d\n"
    "opt_wenccommand %d\n"
    "opt_overwrite %d\n"
    , request_file_idx, opt_dryrun, opt_verbose, opt_direnlenfixed, opt_filenameshex,
    opt_wenccommand, opt_overwrite);
  SCL_printf("local_opt_filenameshex %d\n", local_opt_filenameshex);

  if (opt_dryrun)
    printf("Begin dry run\n");

  for (;;)
  {
#ifdef _WIN32
      if (!LIBNFSVIV_IsFile(viv_name) || !LIBNFSVIV_GetFullPathName(viv_name, NULL, 0, NULL))
#else
      if (!LIBNFSVIV_GetFullPathName(viv_name, NULL))  /* returns NULL if path does not exist */
#endif
    {
      fprintf(stderr, "Unviv: Cannot get full path of archive.\n");
      retv = -1;
      break;
    }

    if (!opt_dryrun)
    {
      if (!LIBNFSVIV_IsDir(outpath))
      {
        printf("Unviv: Attempt creating directory '%s'\n", outpath);
        if (mkdir(outpath, 0755) != 0)
        {
          fprintf(stderr, "Unviv: Cannot create directory '%s'\n", outpath);
          retv = -1;
          break;
        }
      }

#ifdef _WIN32
      if (!LIBNFSVIV_GetFullPathName(outpath, NULL, 0, NULL))
#else
      if (!LIBNFSVIV_GetFullPathName(outpath, NULL))
#endif
      {
        fprintf(stderr, "Unviv: Cannot get full path of outpath.\n");
        retv = -1;
        break;
      }

      if (opt_wenccommand)
      {
        wenc_buf = (char *)malloc(LIBNFSVIV_FilenameMaxLen * sizeof(*wenc_buf));
        if (!wenc_buf)
        {
          fprintf(stderr, "Unviv: Memory allocation failed.\n");
        }
        else
        {
          memcpy(wenc_buf, viv_name, LIBNFSVIV_min(strlen(viv_name) + 1, LIBNFSVIV_FilenameMaxLen));
          wenc_buf[LIBNFSVIV_FilenameMaxLen - 1] = '\0';
          if (!LIBNFSVIV_AppendFileEnding(wenc_buf, LIBNFSVIV_FilenameMaxLen, LIBNFSVIV_WENCFileEnding))
          {
            fprintf(stderr, "Unviv: Cannot append extension '%s' to '%s'\n", viv_name, LIBNFSVIV_WENCFileEnding);
            free(wenc_buf);
            wenc_buf = NULL;
          }
        }
      }  /* if (opt_wenccommand) */
    }  /* if (!opt_dryrun) */

    if (LIBNFSVIV_IsDir(viv_name))
    {
      fprintf(stderr, "Unviv: Cannot open directory as archive '%s'\n", viv_name);
      break;
    }

    if (opt_direnlenfixed >= 10)  printf("\nFixed directory entry length: %d\n", opt_direnlenfixed);
    if (opt_filenameshex)  printf("Filenames as hex: %d\n", opt_filenameshex);
    printf("\nExtracting archive: %s\n", viv_name);
    printf("Extracting to: %s\n", outpath);

    file = fopen(viv_name, "rb");
    if (!file)
    {
      fprintf(stderr, "Unviv: Cannot open '%s'\n", viv_name);
      break;
    }

    viv_filesize = LIBNFSVIV_GetFilesize(viv_name);
    printf("Archive Size (parsed) = %d (0x%x)\n", viv_filesize, viv_filesize);
    if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd, file, viv_filesize, opt_verbose, opt_direnlenfixed, local_opt_filenameshex, 0))
      break;
    LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths(&vd, viv_name, outpath, file, viv_filesize);  /* invalidate files that would overwrite archive */

    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_GetIdxFromFname(&vd, file, request_file_name);
      if (request_file_idx <= 0)  break;
    }

    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsDec(&vd, file, request_file_idx, request_file_name, opt_direnlenfixed, local_opt_filenameshex);
    }

    if (opt_dryrun)
    {
      printf("End dry run\n");
      retv = 1;
      break;
    }

    /* Option: Write re-Encode command to file */
    if (!opt_dryrun && opt_wenccommand && wenc_buf)
    {
      wenc_f = fopen(wenc_buf, "a");
      if (!wenc_f)
      {
        fprintf(stderr, "Unviv: Cannot open '%s' (option -we)\n", wenc_buf);
      }
      else
      {
        if (strncmp(vd.format, "BIGF", 4))  /* omit writing default BIGF  */
        {
          fprintf(wenc_f, "%s %.4s ", "-fmt", vd.format);
        }
        fprintf(wenc_f, "\"%s\"", viv_name);
        fflush(wenc_f);
      }
      free(wenc_buf);  /* no longer needed */
      wenc_buf = NULL;
    }

    /* Extract archive */
    if (chdir(outpath) != 0)
    {
      fprintf(stderr, "Unviv: Cannot change working directory to '%s'\n", outpath);
      break;
    }

    if (request_file_idx == 0)
    {
      for (i = 0; i < vd.count_dir_entries; ++i)
      {
        if (LIBNFSVIV_GetBitmapValue(vd.validity_bitmap, i) == 1)
        {
          /* Continue through failures */
          count_extracted += LIBNFSVIV_VivExtractFile(&vd.buffer[i], file, local_opt_filenameshex, opt_overwrite, wenc_f, outpath);
        }
      }
    }
    else
    {
      if (request_file_idx < 0 || request_file_idx > vd.count_dir_entries_true)
      {
        fprintf(stderr, "Unviv: Requested idx (%d) out of bounds (1-based index)\n", request_file_idx);
        break;
      }
      if (LIBNFSVIV_GetBitmapValue(vd.validity_bitmap, request_file_idx - 1) != 1)
      {
        fprintf(stderr, "Unviv: Requested idx (%d) is invalid entry\n", request_file_idx);
        break;
      }
      if (!LIBNFSVIV_VivExtractFile(&vd.buffer[request_file_idx - 1], file, local_opt_filenameshex, opt_overwrite, wenc_f, outpath))
      {
        break;
      }
      ++count_extracted;
    }

    retv = 1;
    break;
  }  /* for (;;) */

  if (!opt_dryrun)
    printf("Number extracted: %d\n", count_extracted);

  if (wenc_f)
  {
    fprintf(wenc_f, "\n");
    fclose(wenc_f);
  }
  /* if (wenc_buf)  free(wenc_buf); */  /* already free'd */
  LIBNFSVIV_FreeVivDirectory(&vd);
  if (file)  fclose(file);

  return retv;
}

/*
  Assumes (viv_name). Overwrites file 'viv_name'. Skips unopenable infiles.
  Assumes (opt_requestfmt).
*/
int LIBNFSVIV_Viv(const char * const viv_name,
                  char **infiles_paths, const int count_infiles,
                  const int opt_dryrun, const int opt_verbose,
                  const int opt_direnlenfixed, const int opt_filenameshex,
                  const char *opt_requestfmt, const int opt_requestendian,
                  const int opt_faithfulencode)
{
  int retv = 1;
  int i;
  FILE *file = NULL;
  int filesz = -1;
  int count_archived = 0;
  VivDirectory vd = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };

#if SCL_DEBUG > 0
  printf("count_infiles %d\n"
  "opt_dryrun %d\n"
  "opt_verbose %d\n"
  "opt_direnlenfixed %d\n"
  "opt_filenameshex %d\n"
  "opt_requestfmt %.4s\n"
  "opt_faithfulencode %d\n"
  , count_infiles, opt_dryrun, opt_verbose, opt_direnlenfixed, opt_filenameshex, opt_requestfmt, opt_faithfulencode);
#endif

  if (opt_dryrun)
    printf("Begin dry run\n");

  printf("\nCreating archive: %s\n", viv_name);
  printf("Number of files to encode = %d\n", count_infiles);

  if (count_infiles > LIBNFSVIV_DirEntrMax || count_infiles < 0)
  {
    fprintf(stderr, "Viv: Number of files to encode too large (%d > %d)\n", count_infiles, LIBNFSVIV_DirEntrMax);
    return 0;
  }

  for (;;)
  {
    /* Set VIV directory */

    /* Use struct VivDirectory validity_bitmap to capture input file openability.
       In typical use, all input files will be available.
       Hence, any malloc overhead incurred for invalid paths is acceptable.
    */
    if (!LIBNFSVIV_SetVivDirHeader(&vd, infiles_paths, count_infiles, opt_requestfmt, opt_requestendian, opt_direnlenfixed, opt_filenameshex, opt_faithfulencode))
    {
      retv = 0;
      break;
    }

    if (opt_verbose)  LIBNFSVIV_PrintStatsEnc(&vd, infiles_paths, count_infiles, opt_filenameshex, opt_faithfulencode);

    if (opt_dryrun)
    {
      printf("End dry run\n");
      break;
    }

    file = fopen(viv_name, "wb");
    if (!file)
    {
      fprintf(stderr, "Viv: Cannot create output file '%s'\n", viv_name);
      retv = 0;
      break;
    }

    /* Write viv directory to file */
    if (!LIBNFSVIV_WriteVivHeader(&vd, file))
    {
      fprintf(stderr, "Viv: Cannot write Viv header\n");
      retv = 0;
      break;
    }
    printf("Endianness (written) = 0x%x\n", vd.__padding[0]);
    if (!LIBNFSVIV_WriteVivDirectory(&vd, file, infiles_paths, count_infiles, opt_direnlenfixed, opt_filenameshex, opt_faithfulencode))
    {
      retv = 0;
      break;
    }
    printf("Header Size (written) = %d (0x%x)\n", vd.viv_hdr_size_true, vd.viv_hdr_size_true);

    /* Write infiles to file, abandon on failure */
    for (i = 0; i < count_infiles; ++i)
    {
      if (LIBNFSVIV_GetBitmapValue(vd.validity_bitmap, i) == 1)
      {
        filesz = LIBNFSVIV_VivWriteFile(file, NULL, infiles_paths[i], 0, vd.buffer[i].filesize);
        if (filesz < 0)
        {
          retv = 0;
          break;
        }
        ++count_archived;
      }
    }
    printf("Archive Size (written) = %d (0x%x)\n", filesz, filesz);

    if (!opt_dryrun)
      printf("Number archived: %d\n", count_archived);

    /* Validate */
    if (!LIBNFSVIV_CheckVivHeader(&vd, filesz))
    {
      fprintf(stderr, "Viv: New archive failed format check (header)\n");
      retv = 0;
      break;
    }
    if (!LIBNFSVIV_CheckVivDirectory(&vd, filesz))
    {
      fprintf(stderr, "Viv: New archive failed format check (directory)\n");
      retv = 0;
      break;
    }

    break;
  }  /* for (;;) */

  if (file)  fclose(file);
  LIBNFSVIV_FreeVivDirectory(&vd);

  return retv;
}







/* Insert or replace.

  Returns modified entry index (1-based).

  Expects (vd != vd_old).
  Expects (file) and (infile_path).
  opt_insert: 0 (replace), >0 (insert|add), <0 (remove)
  opt_replacefilename: 0 (do not replace filename), !0 (replace filename)
  opt_faithfulencode:

  (opt_insert==1) overrides opt_replacefilename.
*/
int LIBNFSVIV_UpdateVivDirectory(VivDirectory *vd, const VivDirectory * const vd_old, FILE *file, char *infile_path,
                                 const char * const request_file_name, int request_file_idx, const int opt_insert,
                                 const int opt_replacefilename,
                                 const int opt_filenameshex, const int opt_faithfulencode)
{
  int retv = -1;
  int i;
  int len_filename;
  #ifdef _WIN32
    char buf[LIBNFSVIV_FilenameMaxLen];
  #endif

  for (;;)
  {
    if (!vd || !vd_old || !file || !infile_path)
    {
      fprintf(stderr, "UpdateVivDirectory: Invalid input\n");
      break;
    }
    if (vd == vd_old)
    {
      fprintf(stderr, "UpdateVivDirectory: vd and vd_old must be different instances\n");
      break;
    }

    /* Get target index */
    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_GetIdxFromFname(vd, file, request_file_name);
      if (request_file_idx <= 0)  break;
    }

    /**
      Update VivDirectory based on file, even for invalid file.

      opt_faithfulencode == 0:  directory entries for invalid files do not count towards offsets
      opt_faithfulencode != 0:  directory entries for invalid files do count towards offsets (pretend all is well, set filesize 0)
    */
    if (opt_insert >= 0 && LIBNFSVIV_IsFile(infile_path) && !LIBNFSVIV_IsDir(infile_path))
    {
      VivDirEntr vde_old = vd_old->buffer[request_file_idx - 1];
      VivDirEntr vde_temp;

      if (request_file_idx <= 0 || (opt_faithfulencode && request_file_idx > vd->count_dir_entries) || (!opt_faithfulencode && request_file_idx > vd->count_dir_entries_true))
      {
        fprintf(stderr, "VivReplaceEntry: Requested idx (%d) out of bounds (1-based index)\n", request_file_idx);
        break;
      }
      if (LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, request_file_idx - 1) != 1)
      {
        fprintf(stderr, "VivReplaceEntry: Requested idx (%d) is invalid entry\n", request_file_idx);
        break;
      }

      vde_old.offset = vd_old->buffer[request_file_idx - 1].offset;
      vde_old.filesize = vd_old->buffer[request_file_idx - 1].filesize;
      vde_old.filename_ofs_ = vd_old->buffer[request_file_idx - 1].filename_ofs_;
      vde_old.filename_len_ = vd_old->buffer[request_file_idx - 1].filename_len_;
      vde_temp.offset = vd_old->buffer[request_file_idx - 1].offset;
      vde_temp.filesize = LIBNFSVIV_GetFilesize(infile_path);
      vde_temp.filename_ofs_ = vd_old->buffer[request_file_idx - 1].filename_ofs_;
      vde_temp.filename_len_ = vd_old->buffer[request_file_idx - 1].filename_len_;

      if (opt_insert > 0 || opt_replacefilename)
      {
#ifdef _WIN32
        len_filename = (int)GetLongPathName(infile_path, buf, LIBNFSVIV_FilenameMaxLen) + 1;  /* transform short paths that contain tilde (~) */
        if (len_filename < 2 || len_filename > LIBNFSVIV_FilenameMaxLen)
          printf("Warning:SetVivDirHeader: Cannot get long path name for file '%s' (len_filename=%d)\n", infile_path, (int)strlen(LIBNFSVIV_GetPathBasename(infile_path)) + 1);
        else
          len_filename = (int)strlen(LIBNFSVIV_GetPathBasename(buf)) + 1;
#else
        len_filename = (int)strlen(LIBNFSVIV_GetPathBasename(infile_path)) + 1;
#endif
        len_filename = LIBNFSVIV_clamp(len_filename, 1, LIBNFSVIV_FilenameMaxLen);
        if (opt_filenameshex)
          len_filename = len_filename / 2 + len_filename % 2;

        vde_temp.filename_len_ = len_filename - 1;
      }  /* if opt_replacefilename */

      SCL_printf("vd offset: %d\n", vde_temp.offset);
      SCL_printf("vd filesize: %d\n", vde_temp.filesize);
      SCL_printf("vd filename_ofs_: 0x%x\n", vde_temp.filename_ofs_);
      SCL_printf("vd filename_len_: %d\n", vde_temp.filename_len_);

      if (opt_insert == 0)
      {
        /* Update header */
        if (!opt_faithfulencode)  LIBNFSVIV_SetBitmapTrue(vd->validity_bitmap, request_file_idx - 1);
        vd->filesize += vde_temp.filesize - vde_old.filesize;
        vd->header_size += vde_temp.filename_len_ - vde_old.filename_len_;
        vd->viv_hdr_size_true += vde_temp.filename_len_ - vde_old.filename_len_;

        /* Update target entry */
        vd->buffer[request_file_idx - 1].offset = vde_temp.offset;
        vd->buffer[request_file_idx - 1].filesize = vde_temp.filesize;
        vd->buffer[request_file_idx - 1].filename_ofs_ = vde_temp.filename_ofs_;
        vd->buffer[request_file_idx - 1].filename_len_ = vde_temp.filename_len_;

        /* Update existing entries */
        for (i = 0; i < vd->count_dir_entries; ++i)
        {
          /* Offset remaining entries */
          if (i > request_file_idx - 1)
          {
            vd->buffer[i].filename_ofs_ += vde_temp.filename_len_ - vde_old.filename_len_;
          }
          /* Offset contents */
          if (LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) == 1)
          {
            vd->buffer[i].offset += vd->viv_hdr_size_true - vd_old->viv_hdr_size_true;
            if (i != request_file_idx - 1 && vd->buffer[i].offset >= vde_old.offset)
            {
              vd->buffer[i].offset += vde_temp.filesize - vde_old.filesize;
            }
          }
        }  /* for i */
      }
      else if (opt_insert > 0)  /* insert (add) entry at index */
      {
        fprintf(stderr, "not implemented\n");
        break;
      }
    }  /* if() workload */
    else if (opt_insert < 0)  /* remove entry at index */
    {
      fprintf(stderr, "not implemented\n");
      break;
    }
    else
    {
      fprintf(stderr, ": Invalid input '%s'\n", infile_path);
      break;
    }

    retv = request_file_idx;
    break;
  }  /* for (;;) */

  return retv;
}


static
int LIBNFSVIV_HasWritePermission(const char *path)
{
#ifdef _WIN32
  struct _stat sb;
  return !!path && !_stat(path, &sb) && (sb.st_mode & _S_IWRITE) ? 1 : 0;
#else
  struct stat sb;
  return !!path && !stat(path, &sb) && (sb.st_mode & S_IWUSR) ? 1 : 0;
#endif
}


/* buf will hold "/path/to/temp/". Returns strlen() > 0 on success. */
static
int LIBNFSVIV_GetTempPath(const int bufsz, char *buf)
{
#ifdef _WIN32
  const int ret = (int)GetTempPath(bufsz, buf);
  if (ret > 0)  LIBNFSVIV_BkwdToFwdSlash(buf);
  return ret;
#else
  if (!buf)  return -1;
  buf[0] = '\0';
  if (LIBNFSVIV_IsDir("/tmp/") && LIBNFSVIV_HasWritePermission("/tmp/"))
    sprintf(buf, "/tmp/");
  else if (LIBNFSVIV_IsDir("/usr/tmp/") && LIBNFSVIV_HasWritePermission("/usr/tmp/"))
    sprintf(buf, "/usr/tmp/");
  else if (LIBNFSVIV_IsDir("/var/tmp/") && LIBNFSVIV_HasWritePermission("/var/tmp/"))
    sprintf(buf, "/var/tmp/");
  /* else if (strlen(buf) < 1 && !getcwd(buf, bufsz) && sprintf(buf + strlen(buf), "/") != 1)  return -1; */
  else
    return -1;

#if __APPLE__ || _DEFAULT_SOURCE || _BSD_SOURCE || _POSIX_C_SOURCE >= 200809L
  if ((int)strlen(buf) + 8 > bufsz)  return -1;
  buf[strlen(buf) + 6] = '\0';
  memset(buf + strlen(buf), 'X', 6);
  if (!mkdtemp(buf))  return -1;
  sprintf(buf + strlen(buf), "/");
#else  /* non-Windows non-Python C89 fallback, assumes that last 6 characters are digits. quietly bail on failures */
#if SCL_DEBUG > 0
#warning using tmpnam(), missing mkdtemp()
#endif
for (;;)
{
  char temp[LIBNFSVIV_FilenameMaxLen];
  char *p_;
  if (!tmpnam(temp))  break;
  p_ = strrchr(temp, '/');
  if (!p_)  break;
  p_[strlen(p_)] = '\0';
  if ((int)strlen(buf) + (int)strlen(p_) > bufsz)  break;
  sprintf(buf + strlen(buf), "%s", p_);
  if (strlen(p_) > 0)  sprintf(buf + strlen(buf), "/");
  break;
}
#endif
  SCL_printf("LIBNFSVIV_GetTempPath: %s\n", buf);

  return (int)strlen(buf);
#endif
}


#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <copyfile.h>
#elif !defined(_WIN32) && !defined(__APPLE__)
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
/* Returns !0 on success, 0 on failure. */
static
int LIBNFSVIV_CopyFile(char *lpExistingFileName, char *lpNewFileName, int bFailIfExists)
{
#ifdef _WIN32
  return (int)CopyFile(lpExistingFileName, lpNewFileName, bFailIfExists);  /* CopyFile() !0 on success */
#elif defined(__APPLE__)
  if (!bFailIfExists)
    return copyfile(lpExistingFileName, lpNewFileName, NULL, COPYFILE_DATA | COPYFILE_XATTR | COPYFILE_EXCL) == 0;
  return copyfile(lpExistingFileName, lpNewFileName, NULL, COPYFILE_DATA | COPYFILE_XATTR) == 0;
#else
  int retv = 0;
  for (;;)
  {
    if (!lpExistingFileName || !lpNewFileName)  break;
    if (!LIBNFSVIV_IsDir(lpExistingFileName) && !LIBNFSVIV_IsDir(lpNewFileName))
    {
      int fd_in, fd_out;
      struct stat sb;
      off_t offset = 0;
      fd_in = open(lpExistingFileName, O_RDONLY);
      if (fd_in < 0)  break;
      if (fstat(fd_in, &sb) < 0)
      {
        close(fd_in);
        break;
      }
      if (!bFailIfExists)
        fd_out = open(lpNewFileName, O_WRONLY | O_CREAT, sb.st_mode);
      else
        fd_out = open(lpNewFileName, O_WRONLY | O_CREAT | O_EXCL, sb.st_mode);
      if (fd_out >= 0)
      {
        if (sendfile(fd_out, fd_in, &offset, sb.st_size) == (int)sb.st_size)  /* sendfile() bytes on success, -1 on failure */
          retv = (int)sb.st_size;
        close(fd_out);
      }
      close(fd_in);
    }
    break;
  }  /* for (;;) */
  return retv;
#endif
}



/*
  Assumes viv_name is buffer of length >= 4096.
  Assumes viv_name_out is NULL or buffer of length >= 4096.

  opt_insert: 0 (replace), >0 (insert|add), <0 (remove)

  New archive will be written to temporary path. In case of success, will be copied toviv_name_out_
*/
int LIBNFSVIV_Update(char *viv_name, const char * const viv_name_out_,
                        int request_file_idx, const char *request_file_name,
                        char *infile_path,
                        const int opt_insert, const int opt_replacefilename,
                        const int opt_dryrun, const int opt_verbose,
                        const int opt_direnlenfixed, const int opt_filenameshex,
                        const int opt_faithfulencode)
{
  int retv = 0;
  FILE *file = NULL;
  int filesz = -1;
  FILE *file_out = NULL;
  int count_infiles;
  char **infiles_paths = NULL;
  VivDirectory vd = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };
  VivDirectory vd_old = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };
  const int local_opt_filenameshex = (opt_filenameshex || (opt_direnlenfixed >= 10));  /* fixed length entries with all-printable names are not known to exist */
  char *temppath = NULL;
  char *viv_name_out = NULL;  /* temporary archive */
  int i;
  int count_archived = 0;

  SCL_printf("viv_name %s\n", viv_name);
  SCL_printf("viv_name_out_ %s\n", viv_name_out_);
  SCL_printf("request_file_idx %d\n", request_file_idx);
  SCL_printf("request_file_name %s\n", request_file_name);
  SCL_printf("infile_path %s\n", infile_path);
  SCL_printf("opt_insert %d\n", opt_insert);
  SCL_printf("opt_replacefilename %d\n", opt_replacefilename);
  SCL_printf("opt_dryrun %d\n", opt_dryrun);
  SCL_printf("opt_verbose %d\n", opt_verbose);
  SCL_printf("opt_direnlenfixed %d\n", opt_direnlenfixed);
  SCL_printf("opt_filenameshex %d\n", opt_filenameshex);
  SCL_printf("opt_faithfulencode %d\n", opt_faithfulencode);

  if (opt_dryrun)
    printf("Begin dry run\n");

  printf("Updating archive: %s\n", viv_name);

  for (;;)
  {
    temppath = (char *)malloc(LIBNFSVIV_FilenameMaxLen * sizeof(*temppath));
    if (!temppath)
    {
      fprintf(stderr, "VivUpdate: Cannot allocate memory\n");
      break;
    }
    viv_name_out = (char *)malloc(LIBNFSVIV_FilenameMaxLen * sizeof(*viv_name_out));
    if (!viv_name_out)
    {
      fprintf(stderr, "VivUpdate: Cannot allocate memory\n");
      break;
    }
    {
      const char * const viv_name_out_ptr = viv_name_out_ ? viv_name_out_ : viv_name;
      int len_temp;
      const int len_ = LIBNFSVIV_min(strlen(viv_name_out_ptr), LIBNFSVIV_FilenameMaxLen - 1);
      char buf_[LIBNFSVIV_FilenameMaxLen];
      const char *p;
      memcpy(viv_name_out, viv_name_out_ptr, len_);
      viv_name_out[len_] = '\0';
      memcpy(buf_, viv_name_out_ptr, len_);
      buf_[len_] = '\0';
      p = LIBNFSVIV_GetPathBasename(buf_);
      len_temp = LIBNFSVIV_GetTempPath(LIBNFSVIV_FilenameMaxLen, temppath);
      if (len_temp < 1)  break;
      memcpy(temppath + len_temp, p, strlen(p) + 1);
    }

    printf("Writing to archive: %s\n", viv_name_out);
    SCL_printf("temppath %s\n", temppath);

    /* Read VivDirectory */
#ifdef _WIN32
      if (!LIBNFSVIV_IsFile(viv_name) || !LIBNFSVIV_GetFullPathName(viv_name, NULL, 0, NULL))
#else
      if (!LIBNFSVIV_GetFullPathName(viv_name, NULL))  /* returns NULL if path does not exist */
#endif
    {
      fprintf(stderr, "VivUpdate: Cannot get full path of archive.\n");
      break;
    }

    if (LIBNFSVIV_IsDir(viv_name))
    {
      fprintf(stderr, "VivUpdate: Cannot open directory as archive '%s'\n", viv_name);
      break;
    }

    if (LIBNFSVIV_IsDir(viv_name_out))
    {
      fprintf(stderr, "VivUpdate: Cannot open directory as file '%s'\n", viv_name_out);
      break;
    }

    if (opt_direnlenfixed >= 10)  printf("\nFixed directory entry length: %d\n", opt_direnlenfixed);
    if (opt_filenameshex)  printf("Filenames as hex: %d\n", opt_filenameshex);

    file = fopen(viv_name, "rb");
    if (!file)
    {
      fprintf(stderr, "VivUpdate: Cannot open '%s'\n", viv_name);
      break;
    }

    filesz = LIBNFSVIV_GetFilesize(viv_name);
    printf("Archive Size (parsed) = %d (0x%x)\n", filesz, filesz);
    if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd, file, filesz, opt_verbose, opt_direnlenfixed, local_opt_filenameshex, 0))
      break;
    fseek(file, 0, SEEK_SET);
    if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd_old, file, filesz, opt_verbose, opt_direnlenfixed, local_opt_filenameshex, 0))
      break;

    if (opt_verbose)
    {
      printf("\n");
      printf("Before update...\n");
      LIBNFSVIV_PrintStatsDec(&vd, file, request_file_idx, request_file_name, opt_direnlenfixed, local_opt_filenameshex);
    }

    request_file_idx = LIBNFSVIV_UpdateVivDirectory(&vd, &vd_old, file, infile_path,
                                                    request_file_name, request_file_idx,
                                                    opt_insert,
                                                    opt_replacefilename,
                                                    opt_filenameshex, opt_faithfulencode);
    SCL_printf("request_file_idx %d\n", request_file_idx);
    if (request_file_idx < 0)
    {
      break;
    }

    if (opt_verbose)
    {
      printf("\n");
      printf("After update...\n");
      LIBNFSVIV_PrintVivDirEntr(&vd, opt_faithfulencode);
    }

    /** TODO: export to static function  */
    /* Get filenames / filepaths for output archive */
    if (opt_insert == 0)
    {
      int len_ = 0;
      char *p;
      if (vd.count_dir_entries != vd_old.count_dir_entries)
      {
        fprintf(stderr, "VivUpdate: mismatched number of dir entries\n");
        break;
      }
      count_infiles = vd_old.count_dir_entries;
      SCL_printf("infiles_paths count_infiles %d\n", count_infiles);
      infiles_paths = (char **)malloc(count_infiles * sizeof(*infiles_paths));
      if (!infiles_paths)
      {
        fprintf(stderr, "VivUpdate: Cannot allocate memory\n");
        break;
      }
      if (opt_replacefilename)  len_ = strlen(infile_path) + 1;
      for (i = 0; i < vd_old.count_dir_entries; ++i)
      {
        if (opt_replacefilename && i == request_file_idx - 1)
          continue;
        len_ += LIBNFSVIV_clamp(vd_old.buffer[i].filename_len_ + 1, 1, LIBNFSVIV_FilenameMaxLen);
      }
      SCL_printf("infiles_paths len_ %d\n", len_);
      *infiles_paths = (char *)calloc(len_ * sizeof(**infiles_paths), 1);
      if (!*infiles_paths)
      {
        fprintf(stderr, "VivUpdate: Cannot allocate memory (2)\n");
        break;
      }

      p = *infiles_paths;
      for (i = 0; i < vd_old.count_dir_entries; ++i)
      {
        infiles_paths[i] = p;
        if (opt_replacefilename && i == request_file_idx - 1)
        {
          memcpy(p, infile_path, strlen(infile_path) + 1);
          p += strlen(infile_path) + 1;
        }
        else
        {
          const int tmp_bufsz_ = LIBNFSVIV_clamp(vd_old.buffer[i].filename_len_ + 1, 1, LIBNFSVIV_FilenameMaxLen);
          const int tmp_len_ = LIBNFSVIV_min(vd_old.buffer[i].filename_len_, LIBNFSVIV_FilenameMaxLen);
          if (tmp_len_ != LIBNFSVIV_FreadToStr(p,
                                               tmp_bufsz_,
                                               vd_old.buffer[i].filename_ofs_, tmp_len_, file)
          )
          {
            fprintf(stderr, "VivUpdate: LIBNFSVIV_FreadToStr\n");
            break;  /* inner loop */
          }
          p += tmp_len_ + 1;
        }
      }  /* for i */
    }
    else if (opt_insert > 0)
    {
      fprintf(stderr, "VivUpdate: not implemented\n");
      break;
    }
    else
    {
      fprintf(stderr, "VivUpdate: not implemented\n");
      break;
    }

    if (opt_dryrun)
    {
      printf("End dry run\n");
      retv = 1;
      break;
    }

    file_out = fopen(temppath, "wb+");
    if (!file_out)
    {
      fprintf(stderr, "VivUpdate: Cannot open '%s'\n", temppath);
      break;
    }

    /* Write viv directory to file */
    if (!LIBNFSVIV_WriteVivHeader(&vd, file_out))
    {
      fprintf(stderr, "VivUpdate: Cannot write Viv header\n");
      break;
    }
    printf("Endianness (written) = 0x%x\n", vd.__padding[0]);

#if SCL_DEBUG > 0
    {
      for (i = 0; i < count_infiles; ++i)
        printf("%2d: %s \n", i, infiles_paths[i]);
    }
#endif

    if (!LIBNFSVIV_WriteVivDirectory(&vd, file_out, infiles_paths, count_infiles, opt_direnlenfixed, opt_filenameshex, opt_faithfulencode))
    {
      fprintf(stderr, "VivUpdate: Cannot write Viv directory\n");
      break;
    }
    printf("Header Size (written) = %d (0x%x)\n", vd.viv_hdr_size_true, vd.viv_hdr_size_true);

    /* Write files to new archive, abandon on failure */
    if (opt_insert == 0)
    {
      int offs_ = vd.viv_hdr_size_true;
      for (i = 0; i < vd.count_dir_entries; ++i)
      {
        if (LIBNFSVIV_GetBitmapValue(vd.validity_bitmap, i) == 1)
        {
          int err__ = 0;
          int sanity__ = 0;
          while (offs_ < vd.buffer[i].offset && sanity__ < (1 << 22))  /* upper-bounded gap */
          {
            fputc('\0', file_out);
            ++offs_;
            ++sanity__;
            SCL_printf(".");
          }
          if (i != request_file_idx - 1)
            err__ = LIBNFSVIV_VivWriteFile(file_out, file, NULL, vd_old.buffer[i].offset, vd.buffer[i].filesize);
          else
            err__ = LIBNFSVIV_VivWriteFile(file_out, NULL, infile_path, 0, vd.buffer[i].filesize);
          if (err__ < 0)
          {
            fprintf(stderr, "VivUpdate: Cannot write Viv archive\n");
            break;
          }
          offs_ += vd.buffer[i].filesize;
          ++count_archived;
          SCL_printf("| %d %d  (vd_old.buffer[i].offset %d)\n", offs_, (int)ftell(file_out), vd.buffer[i].offset);
        }
      }
      if (count_archived != vd.count_dir_entries_true)
      {
        break;
      }
    }
    else
    {
      fprintf(stderr, "VivUpdate: not implemented\n");
      break;
    }
    printf("Archive Size (written) = %d (0x%x)\n", (int)ftell(file_out), (int)ftell(file_out));
    printf("Number archived: %d\n", count_archived);

    if (opt_verbose)
    {
      printf("After write...\n");
      SCL_printf("LIBNFSVIV_IsFile %d\n", LIBNFSVIV_IsFile(temppath));
      LIBNFSVIV_PrintStatsDec(&vd, file_out, 0, NULL, opt_direnlenfixed, local_opt_filenameshex);
    }

    fclose(file_out);
    file_out = NULL;
    fclose(file);
    file = NULL;

    if (rename(temppath, viv_name_out))
    {
      if (!LIBNFSVIV_CopyFile(temppath, viv_name_out, 0))
      {
        fprintf(stderr, "VivUpdate: Cannot create '%s'\n", viv_name_out);
        break;
      }
    }

    retv = 1;
    break;
  }  /* for (;;) */

  if (infiles_paths)
  {
    if (*infiles_paths)  free(*infiles_paths);
    free(infiles_paths);
  }
  LIBNFSVIV_FreeVivDirectory(&vd_old);
  LIBNFSVIV_FreeVivDirectory(&vd);
  if (file_out)  fclose(file_out);
  if (file)  fclose(file);
  if (viv_name_out)  free(viv_name_out);
  if (temppath)  free(temppath);

  return retv;
}




#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LIBNFSVIV_H_ */
