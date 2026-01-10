/*
  libnfsviv.h - BIGF BIGH BIG4 0xFBC0 decoder/encoder (commonly known as VIV/BIG)
  unvivtool Copyright (C) 2020 and later Benjamin Futasz <https://github.com/bfut>

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
  Compiling:
    * Win98+ (MSVC 6.0+)|Linux|macOS, little endian, 32-bit|64-bit, sizeof(int)=sizeof(float)=4
    * non-Win32 requires _GNU_SOURCE #define'd for realpath()
    * optionally #define UVTUTF8 for the UVTUTF8 branch (decoder supports utf8-filenames within archive header), forces dfa.h dependency
    * optionally #define UVTWWWW for 'wwww' file format support (enabled by default)

    The API in this header-only library is composed of two parts:

  1. LIBNFSVIV_Unviv() and LIBNFSVIV_Viv() are one-and-done functions
  2. Data analysis via
    LIBNFSVIV_GetVivVersion*()
    LIBNFSVIV_GetVivDirectory*() - returns struct *UVT_Directory, the archive header
    LIBNFSVIV_VivDirectoryToFileList*() - returns char** of filenames listed in the archive header

  The decoder performs a single pass buffered read of the archive header only.
  The encoder allows archiving in user-determined order.
  A known BIGF variation with fixed directory entry length and non-printable filenames is supported in a first.

  Supported formats:
    BIGF, BIGF, BIG4, 0xFBC0, wwww

  BIGF theoretical limits, assuming signed int:
    min header len:          16         0x10
    max header len:          2147483631 0x7fffffef
    min directory entry len: 10         0xa
    min dir entries count:   0
    max dir entries count:   214748363  0x0ccccccb

  BIGF BIGH 0xFBC0 headers usually contain big-endian numeric values.

  Special cases:
    archive header can have filesize encoded in little endian
    BIGF header can have a fixed directory entry length (e.g., 80 bytes). This allows names with embedded nul's.

  LIBNFSVIV_unviv() handles the following format deviations {with strategy}:
    * Archive header has incorrect filesize {value unused}
    * Archive header has incorrect number of directory entries {check endianness and/or assume large enough value}
    * Archive header has incorrect number for directory length {value unused}
    * Archive header has incorrect offset {value unused}
    * At least 1 directory entry has illegal offset or length {skip file}
    * Two directory entries have the same file name (use opt->overwrite == 1) {overwrite or rename existing}
    * Directory entry would overwrite archive on extraction {skip file}
    * Directory entry file name contains non-ASCII UTF8 characters {native support in UVTUTF8-branch}
    * Directory entry file name contains non-printable characters (use opt->filenameshex == 1) {skip file or represent filename in base16}
    * Directory entry file name is too long {skip file}
    * Directory entry has fixed length and filename string is followed by large number of nul's (use opt->direnlenfixed == sz) {native support via option opt->direnlenfixed}
*/

#ifndef LIBNFSVIV_H_
#define LIBNFSVIV_H_

#ifndef SCL_DEBUG
#define SCL_DEBUG 0  /* MSG LEVEL: -1 total silence; 0 default; >=1 dev console output */
#endif

#if defined(_MSC_VER)
#define SCL_UNUSED __pragma(warning(suppress:4100 4101))
#elif __cplusplus >= 201703L
#define SCL_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
#define SCL_UNUSED __attribute__((unused))
#else
#define SCL_UNUSED
#endif

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
/* #if defined(_WIN32) && defined(_MSC_VER) */
  #include <windows.h>  /* GetFullPathNameA, GetLongPathNameA */
  #include <direct.h>
  #ifndef memccpy
  #define memccpy _memccpy
  #endif
  #ifndef chdir
  #define chdir _chdir
  #endif
  #ifndef getcwd
  #define getcwd _getcwd
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
  #ifndef S_IFREG
  #define S_IFREG _S_IFREG
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

#if 1
#ifdef _WIN32  /* for LIBNFSVIV_GetVivVersion_FromPath() */
  #include <io.h>
  #include <fcntl.h>
  #ifndef open
  #define open _open
  #endif
  #ifndef read
  #define read _read
  #endif
  #ifndef close
  #define close _close
  #endif
  #ifndef O_RDONLY
  #define O_RDONLY _O_RDONLY
  #endif
#endif
#endif

#if defined(_WIN32)  /* for LIBNFSVIV_CopyFile() */
  #include <windows.h>  /* CopyFile() */
#elif defined(__APPLE__)
  #include <copyfile.h>  /* copyfile() */
  #include <fcntl.h>  /* open() */
  #include <unistd.h>  /* read(), close() */
#else
  #include <fcntl.h>
  #include <sys/sendfile.h>  /* sendfile() */
  #include <sys/stat.h>
  #include <unistd.h>
#endif

#ifndef SCL_LOG_H_
#define SCL_LOG_H_
/* #define SCL_printf_silent(format, ...) ((void)0) */  /* C89-incompatible */
SCL_UNUSED static void SCL_printf_silent(const char *format, ...) { (void)format; }
SCL_UNUSED static void SCL_fprintf_silent(FILE *stream, const char *format, ...) { (void)stream; (void)format; }
SCL_UNUSED void SCL_printf_msg(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}
SCL_UNUSED void SCL_fprintf_msg(FILE *stream, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stream, format, args);
  va_end(args);
}

#if defined(SCL_DEBUG) && SCL_DEBUG > 0
  #include <assert.h>
  #define SCL_log printf
  #define SCL_flog fprintf
  #define SCL_assert assert
#else
  #define SCL_log SCL_printf_silent
  #define SCL_flog SCL_fprintf_silent
  #define SCL_assert(x)
#endif
#endif /* SCL_LOG_H_ */

#if SCL_DEBUG < 0
SCL_UNUSED static void (*UVT_printf)(const char *format, ...) = SCL_printf_silent;
SCL_UNUSED static void (*UVT_fprintf)(FILE *stream, const char *format, ...) = SCL_fprintf_silent;
#else
SCL_UNUSED static void (*UVT_printf)(const char *format, ...) = SCL_printf_msg;
SCL_UNUSED static void (*UVT_fprintf)(FILE *stream, const char *format, ...) = SCL_fprintf_msg;
#endif

#define UVTVERS "3.12"
#define UVTCOPYRIGHT "Copyright (C) 2020 and later Benjamin Futasz (GPLv3+)"

#ifdef UVTUTF8  /* optional branch: unviv() utf8-filename support */
#include "./include/dfa.h"
#endif
#ifndef UVTWWWW  /* optional branch: 'wwww' file format support */
#define UVTWWWW
#endif

#define LIBNFSVIV_max(x,y) ((x)>(y)?(x):(y))
#define LIBNFSVIV_min(x,y) ((x)<(y)?(x):(y))
#define LIBNFSVIV_clamp(x,minv,maxv) ((maxv)<(minv)||(x)<(minv)?(minv):((x)>(maxv)?(maxv):(x)))
#define LIBNFSVIV_ceil(x,y) (((x)-1)/(y)+1)  /* ceil(x/y) for x,y > 0; use ceil0() for x=0 */
#define LIBNFSVIV_ceil0(x,y) (((x)+(y)-1)/(y))  /* ceil(x/y) for x>=0,y>0 */
#define LIBNFSVIV_tomultiple(x, n) ((x+((n)-1))&~((n)-1))  /* increase x to a multiple of n, for n power of 2 */
#define LIBNFSVIV_pow2divisible(x, n) (((x)&((n)-1)) == 0)  /* true if x is divisible by n, for n power of 2 */

#if defined(__GNUC__) || defined(__clang__)
  #define LIBNFSVIV_likely(x) (__builtin_expect(!!(x),1))
  #define LIBNFSVIV_unlikely(x) (__builtin_expect(!!(x),0))
#else
  #define LIBNFSVIV_likely(x) (!!(x))
  #define LIBNFSVIV_unlikely(x) (!!(x))
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
#define LIBNFSVIV_DirEntrMax 1572864  /* (LIBNFSVIV_ceil( ((1024 << 10) * 24), 16)) */

#define LIBNFSVIV_CircBuf_Size LIBNFSVIV_BufferSize  /* must be at least 16 */
/* #define LIBNFSVIV_CircBuf_Size 0x10 + 16 */

#define LIBNFSVIV_WENCFileEnding ".txt"

#ifndef __cplusplus
#define LIBNFSVIV_extern
#else
#define LIBNFSVIV_extern extern "C"
#endif

#ifndef __cplusplus
typedef struct UVT_UnvivVivOpt UVT_UnvivVivOpt;
typedef struct UVT_DirEntr UVT_DirEntr;
typedef struct UVT_Directory UVT_Directory;
#endif

struct UVT_UnvivVivOpt {
  int dryrun;
  int verbose;
  int direnlenfixed;
  int filenameshex;
  int wenccommand;
  int overwrite;
  int requestendian;
  int faithfulencode;
  int insert;  /* Update() */
  int replacefilename;  /* Update() */
  int alignfofs;  /* Viv() */
  char requestfmt[5];
};

struct UVT_DirEntr {
  int e_offset;
  int e_filesize;
  int e_fname_ofs_;
  int e_fname_len_;  /* string length without nul */
};

#if defined(_WIN64) || defined(__LP64__) || defined(_M_X64) || defined(__x86_64__)
#define LIBNFSVIV_VivDirectoryPaddingSize 12
#else
#define LIBNFSVIV_VivDirectoryPaddingSize 20
#endif
struct UVT_Directory {
  char format[4];  /* BIGF|BIGH|BIG4|0x8000FBC0|wwww */
  int h_filesize;
  int num_direntries;  /* per header */
  int header_size;  /* per header. includes directory entries */

  int num_direntries_true;  /* parsed entries count, includes invalid */
  int viv_hdr_size_true;  /* parsed unpadded. includes directory entries. filename lengths include nul */

  int length;  /* allocated length of buffer */
  int null_count;
  char *bitmap;  /* len >= ceil(length/8) */
  UVT_DirEntr *buffer;

  /*
    sizeof = 64 byte
    char[0] bitfield: 0 unused, 1-3 True for big-endianness, 5-7 alignment of file offsets (upper 4 bits indicate powers of 2)
    char[>0], used for bitmap if length sufficiently small
  */
  char __padding[LIBNFSVIV_VivDirectoryPaddingSize];
};

#if LIBNFSVIV_CircBuf_Size < 16
#error "LIBNFSVIV_CircBuf_Size must be at least 16"
#endif
#if LIBNFSVIV_VivDirectoryPaddingSize < 1
#error "LIBNFSVIV_VivDirectoryPaddingSize must be greater than 0"
#endif

/* api -------------------------------------------------------------------------------------------------------------- */

LIBNFSVIV_extern const char *LIBNFSVIV_GetVivVersionString(const int version);
LIBNFSVIV_extern int LIBNFSVIV_GetVivVersion_FromBuf(const char * const buf);

/* util ------------------------------------------------------------------------------------------------------------- */
/* util: BIGF --------------------------------------------------------------- */

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
  if (opt_verbose && temp_ != opt_direnlenfixed)  UVT_printf("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)\n", opt_direnlenfixed, opt_direnlenfixed, LIBNFSVIV_BufferSize + 16 - 1);
  return opt_direnlenfixed;
}

#if SCL_DEBUG > 0
void LIBNFSVIV_PrintUnvivVivOpt(const UVT_UnvivVivOpt * const opt)
{
  SCL_log(" dryrun=%d\n", opt->dryrun);
  SCL_log(" verbose=%d\n", opt->verbose);
  SCL_log(" direnlenfixed=%d\n", opt->direnlenfixed);
  SCL_log(" filenameshex=%d\n", opt->filenameshex);
  SCL_log(" wenccommand=%d\n", opt->wenccommand);
  SCL_log(" overwrite=%d\n", opt->overwrite);
  SCL_log(" requestendian=%d\n", opt->requestendian);
  SCL_log(" faithfulencode=%d\n", opt->faithfulencode);
  SCL_log(" insert=%d\n", opt->insert);
  SCL_log(" replacefilename=%d\n", opt->replacefilename);
  SCL_log(" alignfofs=%d\n", opt->alignfofs);
  SCL_log(" requestfmt='%.5s'\n", opt->requestfmt);
}
#else
#define LIBNFSVIV_PrintUnvivVivOpt(opt)
#endif

/* util: SCL_BITMAP --------------------------------------------------------- */

static
void SCL_BITMAP_Set(char *bitmap, const int idx)
{
  bitmap[idx >> 3] |= 1 << (idx & 7);
}

static
void SCL_BITMAP_Unset(char *bitmap, const int idx)
{
  bitmap[idx >> 3] &= ~(1 << (idx & 7));
}

static
char SCL_BITMAP_IsSet(const char *bitmap, const int idx)
{
  return (bitmap[idx >> 3] >> (idx & 7)) & 1;
}

/* util: UVT_Directory ------------------------------------------------------ */

#if 1
/* free's buffer and bitmap */
static
void LIBNFSVIV_UVT_DirectoryRelease(UVT_Directory *vd)
{
  free(vd->buffer);
}

/* Calloc wrapper. Assumes vd was memset to 0 previously. */
static
UVT_Directory *LIBNFSVIV_UVT_DirectoryInit(UVT_Directory *vd, const int len)
{
  if (len < 0 || len > LIBNFSVIV_DirEntrMax)  return NULL;
  if (len > 0)
  {
    vd->length = len + (len & 1);  /* 2*sizeof(UVT_DirEntr) == 32 */

    if (LIBNFSVIV_likely(vd->length <= (LIBNFSVIV_VivDirectoryPaddingSize - 1) * 8))
    {
      vd->buffer = (UVT_DirEntr *)calloc(vd->length * sizeof(*vd->buffer), 1);
      vd->bitmap = vd->__padding + 1;
    }
    else
    {
      const int bitmapsz = (int)((((unsigned int)vd->length + 255) >> 8) << 5);  /* = ((x+255)/256)*32 = tomultiple(ceil(vd->length, 8), 32) */
      vd->buffer = (UVT_DirEntr *)calloc(vd->length * sizeof(*vd->buffer) + bitmapsz * sizeof(*vd->bitmap), 1);
      vd->bitmap = (char *)vd->buffer + (vd->length * sizeof(*vd->buffer));
    }
  }

  return vd;
}

#else
static
void *LIBNFSVIV_UVT_DirectoryCallocBitmap(UVT_Directory *vd)
{
  if (vd->length <= (LIBNFSVIV_VivDirectoryPaddingSize - 1) * 8)
  {
    char *ptr = vd->__padding + 1;
    memset(ptr, 0, sizeof(vd->__padding) - 1);
    return ptr;
  }
  else
    return calloc(LIBNFSVIV_tomultiple(LIBNFSVIV_ceil(vd->length, 8), 32) * sizeof(*vd->bitmap), 1);
}

static
void LIBNFSVIV_UVT_DirectoryFreeBitmap(UVT_Directory *vd)
{
  if (vd->length > (LIBNFSVIV_VivDirectoryPaddingSize - 1) * 8)  free(vd->bitmap);
}

/* free's buffer and bitmap */
static
void LIBNFSVIV_UVT_DirectoryRelease(UVT_Directory *vd)
{
  free(vd->buffer);
  LIBNFSVIV_UVT_DirectoryFreeBitmap(vd);
}

static
UVT_Directory *LIBNFSVIV_UVT_DirectoryInit(UVT_Directory *vd, const int len)
{
  if (len < 0 || len > LIBNFSVIV_DirEntrMax)  return NULL;
  vd->length = len + (len & 1);  /* 2*sizeof(UVT_DirEntr) == 32 */
  vd->bitmap = (char *)LIBNFSVIV_UVT_DirectoryCallocBitmap(vd);
  vd->buffer = (UVT_DirEntr *)calloc(vd->length * sizeof(*vd->buffer), 1);
  if (LIBNFSVIV_unlikely(!vd->buffer) || LIBNFSVIV_unlikely(!vd->bitmap))
  {
    free(vd->bitmap);
    return NULL;
  }
  return vd;
}
#endif



/* util: misc --------------------------------------------------------------- */

SCL_UNUSED static
unsigned int SCL_deseri_uint(const void *src)
{
  unsigned int val;
  memcpy(&val, src, sizeof(val));
  return val;
}

SCL_UNUSED static
void SCL_seri_uint(void *dest, const unsigned int val)
{
  memcpy(dest, &val, sizeof(val));
}

#if defined(_WIN32) || __STDC_VERSION__ >= 202311L || POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700 || _BSD_SOURCE || _SVID_SOURCE || _DEFAULT_SOURCE
#define LIBNFSVIV_memccpy(dest, src, c, count) memccpy(dest, src, c, count)
#else
/*
  Returns pointer to byte after (c) in (dest), or NULL if (c) not found in first (count) bytes.

  Assumes (dest) and (src) do not overlap.
  Assumes dest is large enough to hold count bytes.

  Does not zero-pad the destination array.
*/
static
void *LIBNFSVIV_memccpy(void* dest, const void* src, int c, size_t count)
{
  unsigned char* d = (unsigned char*)dest;
  const unsigned char* s = (const unsigned char*)src;
  size_t i;
  for (i = 0; i < count; i++)
  {
    *d++ = *s;
    if (*s++ == (unsigned char)c)  return d;
  }
  return NULL;
}
#endif

#if defined(UVTUTF8)
/* Returns UTF8 length without nul. */
static
int LIBNFSVIV_IsPrintString(const void *__s, const size_t max_len)
{
  unsigned char *s = (unsigned char *)__s;
  size_t pos = 0;
  unsigned int codepoint, state = 0;
  if (!__s)  return 0;
  while ((*s != 0) && !(state == UTF8_REJECT) && (pos < max_len))
  {
    DFA_decode(&state, &codepoint, *s++);
    ++pos;
  }
  return (int)pos * (pos <= max_len) * (state == UTF8_ACCEPT);
}
#else
/* Returns isprint() length without nul. */
#define SCL_ISPRINT_REJECT 0
static
int LIBNFSVIV_IsPrintString(const void *__s, const size_t max_len)
{
  unsigned char *s = (unsigned char *)__s;
  size_t pos = 0;
  int state = 1;
  if (!__s)  return 0;
  while ((*s != 0) && !(state == SCL_ISPRINT_REJECT) && (pos < max_len))
  {
    state = isprint(*s++);
    ++pos;
  }
  return (int)pos * (pos <= max_len) * (state != SCL_ISPRINT_REJECT);
}
#undef SCL_ISPRINT_REJECT
#endif

static
int LIBNFSVIV_SwapEndian(const int y)
{
  unsigned int x = y;
  x = ((x >> 24) & 0x000000ff) | ((x << 24) & 0xff000000)
     | ((x >> 8) & 0x0000ff00) | ((x << 8) & 0x00ff0000);
  return x;
}

#if 0
/* Assumes (b) and sz divisible by 2. */
SCL_UNUSED static
void SCL_SwapEndian(unsigned char *b, const int sz)
{
  int i;
  for (i = 0; i < sz / 2; i++)
  {
    const unsigned char t = b[i];
    b[i] = b[sz - i - 1];
    b[sz - i - 1] = t;
  }
}
#endif

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

/* Writes up to 16 null bytes to (f) until ftell(f) == ofs */
static
void LIBNFSVIV_WriteNullBytes(FILE *f, int ofs)
{
  char buf[16];
  memset(buf, 0, sizeof(buf));
  ofs -= (int)ftell(f);
  if (ofs > 0 && ofs <= (int)sizeof(buf))  { fwrite(buf, 1, ofs, f); fflush(f); }
}

/* Assumes input \in 0123456ABCDEFabcdef */
static
int LIBNFSVIV_hextoint(const char in)
{
  if (in >= '0' && in <= '9')  return in - '0';
  if (in >= 'a' && in <= 'f')  return in - 'a' + 10;
  if (in >= 'A' && in <= 'F')  return in - 'A' + 10;
  return 0;
}

/* Assumes (0x00 <= in <= 0x0F). Returns upper-case hexadecimal. */
static
char LIBNFSVIV_inttohex(const int in)
{
  if (in >= 0 && in < 0xA)  return in + '0';
  if (in >= 0xA && in <= 0xF)  return in + 'A' - 10;  /* return in + 'a' - 10; */
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
    buf[i] = (unsigned int)LIBNFSVIV_hextoint(*ptr) << 4;
    buf[i] += LIBNFSVIV_hextoint(*(ptr + 1));
    ptr += 2;
    i++;
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

/*
  On success, returns strlen() > 0.
  filename is not changed; except on Windows, converts '\\' to '/' and transforms short paths that contain tilde (~) to long paths.

  For buf, you can use the same buffer you used for the filename parameter.
  On Windows, assumes (buf) and buf is at least bufsz bytes long, with bufsz > 1.
  Elsewhere, (buf) and bufsz are ignored.

  startptr is updated to point to the start of the basename, within filename (or buf on Windows).
  If startptr is NULL, it is ignored.
*/
static
#ifdef _WIN32
int LIBNFSVIV_GetPathBasename2(char *filename, char **startptr, char *buf, const int bufsz)
{
  char *sptr;
  /*
    GetLongPathName() transforms short paths that contain tilde (~)
    GetLongPathName() returns 0 on error, otherwise strlen().
  */
  int len = (int)GetLongPathName(filename, buf, bufsz);
  if (len >= 1 && len < bufsz - 1)
  {
    sptr = LIBNFSVIV_GetPathBasename(buf);
    len = (int)strlen(sptr);
  }
  else
  {
    UVT_fprintf(stderr, "Warning:GetPathBasename2: Cannot get long path name for file '%s': %d (strlen=%d)\n", filename, len, (int)strlen(LIBNFSVIV_GetPathBasename(filename)));
    sptr = LIBNFSVIV_GetPathBasename(filename);
    len = (int)strlen(LIBNFSVIV_GetPathBasename(sptr));
  }
  if (startptr)  *startptr = sptr;
  return len;
}
#else
int LIBNFSVIV_GetPathBasename2(char * filename, char **startptr, char *_, const int __)
{
  char *sptr = LIBNFSVIV_GetPathBasename(filename);
  (void)_;
  (void)__;
  if (startptr)  *startptr = sptr;
  return (int)strlen(LIBNFSVIV_GetPathBasename(sptr));
}
#endif

/* Returns -1 on error. Allows (path==NULL). */
static
int LIBNFSVIV_GetFilesize(const char *path)
{
  struct stat sb;
  return !!path && !stat(path, &sb) ? (int)sb.st_size : -1;
}

/* Returns: 1 for file, 2 for symlink to file, else 0 */
static
int LIBNFSVIV_IsFile(const char *path)
{
  struct stat sb;
#if !defined(S_IFREG)
  if (!stat(path, &sb) && S_ISREG(sb.st_mode))
#else
  if (!stat(path, &sb) && (sb.st_mode & S_IFMT) == S_IFREG)
#endif
    return 1;
#ifdef _WIN32
  else
    return 0;
#else
  /* resolve possible symlink */
  {
    int ret;
    char *buf = realpath(path, NULL);
    if (!buf) return 0;
#if !defined(S_IFREG)
    ret = (!stat(buf, &sb) && S_ISREG(sb.st_mode)) ? 2 : 0;
#else
    ret = (!stat(buf, &sb) && (sb.st_mode & S_IFMT) == S_IFREG) ? 2 : 0;
#endif
    free(buf);
    return ret;
  }
#endif
}

static
int LIBNFSVIV_IsDir(const char *path)
{
  struct stat sb;
#if !defined(S_IFDIR)
  return (!stat(path, &sb) && S_ISDIR(sb.st_mode)) ? 1 : 0;
#else
  return (!stat(path, &sb) && (sb.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
#endif
}

/* Assumes !buf. Removes trailing '/'. Returned path never ends on '/' */
static
void LIBNFSVIV_GetParentDir(char *buf)
{
  char *ptr = buf + strlen(buf) - 1;
  if (ptr[0] == '/')  ptr[0] = '\0';
  ptr = strrchr(buf, '/');
  if (ptr)  ptr[0] = '\0';
  else
  {
    buf[0] = '.';
    buf[1] = '\0';
  }
}

#ifdef _WIN32
/* Wrapper for GetFullPathName()
  If (!dest): updates src, returns updated src, keeps (!dest) unchanged and ignores nBufferLength.
  Else: updates dest, returns updated dest, keeps src unchanged.

  Returns NULL if src does not exist.

 [out] lpFilePart
A pointer to a buffer that receives the address (within lpBuffer) of the final file name component in the path.
This parameter can be NULL.
If lpBuffer refers to a directory and not a file, lpFilePart receives zero. */
static
char *LIBNFSVIV_GetFullPathName(char *src, char *dest, const size_t nBufferLength, char **lpFilePart)
{
  int len;
  if (!LIBNFSVIV_IsFile(src) && !LIBNFSVIV_IsDir(src))  return NULL;  /* realpath() behavior */
  if (!dest)
  {
    char buf[LIBNFSVIV_FilenameMaxLen];
    len = (int)GetFullPathName(src, LIBNFSVIV_FilenameMaxLen, buf, lpFilePart);  /* returns length without nul */
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
    len = (int)GetFullPathName(src, nBufferLength, dest, lpFilePart);  /* returns length without nul */
    if (len == 0 || len >= nBufferLength)
      dest[0] = '\0';
    else
      LIBNFSVIV_BkwdToFwdSlash(dest);
    return dest;
  }
}
#else
/* Wrapper for realpath(const char *src, char *dest)
  If (!dest): updates src, returns updated src, keeps (!dest) unchanged.
  Else: updates dest, returns updated dest, keeps src unchanged.

  Returns NULL if src does not exist.

  Assumes (src) and sizeof(src) >= 4096 (PATH_MAX) and src is string.
  gcc -std=c89 requires sizeof(dest) >= 4096 to avoid buffer overflow */
static
char *LIBNFSVIV_GetFullPathName(char *src, char *dest, const size_t _, char **__)
{
  char *ptr;
  (void)_;
  (void)__;
  if (!dest)
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
    ptr = realpath(src, dest);
    if (!ptr)
      dest[0] = '\0';
  }
  return ptr;
}
#endif

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
  SCL_log("has mkdtemp()\n");
  if ((int)strlen(buf) + 8 > bufsz)  return -1;
  buf[strlen(buf) + 6] = '\0';
  memset(buf + strlen(buf), 'X', 6);
  if (!mkdtemp(buf))  return -1;  /* creates temp directory */
  sprintf(buf + strlen(buf), "/");
#else  /* non-Windows non-POSIX non-Python C89 fallback, assumes that last 6 characters are digits. quietly bail on failures */
  SCL_log("using tmpnam(), missing mkdtemp()\n");
  for (;;)
  {
    char temp[LIBNFSVIV_FilenameMaxLen];
    char *p_;
    if (!tmpnam(temp))  break;  /* generates temp filename */
    p_ = strrchr(temp, '/');
    if (!p_)  break;
    p_[strlen(p_)] = '\0';
    if ((int)strlen(buf) + (int)strlen(p_) > bufsz)  break;
    sprintf(buf + strlen(buf), "%s", p_);
    break;
  }
#endif
  SCL_log("LIBNFSVIV_GetTempPath: %s\n", buf);

  return (int)strlen(buf);
#endif
}

/* Returns !0 on success, 0 on failure. */
static
int LIBNFSVIV_CopyFile(char *lpExistingFileName, char *lpNewFileName, int bFailIfExists)
{
#ifdef _WIN32
  return (int)CopyFile(lpExistingFileName, lpNewFileName, bFailIfExists);  /* CopyFile() !0 on success */
#elif defined(__APPLE__)
  return copyfile(lpExistingFileName, lpNewFileName, NULL, COPYFILE_DATA | COPYFILE_XATTR | ((bFailIfExists) ? COPYFILE_EXCL : 0) | COPYFILE_NOFOLLOW) == 0;
#else
  int retv = 0;
  for (;;)
  {
    if (!lpExistingFileName || !lpNewFileName)  break;
    if (!LIBNFSVIV_IsDir(lpExistingFileName) && !LIBNFSVIV_IsDir(lpNewFileName))
    {
      int fd_in, fd_out;
      struct stat sb;
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
        if (sendfile(fd_out, fd_in, NULL, sb.st_size) == (int)sb.st_size && !ftruncate(fd_out, sb.st_size))  retv = (int)sb.st_size;
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
  Write len bytes from infile to outfile. Returns 1 on success, 0 on failure.
  Assumes (dest) and (src) and (buf) and bufsz > 0.
*/
static
int LIBNFSVIV_FileCopyData(FILE *dest, FILE *src, int len, char *buf, const int bufsz)
{
  int err = len;
  while (len > 0)
  {
    const int chunk = LIBNFSVIV_min(bufsz, len);
    len -= (int)fread(buf, 1, chunk, src);
    err -= (int)fwrite(buf, 1, chunk, dest);
  }
  return (len == 0) && (err == 0) && !ferror(src) && !ferror(dest);
}

/*
  Invalidates entries whose output path is identical to the archive.
  Assumes (vd) and both, viv_name and outpath are strings.
*/
static
void LIBNFSVIV_EnsureArchiveNotInUVT_DirectoryWritePaths(UVT_Directory *vd, char *viv_name, const char *outpath, FILE *viv, const size_t viv_sz)
{
  char buf[LIBNFSVIV_FilenameMaxLen];

  /** Case: viv parentdir != outpath -> return */
  LIBNFSVIV_memccpy(buf, viv_name, '\0', LIBNFSVIV_FilenameMaxLen);
  buf[LIBNFSVIV_FilenameMaxLen - 1] = '\0';
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
    for (i = 0; i < vd->num_direntries_true; i++)
    {
      if (SCL_BITMAP_IsSet(vd->bitmap, i))
      {
        fseek(viv, vd->buffer[i].e_fname_ofs_, SEEK_SET);
        chunk_size = LIBNFSVIV_min(viv_sz - vd->buffer[i].e_fname_ofs_, LIBNFSVIV_FilenameMaxLen);
        if ((int)fread(buf, 1, chunk_size, viv) != chunk_size)
        {
          UVT_fprintf(stderr, "EnsureArchiveNotInUVT_DirectoryWritePaths: File read error\n");
          break;
        }
        if (!strcmp(buf, viv_basename))
        {
          SCL_BITMAP_Unset(vd->bitmap, i);
          ++vd->null_count;
          UVT_fprintf(stderr, "Warning:EnsureArchiveNotInUVT_DirectoryWritePaths: Skip file '%s' (%d) (would overwrite this archive)\n", buf, i);
        }
      }
    }
  }
}

/*
  Renames "path/to/existing/file.ext" to "path/to/existing/file_N.ext" with N in 0..999

  Returns 0 on failure, !0 on success.
  Assumes (path) and path is string, sz is size of path buffer.

  Expects path is printable.
  Expects path is not a directory path and does not end on '/' (or '\\' on Windows).
*/
static
int LIBNFSVIV_IncrementFile(char *path, size_t sz, const int verbose)
{
  int retv = 0;
  const int len = (int)strlen(path);
  if (LIBNFSVIV_IsPrintString(path, sz) == len)
  {
    if (len < LIBNFSVIV_FilenameMaxLen - 32)
    {
      char buf[LIBNFSVIV_FilenameMaxLen];
      char *ptr = (char *)LIBNFSVIV_memccpy(buf, path, '\0', LIBNFSVIV_FilenameMaxLen);
      char *safety_ = strrchr(buf, '/');
#ifdef _WIN32
      safety_ = safety_ ? LIBNFSVIV_max(safety_, strrchr(buf, '\\')) : strrchr(buf, '\\');  /* can be NULL */
#endif
      if (ptr && ptr != buf && (ptr < buf + sizeof(buf)) && !LIBNFSVIV_IsDir(path) && (safety_ != ptr - 1))
      {
        /*
          handle extension if any
          safety_ ensures '.' is in filename not in path; does not truncate path/dirname./to/no_ext to path/dirname._N/to/no_ext
        */
        const char *ext = strrchr(path, '.');
        ptr = strrchr(buf, '.');  /* end of stem */
        if (!safety_ || ptr > safety_)
        {
          int i;
          if (ptr)
            ptr[0] = '\0';
          else
          {
            ptr = buf + len;
            ext = path + len;  /* no extension, hence we need '\0' */
          }
          for (i = 0; i < 1000; i++)
          {
            sprintf(ptr, "_%d%s", i, ext);  /* cannot overflow, we leave 32 bytes headroom */
            if (!LIBNFSVIV_IsFile(buf))
            {
              if (!rename(path, buf))
              {
                if (verbose)  UVT_printf("IncrementFile: Incremented existing file '%s' to '%s'\n", path, buf);
                retv = 1;
                break;
              }
            }
          }  /* for i */
        }  /* if (!safety_ ... */
      }  /* if */
    }
    if (verbose && !retv)  UVT_printf("IncrementFile: Cannot increment existing file '%s'\n", path);
  }
  else if (verbose)  UVT_printf("IncrementFile: Filename contains non-printable characters\n");
  return retv;
}

/*  Copies path to buf, appends extension to buf. Returns NULL on error.
  Assumes path is not a directory (does not end on '/' or '\\'). */
static
char *LIBNFSVIV_GetWENCPath(const char *path, char *buf, const size_t bufsz)
{
  char *ptr = (char *)LIBNFSVIV_memccpy(buf, path, '\0', bufsz);
  if (!ptr || (ptr > buf + bufsz - sizeof(LIBNFSVIV_WENCFileEnding)))
    return NULL;
  memcpy(ptr - 1, LIBNFSVIV_WENCFileEnding, sizeof(LIBNFSVIV_WENCFileEnding));
  return buf;
}

static
int LIBNFSVIV_PrevPower(int n)
{
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n - (n >> 1);
}

/* n==0 returns 0, otherwise round up to next power of two */
static
int LIBNFSVIV_NextPower(int n)
{
  if (n <= 0)  return 0;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n + 1;
}

static
int LIBNFSVIV_GetBitIndex(const int b)
{
  switch (b)
  {
    case 0:  return 0;
    case 1:  return 2;
    case 2:  return 4;
    case 4:  return 8;
    case 8:  return 16;
    default:  return 0;
  }
}

static
int LIBNFSVIV_GetIndexBit(const int i)
{
  switch (i)
  {
    case 0:  return 0;
    case 2:  return 1;
    case 4:  return 2;
    case 8:  return 4;
    case 16:  return 8;
    default:  return 0;
  }
}

/* util: CircBuf ---------------------------------------------------------------------------------------------------- */

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

#if SCL_DEBUG > 0
/* ignores wr */
static
int LIBNFSVIV_CircBuf_readtoend(const LIBNFSVIV_CircBuf * const cb)
{
  return cb->sz - cb->rd;
}
#endif

/* len is NOT upper-bounded by EOF */
static
int LIBNFSVIV_CircBuf_addFromFile(LIBNFSVIV_CircBuf *cb, FILE *file, const int filesz, int len)
{
  int written = 0;
  int wrlen1 = cb->sz - cb->wr;
  if (len < 0 || !cb->buf)  return -1;
  if (len > filesz)  len = filesz  /* - (int)ftell(file) */;
  if (len > cb->sz)  len = cb->sz;
  if (wrlen1 < len)
  {
    written += (int)fread(cb->buf + cb->wr, 1, wrlen1, file);
    written += (int)fread(cb->buf, 1, len - wrlen1, file);
#if SCL_DEBUG >= 3
    SCL_log("    circbuf_addFromFile() stats: len: %d, written: %d, cb->wr: %d\n", len, written, cb->wr);
#endif
    if (written != len)  return -1;
    cb->wr = len - wrlen1;
  }
  else
  {
    written += (int)fread(cb->buf + cb->wr, 1, len, file);
#if SCL_DEBUG >= 3
    SCL_log("    circbuf_addFromFile() stats: len: %d, written: %d, cb->wr: %d\n", len, written, cb->wr);
#endif
    if (written != len)  return -1;
    cb->wr += len;
  }
  cb->wr %= cb->sz;
#if SCL_DEBUG >= 3
  SCL_log("!   circbuf_addFromFile() stats: len: %d, written: %d, cb->wr: %d\n", len, written, cb->wr);
#endif
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
  if (rdlen1 < 0 || len < 0 || ofs < 0 || !cb->buf)  return 0;
  if (len > cb->sz)  len = cb->sz - ofs;
#if SCL_DEBUG >= 2
  if (rdlen1 < 0)  SCL_log("    circbuf_Peek(): rdlen1: %d, len: %d\n", rdlen1, len);
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
static
void SCL_debug_printbuf(void *buf, int sz, int readat, int writeat)
{
  int l = sz;
  unsigned char *p = (unsigned char *)buf;
  int pos = 0;
  SCL_log("  readat: %d, writeat: %d\n", readat, writeat);
  if (l < 0 || !buf)  return;
  while (l-- > 0)
  {
    if (pos == readat && pos == writeat)  SCL_log("#%02x ", *p++);
    else if (pos == readat)  SCL_log("r%02x ", *p++);
    else if (pos == writeat)  SCL_log("w%02x ", *p++);
    else  SCL_log(" %02x ", *p++);

    if (++pos % 0x10 == 0)  SCL_log("\n");
  }
  if (pos % 0x10 != 0)  SCL_log("\n");
}
#else
#define SCL_debug_printbuf(a,b,c,d)
#endif

static
int LIBNFSVIV_CircBuf_Get(LIBNFSVIV_CircBuf *cb, void *buf, const int ofs, int len)
{
  const int rd = LIBNFSVIV_CircBuf_Peek(cb, buf, ofs, len);
  cb->rd += rd;
  cb->rd %= cb->sz;
  return rd;
}

#if SCL_DEBUG > 2
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
  if (len <= 0)  return NULL;
  if (!cb->buf || cb->sz <= 0) return NULL;
  if (ofs > cb->sz)  { UVT_fprintf(stderr, "warning ofs\n"); return NULL; } /* ofs %= cb->sz; */  /* really an error */
  if (len > cb->sz)  { UVT_fprintf(stderr, "warning len\n"); return NULL; } /* len %= cb->sz; */  /* really an error */

  {
    int rdlen1;
    int rdofs = cb->rd + ofs;
    if (rdofs > cb->sz)  rdofs -= cb->sz;
    len -= ofs;
    rdlen1 = cb->sz - rdofs;
    if (rdlen1 < len)  /* r2e */
    {
      void *p = memchr(cb->buf + rdofs, c, rdlen1);
      return p ? p : memchr(cb->buf, c, len - rdlen1);
    }
    return memchr(cb->buf + rdofs, c, len);
  }
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
    SCL_log("    circbuf_PeekStrlen(): rdlen1: %d, len: %d\n", rdlen1, len);
    SCL_log("    circbuf_PeekStrlen(): stats: len: %d, cb->rd: %d, cb->wr: %d\n", len, cb->rd, cb->wr);
#endif
    return (ret < rdlen1) ? ret : ret + __LIBNFSVIV_CircBuf_strnlen(cb->buf, len - rdlen1);
  }
  return __LIBNFSVIV_CircBuf_strnlen(cb->buf + cb->rd + ofs, len);
}

#if defined(UVTUTF8)
/* Input optionally with terminating nul. Returns length without nul. */
static
int LIBNFSVIV_CircBuf_PeekIsPrint(LIBNFSVIV_CircBuf *cb, const int ofs, int len)
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
  if (rdlen1 < len)
  {
    while (!(state == UTF8_REJECT) && (pos < rdlen1) && *s)
    {
      DFA_decode(&state, &codepoint, *s++);
      ++pos;
    }
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

/* stats ------------------------------------------------------------------------------------------------------------ */

/*
  Sums clamped filename sizes plus nul's.
*/
static
int LIBNFSVIV_UVT_DirectorySumFilenameSizes(const UVT_Directory * const vd, const int opt_invalidentries)
{
  int sz = 0;
  int i;
  for (i = 0; i < vd->num_direntries_true; i++)
  {
    if (!opt_invalidentries && SCL_BITMAP_IsSet(vd->bitmap, i) == 0)
      continue;
    sz += LIBNFSVIV_clamp(vd->buffer[i].e_fname_len_, 0, LIBNFSVIV_FilenameMaxLen - 1);
    ++sz;  /* nul */
  }
  SCL_log("LIBNFSVIV_UVT_DirectorySumFilenameSizes: %d  (opt_invalidentries: %d)\n", sz, opt_invalidentries);
  return sz;
}

static
void LIBNFSVIV_UVT_DirEntrPrint(const UVT_Directory * const vd, const int opt_invalidentries)
{
  int i;
  /* int cnt; */
  UVT_printf("UVT_DirEntrPrint\n");

  UVT_printf("vd->num_direntries: %d\n", vd->num_direntries);
  UVT_printf("vd->num_direntries_true: %d\n", vd->num_direntries_true);
  UVT_printf("vd->length: %d\n", vd->length);
  UVT_printf("vd->null_count: %d\n", vd->null_count);
  UVT_printf("vd->header_size: %d\n", vd->header_size);
  UVT_printf("vd->viv_hdr_size_true: %d\n", vd->viv_hdr_size_true);
  UVT_printf("vd->h_filesize: %d\n", vd->h_filesize);
  UVT_printf("vd valid filenames strings size: %d\n", LIBNFSVIV_UVT_DirectorySumFilenameSizes(vd, 0));
  UVT_printf("vd filenames strings size: %d\n", LIBNFSVIV_UVT_DirectorySumFilenameSizes(vd, opt_invalidentries));
  UVT_printf("i     valid? offset          filesize        e_fname_ofs_        e_fname_len_\n");
  /* for (i = 0, cnt = 0; i < LIBNFSVIV_min(vd->length, 4096) && cnt < vd->num_direntries; i++) */
  for (i = 0; i < LIBNFSVIV_min(vd->length, 8192) && i < vd->num_direntries_true; i++)
  {
    /* cnt += SCL_BITMAP_IsSet(vd->bitmap, i); */
    if (!SCL_BITMAP_IsSet(vd->bitmap, i))
      continue;
    UVT_printf("%2d     %d     %d (0x%x)   %d (0x%x)       %d (0x%x)       %d (nul: 0x%x)\n",
               i, SCL_BITMAP_IsSet(vd->bitmap, i),
               vd->buffer[i].e_offset, vd->buffer[i].e_offset,
               vd->buffer[i].e_filesize, vd->buffer[i].e_filesize,
               vd->buffer[i].e_fname_ofs_, vd->buffer[i].e_fname_ofs_,
               vd->buffer[i].e_fname_len_, vd->buffer[i].e_fname_ofs_ + vd->buffer[i].e_fname_len_ - 1);
  }
}

static
void __LIBNFSVIV_UVT_DirectoryPrintStats_Header(UVT_Directory * const vd, const int iswwww)
{
  const int version = LIBNFSVIV_GetVivVersion_FromBuf(vd->format);
  UVT_printf("File format (header) = %.4s\n", version > 0 ? LIBNFSVIV_GetVivVersionString(version) : "....");
  if (!iswwww)  UVT_printf("Archive Size (header) = %d (0x%x)\n", vd->h_filesize, vd->h_filesize);
  UVT_printf("Directory Entries (header) = %d\n", vd->num_direntries);
  if (!iswwww)  UVT_printf("Header Size (header) = %d (0x%x)\n", vd->header_size, vd->header_size);
}

static
void __LIBNFSVIV_UVT_DirectoryPrintStats_Parsed(UVT_Directory * const vd)
{
  {
    int fsz = 0;
    int i;
    for (i = 0; i < vd->num_direntries; i++)
    {
      if (SCL_BITMAP_IsSet(vd->bitmap, i))  fsz += vd->buffer[i].e_filesize;
    }
    UVT_printf("Archive Size (fsizes) = %d (0x%x)\n", fsz, fsz);
  }
  UVT_printf("Header Size (parsed) = %d (0x%x)\n", vd->viv_hdr_size_true, vd->viv_hdr_size_true);
  UVT_printf("Directory Entries (parsed) = %d\n", vd->num_direntries_true);
  UVT_printf("Endianness (parsed) = 0x%x\n", vd->__padding[0] & 0xE);
  UVT_printf("File offset alignment (parsed) = %d\n", LIBNFSVIV_GetBitIndex(vd->__padding[0] >> 4));
}

/**
  Assumes *file is associated with a file with size viv_filesize.
*/
static
void LIBNFSVIV_PrintStatsDec(UVT_Directory * const vd, FILE *file, const int viv_filesize,
                             const int request_file_idx, const char * const request_file_name,
                             const UVT_UnvivVivOpt * const opt)
{
  int i;
  int contents_size = 0;
  size_t sz;
  const char iswwww = memcmp(vd->format, "wwww", 4) ? 0 : 1;
  char *filename = NULL;

  if (LIBNFSVIV_min(viv_filesize, vd->viv_hdr_size_true) < 16)
  {
    UVT_printf("Empty file\n");
    return;
  }

  UVT_printf("Invalid Entries = %d\n", vd->null_count);
  UVT_printf("Buffer = %d\n", LIBNFSVIV_BufferSize);
  if (opt->direnlenfixed >= 10)
    UVT_printf("Fixed directory entry length: %d\n", opt->direnlenfixed);
  UVT_printf("Filenames as hex: %d\n", opt->filenameshex);
  if (request_file_idx)
    UVT_printf("Requested file idx = %d\n", request_file_idx);
  if ((request_file_name) && (request_file_name[0] != '\0'))
    UVT_printf("Requested file = %.*s\n", LIBNFSVIV_FilenameMaxLen - 1, request_file_name);

  if (vd->num_direntries_true > 0)
  {
    filename = (char *)malloc(LIBNFSVIV_BufferSize * sizeof(*filename));
    if (!filename)
    {
      UVT_fprintf(stderr, "Cannot allocate memory\n");
      return;
    }

    for (i = 0; i < vd->num_direntries_true; i++)
    {
      if (SCL_BITMAP_IsSet(vd->bitmap, i))  contents_size += vd->buffer[i].e_filesize;
    }

#if SCL_DEBUG > 0
    if (opt->direnlenfixed >= 10)  SCL_assert(vd->viv_hdr_size_true == 0x10 + vd->num_direntries_true * opt->direnlenfixed);
#endif

    UVT_printf("\nPrinting archive directory:\n"
               "\n"
               "   id Valid       Offset Gap         Size Len  FnOf  Name\n"
               " ---- ----- ------------ --- ------------ --- -----  -----------------------\n");
    UVT_printf("                       0       %10d            header\n"
               " ---- ----- ------------ --- ------------ --- -----  -----------------------\n", vd->viv_hdr_size_true);

    for (i = 0; i < vd->num_direntries_true; i++)
    {
      int gap;
      if (i > 0)  gap = vd->buffer[i].e_offset - vd->buffer[i - 1].e_offset - vd->buffer[i - 1].e_filesize;
      else if (iswwww)  gap = vd->buffer[0].e_offset - (8 + 4 * vd->num_direntries_true);
      else  gap = vd->buffer[0].e_offset - vd->viv_hdr_size_true;

      UVT_printf(" %4d     %d   %10d %3d   %10d %3d %5x  ", i + 1, SCL_BITMAP_IsSet(vd->bitmap, i), vd->buffer[i].e_offset, gap, vd->buffer[i].e_filesize, vd->buffer[i].e_fname_len_, vd->buffer[i].e_fname_ofs_);
      /*
        avoid printing non-printable string
        filename buffer is always nul-terminated
      */
      if (SCL_BITMAP_IsSet(vd->bitmap, i))
      {
        fseek(file, vd->buffer[i].e_fname_ofs_, SEEK_SET);
        if ((int)fread(filename, 1, LIBNFSVIV_min(vd->buffer[i].e_fname_len_, LIBNFSVIV_BufferSize - 1), file) != vd->buffer[i].e_fname_len_)
        {
          UVT_fprintf(stderr, "File read error (print stats)\n");
          free(filename);
          return;
        }
        filename[vd->buffer[i].e_fname_len_] = '\0';
        if (opt->filenameshex)  LIBNFSVIV_EncBase16(filename, vd->buffer[i].e_fname_len_);
        sz = strlen(filename);
        if (opt->filenameshex || LIBNFSVIV_IsPrintString(filename, LIBNFSVIV_BufferSize) == (int)sz)
          UVT_printf("%.*s", (int)sz, filename);
        else
        {
          LIBNFSVIV_EncBase16(filename, vd->buffer[i].e_fname_len_);
          UVT_printf("%s %s", filename, "<non-printable>");
        }
      }
      UVT_printf("\n");
#ifndef UVT_UNVIVTOOLMODULE
      fflush(stdout);
#endif
    }  /* for i */

    UVT_printf(" ---- ----- ------------ --- ------------ --- -----  -----------------------\n"
               "              %10d       %10d            %d files\n", vd->buffer[vd->num_direntries_true - 1].e_offset + vd->buffer[vd->num_direntries_true - 1].e_filesize, contents_size, vd->num_direntries_true);

    free(filename);
  }  /* if */
}

static
void LIBNFSVIV_PrintStatsEnc(UVT_Directory * const vd,
                             char **infiles_paths, const int count_infiles,
                             const UVT_UnvivVivOpt * const opt)
{
  int gap;
  int i;
  int j;

  SCL_log("Buffer = %d\n", LIBNFSVIV_BufferSize);

  __LIBNFSVIV_UVT_DirectoryPrintStats_Header(vd, !memcmp(vd->format, "wwww", 4));
  /* __LIBNFSVIV_UVT_DirectoryPrintStats_Parsed(vd); */  /* file not yet written */
  UVT_printf("Invalid Entries = %d\n", vd->null_count);
  UVT_printf("Filenames as hex: %d\n", opt->filenameshex);
  if (opt->faithfulencode)
    UVT_printf("Faithful encoder: %d\n", opt->faithfulencode);
  if (opt->alignfofs)
    UVT_printf("Align file offsets: %d\n", opt->alignfofs);

  if (vd->num_direntries > 0)
  {
    UVT_printf("\nPrinting archive directory:\n"
               "\n"
               "   id Valid       Offset Gap         Size Len  FnOf  Name\n"
               " ---- ----- ------------ --- ------------ --- -----  -----------------------\n");
    UVT_printf("                       0       %10d            header\n"
               " ---- ----- ------------ --- ------------ --- -----  -----------------------\n", vd->viv_hdr_size_true);

    for (i = 0, j = 0; i < LIBNFSVIV_min(count_infiles, vd->length); j++)
    {
      if (!opt->faithfulencode && !SCL_BITMAP_IsSet(vd->bitmap, j))
        continue;

      if (i > 0)  gap = vd->buffer[i].e_offset - vd->buffer[i - 1].e_offset - vd->buffer[i - 1].e_filesize;
      else  gap = vd->buffer[0].e_offset - vd->viv_hdr_size_true;

      UVT_printf(" %4d     %d   %10d %3d   %10d %3d %5x  ", i + 1, SCL_BITMAP_IsSet(vd->bitmap, i), vd->buffer[i].e_offset, gap, vd->buffer[i].e_filesize, vd->buffer[i].e_fname_len_, vd->buffer[i].e_fname_ofs_);
      /*
        avoid printing non-printable string
        Windows: transform short paths that contain tilde (~)
      */
      {
        char *ptr;
#ifdef _WIN32
        char buf[LIBNFSVIV_FilenameMaxLen];
        const int sz = LIBNFSVIV_GetPathBasename2(infiles_paths[i], &ptr, buf, sizeof(buf));
#else
        const int sz = LIBNFSVIV_GetPathBasename2(infiles_paths[i], &ptr, NULL, 0);
#endif
        if (LIBNFSVIV_IsPrintString(ptr, sz + 1) == sz)  UVT_printf("%.*s", sz, ptr);
      }
      UVT_printf("\n");
      ++i;
    }  /* for i, j */
    UVT_printf(" ---- ----- ------------ --- ------------ --- -----  -----------------------\n"
               "              %10d       %10d            %d files\n", vd->h_filesize, vd->h_filesize - vd->header_size, vd->num_direntries);
  }
}

/* validate BIGF/BIGH/BIG4/0xFBC0 (Viv) ----------------------------------------------------------------------------- */

static
int __LIBNFSVIV_UVT_DirectoryGetValidMinFileOffset(const UVT_Directory *vd, const int filesize)
{
  int i;
  int min_ = filesize;
  for (i = 0; i < vd->num_direntries_true; i++)
  {
    if (SCL_BITMAP_IsSet(vd->bitmap, i))  min_ = LIBNFSVIV_min(min_, vd->buffer[i].e_offset);
  }
  return min_;
}


static
int LIBNFSVIV_VivCheckHeader(const UVT_Directory * const vd, const int viv_filesize)
{
  const int fmt = LIBNFSVIV_GetVivVersion_FromBuf(vd->format);
  if(fmt <= 1)
  {
    UVT_fprintf(stderr, "VivCheckHeader: Format error (expects BIGF, BIGH, BIG4, 0xFBC0)\n");
    return 0;
  }

  /* avoid buffer overruns (too defensive?) */
  if (vd->num_direntries_true > vd->length)
  {
    UVT_fprintf(stderr, "VivCheckHeader: Error (num_direntries_true > length) %d > %d\n", vd->num_direntries_true, vd->length);
    return 0;
  }

#if 0
  if (vd->num_direntries < 0)
  {
    UVT_fprintf(stderr, "VivCheckHeader: Format error (number of directory entries < 0) %d\n", vd->num_direntries);
    return 0;
  }
#endif

  if (vd->num_direntries < 0 || vd->num_direntries > LIBNFSVIV_DirEntrMax)
  {
    UVT_fprintf(stderr, "VivCheckHeader: Number of purported directory entries not supported and likely invalid (%d) max: %d\n", vd->num_direntries, LIBNFSVIV_DirEntrMax);
    return 0;
  }

  if (vd->header_size < 0 || vd->header_size > viv_filesize)
    UVT_fprintf(stderr, "Warning:VivCheckHeader: Format (reported headersize invalid) (%d)\n", vd->header_size);

#if 0
  if ((unsigned int)vd->header_size > (fmt != 5 ? (unsigned int)vd->num_direntries * (8 + LIBNFSVIV_FilenameMaxLen) + 16 : (unsigned int)vd->num_direntries * (6 + LIBNFSVIV_FilenameMaxLen) + 6))
    UVT_fprintf(stderr, "Warning:VivCheckHeader: Format (invalid headersize) (%d) %d\n", vd->header_size, vd->num_direntries);
#endif

  return 1;
}

static
void LIBNFSVIV_VivValidateDirectory(UVT_Directory *vd, const int viv_filesize)
{
  int contents_size = 0;
  int minimal_ofs;
  int i;

  if (vd->num_direntries != vd->num_direntries_true)
  {
    UVT_fprintf(stderr, "Warning:VivValidateDirectory: incorrect number of archive directory entries in header (%d files listed, %d files found)\n", vd->num_direntries, vd->num_direntries_true);
  }

  /* :HS, :PU allow values greater than true value */
  if ((vd->num_direntries < 1) || (vd->num_direntries_true < 1))
  {
    UVT_fprintf(stderr, "Warning:VivValidateDirectory: empty archive (%d entries listed, %d entries found)\n", vd->num_direntries, vd->num_direntries_true);
    return;
  }

  /* Validate file offsets, sum filesizes */
  for (i = 0; i < vd->num_direntries_true; i++)
  {
    const int ofs_now = vd->buffer[i].e_offset;

    if (!SCL_BITMAP_IsSet(vd->bitmap, i))
      continue;

    if ((vd->buffer[i].e_filesize >= viv_filesize) ||
        (vd->buffer[i].e_filesize < 0))
    {
      UVT_printf("VivValidateDirectory: file %d invalid (filesize out of bounds) (%d ? %d)\n", i, vd->buffer[i].e_filesize, viv_filesize);
      SCL_BITMAP_Unset(vd->bitmap, i);
    }
    if ((ofs_now < vd->viv_hdr_size_true) ||
        (ofs_now < vd->header_size) ||
        (ofs_now >= viv_filesize))
    {
      UVT_printf("VivValidateDirectory: file %d invalid (offset out of bounds) %d\n", i, ofs_now);
      SCL_BITMAP_Unset(vd->bitmap, i);
    }
    if (ofs_now >= INT_MAX - vd->buffer[i].e_filesize)
    {
      UVT_printf("VivValidateDirectory: file %d invalid (offset overflow) %d\n", i, ofs_now);
      SCL_BITMAP_Unset(vd->bitmap, i);
    }
    if ((ofs_now + vd->buffer[i].e_filesize > viv_filesize))
    {
      UVT_printf("VivValidateDirectory: file %d invalid (filesize from offset out of bounds) (%d+%d) > %d\n", i, ofs_now, vd->buffer[i].e_filesize, viv_filesize);
      SCL_BITMAP_Unset(vd->bitmap, i);
    }

    if (SCL_BITMAP_IsSet(vd->bitmap, i) == 1)
      contents_size += vd->buffer[i].e_filesize;
    else
    {
      /* --vd->num_direntries_true; */
      ++vd->null_count;
    }
  }  /* for i */

  minimal_ofs = __LIBNFSVIV_UVT_DirectoryGetValidMinFileOffset(vd, viv_filesize);

  /* not an issue for BIGF, but nice to know */
  if (vd->buffer[0].e_offset != minimal_ofs)  SCL_log("DEV Warning:VivValidateDirectory: smallest e_offset (%d) is not file 0\n", minimal_ofs);

  /*
    Typically, should be equal. Smaller is allowed, as archives may have null-byte padding "gaps" between files.
    example: official DLC walm/car.viv
  */
  if (minimal_ofs + contents_size > viv_filesize)  UVT_fprintf(stderr, "Warning:VivValidateDirectory (valid archive directory filesizes sum too large: overlapping content?)\n");

  /*
    :HS, :PU allow value greater than true value
  */
  if (vd->num_direntries != vd->num_direntries_true)  UVT_fprintf(stderr, "Warning:VivValidateDirectory (archive header has incorrect number of directory entries)\n");

  return;
}

/* decode BIGF/BIGH/BIG4/0xFBC0 (Viv) ------------------------------------------------------------------------------- */

/*
  Clamp number of viv directory entries to be parsed to 0,max
  Check and potentially swap filesize endianness s.t. vd->h_filesize > 0
*/
static
void LIBNFSVIV_VivFixHeader(UVT_Directory *vd, const int filesz)
{
  if (vd->num_direntries < 0)
  {
    UVT_fprintf(stderr, "Warning:VivFixHeader: Format (invalid number of purported directory entries) (%d)(0x%x),\n", vd->num_direntries, vd->num_direntries);
    SCL_log("VivFixHeader: 32 bit (%d)(0x%x) bitmask,\n", vd->num_direntries & 0x7FFFFFFF, vd->num_direntries & 0x7FFFFFFF);
    vd->num_direntries = LIBNFSVIV_min(vd->num_direntries & 0x7FFFFFFF, LIBNFSVIV_DirEntrMax);
    UVT_fprintf(stderr, "Warning:VivFixHeader: assume %d entries\n", vd->num_direntries);
  }
  else if (vd->num_direntries > LIBNFSVIV_DirEntrMax)
  {
    UVT_fprintf(stderr, "Warning:VivFixHeader: Format (unsupported number of purported directory entries) (%d)(0x%x),\n", vd->num_direntries, vd->num_direntries);
    vd->num_direntries = LIBNFSVIV_DirEntrMax;
    UVT_fprintf(stderr, "assume %d entries\n", vd->num_direntries);
  }
  if (LIBNFSVIV_SwapEndian(vd->h_filesize) == filesz)
  {
    vd->h_filesize = filesz;
    vd->__padding[0] ^= 0x2;
  }
}

/* Assumes ftell(file) == 0
   Returns 1, if Viv header can be read. Else 0. */
static
int LIBNFSVIV_VivReadHeader(UVT_Directory *vd, FILE *file, const int filesz)
{
  int sz = 0;

  if (filesz < 16)
  {
    UVT_fprintf(stderr, "VivReadHeader: Format error (invalid filesize) %d\n", filesz);
    return 0;
  }

  sz += (int)fread(vd->format, 1, 4, file);  /* 0x00 */

  if (SCL_deseri_uint(vd->format) != 0x8000FBC0)  /* BIGF, BIGH, BIGF4 */
  {
    sz += (int)fread(&vd->h_filesize, 1, 4, file);  /* 0x04 */
    sz += (int)fread(&vd->num_direntries, 1, 4, file);  /* 0x08 */
    sz += (int)fread(&vd->header_size, 1, 4, file);  /* 0x0C */

    if (sz != 16)
    {
      UVT_fprintf(stderr, "VivReadHeader: File read error\n");
      return 0;
    }
  }
  else  /* example: GameData/Track/Data/tr00a.viv */
  {
    vd->h_filesize = 0;  /* n/a */
    vd->num_direntries = 0;
    sz += (int)fread(&vd->num_direntries, 1, 2, file);  /* 0x04 */
    vd->num_direntries <<= 16;
    vd->header_size = 0;  /* n/a */

    if (sz != 6)
    {
      UVT_fprintf(stderr, "VivReadHeader: File read error\n");
      return 0;
    }
  }

  vd->__padding[0] = (vd->__padding[0] & ~0x2) | 0xC;  /* reset endianness bits 1-3 */
  if (memcmp(vd->format, "BIG4", 4))  /* BIG4 encodes filesize in little endian */
  {
    vd->h_filesize = LIBNFSVIV_SwapEndian(vd->h_filesize);
    vd->__padding[0] |= 1 << 1;  /* set endianness bit */
  }
  vd->num_direntries = LIBNFSVIV_SwapEndian(vd->num_direntries);
  vd->header_size = LIBNFSVIV_SwapEndian(vd->header_size);

  return 1;
}

/*
  Assumes (vd).
  Assumes (vd->num_direntries >= true value).
  Assumes (vd->length == 0) && !(vd->buffer) && !(vd->bitmap)
  Assumes file has BIGF/BIGH/BIG4/0xFBC0 format.
  Returns boolean.

  If (opt->direnlenfixed < 10) assumes variable length directory entries,
  else assumes fixed length directory entries.

  vd->count_dir_entries_true will be the number of entries parsed.
  vd->viv_hdr_size_true will be the true unpadded header size.

  Format variants:
  BIGF, BIGH, BIG4: int32 values
  0xFBC0: int24 values, shifted into int32 fields
*/
static
int LIBNFSVIV_VivReadDirectory(UVT_Directory *vd,
                               const int viv_filesize, FILE *file,
                               const UVT_UnvivVivOpt * const opt)
{
  unsigned char buf[LIBNFSVIV_CircBuf_Size];
  int len = 0;
  int i;
  LIBNFSVIV_CircBuf cbuf;
  int direntr_minsz = 8;  /* int32 direntries */
  int int_sz = 4;
  int int_shift = 0;
  memset(buf, 0, sizeof(buf));  /* initialize in case (viv_filesize < buffer size) */

  cbuf.buf = buf;
  cbuf.sz = sizeof(buf);
  cbuf.rd = 0;
  cbuf.wr = 0;

  if (!LIBNFSVIV_UVT_DirectoryInit(vd, vd->num_direntries))
  {
    UVT_fprintf(stderr, "VivReadDirectory: Cannot allocate memory\n");
    return 0;
  }
  vd->num_direntries_true = vd->num_direntries;  /* will be corrected down if too large */
  if (SCL_deseri_uint(vd->format) != 0x8000FBC0)
  {
    vd->viv_hdr_size_true = 0x10;
  }
  else  /* int24 direntries */
  {
    vd->viv_hdr_size_true = 0x06;

    direntr_minsz = 6;
    int_sz = 3;
    int_shift = 8;
  }

#if SCL_DEBUG >= 2
  if (opt->verbose >= 1)
  {
    SCL_log("Directory Entries (malloc'd): %d (ceil(x/64)=%d), Bitmap (malloc'd): %d, Padding: %d\n", vd->length, LIBNFSVIV_ceil0(vd->length, 64), vd->length > LIBNFSVIV_VivDirectoryPaddingSize * 8, LIBNFSVIV_VivDirectoryPaddingSize);
  }
#endif

  /*
    The following cases can occur when reading filenames from entries:
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
    By default, we are using buffer size 4096 + 16
    to ensure that any valid UTF8-encoded filename can be handled.
    (Given domain knowledge, this can safely be reduced to VAL + 16,
    where VAL is the longest known filename.)

    Directory size can be arbitrarily large. However, typical archive
    directories are well below 1024 bytes in length.
  */
  if (opt->direnlenfixed < 10)  /* variable length entry */
  {
    SCL_log("  Read initial chunk\n");
    if (viv_filesize - (int)ftell(file) >= 10 && LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - 4) < 9)
    {
      UVT_fprintf(stderr, "VivReadDirectory: File read error at %d\n", vd->viv_hdr_size_true);
      return 0;
    }

    for (i = 0; i < vd->num_direntries_true; i++)
    {
      char valid = 1;
      int lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);

#if SCL_DEBUG > 1
      SCL_log("\n");
      SCL_log("i: %d\n", i);
      SCL_log("ftell(file): %d 0x%x\n", (int)ftell(file), (int)ftell(file));
      SCL_log("vd->viv_hdr_size_true: %d\n", vd->viv_hdr_size_true);
      SCL_log("cbuf stats: buf %p, !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", (void *)cbuf.buf, !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
      SCL_log("lefttoread: %d\n", lefttoread);
      SCL_log("readtoend: %d\n", LIBNFSVIV_CircBuf_readtoend(&cbuf));
      SCL_log("memchr: %p\n", lefttoread > 0 ? LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread) : NULL);
      SCL_log("memchr2: %p\n", LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread));
#endif
#if SCL_DEBUG > 2
      SCL_debug_printbuf(cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr);
#endif

      /**
        Read next chunk if:
          - less than 7|9 bytes left to read
          - no nul-termination found where filename is expected

        Read length is bounded by EOF (i.e., reading 0 bytes is valid)
      */
      if (lefttoread < direntr_minsz+1 || !LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', direntr_minsz, lefttoread))
      {
        SCL_log("  Read next chunk\n");
        /* Returns 0 bytes if (ftell(file) == EOF) */
        if (LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - lefttoread) < 0)
        {
          UVT_fprintf(stderr, "VivReadDirectory: File read error at %d\n", vd->viv_hdr_size_true);
          return 0;
        }

        lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);  /* zero if (cb->rd == cb->wr) */
        lefttoread = lefttoread > 0 ? lefttoread : (int)sizeof(buf);
#if SCL_DEBUG > 1
        SCL_log("  cbuf stats: buf %p, !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", (void *)cbuf.buf, !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_log("  memchr: %p\n", LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', 8, lefttoread));
        SCL_debug_printbuf(cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr);
#endif
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
        opt->filenameshex mode:
          Output filenames will be Base16 (hexadecimal) encoded: check for string
      */

      /* Ensure nul-terminated */
      if (!LIBNFSVIV_CircBuf_memchr(&cbuf, '\0', direntr_minsz, lefttoread))
      {
        if (opt->verbose >= 1)
          UVT_fprintf(stderr, "Warning:VivReadDirectory: Filename at %d not a string. Not a directory entry. Stop parsing directory.\n", vd->viv_hdr_size_true);

        vd->num_direntries_true = i;  /* breaks FOR loop */
        break;
      }

      vd->buffer[i].e_fname_len_ = 0;

      valid &= int_sz == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].e_offset, 0, int_sz);
#if SCL_DEBUG >= 2
      SCL_log("  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
#endif
      valid &= int_sz == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].e_filesize, 0, int_sz);
#if SCL_DEBUG >= 2
      SCL_log("  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
#endif
      vd->buffer[i].e_offset = LIBNFSVIV_SwapEndian(vd->buffer[i].e_offset);
      vd->buffer[i].e_filesize = LIBNFSVIV_SwapEndian(vd->buffer[i].e_filesize);
      vd->buffer[i].e_offset = (int)((unsigned int)vd->buffer[i].e_offset >> int_shift);  /* UB without the casts... */
      vd->buffer[i].e_filesize = (int)((unsigned int)vd->buffer[i].e_filesize >> int_shift);  /* UB without the casts... */
#if SCL_DEBUG >= 2
      SCL_log("valid: %d\n", valid);
#endif

      vd->viv_hdr_size_true += direntr_minsz;
      vd->buffer[i].e_fname_ofs_ = vd->viv_hdr_size_true;

#if SCL_DEBUG >= 2
      {
        unsigned char * const ptr = LIBNFSVIV_CircBuf_PeekPtr(&cbuf, 0);
        SCL_log("ptr: %p\n", (void *)ptr);
        {
          int _readtoend = LIBNFSVIV_CircBuf_readtoend(&cbuf);
          unsigned char *_p = ptr;
          SCL_log("ptr: '");
          while (!!_p && *_p != '\0' && _readtoend-- > 0)  SCL_log("%c", *_p++);
          SCL_log("'\n");
        }
      }
#endif

      if (!opt->filenameshex)
      {
#if 1  /* dev */
#if defined(UVTUTF8) && 1
        /* End if filename is not UTF8-string */
        char tmp_UTF8 = 0;
        LIBNFSVIV_CircBuf_Peek(&cbuf, &tmp_UTF8, 0, 1);
        len = LIBNFSVIV_CircBuf_PeekIsPrint(&cbuf, 0, LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        vd->buffer[i].e_fname_len_ = len;
        ++len;
        LIBNFSVIV_CircBuf_Fwd(&cbuf, len);
#if SCL_DEBUG >= 2
        SCL_log("len: %d (0x%x)\n", len, len);
        SCL_log(":  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_log("vd: e_offset: 0x%x\n", vd->buffer[i].e_offset);
        SCL_log("vd: e_filesize: 0x%x\n", vd->buffer[i].e_filesize);
        SCL_log("vd->buffer[i] stats: e_fname_ofs_ 0x%x, e_fname_len_ 0x%x (next 0x%x)\n", vd->buffer[i].e_fname_ofs_, vd->buffer[i].e_fname_len_, vd->buffer[i].e_fname_ofs_ + vd->buffer[i].e_fname_len_);
#endif
        if (!isprint(tmp_UTF8) && (len < 2))
#else
        /* End if filename is not printable string
           very crude check as, e.g., CJK characters end the loop */
        len = LIBNFSVIV_CircBuf_PeekIsPrint(&cbuf, 0, LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        ++len;
        LIBNFSVIV_CircBuf_Fwd(&cbuf, len);
        vd->buffer[i].e_fname_len_ = len;
        if (len < 2)
#endif
        {
          vd->viv_hdr_size_true -= 0x08;
          vd->num_direntries_true = i;  /* breaks while-loop */
          break;
        }
#endif  /* dev */
      }
      else  /* filenames as hex */
      {
        len = LIBNFSVIV_CircBuf_PeekStrlen(&cbuf, 0, LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        vd->buffer[i].e_fname_len_ = len;
        ++len;
        LIBNFSVIV_CircBuf_Fwd(&cbuf, len);
#if SCL_DEBUG >= 2
        SCL_log("len: %d (0x%x)\n", len, len);
        SCL_log(":  cbuf stats: !!buf %d, sz %d, rd %d, wr %d, r2e %d, l2r %d\n", !!cbuf.buf, cbuf.sz, cbuf.rd, cbuf.wr, LIBNFSVIV_CircBuf_readtoend(&cbuf), LIBNFSVIV_CircBuf_lefttoread(&cbuf));
        SCL_log("vd: e_offset: 0x%x\n", vd->buffer[i].e_offset);
        SCL_log("vd: e_filesize: 0x%x\n", vd->buffer[i].e_filesize);
        SCL_log("vd->buffer[i] stats: e_fname_ofs_ 0x%x, e_fname_len_ 0x%x (next 0x%x)\n", vd->buffer[i].e_fname_ofs_, vd->buffer[i].e_fname_len_, vd->buffer[i].e_fname_ofs_ + vd->buffer[i].e_fname_len_);
#endif
      }

      vd->viv_hdr_size_true += len;
      valid &= (len <= LIBNFSVIV_FilenameMaxLen);

      if (valid == 1)  SCL_BITMAP_Set(vd->bitmap, i);
      else  ++vd->null_count;
    }  /* for i */
  }
  else  /* fixed length entry */
  {
    if (opt->direnlenfixed >= (int)sizeof(buf))
    {
      UVT_fprintf(stderr, "VivReadDirectory: fixed directory entry length too large for buffer size (%d > %d)\n", opt->direnlenfixed, (int)sizeof(buf));
      return 0;
    }

    SCL_log("  Read initial chunk\n");
    if (viv_filesize - (int)ftell(file) >= 10 && LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - 4) < opt->direnlenfixed)
    {
      UVT_fprintf(stderr, "VivReadDirectory: File read error at %d\n", vd->viv_hdr_size_true);
      return 0;
    }
    SCL_log("\n");

    for (i = 0; i < vd->num_direntries_true; i++)
    {
      char valid = 1;
      int lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);

      /* update buffer */
      if (lefttoread < opt->direnlenfixed)
      {
        SCL_log("  Read next chunk\n");
        /* Returns 0 bytes if (ftell(file) == EOF) */
        if (LIBNFSVIV_CircBuf_addFromFile(&cbuf, file, viv_filesize - (int)ftell(file), sizeof(buf) - lefttoread) < 0)
        {
          UVT_fprintf(stderr, "VivReadDirectory: File read error at %d\n", vd->viv_hdr_size_true);
          return 0;
        }

        lefttoread = LIBNFSVIV_CircBuf_lefttoread(&cbuf);
        lefttoread = lefttoread > 0 ? lefttoread : (int)sizeof(buf);
      }

      /* Get next entry */

      /* Ensure fixed length (int)opt->direnlenfixed is available */
      if (lefttoread < opt->direnlenfixed)
      {
        if (opt->verbose >= 1)
          UVT_fprintf(stderr, "Warning:VivReadDirectory: Filename at %d not a string. Not a directory entry. Stop parsing directory.\n", vd->viv_hdr_size_true);

        vd->num_direntries_true = i;  /* breaks FOR loop */
        break;
      }

      vd->buffer[i].e_fname_len_ = 0;

      valid &= 4 == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].e_offset, 0, 4);
      valid &= 4 == LIBNFSVIV_CircBuf_Get(&cbuf, &vd->buffer[i].e_filesize, 0, 4);
      vd->buffer[i].e_offset   = LIBNFSVIV_SwapEndian(vd->buffer[i].e_offset);
      vd->buffer[i].e_filesize = LIBNFSVIV_SwapEndian(vd->buffer[i].e_filesize);

      vd->viv_hdr_size_true += 0x08;
      vd->buffer[i].e_fname_ofs_ = vd->viv_hdr_size_true;

      if (opt->filenameshex)  /* filenames as hex */
      {
        /*
          Find last non-nul byte for name.
          Accepts embedded/leading nul's and missing terminating nul.
        */
        {
          int len_ = opt->direnlenfixed - 0x08;
          unsigned char buf_[sizeof(buf)];
          const unsigned char *p_;
          LIBNFSVIV_CircBuf_Peek(&cbuf, buf_, 0, len_);
          p_ = buf_ + len_ - 1;  /* last byte */
          while (*p_-- == '\0' && len_ > 0)
            --len_;
          vd->buffer[i].e_fname_len_ = len_;
        }
      }
      else
      {
        /** fixed length entries with printable filenames are not known to exist */
        UVT_fprintf(stderr, "VivReadDirectory: Not implemented. Try with filenames as hex.\n");
        return 0;
      }

      vd->viv_hdr_size_true += opt->direnlenfixed - 0x08;
      LIBNFSVIV_CircBuf_Fwd(&cbuf, opt->direnlenfixed - 0x08);
      valid &= (len <= LIBNFSVIV_FilenameMaxLen);

      if (valid == 1)  SCL_BITMAP_Set(vd->bitmap, i);
      else  --vd->null_count;
    }  /* for i */
  }

#if SCL_DEBUG > 0
  LIBNFSVIV_UVT_DirEntrPrint(vd, 0);
#endif

  return 1;
}

/* Returns strlen of created filename in (buf), 0 on error. */
static
size_t LIBNFSVIV_CreateExtractFilename(const UVT_DirEntr * const vde, FILE *infile,
                                       const int opt_filenameshex,
                                       char *buf, const int bufsz)
{
  /* Read outfilename to buf */
  if (LIBNFSVIV_FreadToStr(buf, bufsz, vde->e_fname_ofs_, vde->e_fname_len_, infile) < 0)
  {
    UVT_fprintf(stderr, "CreateExtractFilename: File read error at %d (extract outfilename)\n", vde->e_fname_ofs_);
    return 0;
  }
  buf[bufsz - 1] = '\0';

  /* Option: Encode outfilename to Base16 */
  if (opt_filenameshex)  LIBNFSVIV_EncBase16(buf, vde->e_fname_len_);

  return strlen(buf);
}

/*
  Extracts the described file to disk. Returns boolean.
  Assumes (vde), (infile). If (wenc_file), assumes (wenc_outpath).
  Assumes (buf), buf is string and valid filepath.

  If (opt_overwrite == 1) attempts to rename existing file, otherwise attempts to overwrite.
*/
static
int LIBNFSVIV_UVT_DirEntrExtractFile(const UVT_DirEntr * const vde, FILE *infile,
                                     /* const int opt_filenameshex, */ const int opt_overwrite,
                                     FILE *wenc_file, const char * const wenc_outpath,
                                     char *buf, size_t bufsz)
{
  int retv = 1;
  FILE *outfile;

  /* Create outfile */
  if (LIBNFSVIV_IsFile(buf))  /* overwrite mode: for existing files and duplicated filenames in archive */
  {
    if (opt_overwrite == 1)  /* attempt renaming existing file, return on failure */
    {
      if (!LIBNFSVIV_IncrementFile(buf, bufsz, 1))  return 0;
    }
    else
    {
      UVT_fprintf(stderr, "Warning:UVT_DirEntrExtractFile: Attempt overwriting existing '%s' (duplicated filename?)\n", buf);
    }
  }
  outfile = fopen(buf, "wb");
  if (!outfile)
  {
    UVT_fprintf(stderr, "UVT_DirEntrExtractFile: Cannot create output file '%s'\n", buf);
    return 0;
  }

  if (wenc_file)  /* Option: Write re-Encode command to file */
  {
    fprintf(wenc_file, " \"%s/%s\"", wenc_outpath, buf);
    fflush(wenc_file);
  }

  /* Extract */
  {
    char buf__[LIBNFSVIV_BufferSize];
    memset(buf__, 0, sizeof(buf__));
    fseek(infile, vde->e_offset, SEEK_SET);
    retv &= LIBNFSVIV_FileCopyData(outfile, infile, vde->e_filesize, buf__, sizeof(buf__));
  }
  fclose(outfile);
  return retv;
}

/** Assumes (request_file_name) and request_file_name is string.
    Returns 1-based directory entry index of given filename,
    -1 if it does not exist, 0 on error. **/
static
int LIBNFSVIV_UVT_DirectoryGetIdxFromFname(const UVT_Directory *vd, FILE* infile, const char *request_file_name)
{
  char buf[LIBNFSVIV_FilenameMaxLen];
  const int len = (int)strlen(request_file_name);
  int i;

  if (len + 1 > LIBNFSVIV_FilenameMaxLen)
  {
    UVT_fprintf(stderr, "UVT_DirectoryGetIdxFromFname: Requested filename is too long\n");
    return 0;
  }

  for (i = 0; i < vd->num_direntries_true; i++)
  {
    if (len == vd->buffer[i].e_fname_len_)
    {
      if (len != LIBNFSVIV_FreadToStr(buf, len + 1, vd->buffer[i].e_fname_ofs_, vd->buffer[i].e_fname_len_, infile))
        UVT_fprintf(stderr, "UVT_DirectoryGetIdxFromFname: File read error at 0x%x\n", vd->buffer[i].e_fname_ofs_);
      if (!strncmp(buf, request_file_name, len))
        return i + 1;
    }
  }

  UVT_fprintf(stderr, "UVT_DirectoryGetIdxFromFname: Cannot find requested file in archive\n");
  return -1;
}

/* Assumes *vd is valid. */
void LIBNFSVIV_ReadGap(UVT_Directory *vd)
{
  int i;
  int maxgap = 0;
  for (i = 0; i < vd->num_direntries_true; i++)
  {
    int gap;
    if (i > 0)  gap = vd->buffer[i].e_offset - vd->buffer[i - 1].e_offset - vd->buffer[i - 1].e_filesize;
    else gap = vd->buffer[0].e_offset - vd->viv_hdr_size_true;
    if (gap > maxgap)  maxgap = gap;
  }
  maxgap = LIBNFSVIV_NextPower(maxgap);
  vd->__padding[0] &= 0xF;
  switch (maxgap)
  {
    case 2: vd->__padding[0] |= (1 << 4); break;
    case 4: vd->__padding[0] |= (1 << 5); break;
    case 8: vd->__padding[0] |= (1 << 6); break;
    case 16: vd->__padding[0] |= (1 << 7); break;
    default: break;
  }
}

/* encode ----------------------------------------------------------------------------------------------------------- */

/* Aligns file offsets in UVT_Directory 'vd' based on alignment in 'vd->__padding[0] >> 4'.
   Returns total padding added to archive size.

   Assumes (vd).
   Assumes (vd->num_direntries_true + vd->null_count == count_infiles).
*/
int LIBNFSVIV_AlignFileOffsets(UVT_Directory *vd, const int count_infiles, const int opt_faithfulencode)
{
  const int alignfofs = LIBNFSVIV_GetBitIndex(vd->__padding[0] >> 4);
  int sum_padding = LIBNFSVIV_tomultiple(vd->header_size, alignfofs) - vd->header_size;
  int i;
  for (i = 0; i < count_infiles; i++)
  {
    if (SCL_BITMAP_IsSet(vd->bitmap, i) || opt_faithfulencode)
    {
      int local_padding;
      vd->buffer[i].e_offset += sum_padding;
      local_padding = LIBNFSVIV_tomultiple(vd->buffer[i].e_offset, alignfofs) - vd->buffer[i].e_offset;
      vd->buffer[i].e_offset += local_padding;
      sum_padding += local_padding;
    }
  }
  vd->h_filesize += sum_padding;

  return sum_padding;
}

/*
  Assumes (vd) and vd is zero'd.
  Assumes length of opt->requestfmt >= 4
  Assumes opt->alignfofs is zero pr power of two and != 1
  Returns 1 on success, 0 on failure.
*/
static
int LIBNFSVIV_UVT_DirectorySet(UVT_Directory *vd,
                               char **infiles_paths, const int count_infiles,
                               const UVT_UnvivVivOpt *opt)
{
  int sum_filesz = 0;  /* validation */
  int sum_padding = 0;
  const int requestfmt_i = LIBNFSVIV_GetVivVersion_FromBuf(opt->requestfmt);
  const int direntr_minsz = requestfmt_i == 5 ? 0x6 : (requestfmt_i==1?0x4:0x8);
  int i;


  SCL_log("LIBNFSVIV_UVT_DirectorySet: count_infiles: %d\n", count_infiles);
  SCL_log("LIBNFSVIV_UVT_DirectorySet: opt->requestfmt: %s\n", requestfmt_i > 0 ? LIBNFSVIV_GetVivVersionString(LIBNFSVIV_GetVivVersion_FromBuf(opt->requestfmt)) : "");
  SCL_log("LIBNFSVIV_UVT_DirectorySet: requestfmt_i: %d\n", requestfmt_i);
  SCL_log("LIBNFSVIV_UVT_DirectorySet: direntr_minsz: %d\n", direntr_minsz);
  LIBNFSVIV_PrintUnvivVivOpt(opt);

  if (requestfmt_i < 1)
  {
    UVT_fprintf(stderr, "LIBNFSVIV_UVT_DirectorySet: Invalid format (expects 'BIGF', 'BIGH', 'BIG4', 'wwww' or '0x80000FBC0')\n");
    return 0;
  }

  if (!LIBNFSVIV_UVT_DirectoryInit(vd, count_infiles))
  {
    UVT_fprintf(stderr, "LIBNFSVIV_UVT_DirectorySet: Cannot allocate memory\n");
    return 0;
  }
  SCL_assert(vd->length >= count_infiles);

  /*
    Set UVT_Directory directory entries from filelist, even for invalid files.

    opt->faithfulencode == 0:  directory entries for invalid files do not count towards offsets
    opt->faithfulencode != 0:  directory entries for invalid files count towards offsets (pretend all is well, set filesize 0)
  */
  for (i = 0; i < count_infiles; i++)
  {
    /* validity and filesize */
    if (LIBNFSVIV_IsFile(infiles_paths[i]) && !LIBNFSVIV_IsDir(infiles_paths[i]))
    {
      vd->num_direntries_true++;
      SCL_BITMAP_Set(vd->bitmap, i);
      vd->buffer[i].e_filesize = LIBNFSVIV_GetFilesize(infiles_paths[i]);  /* file size */
      sum_filesz += vd->buffer[i].e_filesize;
    }
    else
    {
      ++vd->null_count;
      if (!opt->faithfulencode)
      {
        UVT_fprintf(stderr, "LIBNFSVIV_UVT_DirectorySet: Invalid file. Skipping '%s'\n", infiles_paths[i]);
        continue;
      }
      UVT_fprintf(stderr, "Warning:LIBNFSVIV_UVT_DirectorySet: Invalid file. '%s'\n", infiles_paths[i]);
    }  /* if */

    /* filename length */
    if (requestfmt_i != 1)  /* not wwww */
    {
#ifdef _WIN32
      char buf[LIBNFSVIV_FilenameMaxLen];
      int len_filename = LIBNFSVIV_GetPathBasename2(infiles_paths[i], NULL, buf, sizeof(buf)) + 1;
#else
      int len_filename = LIBNFSVIV_GetPathBasename2(infiles_paths[i], NULL, NULL, 0) + 1;
#endif
      len_filename = LIBNFSVIV_clamp(len_filename, 1, LIBNFSVIV_FilenameMaxLen);
      if (opt->filenameshex)  len_filename = LIBNFSVIV_ceil0(len_filename, 2);
      vd->buffer[i].e_fname_len_ = len_filename - 1;

      if (opt->direnlenfixed < 10)
        vd->h_filesize += vd->buffer[i].e_fname_len_ + 1;
      else
        vd->h_filesize += opt->direnlenfixed - direntr_minsz;
    }  /* if (requestfmt != 5) */
  }  /* for i */
  SCL_assert(vd->num_direntries_true + vd->null_count == count_infiles);

  /*
    Finish writing header

    From above, vd->h_filesize is the total size of filename lenths with nul-terminators
      or the fixed length of directory entries times number of valid entries
      or 0 for 'wwww'.
  */
  memcpy(vd->format, opt->requestfmt, 4);  /* final: format */
  vd->num_direntries = vd->num_direntries_true + (!opt->faithfulencode?0:vd->null_count);  /* final: number of entries */
  vd->header_size = (requestfmt_i==5?0x6:(requestfmt_i==1?0x8:0x10));  /* archive header size w/o entries: 6 bytes for 0x8000FBC0, 8 bytes for wwww, 16 bytes for BIGF/BIGH/BIG4 */
  vd->viv_hdr_size_true = vd->header_size;  /* tracks filename offsets */
  vd->header_size += vd->h_filesize + direntr_minsz * vd->num_direntries;  /* final: archive header size */
  vd->h_filesize = vd->header_size;  /* tracks file offsets */
  vd->__padding[0] &= 0xF;
  if (requestfmt_i != 1)
  {
    vd->__padding[0] |= opt->requestendian & 0xE;  /* final: archive header endianess */
    if (opt->alignfofs > 1)  vd->__padding[0] |= (LIBNFSVIV_GetIndexBit(opt->alignfofs) << 4);  /* final: request file offset alignment */
  }
  SCL_assert((requestfmt_i == 1 && vd->header_size == 4 + 4 + 4*vd->num_direntries) || requestfmt_i != 1);

  for (i = 0; i < count_infiles; i++)
  {
    if (SCL_BITMAP_IsSet(vd->bitmap, i) || opt->faithfulencode)
    {
      /* track file offsets */
      vd->buffer[i].e_offset = vd->h_filesize;
      vd->h_filesize += vd->buffer[i].e_filesize;

      /* tracks filename offsets */
      vd->viv_hdr_size_true += direntr_minsz;
      if (opt->direnlenfixed < 10 || requestfmt_i == 1)  /* variable length entry or wwww */
      {
        vd->buffer[i].e_fname_ofs_ = requestfmt_i != 1 ? vd->viv_hdr_size_true : vd->buffer[i].e_offset;  /* filename offset (for wwww identical to file offset) */
        vd->viv_hdr_size_true += vd->buffer[i].e_fname_len_ + (int)(requestfmt_i != 1);
      }
      else  /* fixed length entry */
      {
        vd->buffer[i].e_fname_ofs_ = vd->viv_hdr_size_true;
        vd->viv_hdr_size_true += opt->direnlenfixed - direntr_minsz;
      }

      if (requestfmt_i == 1)  vd->buffer[i].e_fname_len_ = 4;  /* wwww: artifical value */
    }
  }  /* for i */

  /* If requested, align file offsets to some non-trivial power of 2 */
  if (vd->__padding[0] >> 4)
    sum_padding = LIBNFSVIV_AlignFileOffsets(vd, count_infiles, opt->faithfulencode);

#if SCL_DEBUG > 2
  LIBNFSVIV_UVT_DirEntrPrint(vd, 1);
#endif
  SCL_assert(vd->header_size == vd->viv_hdr_size_true);
  SCL_assert(vd->h_filesize == vd->header_size + sum_filesz + sum_padding);

  return (vd->num_direntries_true + vd->null_count == count_infiles) && (vd->header_size == vd->viv_hdr_size_true) && (vd->h_filesize == vd->header_size + sum_filesz + sum_padding);
}

static
int LIBNFSVIV_WriteVivHeader(UVT_Directory *vd, FILE *file)
{
  size_t err = 0;
  int buf[3];

  buf[1] = vd->num_direntries;
  err += fwrite(vd->format, 1, 4, file);

  if (SCL_deseri_uint(vd->format) != 0x8000FBC0)
  {
    buf[0] = vd->h_filesize;
    if (SCL_BITMAP_IsSet(&vd->__padding[0], 1))  buf[0] = LIBNFSVIV_SwapEndian(buf[0]);
    if (SCL_BITMAP_IsSet(&vd->__padding[0], 2))  buf[1] = LIBNFSVIV_SwapEndian(buf[1]);
    buf[2] = vd->header_size;
    if (SCL_BITMAP_IsSet(&vd->__padding[0], 3))  buf[2] = LIBNFSVIV_SwapEndian(buf[2]);
    err += fwrite(&buf, 1, sizeof(buf), file);
    return err == 16;
  }
  else  /* example: GameData/Track/Data/tr00a.viv */
  {
    if (SCL_BITMAP_IsSet(&vd->__padding[0], 2))
    {
      buf[1] = LIBNFSVIV_SwapEndian(buf[1]);
      buf[1] >>= 16;
    }
    err += fwrite(&buf[1], 1, 2, file);
    return err == 6;
  }
}

/*
  Assumes (ftell(file) == 16).
  Updates vd->viv_hdr_size_true to ftell(file).
  Allows writing broken headers with (opt->faithfulencode != 0)
*/
static
int LIBNFSVIV_WriteVivDirectory(UVT_Directory *vd, FILE *file,
                                char **infiles_paths, const int count_infiles,
                                const UVT_UnvivVivOpt * const opt)
{
  int val;
  char buf[LIBNFSVIV_FilenameMaxLen];
  int len;  /* strlen() of viv directory entry filename  */
  int i;
  int err = 0x10;  /* tracks UVT_Directory written length */
  int int_sz = 4;
  int int_shift = 0;
  if (SCL_deseri_uint(vd->format) == 0x8000FBC0)  /* int24 direntries */
  {
    err = 0x06;
    int_sz = 3;
    int_shift = 8;
  }

  for (i = 0; i < count_infiles; i++)
  {
    char *ptr;

    if (!SCL_BITMAP_IsSet(vd->bitmap, i) && !opt->faithfulencode)  continue;

    val = LIBNFSVIV_SwapEndian(vd->buffer[i].e_offset);
    val >>= int_shift;
    err += (int)fwrite(&val, 1, int_sz, file);

    val = LIBNFSVIV_SwapEndian(vd->buffer[i].e_filesize);
    val >>= int_shift;
    err += (int)fwrite(&val, 1, int_sz, file);

    /*
      Get basename of infiles_paths[i]
      Windows: transform short paths that contain tilde (~)
    */
#ifdef _WIN32
    len = LIBNFSVIV_GetPathBasename2(infiles_paths[i], &ptr, buf, sizeof(buf));
    if (len > 0)  memmove(buf, ptr, len + 1);
#else
    len = LIBNFSVIV_GetPathBasename2(infiles_paths[i], &ptr, NULL, 0);
    if (len > 0)  memcpy(buf, ptr, len + 1);
#endif

    if (opt->filenameshex)
    {
      len = LIBNFSVIV_DecBase16(buf);
      if (len != vd->buffer[i].e_fname_len_)
        UVT_fprintf(stderr, "Warning:WriteVivDirectory: Base16 conversion mishap (%d!=%d)\n", len, vd->buffer[i].e_fname_len_);
    }

    if (opt->direnlenfixed >= 10 && len > opt->direnlenfixed - 0x8)
    {
      UVT_fprintf(stderr, "Warning:WriteVivDirectory: Filename too long. Trim to fixed directory entry length (%d > %d).\n", len, opt->direnlenfixed);
      len = opt->direnlenfixed - 0x8;
    }

    err += len * (len == (int)fwrite(buf, 1, len, file));  /* filename */

    if (opt->direnlenfixed < 10)
      err += '\0' == fputc('\0', file);
    else
    {
      while (len++ < opt->direnlenfixed)
        err += '\0' == fputc('\0', file);
    }
  }  /* for i */

  vd->viv_hdr_size_true = (int)ftell(file);
  return err = (int)ftell(file);
}

/*
  Copies LEN bytes from offset INFILE_OFS, of SRC to DEST.
  Returns ftell(DEST) on success, -1 on failure.

  If (!SRC), opens/closes SRC at infile_path.
  Expects either (SRC) or (infile_path).
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
    UVT_fprintf(stderr, "VivWriteFile: Cannot open file '%s' (src)\n", infile_path);
    return -1;
  }
  fseek(src, LIBNFSVIV_max(0, infile_ofs), SEEK_SET);
  retv &= LIBNFSVIV_FileCopyData(dest, src, len, buf, (int)sizeof(buf));
  if (infile_path)  fclose(src);
  return (retv) ? (int)ftell(dest) : -1;
}

/* api: functions --------------------------------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* wwww --------------------------------------------------------------------- */

#ifdef UVTWWWW  /* optional branch */
#define SCL_WWWW_BUFSZ 64
#define SCL_WWWW_MAX_ENTRIES (((SCL_WWWW_BUFSZ)-8)/4)  /* == 14 > 4-12 entries in known examples */

/* Returns 0 if valid. */
static
int SCL_CheckwwwwDirectory(UVT_Directory *vd, const int wwww_filesize)
{
  int i;
  if (!vd)  return 1 << 0;
  if (memcmp(vd->format, "wwww", 4))  return 1 << 2;
  if (vd->num_direntries_true < 0 || vd->num_direntries_true > LIBNFSVIV_DirEntrMax)  return 1 << 3;
  /* if (vd->h_filesize < 8)  return 1 << 4; */  /* irrelevant */
  if (vd->num_direntries_true == 0)  return 0;
  if (wwww_filesize < vd->num_direntries_true * 4 + 8)  return 1 << 5;
  if (vd->viv_hdr_size_true < 8 + 4 * vd->num_direntries_true)  return 1 << 6;

  for (i = 0; i < vd->num_direntries_true; i++)
  {
    if (SCL_BITMAP_IsSet(vd->bitmap, i))
    {
      if ((vd->buffer[i].e_offset < 8 + 4 * vd->num_direntries_true) ||  /* file offset is in directory */
          (vd->buffer[i].e_offset >= wwww_filesize) ||  /* file offset out of bounds */
          (vd->buffer[i].e_offset + vd->buffer[i].e_filesize < 0) ||  /* overflow */
          (vd->buffer[i].e_filesize < 4))  /* too small or overflow */
      {
        SCL_BITMAP_Unset(vd->bitmap, i);
        vd->null_count++;
      }
    }
  }  /* for i */

  return 0;
}

/*
 buf holds entire wwww directory, filesz is wwww total file size.

 Assumes (vd) and (buf).
 Expects bufsz <= filesz and bufsz >= 8.
*/
UVT_Directory *SCL_GetwwwwInfo_FromBuf(UVT_Directory *vd, const char *buf, const int bufsz, const int filesz, const int opt_verbose)
{
  char err = 0;
  int i;
#ifndef UVT_UNVIVTOOLMODULE
  (void)opt_verbose;
#endif

  if (bufsz > filesz || bufsz < 8)
  {
    UVT_fprintf(stderr, "Format error (invalid filesize) %d, %d\n", bufsz, filesz);
    return NULL;
  }

  /* Read wwww header  */
  memcpy(vd->format, buf, 4);
  if (memcmp(vd->format, "wwww", 4))  return NULL;
  memcpy(&vd->num_direntries, buf + 4, 4);

  /* Check wwww header */
  if (vd->num_direntries < 0 || vd->num_direntries > SCL_WWWW_MAX_ENTRIES)
  {
    UVT_fprintf(stderr, "Warning:Format error (unsupported num_direntries) %d\n", vd->num_direntries);
    /* return NULL; */
  }
  /* if (bufsz < vd->num_direntries * 4 + 8)  return NULL; */
  vd->num_direntries_true = LIBNFSVIV_clamp(vd->num_direntries, 0, LIBNFSVIV_DirEntrMax);
  if (bufsz < vd->num_direntries_true * 4 + 8)  return NULL;

#ifdef UVT_UNVIVTOOLMODULE
  if (opt_verbose)
  {
#endif
  __LIBNFSVIV_UVT_DirectoryPrintStats_Header(vd, 1);
#ifdef UVT_UNVIVTOOLMODULE
  }
#endif
  /* if (vd->num_direntries == 0)  return vd; */

  /* Read wwww directory */
  if (!LIBNFSVIV_UVT_DirectoryInit(vd, vd->num_direntries_true))
  {
    UVT_fprintf(stderr, "GetwwwwInfo_FromBuf: Cannot allocate memory\n");
    return NULL;
  }
  {
    /* Get entry offsets */
    const char *ptr = buf + 8;
    for (i = 0; i < vd->num_direntries_true; i++)
    {
      memcpy(&vd->buffer[i].e_offset, ptr, 4);
      vd->buffer[i].e_fname_ofs_ = vd->buffer[i].e_offset;
      vd->buffer[i].e_fname_len_ = 4;  /* assume format is char[4] */
      if ((vd->buffer[i].e_offset >= 8 + 4 * vd->num_direntries) &&  /* file must not start inside directory */
          (vd->buffer[i].e_offset + 4 < filesz))  /* filesize must be >= 4 and within file */
      {
        SCL_BITMAP_Set(vd->bitmap, i);
      }
      else
      {
        vd->null_count++;
      }
      ptr += 4;
    }
  }
  {
    /* Derive entry filesizes */
    int previous_valid_ofs = filesz;
    for (i = vd->num_direntries_true - 1; i >= 0; i--)
    {
      if (SCL_BITMAP_IsSet(vd->bitmap, i))
      {
        vd->buffer[i].e_filesize = previous_valid_ofs - vd->buffer[i].e_offset;
        if (vd->buffer[i].e_filesize < 0)
        {
          SCL_BITMAP_Unset(vd->bitmap, i);
          vd->null_count++;
        }
        else
        {
          previous_valid_ofs = vd->buffer[i].e_offset;
        }
      }
    }
  }
  vd->viv_hdr_size_true = 8 + 4 * vd->num_direntries_true;

  err |= SCL_CheckwwwwDirectory(vd, filesz);
  if (err)
  {
    LIBNFSVIV_UVT_DirEntrPrint(vd, 1);
    UVT_fprintf(stderr, "GetwwwwInfo_FromBuf: Format error\n");
    return NULL;
  }
#ifdef UVT_UNVIVTOOLMODULE
  if (opt_verbose)
  {
#endif
  __LIBNFSVIV_UVT_DirectoryPrintStats_Parsed(vd);
#ifdef UVT_UNVIVTOOLMODULE
  }
#endif
  return vd;
}


UVT_Directory *SCL_GetwwwwInfo_FromFile(UVT_Directory *vd, FILE *file, const int filesz, const int opt_verbose)
{
  if (vd && file && filesz >= 8)
  {
    char buf[SCL_WWWW_BUFSZ];
    if ((int)fread(&buf, 1, LIBNFSVIV_min(SCL_WWWW_BUFSZ, filesz), file) == LIBNFSVIV_min(SCL_WWWW_BUFSZ, filesz))
    {

      vd = SCL_GetwwwwInfo_FromBuf(vd, buf, LIBNFSVIV_min(SCL_WWWW_BUFSZ, filesz), filesz, opt_verbose);
      if (opt_verbose && vd)
      {
        UVT_UnvivVivOpt opt_;
        memset(&opt_, 0, sizeof(opt_));
        LIBNFSVIV_PrintStatsDec(vd, file, filesz, 0, NULL, &opt_);
      }
    }
  }
  SCL_log("GetwwwwInfo_FromFile(): %s\n", vd ? "OK" : "FAILED");
  return vd;
}


/* Wrapper */
UVT_Directory *SCL_GetwwwwInfo(UVT_Directory *vd, const char * const path, const int opt_verbose)
{
  FILE *file = path ? fopen(path, "rb") : NULL;
  const int filesz = LIBNFSVIV_GetFilesize(path);
  vd = SCL_GetwwwwInfo_FromFile(vd, file, filesz, opt_verbose);
  if (file)  fclose(file);
  SCL_log("GetwwwwInfo(): %s\n", vd ? "OK" : "FAILED");
  return vd;
}


int LIBNFSVIV_WritewwwwInfo(UVT_Directory *vd, FILE *file)
{
  int err = 0;
  int i;
  err += (int)fwrite(vd->format, 1, 4, file);
  err += (int)fwrite(&vd->num_direntries, 1, sizeof(vd->num_direntries), file);
  for (i = 0; i < vd->num_direntries_true; i++)
  {
    err += (int)fwrite(&vd->buffer[i].e_offset, 1, sizeof(vd->buffer[i].e_offset), file);
  }
  return err == 4 + 4 + 4 * vd->num_direntries_true;
}

#endif  /* UVTWWWW */


/* BIGF, BIGH, BIG4, 0x8000FBC0 (viv) --------------------------------------- */

/* Returns string (const char[5]) or NULL. */
const char *LIBNFSVIV_GetVivVersionString(const int version)
{
  switch (version)
  {
    case 7:  return "BIGF";
    case 8:  return "BIGH";
    case 4:  return "BIG4";
    case 1:  return "wwww";
    case 5:  return "C0FB";
    default:  return NULL;
  }
}

/*
  Returns 7 (BIGF), 8 (BIGH), 4 (BIG4), 1 (wwww), 5 (0x8000FBC0), -1 (unknown format)
  Assumes (sz >= 4).

  Attn: Return values differ from LIBNFSVIV_GetVivVersion_FromPath().
*/
int LIBNFSVIV_GetVivVersion_FromBuf(const char * const buf)
{
  if (!memcmp(buf, "BIGF", 4))  return 7;
  if (!memcmp(buf, "BIGH", 4))  return 8;
  if (!memcmp(buf, "BIG4", 4))  return 4;
  if (!memcmp(buf, "wwww", 4))  return 1;
  {
    const unsigned int fbc0 = 0x8000FBC0;
    if (!memcmp(buf, &fbc0, 4))  return 5;
  }
  return -1;
}

/*
  Returns 7 (BIGF), 8 (BIGH), 4 (BIG4), 1 (wwww), 5 (0x8000FBC0), -1 (unknown format), 0 (fread error)

  Attn: Return values differ from LIBNFSVIV_GetVivVersion_FromBuf().
*/
int LIBNFSVIV_GetVivVersion_FromPath(const char * const path)
#if 0
{
  int retv = 0;
  FILE *file = fopen(path, "rb");
  if (file)
  {
    const int filesz = LIBNFSVIV_GetFilesize(path);
    if (filesz >= 6)
    {
      char buf[4];
      setvbuf(file, NULL, _IONBF, 0);
      if (fread(&buf, 1, 4, file) == 4)
      {
        retv = LIBNFSVIV_GetVivVersion_FromBuf(buf);
        retv = (filesz >= 16 || (retv == 1 && filesz >= 8) || retv == 5) ? retv : -1;
      }
      fclose(file);
    }
    else  retv = -1;
  }
  return retv;
}
#else
{
  int retv = 0;
  int fd = open(path, O_RDONLY
#ifdef _WIN32
    |_O_BINARY
#endif
  );
  if (fd != -1)
  {
    const int filesz = LIBNFSVIV_GetFilesize(path);
    if (filesz >= 6)
    {
      char buf[4];
      if (read(fd, &buf, 4) == 4)
      {
        retv = LIBNFSVIV_GetVivVersion_FromBuf(buf);
        retv = (filesz >= 16 || (retv == 1 && filesz >= 8) || retv == 5) ? retv : -1;
      }
      close(fd);
    }
    else  retv = -1;
  }
  return retv;
}
#endif

/*
  The following functions can be used for data analysis.
  These functions are also available in the Python module.

  Usage:
    1. Get the VIV directory either by calling LIBNFSVIV_GetVivDirectoryFromFile() or LIBNFSVIV_GetVivDirectoryFromPath()
    2. To get a list of files in the VIV archive,
      call LIBNFSVIV_VivDirectoryToFileList_FromFile() or LIBNFSVIV_VivDirectoryToFileList_FromPath()
*/

/* Assumes (ftell(file) == 0) */
UVT_Directory *LIBNFSVIV_GetVivDirectory_FromFile(UVT_Directory *vd, FILE *file, const int filesz, const UVT_UnvivVivOpt *opt)
{
  UVT_Directory *retv = NULL;
  for (;;)
  {
    if (!vd || !file)  break;
    if (!LIBNFSVIV_VivReadHeader(vd, file, filesz))  break;
#ifdef UVT_UNVIVTOOLMODULE
    if (opt->verbose)
    {
#endif
    __LIBNFSVIV_UVT_DirectoryPrintStats_Header(vd, 0);
#ifdef UVT_UNVIVTOOLMODULE
    }
#endif
    LIBNFSVIV_VivFixHeader(vd, filesz);
    if (!LIBNFSVIV_VivCheckHeader(vd, filesz))  break;
    if (!LIBNFSVIV_VivReadDirectory(vd, filesz, file, opt))  break;
    LIBNFSVIV_VivValidateDirectory(vd, filesz);
    if (opt->verbose && vd->null_count > 0)  LIBNFSVIV_UVT_DirEntrPrint(vd, 0);
    LIBNFSVIV_ReadGap(vd);
#ifdef UVT_UNVIVTOOLMODULE
    if (opt->verbose)
    {
#endif
    __LIBNFSVIV_UVT_DirectoryPrintStats_Parsed(vd);
#ifdef UVT_UNVIVTOOLMODULE
    }
#endif
#ifndef UVT_UNVIVTOOLCLI
    if (opt->verbose)  LIBNFSVIV_PrintStatsDec(vd, file, filesz, 0, NULL, opt);
#endif
    retv = vd;
    break;
  }
  return retv;
}

/* Wrapper for LIBNFSVIV_GetVivDirectory_FromFile() */
UVT_Directory *LIBNFSVIV_GetVivDirectory(UVT_Directory *vd, char * const path, const UVT_UnvivVivOpt *opt)
{
  const int filesz = LIBNFSVIV_GetFilesize(path);
  FILE *file = path ? fopen(path, "rb") : NULL;
  UVT_Directory *ret = LIBNFSVIV_GetVivDirectory_FromFile(vd, file, filesz, opt);
  if (file)  fclose(file);
  return ret;
}


/* Returns 0 on success. */
static
int LIBNFSVIV_ValidateVivDirectory(const UVT_Directory * const vd)
{
  int err = 0;
  if (!vd)  err |= 1 << 0;
  /* if (vd->num_direntries < 0 || vd->num_direntries > vd->length)  err |= 1 << 1; */  /* irrelevant */
  if (vd->num_direntries_true < 0 || vd->num_direntries_true > vd->length)  err |= 1 << 2;
  /* if (vd->h_filesize < 0)  err |= 1 << 3; */  /* irrelevant */
  /* if (vd->header_size < 16)  err |= 1 << 4; */  /* irrelevant */
  if (vd->viv_hdr_size_true < 16)  err |= 1 << 5;
  /* if (vd->viv_hdr_size_true > vd->h_filesize)  err |= 1 << 6; */  /* irrelevant */
  /* if (vd->viv_hdr_size_true > vd->header_size)  err |= 1 << 7; */  /* irrelevant */
  /* if (vd->num_direntries - vd->num_direntries_true != vd->null_count)  err |= 1 << 8; */  /* irrelevant */
  SCL_log("LIBNFSVIV_ValidateVivDirectory: %d\n", err);
  return err;
}

/*
  Returns NULL on unsuccesful malloc.
  Returns char **filelist, an array of char* arrays. Returned filenames may be non-printable and may have embedded nul's. Consult UVT_Directory for the filenames lengths.

  The first element is a contiguous block of all filenames. The rest are pointers to the start of each filename.

  From (filelist != NULL) follows (sz > 0).
  The number of list elements is (sz-1), the last array element filelist[sz-1] is NULL.

  NB: filelist malloc'd size is upper-bounded, see libnfsviv.h header
*/
char **LIBNFSVIV_VivDirectoryToFileList_FromFile(UVT_Directory *vd, FILE *file, const int opt_invalidentries)
{
  char **ret = NULL;
  if (LIBNFSVIV_ValidateVivDirectory(vd))  return NULL;

  if (!file)  return NULL;

  for (;;)
  {
    char **filelist;
    const int filelist_len = vd->num_direntries_true - (!opt_invalidentries ? 0 : vd->null_count);
    const int filenames_sz = LIBNFSVIV_UVT_DirectorySumFilenameSizes(vd, opt_invalidentries);  /* guarantees (filenames_sz >= vd->num_direntries_true) */
    filelist = (char **)malloc((filelist_len + 1) * sizeof(*filelist) + filenames_sz * sizeof(**filelist));
    if (!filelist)
    {
      UVT_fprintf(stderr, "VivDirectoryToFileList: Cannot allocate memory\n");
      break;
    }
    filelist[filelist_len] = NULL;
    SCL_log("VivDirectoryToFileList_FromFile: filelist_len %d\n", filelist_len);

    /* Create list of all filenames, even invalid ones.
       All values are clamped s.t. they do not exceed file size.

       Expects strings of length 1 at least
    */
    if (vd->num_direntries_true > 0)
    {
      int i;
      int cnt_entries;
      char *ptr = (char *)(filelist + (filelist_len + 1));
      for (i = 0, cnt_entries = 0; i < vd->num_direntries_true; i++)
      {
        const int e_fname_len_ = LIBNFSVIV_clamp(vd->buffer[i].e_fname_len_, 0, LIBNFSVIV_FilenameMaxLen - 1);
        if (!opt_invalidentries && SCL_BITMAP_IsSet(vd->bitmap, i) == 0)
          continue;
        filelist[cnt_entries] = ptr;
        if (LIBNFSVIV_FreadToStr(ptr, e_fname_len_ + 1, vd->buffer[i].e_fname_ofs_, e_fname_len_, file) != e_fname_len_)
        {
          UVT_fprintf(stderr, "VivDirectoryToFileList: File read error at %d\n", vd->buffer[i].e_fname_ofs_);
          free(filelist);
          return NULL;
        }
        ptr[e_fname_len_] = '\0';
        ptr += e_fname_len_ + 1;
        ++cnt_entries;
      }
      SCL_assert(opt_invalidentries || cnt_entries == vd->num_direntries_true);
      SCL_assert(ptr <= (char *)(filelist + (filelist_len + 1)) + filenames_sz);
      SCL_log(".cnt_entries %d, vd->num_direntries_true %d, opt_invalidentries: %d\n", cnt_entries, vd->num_direntries_true, opt_invalidentries);
      SCL_log("filenames_sz %d\n", filenames_sz);
      SCL_log(".ptr: %p, last: %p\n", (void *)ptr, (void *)((char *)(filelist + (filelist_len + 1)) + filenames_sz));
#ifdef UVT_UNVIVTOOLMODULE
      if ((!opt_invalidentries && cnt_entries != vd->num_direntries_true) || ptr > (char *)(filelist + (filelist_len + 1)) + filenames_sz)
      {
        UVT_fprintf(stderr, "VivDirectoryToFileList: buffer overflow or incorrect count\n");
        free(filelist);
        return NULL;
      }
#endif
    }  /* if (vd->num_direntries_true > 0) */

    ret = filelist;
    break;
  }  /* for (;;) */

  return ret;
}

/* Wrapper for LIBNFSVIV_VivDirectoryToFileList_FromFile() */
char **LIBNFSVIV_VivDirectoryToFileList(UVT_Directory *vd, const char *path, const int opt_invalidentries)
{
  FILE *file = path ? fopen(path, "rb") : NULL;
  char **ret = LIBNFSVIV_VivDirectoryToFileList_FromFile(vd, file, opt_invalidentries);
  if (file)  fclose(file);
  return ret;
}

/* api: clients ----------------------------------------------------------------------------------------------------- */

/*
  LIBNFSVIV_Unviv() and LIBNFSVIV_Viv() are one-and-done functions for extracting and creating archives.
  LIBNFSVIV_UpdateVivDirectory() and LIBNFSVIV_Update() are used to update existing archives.
*/

/*
  Returns 1 on success, 0 on failure.
  Assumes viv_name and outpath are NOT const's and have size >= LIBNFSVIV_FilenameMaxLen
  Assumes (viv_name). Assumes (outpath). Overwrites directory 'outpath'.

  When UVT__RESTORE_CWD_IN_UNVIV is #define'd:
    On failure, the working directory may have been changed to 'outpath'.
  Otherwise, the working directory is changed to 'outpath' on success.

  (request_file_idx == 0), extract all
  (request_file_idx > 0), extract file at given 1-based index.
  (!request_file_name), extract file with given name. Overrides 'request_file_idx'.
  (opt->direnlenfixed < 10), do not impose fixed directory length
  (opt->filenameshex != 0), for nonprintable character in filenames, decode/encode to/from Filenames in Hexadecimal
  (opt->wenccommand != 0), write re-Encode command to path/to/input.viv.txt (keep files in order)
  (opt->overwrite < 1), overwrite existing files
  (opt->overwrite == 1), rename existing files
*/
int LIBNFSVIV_Unviv(char *viv_name, char *outpath,
                    int request_file_idx, const char *request_file_name,
                    UVT_UnvivVivOpt *opt)
{
  int retv = 0;
  FILE *file = NULL;
  int viv_filesize;
  int count_extracted = 0;
#ifdef UVT__RESTORE_CWD_IN_UNVIV
  char *cwd_buf = NULL;
#endif
  char *wenc_buf = NULL;
  FILE *wenc_f = NULL;
  int version = 0;  /* 1: wwww; 4,5,7,8 VIV/BIG; <1: invalid */
  UVT_Directory vd;
  memset(&vd, 0, sizeof(vd));

  SCL_log("LIBNFSVIV_Unviv:\n");
  SCL_log("LIBNFSVIV_Unviv: \"%s\" (%p), \"%s\" (%p)\n", viv_name, viv_name, outpath, outpath);
  SCL_log("LIBNFSVIV_Unviv: request_file_idx %d\n", request_file_idx);
  SCL_log("opt->filenameshex (call) %d\n", opt->filenameshex);

  opt->filenameshex = (opt->filenameshex || (opt->direnlenfixed >= 10));  /* fixed length entries with all-printable names are not known to exist */

  LIBNFSVIV_PrintUnvivVivOpt(opt);

  if (opt->dryrun)
    UVT_printf("Begin dry run\n");

  for (;;)
  {
    if (!LIBNFSVIV_GetFullPathName(viv_name, NULL, 0, NULL))
    {
      UVT_fprintf(stderr, "Unviv: Cannot get full path of archive.\n");
      retv = -1;
      break;
    }

    if (!opt->dryrun)
    {
      if (!LIBNFSVIV_IsDir(outpath))
      {
        UVT_printf("Unviv: Attempt creating directory '%s'\n", outpath);
        if (mkdir(outpath, 0755) != 0)
        {
          UVT_fprintf(stderr, "Unviv: Cannot create directory '%s'\n", outpath);
          retv = -1;
          break;
        }
      }

      if (!LIBNFSVIV_GetFullPathName(outpath, NULL, 0, NULL))
      {
        UVT_fprintf(stderr, "Unviv: Cannot get full path of outpath.\n");
        retv = -1;
        break;
      }

      if (opt->wenccommand)
      {
        wenc_buf = (char *)malloc(LIBNFSVIV_FilenameMaxLen * sizeof(*wenc_buf));
        if (!wenc_buf || !LIBNFSVIV_GetWENCPath(viv_name, wenc_buf, LIBNFSVIV_FilenameMaxLen))
          UVT_fprintf(stderr, "Unviv: Cannot append extension '%s' to '%s'\n", viv_name, LIBNFSVIV_WENCFileEnding);
      }  /* if (opt->wenccommand) */
    }  /* if (!opt->dryrun) */

    if (LIBNFSVIV_IsDir(viv_name))
    {
      UVT_fprintf(stderr, "Unviv: Cannot open directory as archive '%s'\n", viv_name);
      break;
    }

    if (opt->direnlenfixed >= 10)  UVT_printf("\nFixed directory entry length: %d\n", opt->direnlenfixed);
    if (opt->filenameshex)  UVT_printf("Filenames as hex: %d\n", opt->filenameshex);
    UVT_printf("\nExtracting archive: %s\n", viv_name);
    UVT_printf("Extracting to: %s\n", outpath);

    file = fopen(viv_name, "rb");
    if (!file)
    {
      UVT_fprintf(stderr, "Unviv: Cannot open '%s'\n", viv_name);
      break;
    }

    viv_filesize = LIBNFSVIV_GetFilesize(viv_name);
    UVT_printf("Archive Size (parsed) = %d (0x%x)\n", viv_filesize, viv_filesize);
#ifdef UVTWWWW
    version = LIBNFSVIV_GetVivVersion_FromPath(viv_name);
    if (version != 1)
    {
#endif
      if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd, file, viv_filesize, opt))
        break;
      SCL_log("vd->null_count: %d\n", vd.null_count);
      LIBNFSVIV_EnsureArchiveNotInUVT_DirectoryWritePaths(&vd, viv_name, outpath, file, viv_filesize);  /* invalidate files that would overwrite archive */
      SCL_log("vd->null_count: %d\n", vd.null_count);
#ifdef UVTWWWW
    }
    else
    {
      if (!SCL_GetwwwwInfo_FromFile(&vd, file, viv_filesize, 0 * opt->verbose))
      {
        SCL_CheckwwwwDirectory(&vd, viv_filesize);
        UVT_fprintf(stderr, "Cannot read file %s\n", viv_name);
        break;
      }
    }
#endif

    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_UVT_DirectoryGetIdxFromFname(&vd, file, request_file_name);
      if (request_file_idx <= 0)  break;
    }

    if (opt->verbose)
      LIBNFSVIV_PrintStatsDec(&vd, file, viv_filesize, request_file_idx, request_file_name, opt);

    if (opt->dryrun)
    {
      UVT_printf("End dry run\n");
      retv = 1;
      break;
    }

    /* Option: Write re-Encode command to file */
    if (!opt->dryrun && opt->wenccommand && wenc_buf)
    {
      wenc_f = fopen(wenc_buf, "a");
      if (!wenc_f)
      {
        UVT_fprintf(stderr, "Unviv: Cannot open '%s' (option -we)\n", wenc_buf);
      }
      else
      {
        if (memcmp(vd.format, "BIGF", 4))  /* omit writing default BIGF  */
        {
          fprintf(wenc_f, "%s%.4s ", "-fmt", LIBNFSVIV_GetVivVersionString(LIBNFSVIV_GetVivVersion_FromBuf(vd.format)));
        }
        if (vd.__padding[0] >> 4)
        {
          fprintf(wenc_f, "%s%d ", "-alf", LIBNFSVIV_GetBitIndex(vd.__padding[0] >> 4));
        }
        fprintf(wenc_f, "\"%s\"", viv_name);
        fflush(wenc_f);
      }
      free(wenc_buf);  /* no longer needed */
      wenc_buf = NULL;
    }

    /* Extract archive */
#ifdef UVT__RESTORE_CWD_IN_UNVIV
    cwd_buf = (char *)malloc((LIBNFSVIV_FilenameMaxLen + 64) * sizeof(*cwd_buf));
    if (!cwd_buf || !getcwd(cwd_buf, LIBNFSVIV_FilenameMaxLen) || !!chdir(outpath))
    {
      UVT_fprintf(stderr, "Unviv: Cannot change working directory to '%s'\n", outpath);
      break;
    }
#else
    if (!!chdir(outpath))
    {
      UVT_fprintf(stderr, "Unviv: Cannot change working directory to '%s'\n", outpath);
      break;
    }
#endif

    {
      char buf[LIBNFSVIV_FilenameMaxLen];
      char *ptr = buf;
      char *ptr2;
      if (version == 1)
      {
        /* <archive_basename>_<id>_<filename> */
        ptr = LIBNFSVIV_GetPathBasename(viv_name);
        ptr = (char *)LIBNFSVIV_memccpy(buf, ptr, '\0', sizeof(buf));
        ptr = buf + strlen(buf);
      }
      ptr2 = ptr;
      if (request_file_idx == 0)
      {
        int i = 0;
        for (i = 0; i < vd.num_direntries_true; i++)
        {
          /* Continue through failures */
          if (SCL_BITMAP_IsSet(vd.bitmap, i) == 1)
          {
            size_t len__ = 0;
            ptr = ptr2;
            if (version == 1)
            {
              sprintf(ptr, "_%04d_", i);
              len__ = strlen(buf);
              ptr = buf + len__;
            }
            len__ = LIBNFSVIV_CreateExtractFilename(&vd.buffer[i], file, version!=1?opt->filenameshex:0, ptr, (int)sizeof(buf) - (int)len__ - 1);
            if (version == 1)  SCL_log("<archive_basename>_<id>_<filename>: %s\n", buf);
            if (len__ == 0 || LIBNFSVIV_IsPrintString(buf, len__) != (int)len__)
            {
              UVT_fprintf(stderr, "Unviv: Cannot create printable filename for entry (%d)\n", i);
              continue;
            }
            count_extracted += LIBNFSVIV_UVT_DirEntrExtractFile(&vd.buffer[i], file, /* opt->filenameshex, */ version==1?1:opt->overwrite, wenc_f, outpath, buf, sizeof(buf));
          }
        }
      }
      else
      {
        size_t len__;
        if (request_file_idx < 0 || request_file_idx > vd.num_direntries_true)
        {
          UVT_fprintf(stderr, "Unviv: Requested idx (%d) out of bounds (1-based index)\n", request_file_idx);
          break;
        }
        if (SCL_BITMAP_IsSet(vd.bitmap, request_file_idx - 1) != 1)
        {
          UVT_fprintf(stderr, "Unviv: Requested idx (%d) is invalid entry\n", request_file_idx);
          break;
        }
        len__ = LIBNFSVIV_CreateExtractFilename(&vd.buffer[request_file_idx - 1], file, opt->filenameshex, buf, sizeof(buf));
        if (!len__)
        {
          UVT_fprintf(stderr, "Unviv: Cannot create filename for requested entry %d\n", request_file_idx);
          break;
        }
        if (LIBNFSVIV_IsPrintString(buf, len__) != (int)len__)
        {
          UVT_fprintf(stderr, "Unviv: Skipping non-printable filename (%d)\n", request_file_idx);
          continue;
        }
        if (!LIBNFSVIV_UVT_DirEntrExtractFile(&vd.buffer[request_file_idx - 1], file, /* opt->filenameshex, */ version==1?1:opt->overwrite, wenc_f, outpath, buf, sizeof(buf)))
        {
          break;
        }
        ++count_extracted;
      }
    }
    retv = 1;
    break;
  }  /* for (;;) */

  if (!opt->dryrun)
    UVT_printf("Number extracted: %d\n", count_extracted);

#ifdef UVT__RESTORE_CWD_IN_UNVIV
  if (cwd_buf && !!chdir(cwd_buf))
    UVT_fprintf(stderr, "Cannot restore working directory\n");
#endif

#ifdef UVT__RESTORE_CWD_IN_UNVIV
  free(cwd_buf);
#endif
  if (wenc_f)
  {
    fprintf(wenc_f, "\n");
    fclose(wenc_f);
  }
  free(wenc_buf);
  LIBNFSVIV_UVT_DirectoryRelease(&vd);
  if (file)  fclose(file);

  return retv;
}

/*
  Assumes (viv_name). Overwrites file 'viv_name'. Skips unopenable infiles.
  Assumes (opt->requestfmt).
  (opt->requestendian): must be called with 0xE (BIGF/BIGH), 0xC (BIG4), 0x0 (wwww)
  (opt->alignfofs) aligns file offsets to powers of 2 (clamped to [0,16] and set to previous power of 2)
  Returns 1 on success, 0 on failure.
*/
int LIBNFSVIV_Viv(const char * const viv_name,
                  char **infiles_paths, const int count_infiles,
                  UVT_UnvivVivOpt *opt)
{
  int err = 0;
  int i;
  FILE *file = NULL;
  int filesz = -1;
  int count_archived = 0;
  const int opt_requestfmt_i = LIBNFSVIV_GetVivVersion_FromBuf(opt->requestfmt);
  UVT_Directory vd;
  memset(&vd, 0, sizeof(vd));

  if (opt->alignfofs != 0)
  {
    opt->alignfofs = LIBNFSVIV_clamp(opt->alignfofs, 0, 16);
    opt->alignfofs = LIBNFSVIV_PrevPower(opt->alignfofs);
  }

  SCL_log(
    "count_infiles %d\n"
    "opt->dryrun %d\n"
    "opt->verbose %d\n"
    "opt->direnlenfixed %d\n"
    "opt->filenameshex %d\n"
    , count_infiles, opt->dryrun, opt->verbose, opt->direnlenfixed, opt->filenameshex);
  SCL_log("opt->requestfmt %s\n", LIBNFSVIV_GetVivVersionString(LIBNFSVIV_GetVivVersion_FromBuf(opt->requestfmt)));
  SCL_log("opt_requestfmt_i %d\n", opt_requestfmt_i);
  SCL_log("opt->faithfulencode %d\n", opt->faithfulencode);

  if (opt->dryrun)
    UVT_printf("Begin dry run\n");

  UVT_printf("\nCreating archive: %s\n", viv_name);
  UVT_printf("Number of files to encode = %d\n", count_infiles);

  if (count_infiles > LIBNFSVIV_DirEntrMax || count_infiles < 0)
  {
    UVT_fprintf(stderr, "Viv: Number of files to encode too large (%d > %d)\n", count_infiles, LIBNFSVIV_DirEntrMax);
    return 0;
  }

  for (;;)
  {
    /* Set directory */

    /*
      Use struct UVT_Directory bitmap to capture input file openability.
      In typical use, all input files will be available.
      Hence, any malloc overhead incurred for invalid paths is acceptable.
    */
    if (!LIBNFSVIV_UVT_DirectorySet(&vd, infiles_paths, count_infiles, opt))
    {
      err++;
      break;
    }

    if (opt->verbose)
      LIBNFSVIV_PrintStatsEnc(&vd, infiles_paths, count_infiles, opt);

    if (opt->dryrun)
    {
      UVT_printf("End dry run\n");
      break;
    }

    file = fopen(viv_name, "wb");
    if (!file)
    {
      UVT_fprintf(stderr, "Viv: Cannot create output file '%s'\n", viv_name);
      err++;
      break;
    }

    /* Write directory to file */
#ifdef UVTWWWW
    if (opt_requestfmt_i >= 4)
    {
#endif
      if (!LIBNFSVIV_WriteVivHeader(&vd, file))
      {
        UVT_fprintf(stderr, "Viv: Cannot write %s header\n", "Viv");
        err++;
        break;
      }
      UVT_printf("Endianness (written) = 0x%x\n", vd.__padding[0] & 0xE);
      if (!LIBNFSVIV_WriteVivDirectory(&vd, file, infiles_paths, count_infiles, opt))
      {
        err++;
        break;
      }
      UVT_printf("File offset alignment (written) = %d\n", LIBNFSVIV_GetBitIndex(vd.__padding[0] >> 4));
#ifdef UVTWWWW
    }
    else
    {
      if (!LIBNFSVIV_WritewwwwInfo(&vd, file))
      {
        UVT_fprintf(stderr, "Viv: Cannot write %s header\n", "wwww");
        err++;
        break;
      }
    }
#endif
    UVT_printf("Header Size (written) = %d (0x%x)\n", vd.viv_hdr_size_true, vd.viv_hdr_size_true);
    filesz = (int)ftell(file);

    /* Write infiles to file, abandon on failure */
    for (i = 0; i < count_infiles; i++)
    {
      if (SCL_BITMAP_IsSet(vd.bitmap, i) == 1)
      {
        LIBNFSVIV_WriteNullBytes(file, vd.buffer[i].e_offset);
        filesz = LIBNFSVIV_VivWriteFile(file, NULL, infiles_paths[i], 0, vd.buffer[i].e_filesize);
        if (filesz < 0)
        {
          err++;
          break;
        }
        ++count_archived;
      }
    }
    UVT_printf("Archive Size (written) = %d (0x%x)\n", filesz, filesz);

    if (!opt->dryrun)
      UVT_printf("Number archived: %d\n", count_archived);

    /* Validate */
#ifdef UVTWWWW
    if (opt_requestfmt_i != 1 ? !LIBNFSVIV_VivCheckHeader(&vd, filesz) : !!SCL_CheckwwwwDirectory(&vd, filesz))
#else
    if (!LIBNFSVIV_VivCheckHeader(&vd, filesz))
#endif
    {
      UVT_fprintf(stderr, "Viv: New archive failed format check (header)\n");
      err++;
      break;
    }
    LIBNFSVIV_VivValidateDirectory(&vd, filesz);
#if SCL_DEBUG >= 3
    if (vd.null_count > 0 && opt->verbose)  LIBNFSVIV_UVT_DirEntrPrint(&vd, 0);
#endif

    break;
  }  /* for (;;) */

  if (file)  fclose(file);
  LIBNFSVIV_UVT_DirectoryRelease(&vd);

  return err == 0;
}



/* Insert or replace.

  Returns modified entry index (1-based) on success, -1 on failure.

  Expects (vd != vd_old).
  Expects (file) and (infile_path).
  opt->insert: 0 (replace), >0 (insert|add), <0 (remove)
  opt->replacefilename: 0 (do not replace filename), !0 (replace filename)
  opt->faithfulencode:
  opt->alignfofs: assumes one of the following values 0 (no alignment), -1 (keep detected alignment), 2/4/8/16 (force alignment)

  (opt->insert==1) overrides opt->replacefilename.
*/
int LIBNFSVIV_UpdateVivDirectory(UVT_Directory *vd, const UVT_Directory * const vd_old, FILE *file, char *infile_path,
                                 const char * const request_file_name, int request_file_idx,
                                 const UVT_UnvivVivOpt *opt)
{
  int retv = -1;
  int i;
  int sum_filesz = 0;  /* validation */
  int sum_padding = 0;

  for (;;)
  {
    if (!vd || !vd_old || !file || !infile_path)
    {
      UVT_fprintf(stderr, "UpdateVivDirectory: Invalid input\n");
      break;
    }
    if (vd == vd_old)
    {
      UVT_fprintf(stderr, "UpdateVivDirectory: vd and vd_old must be different instances\n");
      break;
    }

    /* Get target index */
    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_UVT_DirectoryGetIdxFromFname(vd, file, request_file_name);
      if (request_file_idx <= 0)  break;
    }

    /**
      Update UVT_Directory based on file, even for invalid file.

      opt->faithfulencode == 0:  directory entries for invalid files do not count towards offsets
      opt->faithfulencode != 0:  directory entries for invalid files do count towards offsets (pretend all is well, set filesize 0)
    */
    if (opt->insert >= 0 && LIBNFSVIV_IsFile(infile_path) && !LIBNFSVIV_IsDir(infile_path))
    {
      UVT_DirEntr vde_old;
      UVT_DirEntr vde_temp;

      if (request_file_idx <= 0 || (opt->faithfulencode && request_file_idx > vd->num_direntries) || (!opt->faithfulencode && request_file_idx > vd->num_direntries_true))
      {
        UVT_fprintf(stderr, "UpdateVivDirectory: Requested idx (%d) out of bounds (1-based index)\n", request_file_idx);
        break;
      }
      if (SCL_BITMAP_IsSet(vd->bitmap, request_file_idx - 1) != 1)
      {
        UVT_fprintf(stderr, "UpdateVivDirectory: Requested idx (%d) is invalid entry\n", request_file_idx);
        break;
      }

      vde_old = vd_old->buffer[request_file_idx - 1];
      vde_temp = vde_old;
      vde_temp.e_filesize = LIBNFSVIV_GetFilesize(infile_path);

      if (opt->insert > 0 || opt->replacefilename)
      {
#ifdef _WIN32
        char buf[LIBNFSVIV_FilenameMaxLen];
        int len_filename = LIBNFSVIV_GetPathBasename2(infile_path, NULL, buf, sizeof(buf)) + 1;
#else
        int len_filename = LIBNFSVIV_GetPathBasename2(infile_path, NULL, NULL, 0) + 1;
#endif
        len_filename = LIBNFSVIV_clamp(len_filename, 1, LIBNFSVIV_FilenameMaxLen);
        if (opt->filenameshex)  len_filename = LIBNFSVIV_ceil(len_filename, 2);

        vde_temp.e_fname_len_ = len_filename - 1;
      }  /* if opt->replacefilename */

      if (opt->insert == 0)
      {
        /* Update header */
        if (!opt->faithfulencode)  SCL_BITMAP_Set(vd->bitmap, request_file_idx - 1);
        vd->h_filesize += vde_temp.e_filesize - vde_old.e_filesize;
        vd->h_filesize += vde_temp.e_fname_len_ - vde_old.e_fname_len_;
        vd->header_size += vde_temp.e_fname_len_ - vde_old.e_fname_len_;
        vd->viv_hdr_size_true += vde_temp.e_fname_len_ - vde_old.e_fname_len_;

        /* -1: keep detected alignment, 0: force no alignment, >0: force alignment */
        if (opt->alignfofs >= 0)  vd->__padding[0] &= 0xF;
        if (opt->alignfofs > 0 && LIBNFSVIV_GetVivVersion_FromBuf(vd_old->format) != 1)
        {
          vd->__padding[0] |= (LIBNFSVIV_GetIndexBit(opt->alignfofs) << 4);  /* final: request file offset alignment */
        }

        /* Update target entry */
        vd->buffer[request_file_idx - 1] = vde_temp;

        /* Update existing entries */
        for (i = 0; i < vd->num_direntries; i++)
        {
          /* Offset remaining entries */
          if (i > request_file_idx - 1)
          {
            vd->buffer[i].e_fname_ofs_ += vde_temp.e_fname_len_ - vde_old.e_fname_len_;
          }
          /* Offset contents */
          if (SCL_BITMAP_IsSet(vd->bitmap, i) == 1)
          {
            vd->buffer[i].e_offset += vd->viv_hdr_size_true - vd_old->viv_hdr_size_true;
            if (i != request_file_idx - 1 && vd->buffer[i].e_offset >= vde_old.e_offset)
            {
              vd->buffer[i].e_offset += vde_temp.e_filesize - vde_old.e_filesize;
            }
            sum_filesz += vd->buffer[i].e_filesize;
          }
        }  /* for i */
      }
      else if (opt->insert > 0)  /* insert (add) entry at index */
      {
        UVT_fprintf(stderr, "not implemented\n");
        break;
      }
    }  /* if() workload */
    else if (opt->insert < 0)  /* remove entry at index */
    {
      UVT_fprintf(stderr, "not implemented\n");
      break;
    }
    else
    {
      UVT_fprintf(stderr, ": Invalid input '%s'\n", infile_path);
      break;
    }

    /* If requested, align file offsets to some non-trivial power of 2 */
    if (vd->__padding[0] >> 4)
      sum_padding = LIBNFSVIV_AlignFileOffsets(vd, vd->num_direntries, opt->faithfulencode);

    SCL_assert(vd->header_size == vd->viv_hdr_size_true);
    SCL_assert(vd->h_filesize == vd->header_size + sum_filesz + sum_padding);

    if ((vd->num_direntries_true + vd->null_count == vd->num_direntries) && (vd->header_size == vd->viv_hdr_size_true) && (vd->h_filesize == vd->header_size + sum_filesz + sum_padding))
      retv = request_file_idx;
    break;
  }  /* for (;;) */

  return retv;
}


/*
  Assumes viv_name is buffer of length >= 4096.
  Assumes viv_name_out is NULL or buffer of length >= 4096.

  opt->insert: 0 (replace), >0 (insert|add), <0 (remove)
  opt->alignfofs: 0 (force no alignment), -1 (keep detected alignment), 2,4,8,16 (force alignment)

  New archive will be written to temporary path. In case of success, will be copied to viv_name_out_
*/
int LIBNFSVIV_Update(char *viv_name, const char * const viv_name_out_,
                     int request_file_idx, const char *request_file_name,
                     char *infile_path,
                     UVT_UnvivVivOpt *opt)
{
  int retv = 0;
  int i;
  FILE *file = NULL;
  int filesz = -1;
  FILE *file_out = NULL;
  int count_infiles;
  char **infiles_paths = NULL;
  char *temppath;
  const char * const ptr_viv_name_out = viv_name_out_ ? viv_name_out_ : viv_name;  /* target archive path */
  int count_archived = 0;
  UVT_Directory vd;
  UVT_Directory vd_old;
  memset(&vd, 0, sizeof(vd));
  memset(&vd_old, 0, sizeof(vd_old));
  opt->filenameshex = (opt->filenameshex || (opt->direnlenfixed >= 10));  /* fixed length entries with all-printable names are not known to exist */
  opt->alignfofs = LIBNFSVIV_clamp(opt->alignfofs, -1, 16);
  if (opt->alignfofs > 0)  opt->alignfofs = LIBNFSVIV_PrevPower(opt->alignfofs);

  if (LIBNFSVIV_IsPrintString(viv_name, 4096) != (int)strlen(viv_name)
    || (viv_name_out_ && LIBNFSVIV_IsPrintString(viv_name_out_, 4096) != (int)strlen(viv_name_out_))
    || LIBNFSVIV_IsPrintString(infile_path, 4096) != (int)strlen(infile_path)
    || (request_file_name && LIBNFSVIV_IsPrintString(request_file_name, 4096) != (int)strlen(request_file_name))
  )
  {
    UVT_fprintf(stderr, "VivUpdate: Non-printable characters in input\n");
    return 0;
  }

  SCL_log("viv_name %s\n", viv_name);
  SCL_log("viv_name_out_ %s\n", viv_name_out_?viv_name_out_:"(null)");
  SCL_log("request_file_idx %d\n", request_file_idx);
  SCL_log("request_file_name %s\n", request_file_name?request_file_name:"(null)");
  SCL_log("infile_path %s\n", infile_path);
  LIBNFSVIV_PrintUnvivVivOpt(opt);

  if (opt->dryrun)
    UVT_printf("Begin dry run\n");

  for (;;)
  {
    UVT_printf("Updating archive: %s\n", viv_name);

    temppath = (char *)malloc(LIBNFSVIV_FilenameMaxLen * sizeof(*temppath));
    if (!temppath)
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot allocate memory\n");
      break;
    }

    /* Build write path in temp directory */
    {
      const int len_temp = LIBNFSVIV_GetTempPath(LIBNFSVIV_FilenameMaxLen, temppath);
      if (len_temp > 0 && len_temp < LIBNFSVIV_FilenameMaxLen - 1)
      {
        char *ptr;
        char buf_[LIBNFSVIV_FilenameMaxLen];
        LIBNFSVIV_memccpy(buf_, ptr_viv_name_out, '\0', LIBNFSVIV_FilenameMaxLen);
        if (LIBNFSVIV_GetPathBasename2(buf_, &ptr, buf_, sizeof(buf_)) < 1)  break;
        if (!LIBNFSVIV_memccpy(temppath + len_temp, ptr, '\0', LIBNFSVIV_FilenameMaxLen - len_temp))  break;
      }
      else
      {
        UVT_fprintf(stderr, "VivUpdate: Cannot get temporary path\n");
        break;
      }
    }

    UVT_printf("Writing to archive: %s\n", ptr_viv_name_out);
    SCL_log("temppath %s\n", temppath);

    /* Read UVT_Directory */
    if (!LIBNFSVIV_GetFullPathName(viv_name, NULL, 0, NULL))
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot get full path of archive.\n");
      break;
    }

    if (LIBNFSVIV_IsDir(viv_name))
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot open directory as archive '%s'\n", viv_name);
      break;
    }

    if (LIBNFSVIV_IsDir(ptr_viv_name_out))
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot open directory as file '%s'\n", ptr_viv_name_out);
      break;
    }

    if (opt->direnlenfixed >= 10)  UVT_printf("\nFixed directory entry length: %d\n", opt->direnlenfixed);
    if (opt->filenameshex)  UVT_printf("Filenames as hex: %d\n", opt->filenameshex);
    if (opt->alignfofs >= 0)  UVT_printf("Alignment of file offsets: %d\n", opt->alignfofs);

    file = fopen(viv_name, "rb");
    if (!file)
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot open '%s'\n", viv_name);
      break;
    }

    filesz = LIBNFSVIV_GetFilesize(viv_name);
    UVT_printf("Archive Size (parsed) = %d (0x%x)\n", filesz, filesz);
    if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd, file, filesz, opt))  break;
    /* silently parse again instead of deep-copying &vd ... */
    fseek(file, 0, SEEK_SET);
    {
      void (*temp)(const char *format, ...) = UVT_printf;
      UVT_printf = SCL_printf_silent;
      if (!LIBNFSVIV_GetVivDirectory_FromFile(&vd_old, file, filesz, opt))  break;
      UVT_printf = temp;
    }

    if (opt->verbose)
    {
      UVT_printf("\n");
      UVT_printf("Before update...\n");
      LIBNFSVIV_PrintStatsDec(&vd, file, filesz, request_file_idx, request_file_name, opt);
    }

    request_file_idx = LIBNFSVIV_UpdateVivDirectory(&vd, &vd_old, file, infile_path,
                                                    request_file_name, request_file_idx,
                                                    opt);
    SCL_log("request_file_idx %d\n", request_file_idx);
    if (request_file_idx < 0)
    {
      break;
    }

    if (opt->verbose)
    {
      UVT_printf("After update...\n");
      LIBNFSVIV_UVT_DirEntrPrint(&vd, opt->faithfulencode);
    }

    /** TODO: export to static function  */
    /* Get filenames / filepaths for output archive */
    if (opt->insert == 0)
    {
      int len_ = 0;
      char *ptr;
      if (vd.num_direntries_true != vd_old.num_direntries_true)
      {
        UVT_fprintf(stderr, "VivUpdate: mismatched number of dir entries\n");
        break;
      }
      count_infiles = vd_old.num_direntries_true;
      SCL_log("vd_old count_infiles %d\n", count_infiles);
      if (opt->replacefilename)  len_ = strlen(infile_path) + 1;  /* safety: space for new filename */
      for (i = 0; i < vd_old.num_direntries_true; i++)
      {
        if (opt->replacefilename && i == request_file_idx - 1)
          continue;
        len_ += LIBNFSVIV_clamp(vd_old.buffer[i].e_fname_len_ + 1, 1, LIBNFSVIV_FilenameMaxLen);
      }
      SCL_log("vd sum filenames lengths: len_ %d\n", len_);
      infiles_paths = (char **)malloc(count_infiles * sizeof(*infiles_paths) + len_ * sizeof(**infiles_paths));
      if (!infiles_paths)
      {
        UVT_fprintf(stderr, "VivUpdate: Cannot allocate memory\n");
        break;
      }

      ptr = (char *)(infiles_paths + count_infiles);
      for (i = 0; i < vd_old.num_direntries_true; i++)
      {
        infiles_paths[i] = ptr;
        if (i != request_file_idx - 1 || !opt->replacefilename)
        {
          const int tmp_bufsz_ = LIBNFSVIV_clamp(vd_old.buffer[i].e_fname_len_ + 1, 1, LIBNFSVIV_FilenameMaxLen);
          const int tmp_len_ = LIBNFSVIV_min(vd_old.buffer[i].e_fname_len_, LIBNFSVIV_FilenameMaxLen);
          if (tmp_len_ != LIBNFSVIV_FreadToStr(ptr,
                                               tmp_bufsz_,
                                               vd_old.buffer[i].e_fname_ofs_, tmp_len_, file)
          )
          {
            UVT_fprintf(stderr, "VivUpdate: LIBNFSVIV_FreadToStr\n");
            break;  /* inner loop */
          }
          ptr[tmp_len_] = '\0';
          ptr += tmp_len_ + 1;
        }
        else
        {
          const int tmp_len_ = strlen(infile_path) + 1;
          memcpy(ptr, infile_path, tmp_len_);
          ptr += tmp_len_;
        }
      }  /* for i */
      for (i = 0; i < vd_old.num_direntries_true; i++)  SCL_log(" infiles_paths[%d] %s\n", i, infiles_paths[i]);
    }
    else if (opt->insert > 0)
    {
      UVT_fprintf(stderr, "VivUpdate: not implemented\n");
      break;
    }
    else
    {
      UVT_fprintf(stderr, "VivUpdate: not implemented\n");
      break;
    }

    if (opt->dryrun)
    {
      UVT_printf("End dry run\n");
      retv = 1;
      break;
    }

    file_out = fopen(temppath, "wb+");
    if (!file_out)
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot open '%s'\n", temppath);
      break;
    }

    /* Write viv directory to file */
    if (!LIBNFSVIV_WriteVivHeader(&vd, file_out))
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot write Viv header\n");
      break;
    }
    UVT_printf("Endianness (written) = 0x%x\n", vd.__padding[0] & 0xE);

    if (!LIBNFSVIV_WriteVivDirectory(&vd, file_out, infiles_paths, count_infiles, opt))
    {
      UVT_fprintf(stderr, "VivUpdate: Cannot write Viv directory\n");
      break;
    }
    UVT_printf("File offset alignment (written) = %d\n", LIBNFSVIV_GetBitIndex(vd.__padding[0] >> 4));
    UVT_printf("Header Size (written) = %d (0x%x)\n", vd.viv_hdr_size_true, vd.viv_hdr_size_true);

    /* Write files to new archive, abandon on failure */
    if (opt->insert == 0)
    {
      int offs_ = vd.viv_hdr_size_true;
      for (i = 0; i < vd.num_direntries_true; i++)
      {
        if (SCL_BITMAP_IsSet(vd.bitmap, i) == 1)
        {
          int err__ = 0;
          int sanity__ = 0;
          while (offs_ < vd.buffer[i].e_offset && sanity__ < (1 << 22))  /* upper-bounded gap */
          {
            fputc('\0', file_out);
            ++offs_;
            ++sanity__;
            SCL_log(".");
          }
          if (i != request_file_idx - 1)
            err__ = LIBNFSVIV_VivWriteFile(file_out, file, NULL, vd_old.buffer[i].e_offset, vd.buffer[i].e_filesize);
          else
            err__ = LIBNFSVIV_VivWriteFile(file_out, NULL, infile_path, 0, vd.buffer[i].e_filesize);
          if (err__ < 0)
          {
            UVT_fprintf(stderr, "VivUpdate: Cannot write Viv archive\n");
            break;
          }
          offs_ += vd.buffer[i].e_filesize;
          ++count_archived;
          SCL_log("| %d %d  (vd_old.buffer[i].e_offset %d)\n", offs_, (int)ftell(file_out), vd.buffer[i].e_offset);
        }
      }
      if (count_archived != vd.num_direntries_true)
      {
        break;
      }
    }
    else
    {
      UVT_fprintf(stderr, "VivUpdate: not implemented\n");
      break;
    }
    UVT_printf("Archive Size (written) = %d (0x%x)\n", (int)ftell(file_out), (int)ftell(file_out));
    UVT_printf("Number archived: %d\n", count_archived);

    if (opt->verbose)
    {
      UVT_printf("After write...\n");
      LIBNFSVIV_PrintStatsDec(&vd, file_out, (int)ftell(file_out), 0, NULL, opt);
    }
    fclose(file_out);  /* must close before copying file */
    file_out = NULL;

    {
      char buf[LIBNFSVIV_FilenameMaxLen];
      if (!LIBNFSVIV_memccpy(buf, ptr_viv_name_out, '\0', LIBNFSVIV_FilenameMaxLen))
      {
        UVT_fprintf(stderr, "VivUpdate: Cannot copy archive name\n");
        break;
      }
      if (!LIBNFSVIV_CopyFile(temppath, buf, 0))
      {
        UVT_fprintf(stderr, "VivUpdate: Cannot create '%s'\n", buf);
        break;
      }
    }
    retv = 1;
    break;
  }  /* for (;;) */

  free(infiles_paths);
  LIBNFSVIV_UVT_DirectoryRelease(&vd_old);
  LIBNFSVIV_UVT_DirectoryRelease(&vd);
  if (file_out)  fclose(file_out);
  if (file)  fclose(file);
  free(temppath);

  return retv;
}



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LIBNFSVIV_H_ */
