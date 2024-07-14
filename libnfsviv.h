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

/**
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
    * Directory entry has a file name identical to archive name {skip file}  TODO: clarify
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
#define chdir _chdir
#define mkdir(a,b) _mkdir(a)
#define stat _stat
#define S_IFDIR _S_IFDIR
#define S_IFMT _S_IFMT
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

#define UVTVERS "2.3"
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
LIBNFSVIV_extern int LIBNFSVIV_GetVivVersion_FromBuf(const char * const buf);  /* see below */

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
/* Returns UTF8 length without nul. If __s is not nul-terminated, set nul_terminate=0 */
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
  return pos * (!nul_terminate || (pos < max_len)) * (state == UTF8_ACCEPT);
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

/* Assumes src_len is length without nul. */
static
void *LIBNFSVIV_memcpyToString(void *dest, const void *src,
                               const int src_len, const int dest_len)
{
  unsigned char *p = (unsigned char *)dest;
#if 0  /* irrelevant here */
  if (dest_len > 0)
  {
#endif
    memcpy(dest, src, src_len);
    p[LIBNFSVIV_min(src_len, dest_len - 1)] = '\0';
    return dest;
#if 0  /* irrelevant here */
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
  char buf[LIBNFSVIV_FilenameMaxLen]; /* = {0}; */
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
  char buf[LIBNFSVIV_FilenameMaxLen]; /* = {0}; */
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
  if (!file)
    return 0;
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

/* Assumes (vd) and both, viv_name and outpath are strings.

Invalidates entries whose output path is identical to the archive. */
static
void LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths(VivDirectory *vd, char *viv_name, const char *outpath, FILE *viv, const size_t viv_sz)
{
  char buf[LIBNFSVIV_FilenameMaxLen];  /* = {0}; */

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

static
int LIBNFSVIV_RenameExistingFile(const char *oldName, const int oldName_sz)
{
  int retv = 0;
  const char *extension = strrchr(oldName, '.');
  const char *nul = "\0";
  int oldbasename_len;
  int i;
  char *newFullName;

  if (strlen(oldName) + 32 > LIBNFSVIV_FilenameMaxLen || oldName_sz < LIBNFSVIV_FilenameMaxLen)
  {
    printf("RenameExistingFile: Failed to rename file (filename too long) '%s'\n", oldName);
    return retv;
  }

  if (!extension)
    extension = nul;
  oldbasename_len = strlen(oldName) - strlen(extension);

  newFullName = (char *)calloc(strlen(oldName) + 32 * sizeof(*newFullName), 1);
  if (!newFullName)
  {
    printf("RenameExistingFile: Failed to allocate memory.\n");
    return retv;
  }

  memcpy(newFullName, oldName, oldbasename_len);

  for (i = 0; i < 1000; i++)
  {
    sprintf(newFullName + oldbasename_len, "_%d%s", i, extension);
    if (!LIBNFSVIV_IsFile(newFullName))
    {
      if (!rename(oldName, newFullName))
      {
        printf("RenameExistingFile: Renamed existing file '%s' to '%s'\n", oldName, newFullName);
        retv = 1;
      }
      else
        printf("RenameExistingFile: Failed to rename '%s'\n", oldName);
      break;
    }
  }

  free(newFullName);
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
typedef unsigned char CIRCBUF_CHAR;

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
#if 1
  return cb->sz - cb->rd;
#else
  const int d = cb->sz - cb->rd;
  return cb->rd >= cb->wr ? d : cb->wr - cb->rd;
#endif
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
  SCL_printf("    circbuf_Fwd() stats: len: %d, cb->rd: %d, cb->wr: %d\n", len, cb->rd, cb->wr);
  cb->rd += len;
  SCL_printf("    circbuf_Fwd() stats: len: %d, cb->rd: %d, cb->wr: %d\n", len, cb->rd, cb->wr);
  cb->rd %= cb->sz;
  SCL_printf("    circbuf_Fwd() stats: len: %d, cb->rd: %d, cb->wr: %d\n", len, cb->rd, cb->wr);
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
    /* SCL_debug_printbuf(cb->buf, cb->sz, cb->rd, cb->wr);
    SCL_printf("    circbuf_PeekStrlen(): rdlen1: %d, len: %d\n", rdlen1, len);
    SCL_printf("    circbuf_PeekStrlen(): stats: len: %d, cb->rd: %d, cb->wr: %d\n", len, cb->rd, cb->wr); */
    return (ret < rdlen1) ? ret : ret + __LIBNFSVIV_CircBuf_strnlen(cb->buf, len - rdlen1);
  }
  return __LIBNFSVIV_CircBuf_strnlen(cb->buf + cb->rd + ofs, len);
  /* return memchr(cb->buf + cb->rd + ofs, c, len); */
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
void LIBNFSVIV_PrintVivDirEntr(const VivDirectory * const vd)
{
  int i;
  int cnt;
  printf("PrintVivDirEntr\n");

  printf("vd->count_dir_entries: %d\n", vd->count_dir_entries);
  printf("vd->count_dir_entries_true: %d\n", vd->count_dir_entries_true);
  printf("vd->length: %d\n", vd->length);
  printf("vd->null_count: %d\n", vd->null_count);
  printf("vd valid filenames strings size: %d\n", LIBNFSVIV_SumVivDirectoryFilenameSizes(vd, 0));
  printf("i     valid? offset          filesize        filename_ofs_        filename_len_\n");
  if (1)  return;  /* debug... */
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

/* Assumes that 'file' is valid VIV data. */
static
void LIBNFSVIV_PrintStatsDec(VivDirectory * const vd,
                             const int viv_filesize, FILE *file,
                             const int request_file_idx, const char *request_file_name,
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
    rewind(file);
    if ((int)fread(buf, 1, bufsize, file) != bufsize)
    {
      fprintf(stderr, "File read error (print stats)\n");
      free(buf);
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
      LIBNFSVIV_memcpyToString(filename, buf + vd->buffer[i].filename_ofs_, vd->buffer[i].filename_len_, LIBNFSVIV_FilenameMaxLen);
      if (opt_filenameshex)
        LIBNFSVIV_EncBase16(filename, vd->buffer[i].filename_len_);
      /* avoid printing non-UTF8 / non-printable string */
      sz = strlen(filename) + 1;
#ifdef UVTUTF8
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s\n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsUTF8String(buf + vd->buffer[i].filename_ofs_, sz, 1) > 0) ? filename : "<non-UTF8>");
#else
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s\n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsPrintString(buf + vd->buffer[i].filename_ofs_, sz, 1) > 0) ? filename : "<non-printable>");
#endif
    }

    printf(" ---- ----- ------------ ------------ ------------ --- ----  -----------------------\n"
           "              %10d                %10d           %d files\n", vd->buffer[vd->count_dir_entries_true - 1].offset + vd->buffer[vd->count_dir_entries_true - 1].filesize, contents_size, vd->count_dir_entries_true);

    free(buf);
  }  /* if */
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
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s\n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsUTF8String(ptr, sz, 1) > 0) ? LIBNFSVIV_GetPathBasename(ptr) : "<non-UTF8>");
#else
      printf(" %4d     %d   %10d   %10d   %10d %3d %4x  %s\n", i + 1, LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i), vd->buffer[i].offset, gap, vd->buffer[i].filesize, vd->buffer[i].filename_len_, vd->buffer[i].filename_ofs_, (opt_filenameshex || /* LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) || */ LIBNFSVIV_IsPrintString(ptr, sz, 1) > 0) ? LIBNFSVIV_GetPathBasename(ptr) : "<non-printable>");
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
int LIBNFSVIV_CheckVivHdr(const VivDirectory *vd, const int viv_filesize)
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
void LIBNFSVIV_FixVivHdr(VivDirectory *vd, const int filesz)
{
  if (vd->count_dir_entries < 0)
  {
    printf("Warning:FixVivHdr: Format (invalid number of purported directory entries) (%d)(0x%x),\n", vd->count_dir_entries, vd->count_dir_entries);
    SCL_printf("FixVivHdr: 32 bit (%d)(0x%x) bitmask,\n", vd->count_dir_entries & 0x7FFFFFFF, vd->count_dir_entries & 0x7FFFFFFF);
    vd->count_dir_entries = LIBNFSVIV_min(vd->count_dir_entries & 0x7FFFFFFF, LIBNFSVIV_DirEntrMax);
    printf("Warning:FixVivHdr: assume %d entries\n", vd->count_dir_entries);
  }
  else if (vd->count_dir_entries > LIBNFSVIV_DirEntrMax)
  {
    printf("Warning:FixVivHdr: Format (unsupported number of purported directory entries) (%d)(0x%x),\n", vd->count_dir_entries, vd->count_dir_entries);
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
int LIBNFSVIV_GetVivHdr(VivDirectory *vd, FILE *file)
{
  int sz = 0;

  sz += fread(vd->format, 1, 4, file);
  sz += fread(&vd->filesize, 1, 4, file);
  sz += fread(&vd->count_dir_entries, 1, 4, file);
  sz += fread(&vd->header_size, 1, 4, file);

  if (sz != 16)
  {
    fprintf(stderr, "GetVivHeader: File read error\n");
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
int LIBNFSVIV_GetVivDir(VivDirectory *vd,
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
    fprintf(stderr, "GetVivDir: Cannot allocate memory\n");
    return 0;
  }
  vd->null_count = vd->count_dir_entries;
  vd->viv_hdr_size_true = 0x10;
  if (opt_verbose >= 1)
  {
    printf("Directory Entries (malloc'd): %d (ceil(x/64)=%d), Bitmap (malloc'd): %d, Padding: %d\n", vd->length, LIBNFSVIV_ceil(vd->length, 64), vd->length > LIBNFSVIV_VivDirectoryPaddingSize * 8, LIBNFSVIV_VivDirectoryPaddingSize);
  }

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
      fprintf(stderr, "GetVivDir: File read error at %d\n", vd->viv_hdr_size_true);
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
      SCL_debug_printbuf(cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr);

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
          fprintf(stderr, "GetVivDir: File read error at %d\n", vd->viv_hdr_size_true);
          return 0;
        }

        lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);  /* zero if (cb->rd == cb->wr) */
        lefttoread = lefttoread > 0 ? lefttoread : (int)sizeof(buf);
        SCL_printf("  cbuf stats: buf %p, !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", cbuf.buf, !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_printf("  memchr: %p\n", LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread));
        SCL_debug_printbuf(cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr);
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
          fprintf(stderr, "Warning:GetVivDir: Filename at %d not a string. Not a directory entry. Stop parsing directory.\n", vd->viv_hdr_size_true);

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
        char tmp_UTF8;
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
      fprintf(stderr, "GetVivDir: fixed directory entry length too large for buffer size (%d > %d)\n", opt_direnlenfixed, (int)sizeof(buf));
      return 0;
    }

    SCL_printf("  Read initial chunk\n");
    if (viv_filesize - (int)ftell(file) >= 10 && LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - 4) < opt_direnlenfixed)
    {
      fprintf(stderr, "GetVivDir: File read error at %d\n", vd->viv_hdr_size_true);
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
          fprintf(stderr, "GetVivDir: File read error at %d\n", vd->viv_hdr_size_true);
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
          fprintf(stderr, "Warning:GetVivDir: Filename at %d not a string. Not a directory entry. Stop parsing directory.\n", vd->viv_hdr_size_true);

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
        fprintf(stderr, "GetVivDir: Not implemented. Try with filenames as hex.\n");
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
    LIBNFSVIV_PrintVivDirEntr(vd);
  #endif

  return 1;
}


/* Accepts a directory entry, extracts the described file. Returns boolean. */
static
int LIBNFSVIV_VivExtractFile(const VivDirEntr vd, const int viv_filesize,
                             FILE *infile,
                             const int opt_filenameshex, const int opt_overwrite,
                             FILE *wenc_file, const char *wenc_outpath)
{
  unsigned char buf[LIBNFSVIV_BufferSize] = {0};
  size_t curr_chunk_size;
  int curr_offset;
  FILE *outfile = NULL;

  /* Read outfilename from file into buf */
  curr_offset = vd.filename_ofs_;
  curr_chunk_size = LIBNFSVIV_min(LIBNFSVIV_BufferSize, viv_filesize - curr_offset);
  fseek(infile, curr_offset, SEEK_SET);
  if (fread(buf, 1, curr_chunk_size, infile) != curr_chunk_size)
  {
    fprintf(stderr, "VivExtractFile: File read error at %d (extract outfilename)\n", curr_offset);
    return 0;
  }

  if (opt_filenameshex)  /* Option: Encode outfilename to Base16 */
    LIBNFSVIV_EncBase16((char *)buf, vd.filename_len_);

  /* Create outfile */
  if (LIBNFSVIV_IsFile((const char *)buf))  /* overwrite mode: for existing files and duplicated filenames in archive */
  {
    if (opt_overwrite == 1)  /* attempt renaming existing file, return on failure */
    {
      if (!LIBNFSVIV_RenameExistingFile((const char *)buf, (int)sizeof(buf)))
      {
        fprintf(stderr, "VivExtractFile: Cannot rename existing '%s'\n", (const char *)buf);
        return 0;
      }
    }
    else
    {
      fprintf(stderr, "Warning:VivExtractFile: Attempt overwriting existing '%s' (duplicated filename?)\n", (const char *)buf);
    }
  }
  outfile = fopen((const char *)buf, "wb");
  if (!outfile)
  {
    fprintf(stderr, "VivExtractFile: Cannot create output file '%s'\n", (const char *)buf);
    return 0;
  }

  if (wenc_file)  /* Option: Write re-Encode command to file */
  {
    fprintf(wenc_file, " \"%s/%s\"", wenc_outpath, (char *)buf);
    fflush(wenc_file);
  }

  memset(buf, 0, LIBNFSVIV_BufferSize);
  curr_offset = vd.offset;
  fseek(infile, curr_offset, SEEK_SET);

  while (curr_offset < vd.offset + vd.filesize)
  {
    curr_chunk_size = LIBNFSVIV_min(LIBNFSVIV_BufferSize, vd.offset + vd.filesize - curr_offset);

    if (fread(buf, 1, curr_chunk_size, infile) != curr_chunk_size)
    {
      fprintf(stderr, "VivExtractFile: File read error (archive)\n");
      fclose(outfile);
      return 0;
    }

    if (fwrite(buf, 1, curr_chunk_size, outfile) != curr_chunk_size)
    {
      fprintf(stderr, "VivExtractFile: File write error (output)\n");
      fclose(outfile);
      return 0;
    }

    curr_offset += curr_chunk_size;
  }

  fclose(outfile);
  return 1;
}

/** Assumes (request_file_name), and request_file_name is string.
    Returns 1-based directory entry index for given filename, -1 if it does not
    exist, 0 on error. **/
static
int LIBNFSVIV_GetIdxFromFname(const VivDirectory *vd,
                              FILE* infile, const int infilesize,
                              const char *request_file_name)
{
  int retv = -1;
  int i;
  int chunk_size;
  char buf[LIBNFSVIV_FilenameMaxLen];

  if (strlen(request_file_name) + 1 > LIBNFSVIV_FilenameMaxLen)
  {
    fprintf(stderr, "GetIdxFromFname: Requested filename is too long\n");
    return 0;
  }

  for (i = 0; i < vd->count_dir_entries_true; ++i)
  {
/** TODO: validity check required? */
#if 0
    if (!LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
      continue;
#endif
    fseek(infile, vd->buffer[i].filename_ofs_, SEEK_SET);
    chunk_size = LIBNFSVIV_min(infilesize - vd->buffer[i].filename_ofs_, LIBNFSVIV_FilenameMaxLen);

    if (fread(buf, 1, chunk_size, infile) != (size_t)chunk_size)
    {
      fprintf(stderr, "GetIdxFromFname: File read error (find index)\n");
      retv = 0;
      break;
    }

    if (!strcmp(buf, request_file_name))
      return i + 1;
  }

  fprintf(stderr, "GetIdxFromFname: Cannot find requested file in archive (cAse-sEnsitivE filename)\n");
  return retv;
}

/* internal: encode --------------------------------------------------------- */

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
      len_filename = strlen(LIBNFSVIV_GetPathBasename(buf)) + 1;
#else
    len_filename = strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1;
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

  buf[0] = LIBNFSVIV_SwapEndian(vd->filesize);
  buf[1] = LIBNFSVIV_SwapEndian(vd->count_dir_entries);
  buf[2] = LIBNFSVIV_SwapEndian(vd->header_size);

  SCL_printf("WriteVivHeader: endianness: 0x%x\n", vd->__padding[0]);
  if (LIBNFSVIV_GetBitmapValue(&vd->__padding[0], 1))
    buf[0] = LIBNFSVIV_SwapEndian(buf[0]);
  if (LIBNFSVIV_GetBitmapValue(&vd->__padding[0], 2))
    buf[1] = LIBNFSVIV_SwapEndian(buf[0]);
  if (LIBNFSVIV_GetBitmapValue(&vd->__padding[0], 3))
    buf[2] = LIBNFSVIV_SwapEndian(buf[0]);

  err += fwrite(vd->format, 1, 4, file);
  err += fwrite(&buf[0], 1, 4, file);
  err += fwrite(&buf[1], 1, 4, file);
  err += fwrite(&buf[2], 1, 4, file);

  return err == 16;
}

/* Assumes (ftell(file) == 16). Updates vd->viv_hdr_size_true to ftell(file). */
static
int LIBNFSVIV_WriteVivDirectory(VivDirectory *vd, FILE *file,
                                char **infiles_paths, const int count_infiles,
                                const int opt_direnlenfixed, const int opt_filenameshex)
{
  int val;
  char buf[LIBNFSVIV_FilenameMaxLen] = {0};
  size_t size;
  int i;
  size_t err = 0;

  for (i = 0; i < count_infiles; ++i)
  {
    if (!LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i))
      continue;

    val = LIBNFSVIV_SwapEndian(vd->buffer[i].offset);
    err += fwrite(&val, 1, 4, file);

    val = LIBNFSVIV_SwapEndian(vd->buffer[i].filesize);
    err += fwrite(&val, 1, 4, file);

#ifdef _WIN32
    size = (int)GetLongPathName(infiles_paths[i], buf, LIBNFSVIV_FilenameMaxLen) + 1;  /* transform short paths that contain tilde (~) */
    if (size < 2 || size > LIBNFSVIV_FilenameMaxLen)
    {
      printf("WriteVivDirectory: Cannot get long path name for file '%s' (len=%d)\n", infiles_paths[i], (int)strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1);
      break;
    }
    {
      char *ptr = LIBNFSVIV_GetPathBasename(buf);
      size = strlen(ptr) + 1;
      memmove(buf, ptr, size);  /* includes nul-terminator */
    }
#else
    size = strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1;
    if (size < 2 || size > LIBNFSVIV_FilenameMaxLen - 1)
    {
      fprintf(stderr, "WriteVivDirectory: infile basename length incompatible (%d)\n", (int)size);
      return 0;
    }
    memcpy(buf, LIBNFSVIV_GetPathBasename(infiles_paths[i]), size);  /* includes nul-terminator */
#endif

    if (opt_filenameshex)
    {
      size = LIBNFSVIV_DecBase16(buf) + 1;
      if (size != (size_t)vd->buffer[i].filename_len_ + 1)
        fprintf(stderr, "Warning:WriteVivDirectory: Base16 conversion mishap (%d!=%d)\n", (int)size, vd->buffer[i].filename_len_ + 1);
    }

    err *= 0 < fwrite(buf, 1, size, file);

    if (opt_direnlenfixed > 10)
    {
      if (size > (size_t)opt_direnlenfixed)
      {
        fprintf(stderr, "WriteVivDirectory: Filename too long for fixed directory entry length (%d > %d)\n", (int)size, opt_direnlenfixed);
        return 0;
      }
      size += 0x08;
      while (err > 0 && size++ < (size_t)opt_direnlenfixed)
        err += fputc('\0', file);
    }
  }  /* for i */

  if (err != (size_t)vd->count_dir_entries * 8)
  {
    fprintf(stderr, "WriteVivDirectory: File write error\n");
    return 0;
  }

  vd->viv_hdr_size_true = (int)ftell(file);  /* used in format checks */
  if (vd->viv_hdr_size_true != vd->header_size)
  {
    fprintf(stderr, "WriteVivDirectory: output has invalid header size (%d!=%d)\n", vd->viv_hdr_size_true, vd->header_size);
    return 0;
  }

  return 1;
}

/*
  Chunked write from file at infile_path to outfile.
  Returns outfile filesize on success, -1 on failure.
*/
static
int LIBNFSVIV_VivWriteFile(FILE *outfile, const char * const infile_path, const int infile_size)
{
  int retv = 1;
  unsigned char buf[LIBNFSVIV_BufferSize];
  int curr_ofs;
  int curr_chunk_size = LIBNFSVIV_BufferSize;
  FILE *infile = fopen(infile_path, "rb");
  if (!infile)
  {
    fprintf(stderr, "VivWriteFile: Cannot open file '%s' (infile)\n", infile_path);
    return -1;
  }

  while (curr_chunk_size > 0)
  {
    curr_ofs = (int)ftell(infile);
    curr_chunk_size = LIBNFSVIV_min(LIBNFSVIV_BufferSize, infile_size - curr_ofs);

    if (fread(buf, 1, curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivWriteFile: File read error at %d in '%s' (infile)\n", curr_ofs, infile_path);
      retv = -1;
      break;
    }

    if (fwrite(buf, 1, curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivWriteFile: File write error at %d (outfile)\n", curr_chunk_size);
      retv = -1;
      break;
    }
  }

  fclose(infile);
  return (retv > 0) ? (int)ftell(outfile) : -1;
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
                                                 const int opt_filenameshex)
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
    if (!LIBNFSVIV_GetVivHdr(vd, file))
      break;
#ifdef UVT_UNVIVTOOLMODULE
    if (opt_verbose)
    {
#endif
    __LIBNFSVIV_PrintVivDirectoryStats_Header(vd);
#ifdef UVT_UNVIVTOOLMODULE
    }
#endif
    LIBNFSVIV_FixVivHdr(vd, filesz);
    if (!LIBNFSVIV_CheckVivHdr(vd, filesz))
      break;
    if (!LIBNFSVIV_GetVivDir(vd, filesz, file, opt_verbose, opt_direnlenfixed, opt_filenameshex))
      break;
    if (!LIBNFSVIV_CheckVivDirectory(vd, filesz))
    {
      LIBNFSVIV_PrintVivDirEntr(vd);
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
#ifdef UVT_UNVIVTOOLMODULE
    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsDec(vd, filesz, file, 0, NULL, opt_direnlenfixed, opt_filenameshex);
    }
#endif

    retv = vd;
    break;
  }
  return retv;
}

/* Wrapper for LIBNFSVIV_GetVivDirectory_FromFile() */
VivDirectory *LIBNFSVIV_GetVivDirectory(VivDirectory *vd, char * const path,
                                        const int opt_verbose, const int opt_direnlenfixed,
                                        const int opt_filenameshex)
{
  const int filesz = LIBNFSVIV_GetFilesize(path);
  FILE *file = path ? fopen(path, "rb") : NULL;
  VivDirectory *ret = LIBNFSVIV_GetVivDirectory_FromFile(vd, file, filesz, opt_verbose, opt_direnlenfixed, opt_filenameshex);
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
char **LIBNFSVIV_VivDirectoryToFileList_FromFile(VivDirectory *vd, FILE *file, const int filesz, const int opt_invalidentries)
{
  char **ret = NULL;
  if (LIBNFSVIV_ValidateVivDirectory(vd))  return NULL;

  if (!file)  return NULL;

  for (;;)
  {
    char **filelist;
    filelist = (char **)malloc((vd->count_dir_entries_true + 1) * sizeof(*filelist));
    if (!filelist)
    {
      fprintf(stderr, "VivDirectoryToFileList: Cannot allocate memory\n");
      break;
    }
    filelist[vd->count_dir_entries_true] = NULL;

    /* Create list of all filenames, even invalid ones.
       All values are clamped s.t. they do not exceed file size.

       Expects strings of length 1 at least
    */
    if (vd->count_dir_entries_true > 0 || (opt_invalidentries && vd->count_dir_entries > 0))
    {
      int filenames_sz;
      filenames_sz = LIBNFSVIV_SumVivDirectoryFilenameSizes(vd, opt_invalidentries);  /* guarantees (filenames_sz < vd->count_dir_entries_true) */
      *filelist = (char *)calloc(filenames_sz * sizeof(**filelist), 1);
      if (!*filelist)
      {
        fprintf(stderr, "VivDirectoryToFileList: Cannot allocate memory\n");
        free(filelist);
        break;
      }
      {
        /* DEBUG */ int sum_sizes = 0;
        int i;
        int cnt_entries = 0;
        char *p = *filelist;
        SCL_printf("!  p: %p, *filelist: %p, last: %p\n", p, *filelist, *filelist + (filenames_sz-1));
        for (i = 0; i < vd->count_dir_entries; ++i)
        {
          int len = vd->buffer[i].filename_len_;  /* length without nul */
          if (!opt_invalidentries && LIBNFSVIV_GetBitmapValue(vd->validity_bitmap, i) == 0)
            continue;
          SCL_assert(p <= *filelist + filenames_sz);
          if (p > *filelist + filenames_sz)
          {
            fprintf(stderr, "VivDirectoryToFileList: buffer overflow\n");
            fflush(0);
            free(*filelist);
            free(filelist);
            return NULL;
          }
          filelist[cnt_entries] = p;
          len = LIBNFSVIV_clamp(len, 0, filesz - (int)ftell(file));  /* read at least 0 bytes */
          fseek(file, LIBNFSVIV_clamp(vd->buffer[i].filename_ofs_, 0, filesz - len), SEEK_SET);  /* ensure fread() within file */
          SCL_printf("i %d fread(p, _, len, _) %p, %d, ftell(file) 0x%x\n", i, p, len, (int)ftell(file));
          /* DEBUG */ sum_sizes += len + 1;
          SCL_printf("count_lengths %d, filenames_sz %d\n", sum_sizes, filenames_sz);
          SCL_assert((int)ftell(file) == filesz + len);
          if ((int)fread(p, 1, len, file) != len)
          {
            fprintf(stderr, "VivDirectoryToFileList: File read error at %d\n", vd->buffer[i].filename_ofs_);
            free(*filelist);
            free(filelist);
            return NULL;
          }
          p += len;
          *p++ = '\0';
          ++cnt_entries;
        }
        SCL_assert(opt_invalidentries || cnt_entries == vd->count_dir_entries_true);
        SCL_assert(p == *filelist + filenames_sz);
        SCL_printf(".cnt_entries %d, vd->count_dir_entries_true %d, opt_invalidentries: %d\n", cnt_entries, vd->count_dir_entries_true, opt_invalidentries);
        SCL_printf(".sum_sizes %d, filenames_sz %d\n", sum_sizes, filenames_sz);
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
  const int filesz = LIBNFSVIV_GetFilesize(path);
  FILE *file = path ? fopen(path, "rb") : NULL;
  char **ret = LIBNFSVIV_VivDirectoryToFileList_FromFile(vd, file, filesz, opt_invalidentries);
  if (file)  fclose(file);
  return ret;
}


/* LIBNFSVIV_Unviv() and LIBNFSVIV_Viv() are one-and-done functions */

/*
  Assumes viv_name and outpath are NOT const's and have size >= LIBNFSVIV_FilenameMaxLen
  Assumes (viv_name). Assumes (outpath). Overwrites directory 'outpath'.
  Changes working directory to 'outpath'.

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
    }

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
    if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd, file, viv_filesize, opt_verbose, opt_direnlenfixed, local_opt_filenameshex))
      break;
    LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths(&vd, viv_name, outpath, file, viv_filesize);  /* invalidate files that would overwrite archive */

    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_GetIdxFromFname(&vd, file, viv_filesize, request_file_name);
      if (request_file_idx <= 0)
      {
        break;
      }
    }

    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsDec(&vd, viv_filesize, file, request_file_idx, request_file_name, opt_direnlenfixed, local_opt_filenameshex);
    }

    if (opt_dryrun)
    {
      printf("End dry run\n");
      retv = 1;
      break;
    }

    if (!opt_dryrun && opt_wenccommand && wenc_buf)  /* Option: Write re-Encode command to file */
    {
      wenc_f = fopen(wenc_buf, "a");
      if (!wenc_f)
      {
        fprintf(stderr, "Unviv: Cannot open '%s' (option -we)\n", wenc_buf);
      }
      else
      {
        if (strncmp(vd.format, "BIGF", 4))
          fprintf(wenc_f, "%s %.4s ", "-fmt", vd.format);
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

    if (request_file_idx != 0)
    {
      if ((request_file_idx < 0) || (request_file_idx > vd.count_dir_entries_true))
      {
        fprintf(stderr, "Unviv: Requested idx (%d) out of bounds (1-based index)\n", request_file_idx);
        break;
      }
      if (LIBNFSVIV_GetBitmapValue(vd.validity_bitmap, request_file_idx - 1) != 1)
      {
        fprintf(stderr, "Unviv: Requested idx (%d) is an invalid entry\n", request_file_idx);
        break;
      }
      if (!LIBNFSVIV_VivExtractFile(vd.buffer[request_file_idx - 1], viv_filesize, file, local_opt_filenameshex, opt_overwrite, wenc_f, outpath))
      {
        break;
      }
      ++count_extracted;
    }
    else
    {
      for (i = 0; i < vd.count_dir_entries_true; ++i)
      {
        if (LIBNFSVIV_GetBitmapValue(vd.validity_bitmap, i) == 1)
        {
          /* Continue extracting through failures */
          if (LIBNFSVIV_VivExtractFile(vd.buffer[i], viv_filesize, file, local_opt_filenameshex, opt_overwrite, wenc_f, outpath))
          {
            ++count_extracted;
          }
        }
      }
    }

    retv = 1;
    break;
  }  /* for (;;) */

  if (!opt_dryrun)
    printf("Number extracted: %d\n", count_extracted);

  if (wenc_f)
  {
    fprintf(wenc_f, "\n");
    fflush(wenc_f);
    fclose(wenc_f);
  }
  /* if (wenc_buf)
    free(wenc_buf); */  /* already free'd */
  if (file)
    fclose(file);
  LIBNFSVIV_FreeVivDirectory(&vd);

  return retv;
}

/*
  Assumes viv_name is NOT const and has size >= LIBNFSVIV_FilenameMaxLen
  Assumes (viv_name). Overwrites file 'viv_name'. Skips unopenable infiles.
  Assumes (opt_requestfmt).
*/
int LIBNFSVIV_Viv(const char *viv_name,
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
  VivDirectory vd = {
    {0}, 0, 0, 0,
    0, 0,
    0, 0, NULL, NULL,
    {0}
  };

#if SCL_DEBUG >= 1
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

  if (count_infiles > LIBNFSVIV_DirEntrMax)
  {
    fprintf(stderr, "Viv: Number of files to encode too large (%d > %d)\n", count_infiles, LIBNFSVIV_DirEntrMax);
    return 0;
  }
  else if (count_infiles < 1)
  {
    return 1;
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
      fprintf(stderr, "Viv(): Cannot write Viv header\n");
      retv = 0;
      break;
    }
    if (!LIBNFSVIV_WriteVivDirectory(&vd, file, infiles_paths, count_infiles, opt_direnlenfixed, opt_filenameshex))
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
        filesz = LIBNFSVIV_VivWriteFile(file, infiles_paths[i], vd.buffer[i].filesize);
        if (filesz < 0)
        {
          retv = 0;
          break;
        }
      }
    }

    /* Validate */
    printf("Archive Size (written) = %d (0x%x)\n", filesz, filesz);
    if (!LIBNFSVIV_CheckVivHdr(&vd, filesz))
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

  if (file)
    fclose(file);
  LIBNFSVIV_FreeVivDirectory(&vd);

  return retv;
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LIBNFSVIV_H_ */
