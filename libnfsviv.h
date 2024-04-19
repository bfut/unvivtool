/*
  libnfsviv.h - implements BIGF decoding/encoding (commonly known as VIV/BIG)
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
  BIGF theoretical limits, assuming signed int:
    min header len:          16         0x10
    max header len:          2147483631 0x7fffffef
    min directory entry len: 10         0xa
    min dir entries count:   0
    max dir entries count:   214748363  0x0ccccccb

  Special cases:
    BIGF header can have a fixed directory entry length (e.g., 80 bytes)
    BIG4 has VivHeader.filesize encoded in little endian

  unviv() handles the following archive manipulations / oddities {with strategy}:
    * Archive header has incorrect number of directory entries {assume large enough value}
    * Archive header has incorrect number directory length {value unused}
    * Archive header has incorrect offset {value unused}
    * At least 1 directory entry has illegal offset or length {skip file}
    * Two directory entries have the same file name (use opt_overwrite == 1) {overwrite or rename existing}
    * Directory entry has a file name identical to archive name {skip file}
    * Directory entry file name is followed by more than one nul (possibly directory entry length padded to multiples of 4, etc.) {native support}
    * Directory entry file name contains non-ASCII UTF8 characters {native support in UVTUTF8-branch}
    * Directory entry file name contains unprintable characters (use opt_direnlenfixed == sz and opt_filenameshex == 1) {skip file or represent filename in base16}
    * Directory entry file name is too long {skips file}
    * Directory entry has fixed length and filename string is followed by large number of nul's (use opt_direnlenfixed == sz) {native support via option opt_direnlenfixed}
*/

/*
  Compiling
  realpath() on non-Win32 systems requires _GNU_SOURCE ...
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

#define UVTVERS "1.19"
#define UVTCOPYRIGHT "Copyright (C) 2020-2024 Benjamin Futasz (GPLv3+)"

#ifndef UVTVERBOSE
#define UVTVERBOSE 0  /* >=1 enables debug console output */
#endif

#ifdef UVTUTF8  /* unviv() utf8-filename support */
#include "./include/dfa.h"
#endif

/* BIGF has big-endian numeric values */
typedef struct {
  char format[4];  /* BIGF, BIGH or BIG4 */
  int filesize;
  int count_dir_entries;
  int header_size;  /* includes VIV directory. filename lengths include nul */
} VivHeader;

typedef struct {
  int offset;
  int filesize;
  int ofs_begin_filename;
  int filename_len_;  /* internal, not part of format */
  int valid_entr_;  /* internal, not part of format */
} VivDirEntr;

#ifndef LIBNFSVIV_max
#define LIBNFSVIV_max(x,y) ((x)<(y)?(y):(x))
#define LIBNFSVIV_min(x,y) ((x)<(y)?(x):(y))
#define LIBNFSVIV_clamp(x,minv,maxv) ((maxv)<(minv)||(x)<(minv)?(minv):((x)>(maxv)?(maxv):(x)))
#endif

#if !defined(PATH_MAX) && !defined(_WIN32)
#define PATH_MAX 4096  /* for realpath() */
#endif

#define kLibnfsvivBufferSize 4096
#if !defined(_WIN32)
#define kLibnfsvivFilenameMaxLen PATH_MAX
#else
#define kLibnfsvivFilenameMaxLen 260 * 4  /* hedge for utf8 */
#endif
#define kLibnfsvivDirEntrMax 2097152  /* size: 12 * kLibnfsvivDirEntrMax */

/* util --------------------------------------------------------------------- */

/* Return length (excluding nul) if string, else 0. */
#ifdef UVTUTF8
static
int LIBNFSVIV_IsUTF8String(unsigned char *s, const size_t max_len)
{
  size_t pos = 0;
  unsigned int codepoint, state = 0;
  while (!(state == UTF8_REJECT) && (pos < max_len) && *s)
  {
    DFA_decode(&state, &codepoint, *s++);
    ++pos;
  }
  return pos * (pos < max_len) * (state == UTF8_ACCEPT);
}
#else
static
int LIBNFSVIV_IsPrintString(unsigned char *s, const size_t max_len)
{
  size_t pos = 0;
  while ((pos < max_len) && *s)
  {
    if (!isprint(*s++))
      pos = max_len;
    ++pos;
  }
  return pos * (pos < max_len);
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

/* Memsets 'dest' to '\\0'. Assumes (dest_len >= src_len) */
static
void LIBNFSVIV_UcharToNulChar(const unsigned char *src, char *dest,
                              const size_t src_len, const size_t dest_len)
{
  memset(dest, '\0', dest_len);
  memcpy(dest, src, src_len);
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
  char buf[kLibnfsvivFilenameMaxLen] = {0};
  while (*ptr && *ptr + 1 && i < kLibnfsvivFilenameMaxLen - 2)  /* buf always ends on nul */
  {
    buf[i] = LIBNFSVIV_hextoint(*ptr) << 4;
    buf[i] += LIBNFSVIV_hextoint(*(ptr + 1));
    ptr += 2;
    ++i;
  }
  memcpy(str, buf, kLibnfsvivFilenameMaxLen);
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
  char buf[kLibnfsvivFilenameMaxLen] = {0};
  while ((*ptr || i < 2*min_len) && i < kLibnfsvivFilenameMaxLen - 2 - 1)  /* buf always ends on nul */
  {
    buf[i] = LIBNFSVIV_inttohex((*ptr & 0xF0) >> 4);
    buf[i + 1] = LIBNFSVIV_inttohex(*ptr & 0xF);
    ++ptr;
    i += 2;
  }
  memcpy(str, buf, kLibnfsvivFilenameMaxLen);
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

static
int LIBNFSVIV_GetFilesize(FILE *file)
{
  int filesize;
  fseek(file, 0, SEEK_END);
  filesize = (int)ftell(file);
  rewind(file);
  return filesize;
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
#if !defined(_WIN32) && defined(__STDC__) && !defined(__STDC_VERSION__) && !defined(__cplusplus)  /* gcc/clang -std=c89 */
  DIR *dir = opendir(path);
  if (dir)
  {
    closedir(dir);
    return 1;
  }
  return 0;
#else
  struct stat sb;
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
    char buf[kLibnfsvivFilenameMaxLen];
    len = GetFullPathName(src, kLibnfsvivFilenameMaxLen, buf, lpFilePart);  /* returns length without nul */
    if (len == 0 || len >= kLibnfsvivFilenameMaxLen)
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
    len = GetFullPathName(src, nBufferLength, dst, lpFilePart);  /* returns length without nul */
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
    char buf[kLibnfsvivFilenameMaxLen];
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

/* Assumes (viv_directory) and both, viv_name and outpath are strings. */
static
void LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths(VivDirEntr *viv_directory, const int count_dir_entries, char *viv_name, const char *outpath, FILE *viv, const size_t viv_sz)
{
  char buf[kLibnfsvivFilenameMaxLen] = {0};

  /** Case: viv parentdir != outpath -> return */
  memcpy(buf, viv_name, LIBNFSVIV_min(strlen(viv_name), kLibnfsvivFilenameMaxLen - 1));
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
    for (i = 0; i < count_dir_entries; ++i)
    {
      fseek(viv, viv_directory[i].ofs_begin_filename, SEEK_SET);
      chunk_size = LIBNFSVIV_min(viv_sz - viv_directory[i].ofs_begin_filename, kLibnfsvivFilenameMaxLen);
      if (fread(buf, 1, chunk_size, viv) != (size_t)chunk_size)  { fprintf(stderr, "LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths: File read error (strcmp)\n"); break; }
      if (viv_directory[i].valid_entr_ == 1 && !strcmp(buf, viv_basename))
      {
        viv_directory[i].valid_entr_ = 0;
        printf("Warning:ValidateVivDirWritePaths: Skip file '%s' (%d) (would overwrite this archive)\n", buf, i);
      }
    }
  }
}

static
int LIBNFSVIV_RenameExistingFile(const char *oldName)
{
  int retv = 0;
  const char *extension = strrchr(oldName, '.');
  const char *nul = "\0";
  int oldbasename_len;
  int i;
  char *newFullName;

  if (strlen(oldName) + 32 > kLibnfsvivFilenameMaxLen)
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

static
void LIBNFSVIV_PrintVivDirEntr(const VivDirEntr *viv_dir, const int viv_dir_size)
{
  int i;
  printf("PrintVivDirEntr\n");
  printf("i     valid? offset      filesize  ofs_begin_filename\n");
  for (i = 0; i < viv_dir_size; ++i)
  {
    printf("%d     %d     %d (0x%x)   %d (0x%x)       %d (0x%x)\n",
           i, (viv_dir[i].valid_entr_),
           viv_dir[i].offset, viv_dir[i].offset,
           viv_dir[i].filesize, viv_dir[i].filesize,
           viv_dir[i].ofs_begin_filename, viv_dir[i].ofs_begin_filename);
  }
}

/* stats -------------------------------------------------------------------- */

/* Assumes that 'file' is valid VIV data. */
static
void LIBNFSVIV_PrintStatsDec(const VivDirEntr *viv_dir, const VivHeader *viv_hdr,
                             const int count_dir_entries, const int viv_filesize,
                             FILE *file,
                             const int request_file_idx, const char *request_file_name,
                             const int opt_direnlenfixed, const int opt_filenameshex)
{
  int gap;
  int i;
  int contents_size = 0;
  int hdr_size;
  int bufsize;
  unsigned char *buf;
  char filename[kLibnfsvivFilenameMaxLen];
  int filenamemaxlen = kLibnfsvivFilenameMaxLen;
  size_t sz;

  if (opt_direnlenfixed >= 10)
    filenamemaxlen = LIBNFSVIV_min(filenamemaxlen, opt_direnlenfixed - 0x08);

  if (count_dir_entries > 0)
  {
    bufsize = LIBNFSVIV_min(
      viv_filesize,
      viv_dir[count_dir_entries - 1].ofs_begin_filename + kLibnfsvivFilenameMaxLen);
  }
  else
  {
    bufsize = viv_filesize;
  }
  printf("Buffer Size = %d (0x%x)\n", bufsize, bufsize);

  if (bufsize > (1<<22))
  {
    printf("Header purports to be greater than 4MB\n");
    return;
  }
  else if (bufsize < 1)
  {
    printf("Empty file\n");
    return;
  }

  printf("Buffer = %d\n", kLibnfsvivBufferSize);
  printf("Archive Size (header) = %d (0x%x)\n", viv_hdr->filesize, viv_hdr->filesize);
  printf("Header Size (header) = %d (0x%x)\n", viv_hdr->header_size, viv_hdr->header_size);
  printf("Directory Entries (parsed) = %d\n", count_dir_entries);
  if (request_file_idx)
    printf("Requested file idx = %d\n", request_file_idx);
  if ((request_file_name) && (request_file_name[0] != '\0'))
    printf("Requested file = %.*s\n", kLibnfsvivFilenameMaxLen - 1, request_file_name);

  if (count_dir_entries > 0)
  {
    buf = (unsigned char *)malloc(bufsize * sizeof(*buf));
    if (!buf)
    {
      fprintf(stderr, "Cannot allocate memory\n");
      return;
    }

    for (i = 0; i < count_dir_entries; ++i)
    {
      if (viv_dir[i].valid_entr_ == 1)
        contents_size += viv_dir[i].filesize;
    }

    /* Parse entire header */
    rewind(file);
    if (fread(buf, 1, bufsize, file) != (size_t)bufsize)
    {
      fprintf(stderr, "File read error (print stats)\n");
      free(buf);
      return;
    }

    /* Actual length of filenames is unknown. Copy as much as possible, then get length. */

    /* Print header size */
    if (opt_direnlenfixed < 10)
      hdr_size = viv_dir[count_dir_entries - 1].ofs_begin_filename + viv_dir[count_dir_entries - 1].filename_len_;
    else
      hdr_size = 0x10 + count_dir_entries * opt_direnlenfixed;
    printf("Header Size (parsed) = %d (0x%x)\n", hdr_size, hdr_size);

    printf("\nPrinting archive directory:\n"
           "\n"
           "   id Valid       Offset          Gap         Size Len  Name\n"
           " ---- ----- ------------ ------------ ------------ ---  -----------------------\n");
    printf("                       0                %10d      header\n"
           " ---- ----- ------------ ------------ ------------ ---  -----------------------\n", hdr_size);

    /* 0th entry */
    LIBNFSVIV_UcharToNulChar(buf + viv_dir[0].ofs_begin_filename,
                             filename,
                             LIBNFSVIV_min(kLibnfsvivFilenameMaxLen, bufsize - viv_dir[0].ofs_begin_filename),
                             kLibnfsvivFilenameMaxLen);
    if (opt_filenameshex)
      LIBNFSVIV_EncBase16(filename, viv_dir[0].filename_len_);
    /* avoid printing non-UTF8 / non-printable string */
    sz = strlen(filename) + 1;
#ifdef UVTUTF8
    printf(" %4d     %d   %10d   %10d   %10d %3d  %s\n", 1, viv_dir[0].valid_entr_, viv_dir[0].offset, viv_dir[0].offset - hdr_size, viv_dir[0].filesize, viv_dir[0].filename_len_, (viv_dir[0].valid_entr_ || LIBNFSVIV_IsUTF8String(buf + viv_dir[0].ofs_begin_filename, sz) > 0) ? filename : "<non-UTF8>");
#else
    printf(" %4d     %d   %10d   %10d   %10d %3d  %s\n", 1, viv_dir[0].valid_entr_, viv_dir[0].offset, viv_dir[0].offset - hdr_size, viv_dir[0].filesize, viv_dir[0].filename_len_, (viv_dir[0].valid_entr_ || LIBNFSVIV_IsPrintString(buf + viv_dir[0].ofs_begin_filename, sz) > 0) ? filename : "<non-printable>");
#endif

    /* further entries */
    for (i = 1; i < count_dir_entries; ++i)
    {
      gap = viv_dir[i].offset - viv_dir[i - 1].offset - viv_dir[i - 1].filesize;

      LIBNFSVIV_UcharToNulChar(buf + viv_dir[i].ofs_begin_filename,
                               filename,
                               LIBNFSVIV_min(kLibnfsvivFilenameMaxLen, bufsize - viv_dir[i].ofs_begin_filename),
                               kLibnfsvivFilenameMaxLen);
      if (opt_filenameshex)
        LIBNFSVIV_EncBase16(filename, viv_dir[i].filename_len_);
      /* avoid printing non-UTF8 / non-printable string */
      sz = strlen(filename) + 1;
#ifdef UVTUTF8
      printf(" %4d     %d   %10d   %10d   %10d %3d  %s\n", i + 1, viv_dir[i].valid_entr_, viv_dir[i].offset, gap, viv_dir[i].filesize, viv_dir[i].filename_len_, (viv_dir[i].valid_entr_ || LIBNFSVIV_IsUTF8String(buf + viv_dir[i].ofs_begin_filename, sz) > 0) ? filename : "<non-UTF8>");
#else
      printf(" %4d     %d   %10d   %10d   %10d %3d  %s\n", i + 1, viv_dir[i].valid_entr_, viv_dir[i].offset, gap, viv_dir[i].filesize, viv_dir[i].filename_len_, (viv_dir[i].valid_entr_ || LIBNFSVIV_IsPrintString(buf + viv_dir[i].ofs_begin_filename, sz) > 0) ? filename : "<non-printable>");
#endif
    }

    printf(" ---- ----- ------------ ------------ ------------ ---  -----------------------\n"
           "              %10d                %10d      %d files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, contents_size, count_dir_entries);

    free(buf);
  }  /* if */
}

static
void LIBNFSVIV_PrintStatsEnc(VivDirEntr *viv_dir, const VivHeader viv_hdr,
                             char **infiles_paths, const int count_infiles,
                             int **infile_exists, const int count_dir_entries)
{
  int i;
  int j;
#ifdef _WIN32
  char buf[kLibnfsvivFilenameMaxLen];
#endif

  printf("Buffer = %d\n", kLibnfsvivBufferSize);
  printf("Header Size = %d (0x%x)\n", viv_hdr.header_size, viv_hdr.header_size);
  printf("Directory Entries = %d\n", viv_hdr.count_dir_entries);
  printf("Archive Size = %d (0x%x)\n", viv_hdr.filesize, viv_hdr.filesize);
  printf("File format = %.4s\n", viv_hdr.format);

  if (count_dir_entries > 0)
  {
    printf("\n"
           "   id       Offset         Size Len  Name\n"
           " ---- ------------ ------------ ---  -----------------------\n");

    for (i = 0, j = 0; i < count_infiles; ++i)
    {
      if ((*infile_exists)[i] < 1)
        continue;
#ifdef _WIN32
      if (GetLongPathName(infiles_paths[i], buf, kLibnfsvivFilenameMaxLen))  /* transform short paths that contain tilde (~) */
        printf(" %4d   %10d   %10d %3d  %s\n", j + 1, viv_dir[j].offset, viv_dir[j].filesize, viv_dir[j].filename_len_ + 1, LIBNFSVIV_GetPathBasename(buf));
#else
      printf(" %4d   %10d   %10d %3d  %s\n", j + 1, viv_dir[j].offset, viv_dir[j].filesize, viv_dir[j].filename_len_ + 1, LIBNFSVIV_GetPathBasename(infiles_paths[i]));
#endif
      ++j;
    }
    printf(" ---- ------------ ------------ ---  -----------------------\n"
           "        %10d   %10d      %d files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, viv_hdr.filesize - viv_hdr.header_size, count_dir_entries);
  }
}

/* internal: validate ------------------------------------------------------- */

static
int LIBNFSVIV_GetVivFileMinOffset(const VivDirEntr *viv_dir, const int start, const int end,
                                  const int filesize)
{
  int i = start;
  int min_ = filesize;
  while (i < 0)
    ++i;
  for ( ; i < end; ++i)
    min_ = LIBNFSVIV_min(min_, viv_dir[i].offset);
  return min_;
}

static
int LIBNFSVIV_CheckVivHdr(const VivHeader viv_hdr, const int viv_filesize)
{
  int retv = 1;

  if (strncmp(viv_hdr.format, "BIGF", 4) &&
      strncmp(viv_hdr.format, "BIGH", 4) &&
      strncmp(viv_hdr.format, "BIG4", 4))
  {
    fprintf(stderr, "CheckVivHeader: Format error (expects BIGF, BIGH, BIG4)\n");
    retv = 0;
  }

  if (viv_hdr.count_dir_entries < 0)
  {
    fprintf(stderr, "CheckVivHeader: Format error (number of directory entries < 0) %d\n", viv_hdr.count_dir_entries);
    retv = 0;
  }

  if (viv_hdr.count_dir_entries > kLibnfsvivDirEntrMax)
  {
    fprintf(stderr, "CheckVivHeader: Number of purported directory entries not supported and likely invalid (%d > %d)\n", viv_hdr.count_dir_entries, kLibnfsvivDirEntrMax);
    retv = 0;
  }

  if (viv_hdr.header_size > viv_filesize)
    fprintf(stderr, "Warning:CheckVivHeader: Format (headersize > filesize)\n");

  if (viv_hdr.header_size > viv_hdr.count_dir_entries * (8 + kLibnfsvivFilenameMaxLen) + 16)
    fprintf(stderr, "Warning:CheckVivHeader: Format (invalid headersize) (%d) %d\n", viv_hdr.header_size, viv_hdr.count_dir_entries);

  return retv;
}

static
int LIBNFSVIV_CheckVivDir(const VivHeader *viv_header,  VivDirEntr *viv_dir,
                          const int hdr_size, const int viv_filesize,
                          const int count_dir_entries)
{
  int retv = 1;
  int contents_size = 0;
  int ofs_now;
  int i;

  if (viv_header->count_dir_entries != count_dir_entries)
  {
    printf("Warning:CheckVivDir: incorrect number of archive directory entries in header (%d files listed, %d files found)\n", viv_header->count_dir_entries, count_dir_entries);
    /* printf("Warning:CheckVivDir: try option '-we' for suspected non-printable filenames\n"); */
  }

  /* :HS, :PU allow values greater than true value */
  if ((viv_header->count_dir_entries < 1) || (count_dir_entries < 1))
  {
    printf("Warning:CheckVivDir: empty archive (%d files listed, %d files found)\n", viv_header->count_dir_entries, count_dir_entries);
    return 1;
  }

  if (viv_dir[0].offset != LIBNFSVIV_GetVivFileMinOffset(viv_dir, 0, count_dir_entries, viv_filesize))
  {
    printf("Warning:CheckVivDir: smallest offset (%d) is not file 0\n", LIBNFSVIV_GetVivFileMinOffset(viv_dir, 0, count_dir_entries, viv_filesize));
  }

  /* Validate file offsets, sum filesizes */
  for (i = 0; i < count_dir_entries; ++i)
  {
    ofs_now = viv_dir[i].offset;

    if ((viv_dir[i].filesize >= viv_filesize) ||
        (viv_dir[i].filesize < 0))
    {
      printf("CheckVivDir: file %d invalid (filesize out of bounds) %d\n", i, viv_dir[i].filesize);
      viv_dir[i].valid_entr_ = 0;
    }
    if ((ofs_now < hdr_size) ||
        (ofs_now < viv_header->header_size) ||
        (ofs_now >= viv_filesize))
    {
      printf("CheckVivDir: file %d invalid (offset out of bounds) %d\n", i, ofs_now);
      viv_dir[i].valid_entr_ = 0;
    }
    if (ofs_now >= INT_MAX - viv_dir[i].filesize)
    {
      printf("CheckVivDir: file %d invalid (offset overflow) %d\n", i, ofs_now);
      viv_dir[i].valid_entr_ = 0;
    }
    if ((ofs_now + viv_dir[i].filesize > viv_filesize))
    {
      printf("CheckVivDir: file %d invalid (filesize from offset out of bounds) %d\n", i, ofs_now);
      viv_dir[i].valid_entr_ = 0;
    }

    if (viv_dir[i].valid_entr_ == 1)
      contents_size += viv_dir[i].filesize;
  }  /* for i */

  /** TODO: re-add file overlap test for valid entries? */

  /* Normally, should be equal. Smaller is allowed, as archives may have null-byte padding "gaps" between files.
     example: official DLC walm/car.viv  */
  if (viv_dir[0].offset + contents_size > viv_filesize)
  {
    fprintf(stderr, "CheckVivDir: Format error (archive directory filesizes sum too large)\n");
    retv = 0;
  }

  /* :HS, :PU allow value greater than true value */
  if (viv_header->count_dir_entries != count_dir_entries)
    fprintf(stderr, "Warning:CheckVivDir (archive header has incorrect number of directory entries)\n");

  return retv;
}

/* decode ------------------------------------------------------------------- */

/* Clamp number of viv directory entries to be parsed to 0,max */
static
void LIBNFSVIV_FixVivHdr(VivHeader *viv_hdr)
{
  if (viv_hdr->count_dir_entries < 0)
  {
    fprintf(stderr, "Warning:FixVivHdr: Format (invalid number of purported directory entries) (%d),\n", viv_hdr->count_dir_entries);
    fprintf(stderr, "32 bit (%d) bitmask,\n", viv_hdr->count_dir_entries & 0x7FFFFFFF);
    fprintf(stderr, "28 bit (%d),\n", viv_hdr->count_dir_entries & 0x0FFFFFFF);
    fprintf(stderr, "24 bit (%d),\n", viv_hdr->count_dir_entries & 0x00FFFFFF);
    fprintf(stderr, "20 bit (%d),\n", viv_hdr->count_dir_entries & 0x000FFFFF);
    fprintf(stderr, "16 bit (%d),\n", viv_hdr->count_dir_entries & 0x0000FFFF);
    viv_hdr->count_dir_entries = LIBNFSVIV_min(viv_hdr->count_dir_entries & 0x7FFFFFFF, kLibnfsvivDirEntrMax);
    fprintf(stderr, "assume %d entries\n", viv_hdr->count_dir_entries);
  }
  else if (viv_hdr->count_dir_entries > kLibnfsvivDirEntrMax)
  {
    fprintf(stderr, "Warning:FixVivHdr: Format (unsupported number of purported directory entries) (%d),\n", viv_hdr->count_dir_entries);
    fprintf(stderr, "32 bit (%x),\n", viv_hdr->count_dir_entries);
    viv_hdr->count_dir_entries = kLibnfsvivDirEntrMax;
    fprintf(stderr, "assume %d entries\n", viv_hdr->count_dir_entries);
  }
}

/* Assumes ftell(file) == 0
   Returns 1, if Viv header can be read. Else 0. */
static
int LIBNFSVIV_GetVivHdr(VivHeader *viv_hdr, FILE *file)
{
  int sz = 0;

  sz += fread(viv_hdr->format, 1, 4, file);
  sz += fread(&viv_hdr->filesize, 1, 4, file);
  sz += fread(&viv_hdr->count_dir_entries, 1, 4, file);
  sz += fread(&viv_hdr->header_size, 1, 4, file);

  if (sz != 16)
  {
    fprintf(stderr, "GetVivHeader: File read error\n");
    return 0;
  }

  if (strncmp(viv_hdr->format, "BIG4", 4))  /* BIG4 encodes filesize in little endian */
    viv_hdr->filesize = LIBNFSVIV_SwapEndian(viv_hdr->filesize);
  viv_hdr->count_dir_entries = LIBNFSVIV_SwapEndian(viv_hdr->count_dir_entries);
  viv_hdr->header_size = LIBNFSVIV_SwapEndian(viv_hdr->header_size);

  return 1;
}

/* Assumes (viv_dir). Assumes (*count_dir_entries >= true value). Returns boolean. */
static
int LIBNFSVIV_GetVivDir(VivDirEntr *viv_dir, int *count_dir_entries,
                        int *viv_hdr_size_true,
                        const int viv_filesize, FILE *file,
                        const int opt_verbose, const int opt_direnlenfixed,
                        const int opt_filenameshex)
{
  unsigned char buf[kLibnfsvivBufferSize] = {0};
  int len;
  int curr_offset_buffer;
  int curr_chunk_size;
  int i = 0;
  unsigned char *ptr;
  unsigned char *ptr_tmp;
#ifdef UVTUTF8
  char tmp_UTF8;
#endif

  *viv_hdr_size_true = 0x10;  /* at return, will be true unpadded header size */

  viv_dir[0].offset = viv_filesize;

#if 0
  while ((*viv_hdr_size_true < viv_dir[0].offset) && (i < *count_dir_entries))
#elif 0
  while ((*viv_hdr_size_true < LIBNFSVIV_GetVivFileMinOffset(viv_dir, i - 1, i, viv_filesize)) && (i < *count_dir_entries))
#else
  while (i < *count_dir_entries)
#endif
  {
    /* Read next chunk */
    curr_chunk_size = LIBNFSVIV_min(kLibnfsvivBufferSize, viv_filesize - *viv_hdr_size_true);
    fseek(file, *viv_hdr_size_true, SEEK_SET);

    if (fread(buf, 1, curr_chunk_size, file) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "GetVivDir: File read error at %d\n", *viv_hdr_size_true);
      return 0;
    }

    curr_offset_buffer = 0;

    /* Get entries safely within chunk
       (max. DirEntry size = 8 + kLibnfsvivFilenameMaxLen) */
    for ( ; i < *count_dir_entries; ++i)
    {
      viv_dir[i].valid_entr_ = 1;
      viv_dir[i].filename_len_ = 0;
#if UVTVERBOSE
printf("read entry i%d at curr_offset_buffer 0x%x at 0x%x\n",
       i,
       curr_offset_buffer,
       *viv_hdr_size_true);
#endif

      memcpy(&viv_dir[i].offset, buf + curr_offset_buffer + 0x0, 4);
      memcpy(&viv_dir[i].filesize, buf + curr_offset_buffer + 0x4, 4);

      viv_dir[i].offset   = LIBNFSVIV_SwapEndian(viv_dir[i].offset);
      viv_dir[i].filesize = LIBNFSVIV_SwapEndian(viv_dir[i].filesize);

#if UVTVERBOSE
printf("i%d 0x%x offset0x%x\n",
       i,
       LIBNFSVIV_GetVivFileMinOffset(viv_dir, 0, i, viv_filesize),
       viv_dir[i].offset);
#endif

      curr_offset_buffer += 0x08;
      *viv_hdr_size_true   += 0x08;

      viv_dir[i].ofs_begin_filename = *viv_hdr_size_true;

      /* Expect a string here (filename). If no string can be found, offset
         is definitely past the directory. Then the previous entry ended the
         directory, and while is ended.

         UVTUTF8-branch:
           check for UTF8-string
         Default mode:
           check for printable string
         opt_filenameshex mode:
           Output filenames will be Base16 (hexadecimal) encoded: check for string
      */
      ptr = buf + curr_offset_buffer - 1;

      if (!opt_filenameshex)
      {
        ptr_tmp = ptr + 1;
#ifdef UVTUTF8
        /* End if filename is not UTF8-string */
        len = LIBNFSVIV_IsUTF8String(ptr_tmp, kLibnfsvivBufferSize - curr_offset_buffer) + 1;
        viv_dir[i].filename_len_ = len;
        memcpy(&tmp_UTF8, ptr + 1, 1);
        if (!isprint(tmp_UTF8) && (len < 2))
#else
        /* End if filename is not printable string
           very crude check as, e.g., CJK characters end the loop */
        len = LIBNFSVIV_IsPrintString(ptr_tmp, kLibnfsvivBufferSize - curr_offset_buffer) + 1;
        viv_dir[i].filename_len_ = len;
        if (len < 2)
#endif
        {
          viv_dir[i].valid_entr_ = 0;
          *viv_hdr_size_true -= 0x08;
          *count_dir_entries = i;  /* breaks while loop */
          break;
        }
        ptr += len;

#if UVTVERBOSE
printf("i%d len=%d  %d\n",
       i,
       len, UVTVERBOSE);
#endif
      }
      else  /* With option filenames as hex, just check if string */
      {
        ptr_tmp = ptr + 1;

        /* Get length to nul or end of buf */
        len = 1;
        while ((len < kLibnfsvivFilenameMaxLen) &&
               (curr_offset_buffer + len < kLibnfsvivBufferSize) &&
               *(++ptr) &&
               (curr_offset_buffer + len < viv_filesize))
        {
          ++len;
        }
        viv_dir[i].filename_len_ = len - 1;

#if 0
        /* Get length to nul or end of buf */
        len = 1;
        while ((len < kLibnfsvivFilenameMaxLen) &&
               (curr_offset_buffer + len < kLibnfsvivBufferSize) &&
               *(++ptr) &&
               (curr_offset_buffer + len < viv_filesize))
        {
          ++len;
        }
#endif
#if UVTVERBOSE || 0
printf("i%d len=%d\n",
       i,
       len);
#endif

        /* Filenames as hex: Accept string of length 1 in option, later represent as 00. */
        if (opt_direnlenfixed >= 10)
        {
          /* Find last non-nul byte for string length,
              accepting embedded/leading null-bytes */
          unsigned char buf_[kLibnfsvivFilenameMaxLen] = {0};
          unsigned char *uptr_;
          int size_ = LIBNFSVIV_min(opt_direnlenfixed - 0x8, kLibnfsvivFilenameMaxLen) - 1;
          memcpy(buf_, ptr_tmp + 0x0, size_);
          uptr_ = buf_ + size_ - 1;
          while (!*uptr_--)
            --size_;
          len = size_;
          viv_dir[i].filename_len_ = size_;
#if UVTVERBOSE > 0
          {
            int k_;
            printf("i%d len=%d (capture leading/embedded null's)\n", i, size_);
            fflush(0);
            uptr_ = buf_;
            for (k_ = 0; k_ < size_; ++k_)
            {
              printf("%02X ", ((char)buf_[k_]) & 0xff);
            }
            printf("\\ %02X ", ((char)buf_[k_ + 1]) & 0xff);
            printf("\n");
          }
#endif
        }  /* if (opt_direnlenfixed >= 10) */
#if 0
        else if (len < 2 || *ptr)
        {
          /* Filenames as hex: Accept string of length 1 in option, later represent as 00. */

          /* Default: If len < 2, not a string, invalid entry */
          if (!opt_filenameshex)
          {
            viv_dir[i].valid_entr_ = 0;
          }
        }
#endif

#if 0
        /* Expect a string here (filename). If len < 2, not a string. */
        if (len < 2 || *ptr)
        {
          viv_dir[i].valid_entr_ = 0;
          *viv_hdr_size_true -= 0x08;
          *count_dir_entries = i;  /* breaks while loop */
          break;
        }
#endif
      }  /* if (!opt_filenameshex) */

      if (len >= kLibnfsvivFilenameMaxLen)
      {
        if (opt_verbose >= 1)
          fprintf(stderr, "Warning:GetVivDir: Filename length at %d not supported (must be < %d). Ignore entry. Stop parsing directory.\n", *viv_hdr_size_true, kLibnfsvivFilenameMaxLen);
        *viv_hdr_size_true -= 0x08;
        *count_dir_entries = i;  /* breaks while loop and ignores the most recent entry */
        break;
      }

#if 1  /* can this happen? */
      if (curr_offset_buffer + len >= kLibnfsvivBufferSize)
      {
        fprintf(stderr, "GetVivDir: Filename out of bounds at %d (must be < %d)\n", *viv_hdr_size_true, kLibnfsvivBufferSize - curr_offset_buffer);
        return 0;
      }
#endif

#if 1  /* can this happen? */
      /* Not a string and EOF reached? Not a directory entry. Quietly stop looking for entries. */
      if (curr_offset_buffer + len > viv_filesize)
      {
        if (opt_verbose >= 1)
          fprintf(stderr, "Warning:GetVivDir: Filename at %d not a string, reaches EOF. Not a directory entry. Stop parsing directory.\n", *viv_hdr_size_true);
        viv_dir[i].valid_entr_ = 0;
        *viv_hdr_size_true -= 0x08;
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }
#endif

      if (opt_direnlenfixed < 0x0A)
      {
        curr_offset_buffer += len;
        *viv_hdr_size_true += len;
      }
      else  /* using fixed directory entry length */
      {
        curr_offset_buffer += opt_direnlenfixed - 0x08;
        *viv_hdr_size_true += opt_direnlenfixed - 0x08;
      }

      /* Does next directory entry fit into current chunk? (including nul) */
      if (!(i < *count_dir_entries) ||
          (8 + kLibnfsvivFilenameMaxLen > ftell(file) - *viv_hdr_size_true) ||
          (opt_direnlenfixed >= 0x0A && opt_direnlenfixed >= ftell(file) - *viv_hdr_size_true))
      {
        ++i;
        break;
      }
    }  /* for i */
  }  /* while */

  *count_dir_entries = i;

  printf("Directory Entries (parsed) = %d\n", *count_dir_entries);

#if UVTVERBOSE > 0
  LIBNFSVIV_PrintVivDirEntr(viv_dir, *count_dir_entries);
#endif

  return 1;
}

/* Accepts a directory entry, extracts the described file. Returns boolean. */
static
int LIBNFSVIV_VivExtractFile(const VivDirEntr viv_dir, const int viv_filesize,
                             FILE *infile,
                             const int opt_filenameshex, const int opt_overwrite,
                             FILE *wenc_file, const char *wenc_outpath)
{
  unsigned char buf[kLibnfsvivBufferSize] = {0};
  size_t curr_chunk_size;
  int curr_offset;
  FILE *outfile = NULL;

  /* Read outfilename from file into buf */
  curr_offset = viv_dir.ofs_begin_filename;
  curr_chunk_size = LIBNFSVIV_min(kLibnfsvivBufferSize, viv_filesize - curr_offset);
  fseek(infile, curr_offset, SEEK_SET);
  if (fread(buf, 1, curr_chunk_size, infile) != curr_chunk_size)
  {
    fprintf(stderr, "VivExtractFile: File read error at %d (extract outfilename)\n", curr_offset);
    return 0;
  }

  if (opt_filenameshex)  /* Option: Encode outfilename to Base16 */
    LIBNFSVIV_EncBase16((char *)buf, viv_dir.filename_len_);

  /* Create outfile */
  if (LIBNFSVIV_IsFile((const char *)buf))  /* overwrite mode: for existing files and duplicated filenames in archive */
  {
    if (opt_overwrite == 1)  /* attempt renaming existing file, return on failure */
    {
      if (!LIBNFSVIV_RenameExistingFile((const char *)buf))
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

  memset(buf, 0, kLibnfsvivBufferSize);
  curr_offset = viv_dir.offset;
  fseek(infile, curr_offset, SEEK_SET);

  while (curr_offset < viv_dir.offset + viv_dir.filesize)
  {
    curr_chunk_size = LIBNFSVIV_min(kLibnfsvivBufferSize, viv_dir.offset + viv_dir.filesize - curr_offset);

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
int LIBNFSVIV_GetIdxFromFname(const VivDirEntr *viv_dir,
                              FILE* infile, const int infilesize,
                              const int count_dir_entries,
                              const char *request_file_name)
{
  int retv = -1;
  int i;
  int chunk_size;
  char buf[kLibnfsvivFilenameMaxLen];

  if (strlen(request_file_name) + 1 > kLibnfsvivFilenameMaxLen)
  {
    fprintf(stderr, "GetIdxFromFname: Requested filename is too long\n");
    return 0;
  }

  for (i = 0; i < count_dir_entries; ++i)
  {
    fseek(infile, viv_dir[i].ofs_begin_filename, SEEK_SET);
    chunk_size = LIBNFSVIV_min(infilesize - viv_dir[i].ofs_begin_filename, kLibnfsvivFilenameMaxLen);

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
int LIBNFSVIV_SetVivDirHeader(VivHeader *viv_hdr, VivDirEntr *viv_dir,
                              char **infiles_paths, const int count_infiles,
                              int **infile_exists, const int count_infiles_exist,
                              const char *opt_requestfmt,
                              const int opt_direnlenfixed,
                              const int opt_filenameshex)
{
  int retv = 1;
  FILE *file;
  int curr_offset;
  int i;
  int j;
  int len;  /* basename string length including nul */
  int filesize;
#ifdef _WIN32
  char buf[kLibnfsvivFilenameMaxLen];
#endif

  viv_hdr->filesize = 0;
  curr_offset = 16;

  for (i = 0, j = 0; i < count_infiles; ++i)
  {
    if ((*infile_exists)[i] < 1)
      continue;
#ifdef _WIN32
    len = (int)GetLongPathName(infiles_paths[i], buf, kLibnfsvivFilenameMaxLen) + 1;  /* transform short paths that contain tilde (~) */
    if (len < 2 || len > kLibnfsvivFilenameMaxLen)
    {
      fprintf(stderr, "SetVivDirHeader: Cannot get long path name for file '%s' (len=%d)\n", infiles_paths[i], (int)strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1);
      retv = 0;
      break;
    }
    len = strlen(LIBNFSVIV_GetPathBasename(buf)) + 1;
#else
    len = strlen(LIBNFSVIV_GetPathBasename(infiles_paths[i])) + 1;
#endif
    if (opt_filenameshex)
      len = len / 2 + len % 2;

    filesize = -1;
    file = fopen(infiles_paths[i], "rb");
    if (!file)
    {
      fprintf(stderr, "SetVivDirHeader: Cannot open file '%s'\n", infiles_paths[i]);
      retv = 0;
      viv_dir[j].valid_entr_ = 0;
    }
    else
    {
      filesize = LIBNFSVIV_GetFilesize(file);
      fclose(file);
      viv_dir[j].valid_entr_ = 1;
    }
    viv_dir[j].filename_len_ = len - 1;  /* string length excluding nul */

    viv_dir[j].filesize = filesize;
    viv_hdr->filesize += filesize;
    curr_offset += 0x8;
    viv_dir[j].ofs_begin_filename = curr_offset;
    curr_offset += len;
    if (opt_direnlenfixed > 10 && len <= opt_direnlenfixed)
      curr_offset += opt_direnlenfixed - len - 0x8;

    ++j;
  }

  viv_dir[0].offset = curr_offset;
  for (j = 1; j < count_infiles_exist; ++j)
    viv_dir[j].offset = viv_dir[j - 1].offset + viv_dir[j - 1].filesize;

  memcpy(viv_hdr->format, opt_requestfmt, 4);
  viv_hdr->filesize += curr_offset;
  viv_hdr->count_dir_entries = count_infiles_exist;
  viv_hdr->header_size = curr_offset;

  return retv;
}

static
int LIBNFSVIV_WriteVivHeader(VivHeader viv_hdr, FILE *file)
{
  int retv = 1;
  size_t err = 0;

  if (strncmp(viv_hdr.format, "BIG4", 4))  /* BIG4 encodes filesize in little endian */
    viv_hdr.filesize = LIBNFSVIV_SwapEndian(viv_hdr.filesize);
  viv_hdr.count_dir_entries = LIBNFSVIV_SwapEndian(viv_hdr.count_dir_entries);
  viv_hdr.header_size = LIBNFSVIV_SwapEndian(viv_hdr.header_size);

  err += fwrite(viv_hdr.format, 1, 4, file);
  err += fwrite(&viv_hdr.filesize, 1, 4, file);
  err += fwrite(&viv_hdr.count_dir_entries, 1, 4, file);
  err += fwrite(&viv_hdr.header_size, 1, 4, file);

  if (err != 16)
  {
    fprintf(stderr, "Warning:WriteVivHeader: File write error\n");
    retv = 0;
  }

  return retv;
}

static
int LIBNFSVIV_WriteVivDirectory(VivDirEntr *viv_directory,
                                char **infiles_paths, const int count_infiles,
                                int **infile_exists, const int count_infiles_exist,
                                FILE *file, const int opt_direnlenfixed,
                                const int opt_filenameshex)
{
  int val;
  char buf[kLibnfsvivFilenameMaxLen] = {0};
  size_t size;
  int i;
  int j;
  size_t err = 0;

  for (i = 0, j = 0; i < count_infiles; ++i)
  {
    if ((*infile_exists)[i] < 1)
      continue;

    val = LIBNFSVIV_SwapEndian(viv_directory[j].offset);
    err += fwrite(&val, 1, 4, file);

    val = LIBNFSVIV_SwapEndian(viv_directory[j].filesize);
    err += fwrite(&val, 1, 4, file);

#ifdef _WIN32
    size = (int)GetLongPathName(infiles_paths[i], buf, kLibnfsvivFilenameMaxLen) + 1;  /* transform short paths that contain tilde (~) */
    if (size < 2 || size > kLibnfsvivFilenameMaxLen)
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
    if (size < 2 || size > kLibnfsvivFilenameMaxLen - 1)
    {
      fprintf(stderr, "WriteVivDirectory: infile basename length incompatible (%d)\n", (int)size);
      return 0;
    }
    memcpy(buf, LIBNFSVIV_GetPathBasename(infiles_paths[i]), size);  /* includes nul-terminator */
#endif

    if (opt_filenameshex)
    {
      size = LIBNFSVIV_DecBase16(buf) + 1;
      if (size != (size_t)viv_directory[j].filename_len_ + 1)
        fprintf(stderr, "Warning:WriteVivDirectory: viv_dir mishap (%d != %d)\n", (int)size, viv_directory[j].filename_len_ + 1);
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

    ++j;
  }  /* for i, j */

  if (err != (size_t)count_infiles_exist * 8)
  {
    fprintf(stderr, "WriteVivDirectory: File write error\n");
    return 0;
  }

  return 1;
}

/* Chunked write from file at infile_path to outfile. */
static
int LIBNFSVIV_VivWriteFile(FILE *outfile, const char *infile_path,
                           const int infile_size)
{
  int retv = 1;
  unsigned char buf[kLibnfsvivBufferSize];
  int curr_ofs;
  int curr_chunk_size = kLibnfsvivBufferSize;
  FILE *infile = fopen(infile_path, "rb");
  if (!infile)
  {
    fprintf(stderr, "VivWriteFile: Cannot open file '%s' (infile)\n", infile_path);
    return 0;
  }

  while (curr_chunk_size > 0)
  {
    curr_ofs = ftell(infile);
    curr_chunk_size = LIBNFSVIV_min(kLibnfsvivBufferSize, infile_size - curr_ofs);

    if (fread(buf, 1, curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivWriteFile: File read error at %d in '%s' (infile)\n", curr_ofs, infile_path);
      retv = 0;
      break;
    }

    if (fwrite(buf, 1, curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivWriteFile: File write error at %d (outfile)\n", curr_chunk_size);
      retv = 0;
      break;
    }
  }

  fclose(infile);
  return retv;
}

/* api ---------------------------------------------------------------------- */

/* Assumes (dst) && (ending). Returns 1 if dst + ending fits in dst, 0 otherwise. */
int LIBNFSVIV_AppendFileEnding(char *dst, const size_t dst_sz, const char *ending)
{
  const size_t ofs = strlen(dst);
  const size_t ending_sz = strlen(ending);
  if (ofs + ending_sz < dst_sz)
  {
    memcpy(dst + ofs, ending, ending_sz);
    return 1;
  }
  return 0;
}

/*
  Assumes viv_name and outpath are NOT const's and have size >= kLibnfsvivFilenameMaxLen
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
  int viv_hdr_size;
  VivHeader viv_hdr;
  VivDirEntr *viv_directory = NULL;
  int count_dir_entries;
  int i = 0;
  int count_extracted = 0;
  char *wenc_buf = NULL;
  FILE *wenc_f = NULL;

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
      wenc_buf = (char *)calloc(kLibnfsvivFilenameMaxLen * sizeof(*wenc_buf), 1);
      if (!wenc_buf)
      {
        fprintf(stderr, "Unviv: Memory allocation failed.\n");
      }
      else
      {
        memcpy(wenc_buf, viv_name, LIBNFSVIV_min(strlen(viv_name), kLibnfsvivFilenameMaxLen - 1));
        if (!LIBNFSVIV_AppendFileEnding(wenc_buf, kLibnfsvivFilenameMaxLen, ".txt"))
        {
          fprintf(stderr, "Unviv: Cannot append extension '.txt' to '%s'\n", viv_name);
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

    printf("\nExtracting archive: %s\n", viv_name);
    printf("Extracting to: %s\n", outpath);

    file = fopen(viv_name, "rb");
    if (!file)
    {
      fprintf(stderr, "Unviv: Cannot open '%s'\n", viv_name);
      break;
    }

    viv_filesize = LIBNFSVIV_GetFilesize(file);
    printf("Archive Size (parsed) = %d (0x%x)\n", viv_filesize, viv_filesize);
    if (viv_filesize > INT_MAX || viv_filesize < 16)
    {
      fprintf(stderr, "Unviv: Format error (invalid filesize) %d\n", viv_filesize);
      break;
    }

    /* Get header and validate */
    if (!LIBNFSVIV_GetVivHdr(&viv_hdr, file))
      break;
    printf("Archive Size (header) = %d (0x%x)\n", viv_hdr.filesize, viv_hdr.filesize);
    printf("Directory Entries (header) = %d (0x%x)\n", viv_hdr.count_dir_entries, viv_hdr.count_dir_entries);
    printf("Header Size (header) = %d (0x%x)\n", viv_hdr.header_size, viv_hdr.header_size);
    printf("File format (parsed) = %.4s\n", viv_hdr.format);
    LIBNFSVIV_FixVivHdr(&viv_hdr);
    if (!LIBNFSVIV_CheckVivHdr(viv_hdr, viv_filesize))
      break;
    count_dir_entries = viv_hdr.count_dir_entries;  /* is non-negative here */

    viv_directory = (VivDirEntr *)malloc((count_dir_entries + 1) * sizeof(*viv_directory));
    if (!viv_directory)
    {
      fprintf(stderr, "Unviv: Cannot allocate memory\n");
      break;
    }

    if (!LIBNFSVIV_GetVivDir(viv_directory, &count_dir_entries, &viv_hdr_size, viv_filesize, file, opt_verbose, opt_direnlenfixed, opt_filenameshex))
    {
      break;
    }

    if (!LIBNFSVIV_CheckVivDir(&viv_hdr, viv_directory, viv_hdr_size, viv_filesize, count_dir_entries))
    {
      LIBNFSVIV_PrintVivDirEntr(viv_directory, count_dir_entries);
      break;
    }

    LIBNFSVIV_EnsureVivPathNotInVivDirWritePaths(viv_directory, count_dir_entries, viv_name, outpath, file, viv_filesize);  /* invalidate files that would overwrite archive */

    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_GetIdxFromFname(viv_directory, file, viv_filesize, count_dir_entries, request_file_name);
      if (request_file_idx <= 0)
      {
        break;
      }
    }

    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsDec(viv_directory, &viv_hdr, count_dir_entries, viv_filesize, file, request_file_idx, request_file_name, opt_direnlenfixed, opt_filenameshex);
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
        if (strncmp(viv_hdr.format, "BIGF", 4))
          fprintf(wenc_f, "%s %.4s ", "-fmt", viv_hdr.format);
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
      if ((request_file_idx < 0) || (request_file_idx > count_dir_entries))
      {
        fprintf(stderr, "Unviv: Requested idx (%d) out of bounds\n", request_file_idx);
        break;
      }

      if (!LIBNFSVIV_VivExtractFile(viv_directory[request_file_idx - 1], viv_filesize, file, opt_filenameshex, opt_overwrite, wenc_f, outpath))
      {
        break;
      }
      ++count_extracted;
    }
    else
    {
      for (i = 0; i < count_dir_entries; ++i)
      {
        if (viv_directory[i].valid_entr_ != 1)
          continue;

        /* Continue extracting through failures */
        if (LIBNFSVIV_VivExtractFile(viv_directory[i], viv_filesize, file, opt_filenameshex, opt_overwrite, wenc_f, outpath))
        {
          ++count_extracted;
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
  if (viv_directory)
    free(viv_directory);

  return retv;
}

/*
  Assumes viv_name is NOT const and has size >= kLibnfsvivFilenameMaxLen
  Assumes (viv_name). Overwrites file 'viv_name'. Skips unopenable infiles.
  Assumes (opt_requestfmt).
*/
int LIBNFSVIV_Viv(const char *viv_name,
                  char **infiles_paths, const int count_infiles,
                  const int opt_dryrun, const int opt_verbose,
                  const int opt_direnlenfixed, const int opt_filenameshex,
                  const char *opt_requestfmt)
{
  int retv = 1;
  FILE *file = NULL;
  int viv_filesize;
  VivHeader viv_header;
  VivDirEntr *viv_directory = NULL;
  int i;
  int j;
  int hdr_size;
  int *infile_exists = NULL;  /* indicator array */
  int count_infiles_exist = 0;

  if (opt_dryrun)
    printf("Begin dry run\n");

  printf("\nCreating archive: %s\n", viv_name);
  printf("Number of files to encode = %d\n", count_infiles);

  if (count_infiles > kLibnfsvivDirEntrMax)
  {
    fprintf(stderr, "Viv: Number of files to encode too large (%d > %d)\n", count_infiles, kLibnfsvivDirEntrMax);
    return 0;
  }
  else if (count_infiles < 1)
  {
    return 1;
  }

  for (;;)
  {
    infile_exists = (int *)calloc((count_infiles + 1) * sizeof(*infile_exists), 1);
    if (!infile_exists)
    {
      fprintf(stderr, "Viv: Cannot allocate memory\n");
      return 0;
    }
    for (i = 0; i < count_infiles; ++i)
    {
      if (LIBNFSVIV_IsFile(infiles_paths[i]) && !LIBNFSVIV_IsDir(infiles_paths[i]))
      {
        infile_exists[i] = 1;
        ++count_infiles_exist;
      }
      else
        printf("Cannot open file. Skipping '%s'\n", infiles_paths[i]);
    }
#if UVTVERBOSE >= 1
    printf("viv:debug: infile_exists ");
    for (i = 0; i < count_infiles; ++i)
      printf("%d ", infile_exists[i]);
    printf("/%d\n", count_infiles);
#endif

    /* Set VIV directory */
    viv_directory = (VivDirEntr *)malloc((count_infiles_exist + 1) * sizeof(*viv_directory));
    if (!viv_directory)
    {
      fprintf(stderr, "Viv: Cannot allocate memory\n");
      return 0;
    }
    if (!LIBNFSVIV_SetVivDirHeader(&viv_header, viv_directory, infiles_paths, count_infiles, &infile_exists, count_infiles_exist, opt_requestfmt, opt_direnlenfixed, opt_filenameshex))
    {
      retv = 0;
      break;
    }

    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsEnc(viv_directory, viv_header, infiles_paths, count_infiles, &infile_exists, count_infiles_exist);
    }

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

    /* Write archive header to file */
    if (!LIBNFSVIV_WriteVivHeader(viv_header, file))
    {
      retv = 0;
      break;
    }
    if (!LIBNFSVIV_WriteVivDirectory(viv_directory, infiles_paths, count_infiles, &infile_exists, count_infiles_exist, file, opt_direnlenfixed, opt_filenameshex))
    {
      retv = 0;
      break;
    }
    hdr_size = (int)ftell(file);  /* used in format checks */

    if (hdr_size != viv_header.header_size)
    {
      fprintf(stderr, "Viv: output has invalid header size (%d!=%d)\n", hdr_size, viv_header.header_size);
      retv = 0;
      break;
    }

    /* Write infiles to file */
    for (i = 0, j = 0; i < count_infiles; ++i)
    {
      if (infile_exists[i] < 1)
        continue;
      if (!LIBNFSVIV_VivWriteFile(file, infiles_paths[i], viv_directory[j].filesize))
      {
        retv = 0;
        break;
      }
      ++j;
    }

    if (!opt_dryrun)
      printf("Number encoded: %d\n", j);

    /* Validate */
    viv_filesize = LIBNFSVIV_GetFilesize(file);
    if (!LIBNFSVIV_CheckVivHdr(viv_header, viv_filesize))
    {
      fprintf(stderr, "Viv: New archive failed format check (header)\n");
      retv = 0;
      break;
    }
    if (!LIBNFSVIV_CheckVivDir(&viv_header, viv_directory, hdr_size, viv_filesize, count_infiles_exist))
    {
      fprintf(stderr, "Viv: New archive failed format check (directory)\n");
      retv = 0;
      break;
    }
    break;
  }  /* for (;;) */

  if (file)
    fclose(file);
  if (infile_exists)
    free(infile_exists);
  if (viv_directory)
    free(viv_directory);

  return retv;
}

#endif  /* LIBNFSVIV_H_ */
