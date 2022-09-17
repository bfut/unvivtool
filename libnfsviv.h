/*
  libnfsviv.h - implements BIGF decoding/encoding (best known as VIV/BIG)
  unvivtool Copyright (C) 2020-2022 Benjamin Futasz <https://github.com/bfut>

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

/* API: LIBNFSVIV_Unviv(), LIBNFSVIV_Viv()

BIGF format theoretical limits, assuming signed int:
  min header len:          16         0x10
  max header len:          2147483631 0x7fffffef
  min directory entry len: 10         0xa
  min dir entries count:   0
  max dir entries count:   214748363  0x0ccccccb
*/
#ifndef LIBNFSVIV_H_
#define LIBNFSVIV_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define UVTVERS "1.12"
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

/* branches */
#ifndef UVTVERBOSE
#define UVTVERBOSE 0  /* >=1 for debug console output */
#endif

#ifndef UVTVUTF8
#define UVTVUTF8
#endif

#ifdef UVTVUTF8  /* branch: unviv() detects utf8 */
#include "./include/dfa.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
enum { kLibnfsvivBufferSize = 4096 };
enum { kLibnfsvivFilenameMaxLen = 255 };
enum { kLibnfsvivDirEntrMax = 2097152 };  /* size: 12 * kLibnfsvivDirEntrMax */
#else
static const int kLibnfsvivBufferSize = 4096;
static const int kLibnfsvivFilenameMaxLen = 255;
static const int kLibnfsvivDirEntrMax = 2097152;
#endif

/* BIGF has big-endian numeric values */
typedef struct {
  char BIGF[4];  /* = 'BIGF' */
  int filesize;
  int count_dir_entries;
  int header_size;  /* includes VIV directory. filename lengths include nul */
} VivHeader;

typedef struct {
  int offset;
  int filesize;
  int ofs_begin_filename;
} VivDirEntr;

/* internal: util ----------------------------------------------------------- */

#ifdef UVTVUTF8
static
int LIBNFSVIV_IsNulTerminated(const unsigned char *s, const size_t len)
{
  size_t pos = 0;
  const unsigned char *ptr = s;
  while ((*ptr++) && (pos < len))
  {
    ++pos;
  }
  return pos < len;
}

static
int LIBNFSVIV_IsUTF8(unsigned char *s, const size_t len)
{
  unsigned int codepoint, state = 0;
  if (LIBNFSVIV_IsNulTerminated(s, len))
  {
    while (*s)
    {
      DFA_decode(&state, &codepoint, *s++);
    }
  }
  return state == UTF8_ACCEPT;
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

static
int LIBNFSVIV_Min(const int a, const int b)
{
  if (a < b)
    return a;
  else
    return b;
}

static
int LIBNFSVIV_Sign(const int x)
{
  return (x > 0) - (x < 0);
}

static
int LIBNFSVIV_GetFilesize(FILE *file)
{
  int filesize;
  fseek(file, (long)0, SEEK_END);
  filesize = (int)ftell(file);
  rewind(file);
  return filesize;
}

static
int LIBNFSVIV_Exists(const char *path)
{
  FILE *file = fopen(path, "rb");
  if (!file)
  {
    return 0;
  }
  fclose(file);
  return 1;
}

static
int LIBNFSVIV_IsDirectory(const char *path)
{
  int ret;
#if !defined(__APPLE__) && (defined(_WIN32) || !defined(__STD_VERSION__))
  FILE *file = fopen(path, "rb");
#else
  struct stat sb;
#endif
#if !defined(__APPLE__) && (defined(_WIN32) || !defined(__STD_VERSION__))
  if (!file)
  {
    return 0;
  }
  ret = (LIBNFSVIV_GetFilesize(file) < 0);
  fclose(file);
#else  /* POSIX and not C89 */
  ret = (!stat(path, &sb) && (sb.st_mode & S_IFMT) == S_IFDIR) ? 2 : 0;
#endif
  return ret;
}

/* Memsets 'src' to '\\0' */
static
void LIBNFSVIV_UcharToNulChar(const unsigned char *src, char *dest,
                              const size_t src_len, const size_t dest_len)
{
  memset(dest, '\0', dest_len);
  memcpy(dest, src, src_len);
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

/* 'path/to/file.ext' returns 'file.ext' */
static
char *LIBNFSVIV_GetBasename(char *filename)
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

#if defined(UVTVERBOSE) && UVTVERBOSE >= 1
static
void LIBNFSVIV_PrintVivDirEntr(const VivDirEntr *viv_dir, const int size)
{
  int i;
  printf("PrintVivDirEntr\n");
  printf("i     offset      filesize  ofs_begin_filename\n");
  for (i = 0; i < size; ++i)
  {
    printf("%d     %d (0x%x)   %d       %d\n",
           i, viv_dir[i].offset, viv_dir[i].offset,
           viv_dir[i].filesize, viv_dir[i].ofs_begin_filename);
  }
}
#endif


/* internal: verbose option ------------------------------------------------- */

/* Assumes that 'file' is valid VIV data. */
static
void LIBNFSVIV_PrintStatsDec(
  const VivDirEntr *viv_dir, const VivHeader viv_hdr,
  const int count_dir_entries, const int viv_filesize,
  FILE *file,
  const int request_file_idx, const char *request_file_name)
{
  int curr_chunk_size;
  int i;
  int contents_size = 0;
  int hdr_size;
  int chunk_size;
  unsigned char *buffer;
  char filename[kLibnfsvivFilenameMaxLen * 4];
  int len;

  if (count_dir_entries > 0)
  {
    chunk_size = LIBNFSVIV_Min(
      viv_filesize,
      viv_dir[count_dir_entries - 1].ofs_begin_filename + kLibnfsvivFilenameMaxLen * 4);
  }
  else
  {
    chunk_size = viv_filesize;
  }
  printf("Chunk Size = %d\n", chunk_size);

  if (chunk_size > (1<<22))
  {
    printf("Header purports to be greater than 4MB\n");
    return;
  }
  else if (chunk_size < 1)
  {
    printf("Empty file\n");
    return;
  }

  printf("Buffer = %d\n", kLibnfsvivBufferSize);
  printf("Archive Size (header) = %d\n", viv_hdr.filesize);
  printf("Header Size (header) = %d\n", viv_hdr.header_size);
  printf("Directory Entries (parsed) = %d\n", count_dir_entries);
  if (request_file_idx)
  {
    printf("Requested file idx = %d\n", request_file_idx);
  }
  if ((request_file_name) && (request_file_name[0] != '\0'))
  {
    printf("Requested file = %.*s\n", kLibnfsvivFilenameMaxLen * 4 - 1, request_file_name);
  }

  if (count_dir_entries > 0)
  {
    buffer = (unsigned char *)malloc((size_t)chunk_size * sizeof(*buffer));
    if (!buffer)
    {
      fprintf(stderr, "Cannot allocate memory\n");
      return;
    }

    for (i = 0; i < count_dir_entries; ++i)
    {
      contents_size += viv_dir[i].filesize;
    }

    /* Parse entire header */
    rewind(file);
    if (fread(buffer, (size_t)1, (size_t)chunk_size, file) != (size_t)chunk_size)
    {
      fprintf(stderr, "File read error (print stats)\n");
      free(buffer);
      return;
    }

    /* Actual length of filenames is unknown. Copy as much as possible,
       then get length. */

    /* Print header size */
    LIBNFSVIV_UcharToNulChar(
      buffer + viv_dir[count_dir_entries - 1].ofs_begin_filename,
      filename,
      (size_t)LIBNFSVIV_Min(kLibnfsvivFilenameMaxLen * 4, chunk_size - viv_dir[count_dir_entries - 1].ofs_begin_filename),
      (size_t)(kLibnfsvivFilenameMaxLen * 4));
    len = (int)strlen(filename);
    hdr_size = viv_dir[count_dir_entries - 1].ofs_begin_filename + len + 1;
    printf("Header Size (parsed) = %d\n", hdr_size);

    printf("\nPrinting VIV directory:\n"
           "\n"
           "   id       Offset Gap         Size Len  Name\n"
           " ---- ------------ --- ------------ ---  -----------------------\n");

    LIBNFSVIV_UcharToNulChar(buffer + viv_dir[0].ofs_begin_filename,
                             filename,
                             (size_t)LIBNFSVIV_Min(kLibnfsvivFilenameMaxLen * 4, chunk_size - viv_dir[0].ofs_begin_filename),
                             (size_t)(kLibnfsvivFilenameMaxLen * 4));
    len = (int)strlen(filename);
    printf(" %4d   %10d %3d   %10d %3d  %s\n", 1, viv_dir[0].offset, viv_dir[0].offset - hdr_size, viv_dir[0].filesize, len + 1, filename);

    for (i = 1; i < count_dir_entries; ++i)
    {
      curr_chunk_size = viv_dir[i].offset - viv_dir[i - 1].offset - viv_dir[i - 1].filesize;

      LIBNFSVIV_UcharToNulChar(buffer + viv_dir[i].ofs_begin_filename,
                               filename,
                               (size_t)LIBNFSVIV_Min(kLibnfsvivFilenameMaxLen * 4, chunk_size - viv_dir[i].ofs_begin_filename),
                               (size_t)(kLibnfsvivFilenameMaxLen * 4));
      len = (int)strlen(filename);
      printf(" %4d   %10d %3d   %10d %3d  %s\n", i + 1, viv_dir[i].offset, curr_chunk_size, viv_dir[i].filesize, len + 1, filename);
    }
    printf(" ---- ------------ --- ------------ ---  -----------------------\n"
           "        %10d       %10d      %d files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, contents_size, count_dir_entries);

    free(buffer);
  }  /* if */
}

static
void LIBNFSVIV_PrintStatsEnc(
  VivDirEntr *viv_dir, const VivHeader viv_hdr,
  char **infiles_paths, const int count_infiles,
  int **infile_exists, const int count_dir_entries)
{
  int i;
  int j;

  printf("Buffer = %d\n", kLibnfsvivBufferSize);
  printf("Header Size = %d\n", viv_hdr.header_size);
  printf("Directory Entries = %d\n", viv_hdr.count_dir_entries);
  printf("Archive Size = %d\n", viv_hdr.filesize);

  if (count_dir_entries > 0)
  {
    printf("\n"
           "   id       Offset         Size Len  Name\n"
           " ---- ------------ ------------ ---  -----------------------\n");

    for (i = 0, j = 0; i < count_infiles; ++i)
    {
      if ((*infile_exists)[i] < 1)
      {
        continue;
      }
      printf(" %4d   %10d   %10d %3d  %s\n", j + 1, viv_dir[j].offset, viv_dir[j].filesize, (int)strlen(LIBNFSVIV_GetBasename(infiles_paths[i])) + 1, LIBNFSVIV_GetBasename(infiles_paths[i]));
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
  {
    min_ = LIBNFSVIV_Min(min_, viv_dir[i].offset);
  }
  return min_;
}

static
int LIBNFSVIV_CheckVivHdr(const VivHeader viv_hdr, const int viv_filesize,
                          const int opt_strictchecks)
{
  int ret = 0;
  for (;;)
  {
    if (strncmp(viv_hdr.BIGF, "BIGF", (size_t)4) != 0)
    {
      fprintf(stderr, "CheckVivHeader: Format error (header missing BIGF)\n");
      break;
    }

    if (viv_hdr.count_dir_entries < 0)
    {
      fprintf(stderr, "CheckVivHeader: Format error (number of directory entries < 0) %d\n", viv_hdr.count_dir_entries);
      break;
    }

    if (viv_hdr.count_dir_entries > kLibnfsvivDirEntrMax)
    {
      fprintf(stderr, "CheckVivHeader: Number of purported directory entries not supported and likely invalid (%d > %d)\n", viv_hdr.count_dir_entries, kLibnfsvivDirEntrMax);
      break;
    }

    if (viv_hdr.header_size > viv_filesize)
    {
      fprintf(stderr, "CheckVivHeader: Format warning (headersize > filesize)\n");
    }

    if (opt_strictchecks)
    {
      if (viv_hdr.header_size > viv_filesize)
      {
        fprintf(stderr, "CheckVivHeader: Strict Format error (headersize > filesize)\n");
        break;
      }

      if (viv_hdr.filesize != viv_filesize)
      {
        fprintf(stderr, "CheckVivHeader: Strict Format error (header filesize != filesize) %d, %d\n", viv_hdr.filesize, viv_filesize);
        break;
      }

      if (viv_hdr.count_dir_entries * (8 + kLibnfsvivFilenameMaxLen) + 16 > viv_filesize)
      {
        fprintf(stderr, "CheckVivHeader: Format error (invalid number of directory entries) (%d) %d\n", viv_hdr.count_dir_entries, viv_filesize);
        break;
      }

      if (viv_hdr.header_size > viv_hdr.count_dir_entries * (8 + kLibnfsvivFilenameMaxLen) + 16)
      {
        fprintf(stderr, "CheckVivHeader: Format error (invalid headersize) (%d) %d\n", viv_hdr.header_size, viv_hdr.count_dir_entries);
        break;
      }
    }

    ret = 1;
    break;
  }  /* for (;;) */

  return ret;
}

static
int LIBNFSVIV_CheckVivDir(const VivHeader viv_header, const VivDirEntr *viv_dir,
                          const int hdr_size, const int viv_filesize,
                          const int count_dir_entries,
                          const int opt_strictchecks)
{
  int ret = 1;
  int contents_size = 0;
  int ofs_now;
  int i;

  if (viv_header.count_dir_entries != count_dir_entries)
  {
    printf("Warning:CheckVivDir: incorrect number of directory entries in header (%d files listed, %d files found)\n", viv_header.count_dir_entries, count_dir_entries);
  }

  /* :HS, :PU allow >= truth */
  if ((viv_header.count_dir_entries < 1) || (count_dir_entries < 1))
  {
    printf("Warning:CheckVivDir: empty archive (%d files listed, %d files found)\n", viv_header.count_dir_entries, count_dir_entries);
    return 1;
  }

  if (viv_dir[0].offset != LIBNFSVIV_GetVivFileMinOffset(viv_dir, 0, count_dir_entries, viv_filesize))
  {
    printf("Warning:CheckVivDir: Smallest offset (%d) is not file 0\n", LIBNFSVIV_GetVivFileMinOffset(viv_dir, 0, count_dir_entries, viv_filesize));
  }

  /* Validate file offsets, sum filesizes */
  for (i = 0; i < count_dir_entries; ++i)
  {
    ofs_now = viv_dir[i].offset;

    if ((viv_dir[i].filesize >= viv_filesize) ||
        (viv_dir[i].filesize < 0))
    {
      fprintf(stderr, "CheckVivDir: Format error (filesize out of bounds) (file %d) %d\n", i, viv_dir[i].filesize);
      ret = 0;
      break;
    }
    if ((ofs_now < hdr_size) ||
        (ofs_now < viv_header.header_size) ||
        (ofs_now >= viv_filesize))
    {
      fprintf(stderr, "CheckVivDir: Format error (offset out of bounds) (file %d) %d\n", i, ofs_now);

#if 0
printf("%d %d     <%d || <%d || >=%d\n",
           i, ofs_now, hdr_size,
           viv_header.header_size, viv_filesize);
printf("%d         %d || %d || %d\n",
           i, (ofs_now < hdr_size),
           (ofs_now < viv_header.header_size), (ofs_now >= viv_filesize));
#endif

      ret = 0;
      break;
    }
    if (ofs_now >= INT_MAX - viv_dir[i].filesize)
    {
      fprintf(stderr, "CheckVivDir: Format error (offset overflow) (file %d) %d\n", i, ofs_now);
      ret = 0;
      break;
    }
    if ((ofs_now + viv_dir[i].filesize > viv_filesize))
    {
      fprintf(stderr, "CheckVivDir: Format error (offset out of file bounds) (file %d) %d\n", i, ofs_now);
      ret = 0;
      break;
    }
    contents_size += viv_dir[i].filesize;

    if (i < 1)
    {
      continue;
    }
    if (ofs_now - viv_dir[i - 1].offset < viv_dir[i - 1].filesize)
    {
      fprintf(stderr, "CheckVivDir: Format error (file %d overlaps file %d) %d\n", i - 1, i, viv_dir[i - 1].offset + viv_dir[i - 1].filesize - ofs_now);
      return 0;
    }
  }
  if (ret != 1)
  {
    return 0;
  }

  /* Normally, should be equal. See strict checks below.
     counterexample: official DLC walm/car.viv broken header purports gaps
     between contained files */
  if (viv_dir[0].offset + contents_size > viv_filesize)
  {
    fprintf(stderr, "CheckVivDir: Format error (Viv directory filesizes too large)\n");
    ret = 0;
  }

  if (viv_dir[0].offset + contents_size != viv_filesize)
  {
    if (opt_strictchecks == 1)
    {
      fprintf(stderr, "CheckVivDir: Strict Format error (Viv directory filesizes do not match archive size)\n");
      ret = 0;
    }
    else
    {
      fprintf(stderr, "CheckVivDir: Strict Format warning (Viv directory filesizes do not match archive size)\n");
    }
  }

  /* :HS, :PU allow >= truth */
  if (viv_header.count_dir_entries != count_dir_entries)
  {
    if (opt_strictchecks == 1)
    {
      fprintf(stderr, "CheckVivDir: Strict Format error (Viv header has incorrect number of directory entries)\n");
      ret = 0;
    }
    else
    {
      fprintf(stderr, "CheckVivDir: Strict Format warning (Viv header has incorrect number of directory entries)\n");
    }
  }

  return ret;
}


/* internal: decode --------------------------------------------------------- */

static
void LIBNFSVIV_FixVivHdr(VivHeader *viv_hdr)
{
  if (viv_hdr->count_dir_entries < 0)
  {
    fprintf(stderr, "FixVivHdr:Warning: Format (invalid number of purported directory entries) (%d),\n", viv_hdr->count_dir_entries);
    fprintf(stderr, "32 bit (%d) bitmask,\n", viv_hdr->count_dir_entries & 0x7FFFFFFF);
    fprintf(stderr, "28 bit (%d),\n", viv_hdr->count_dir_entries & 0x0FFFFFFF);
    fprintf(stderr, "24 bit (%d),\n", viv_hdr->count_dir_entries & 0x00FFFFFF);
    fprintf(stderr, "20 bit (%d),\n", viv_hdr->count_dir_entries & 0x000FFFFF);
    fprintf(stderr, "16 bit (%d),\n", viv_hdr->count_dir_entries & 0x0000FFFF);
    viv_hdr->count_dir_entries = LIBNFSVIV_Min(viv_hdr->count_dir_entries & 0x7FFFFFFF, kLibnfsvivDirEntrMax);
    fprintf(stderr, "assume %d entries\n", viv_hdr->count_dir_entries);
  }
  else if (viv_hdr->count_dir_entries > kLibnfsvivDirEntrMax)
  {
    fprintf(stderr, "FixVivHdr:Warning: Format (unsupported number of purported directory entries) (%d),\n", viv_hdr->count_dir_entries);
    fprintf(stderr, "32 bit (%x),\n", viv_hdr->count_dir_entries);
    viv_hdr->count_dir_entries = kLibnfsvivDirEntrMax;
    fprintf(stderr, "assume %d entries\n", viv_hdr->count_dir_entries);
  }
}

/* Assumes ftell(file) == 0
   Returns 1, if Viv header can be read and passes checks. Else, return 0. */
static
int LIBNFSVIV_GetVivHdr(VivHeader *viv_hdr, FILE *file)
{
  unsigned char buffer[16];
  int ret = 0;

  for (;;)
  {
    if (fread(buffer, (size_t)1, (size_t)16, file) != (size_t)16)
    {
      fprintf(stderr, "GetVivHeader: File read error\n");
      break;
    }

    memcpy(viv_hdr->BIGF,               buffer,      (size_t)4);
    memcpy(&viv_hdr->filesize,          buffer + 4,  (size_t)4);
    memcpy(&viv_hdr->count_dir_entries, buffer + 8,  (size_t)4);
    memcpy(&viv_hdr->header_size,       buffer + 12, (size_t)4);

    viv_hdr->filesize          = LIBNFSVIV_SwapEndian(viv_hdr->filesize);
    viv_hdr->count_dir_entries = LIBNFSVIV_SwapEndian(viv_hdr->count_dir_entries);
    viv_hdr->header_size       = LIBNFSVIV_SwapEndian(viv_hdr->header_size);

    ret = 1;
    break;
  }

  return ret;
}

/* Assumes (viv_dir). Assumes (*count_dir_entries >= true value). Returns boolean.
   */
static
int LIBNFSVIV_GetVivDir(VivDirEntr *viv_dir, int *count_dir_entries,
                        const int viv_filesize,
                        const VivHeader viv_header, FILE *file,
                        const int opt_strictchecks, const int opt_verbose)
{
  unsigned char buffer[kLibnfsvivBufferSize];
  int len;
  int curr_offset_file = 0x10;  /* at return, will be true unpadded header size */
  int curr_offset_buffer;
  int curr_chunk_size;
  int i = 0;
  unsigned char *ptr;
  char tmp;
#ifdef UVTVUTF8
  unsigned char *ptr_utf8;
#endif

  memset(buffer, '\0', (size_t)kLibnfsvivBufferSize);

  viv_dir[0].offset = viv_filesize;

#if 0
  while ((curr_offset_file < viv_dir[0].offset) && (i < *count_dir_entries))
#elif 0
  while ((curr_offset_file < LIBNFSVIV_GetVivFileMinOffset(viv_dir, i - 1, i, viv_filesize)) && (i < *count_dir_entries))
#else
  while (i < *count_dir_entries)
#endif
  {
    /* Read next chunk */
    curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, viv_filesize - curr_offset_file);
    fseek(file, (long)curr_offset_file, SEEK_SET);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, file) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "GetVivDir: File read error at %d\n", curr_offset_file);
      return 0;
    }

    curr_offset_buffer = 0;

    /* Get entries safely within chunk
       (max. DirEntry size = 8 + kLibnfsvivFilenameMaxLen * 4) */
    for ( ; i < *count_dir_entries; ++i)
    {
      memcpy(&viv_dir[i].offset, buffer + curr_offset_buffer + 0x0, (size_t)4);
      memcpy(&viv_dir[i].filesize, buffer + curr_offset_buffer + 0x4, (size_t)4);

      viv_dir[i].offset   = LIBNFSVIV_SwapEndian(viv_dir[i].offset);
      viv_dir[i].filesize = LIBNFSVIV_SwapEndian(viv_dir[i].filesize);

#if 0
printf("i%d 0x%x 0x%x\n",
       i,
       LIBNFSVIV_GetVivFileMinOffset(viv_dir, 0, i, viv_filesize),
       viv_dir[i].offset);
#endif

      curr_offset_buffer += 0x08;
      curr_offset_file   += 0x08;

      viv_dir[i].ofs_begin_filename = curr_offset_file;

      /* Expect a string here (filename). If no string can be found, offset
         is definitely past the directory. Then the previous entry ended the
         directory, and while is ended. */
      len = 1;
      ptr = buffer + curr_offset_buffer - 1;

#ifdef UVTVUTF8
      memcpy(&tmp, ptr + 1, (size_t)1);
      ptr_utf8 = ptr;
      if (!isprint(tmp) &&
          !LIBNFSVIV_IsUTF8(ptr_utf8, kLibnfsvivBufferSize - curr_offset_buffer))
#else
      memcpy(&tmp, ptr + 1, (size_t)1);
      /* very crude check as, e.g., CJK characters end the loop */
      if (!isprint(tmp))
#endif
      {
        curr_offset_file -= 0x08;
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      while ((len < kLibnfsvivFilenameMaxLen) &&
             (curr_offset_buffer + len < kLibnfsvivBufferSize) &&
             *(++ptr) &&
             (curr_offset_buffer + len < viv_filesize))
      {
        ++len;
      }
#if 0
printf("i%d len=%d\n",
       i,
       len);
#endif

      /* Expect a string here (filename). If len < 2, not a string. */
      if (len < 2)
      {
        curr_offset_file -= 0x08;
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      if (len >= kLibnfsvivFilenameMaxLen)
      {
        if (opt_verbose >= 1)
        {
          fprintf(stderr, "GetVivDir:Warning: Filename length at %d not supported (must be < %d). Ignore entry. Stop parsing directory.\n", curr_offset_file, kLibnfsvivFilenameMaxLen);
        }
        curr_offset_file -= 0x08;
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

#if 1
      if (curr_offset_buffer + len >= kLibnfsvivBufferSize)
      {
        fprintf(stderr, "GetVivDir: Filename out of bounds at %d (must be < %d)\n", curr_offset_file, kLibnfsvivBufferSize - curr_offset_buffer);
        return 0;
      }
#endif

      /* Not a string and EOF reached? Not a directory entry. Quietly stop looking for entries. */
      if (curr_offset_buffer + len > viv_filesize)
      {
        if (opt_verbose >= 1)
        {
          fprintf(stderr, "GetVivDir:Warning: Filename at %d not a string, reaches EOF. Not a directory entry. Stop parsing directory.\n", curr_offset_file);
        }
        curr_offset_file -= 0x08;
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      curr_offset_buffer += len;
      curr_offset_file += len;

      /* Does next directory entry fit into current chunk? (including nul) */
      if ((8 + kLibnfsvivFilenameMaxLen * 4 > (int)ftell(file) - curr_offset_file) ||
          !(i < *count_dir_entries))
      {
        ++i;
        break;
      }
    }  /* for */
  }  /* while */

  *count_dir_entries = i;

  printf("Directory Entries (parsed) = %d\n", *count_dir_entries);

#if defined(UVTVERBOSE) && UVTVERBOSE >= 1
  LIBNFSVIV_PrintVivDirEntr(viv_dir, *count_dir_entries);
#endif

  if (!LIBNFSVIV_CheckVivDir(viv_header, viv_dir, curr_offset_file,
                             viv_filesize, *count_dir_entries,
                             opt_strictchecks))
  {
    return 0;
  }

  return 1;
}

/* Accepts a directory entry, extracts the described file. Returns boolean. */
static
int LIBNFSVIV_VivExtractFile(const VivDirEntr viv_dir, const int viv_filesize,
                             FILE *infile)
{
  unsigned char buffer[kLibnfsvivBufferSize];
  int curr_chunk_size;
  int curr_offset;
  FILE *outfile;

  memset(buffer, '\0', (size_t)kLibnfsvivBufferSize);

  /* Get outfilename */
  curr_offset = viv_dir.ofs_begin_filename;
  curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, viv_filesize - curr_offset);

  fseek(infile, (long)curr_offset, SEEK_SET);

  if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
  {
    fprintf(stderr, "VivExtractFile: File read error at %d (extract outfilename)\n", curr_offset);
    return 0;
  }

  /* Create outfile */
  outfile = fopen((const char *)buffer, "wb");
  if (!outfile)
  {
    fprintf(stderr, "VivExtractFile: Cannot create output file '%s'\n", (const char *)buffer);
    return 0;
  }

  curr_offset = viv_dir.offset;
  fseek(infile, (long)curr_offset, SEEK_SET);

  while (curr_offset < viv_dir.offset + viv_dir.filesize)
  {
    curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, viv_dir.offset + viv_dir.filesize - curr_offset);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivExtractFile: File read error (archive)\n");
      fclose(outfile);
      return 0;
    }

    if (fwrite(buffer, (size_t)1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
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

/** Assumes (request_file_name).
    Returns 1-based directory entry index for given filename, -1 if it does not
    exist, 0 on error. **/
static
int LIBNFSVIV_GetIdxFromFname(const VivDirEntr *viv_dir,
                              FILE* infile, const int infilesize,
                              const int count_dir_entries,
                              const char *request_file_name)
{
  int ret = -1;
  int i;
  int chunk_size;
  char buffer[kLibnfsvivBufferSize];

  if (strlen(request_file_name) + 1 > kLibnfsvivFilenameMaxLen * 4)
  {
    fprintf(stderr, "GetIdxFromFname: Requested filename is too long\n");
    return 0;
  }

  memset(buffer, '\0', (size_t)kLibnfsvivBufferSize);

  for (i = 0; i < count_dir_entries; ++i)
  {
    fseek(infile, (long)viv_dir[i].ofs_begin_filename, SEEK_SET);
    chunk_size = LIBNFSVIV_Min(infilesize - viv_dir[i].ofs_begin_filename, kLibnfsvivFilenameMaxLen * 4);

    if (fread(buffer, (size_t)1, (size_t)chunk_size, infile) != (size_t)chunk_size)
    {
      fprintf(stderr, "GetIdxFromFname: File read error (find index)\n");
      ret = 0;
      break;
    }

    if (!strcmp(buffer, request_file_name))
    {
      return i + 1;
    }
  }

  fprintf(stderr, "GetIdxFromFname: Cannot find requested file in archive (cAse-sEnsitivE filename)\n");
  return ret;
}


/* internal: encode --------------------------------------------------------- */

static
int LIBNFSVIV_SetVivDirHeader(VivHeader *viv_hdr, VivDirEntr *viv_dir,
                              char **infiles_paths, const int count_infiles,
                              int **infile_exists, const int count_infiles_exist)
{
  int ret = 1;
  FILE *file;
  int curr_offset;
  int i;
  int j;
  char *name;
  int len;
  int filesize;

  viv_hdr->filesize = 0;
  curr_offset = 16;

  for (i = 0, j = 0; i < count_infiles; ++i)
  {
    if ((*infile_exists)[i] < 1)
    {
      continue;
    }
    name = LIBNFSVIV_GetBasename(infiles_paths[i]);
    len = (int)strlen(name);

    filesize = -1;
    file = fopen(infiles_paths[i], "rb");
    if (!file)
    {
      printf("SetVivDirHeader: Cannot open file '%s'\n", infiles_paths[i]);
      ret = 0;
    }
    else
    {
      filesize = LIBNFSVIV_GetFilesize(file);
      fclose(file);
    }

    viv_dir[j].filesize = filesize;
    viv_hdr->filesize += filesize;
    curr_offset += 8;
    viv_dir[j].ofs_begin_filename = curr_offset;
    curr_offset += len + 1;
    ++j;
  }

  viv_dir[0].offset = curr_offset;
  for (j = 1; j < count_infiles_exist; ++j)
  {
    viv_dir[j].offset = viv_dir[j - 1].offset + viv_dir[j - 1].filesize;
  }

  memcpy(viv_hdr->BIGF, "BIGF", (size_t)4);
  viv_hdr->filesize          = viv_hdr->filesize + curr_offset;
  viv_hdr->count_dir_entries = count_infiles_exist;
  viv_hdr->header_size       = curr_offset;
  return ret;
}

static
int LIBNFSVIV_WriteVivHeader(VivHeader viv_hdr, FILE *file)
{
  int ret = 1;
  size_t err = 0;

  viv_hdr.filesize          = LIBNFSVIV_SwapEndian(viv_hdr.filesize);
  viv_hdr.count_dir_entries = LIBNFSVIV_SwapEndian(viv_hdr.count_dir_entries);
  viv_hdr.header_size       = LIBNFSVIV_SwapEndian(viv_hdr.header_size);

  err += fwrite(viv_hdr.BIGF, (size_t)1, (size_t)4, file);
  err += fwrite(&viv_hdr.filesize, (size_t)1, (size_t)4, file);
  err += fwrite(&viv_hdr.count_dir_entries, (size_t)1, (size_t)4, file);
  err += fwrite(&viv_hdr.header_size, (size_t)1, (size_t)4, file);

  if (err != 16)
  {
    fprintf(stderr, "WriteVivHeader: Warning: File write error\n");
    ret = 0;
  }

  return ret;
}

static
int LIBNFSVIV_WriteVivDirectory(VivDirEntr *viv_directory,
                                char **infiles_paths, const int count_infiles,
                                int **infile_exists, const int count_infiles_exist,
                                FILE *file)
{
  int val;
  int i;
  int j;
  int err = 0;

  for (i = 0, j = 0; i < count_infiles; ++i)
  {
    if ((*infile_exists)[i] < 1)
    {
      continue;
    }

    val = LIBNFSVIV_SwapEndian(viv_directory[j].offset);
    err += (int)fwrite(&val, (size_t)1, (size_t)4, file);

    val = LIBNFSVIV_SwapEndian(viv_directory[j].filesize);
    err += (int)fwrite(&val, (size_t)1, (size_t)4, file);

    /* nul is always printed */
    err *= LIBNFSVIV_Sign(
      fprintf(file, "%s%c", LIBNFSVIV_GetBasename(infiles_paths[i]), '\0'));

    fflush(file);
    ++j;
  }

  if (err != count_infiles_exist * 8)
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
  int ret = 1;
  unsigned char buffer[kLibnfsvivBufferSize];
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
    curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, infile_size - curr_ofs);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivWriteFile: File read error at %d in '%s' (infile)\n", curr_ofs, infile_path);
      ret = 0;
      break;
    }

    if (fwrite(buffer, (size_t)1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "VivWriteFile: File write error at %d (outfile)\n", curr_chunk_size);
      ret = 0;
      break;
    }
  }

  fclose(infile);
  return ret;
}


/* api ---------------------------------------------------------------------- */

#ifdef UVTDEBUG
int LIBNFSVIV_SanityTest(void)  /* src: D. Auroux fshtool.c */
{
  int ret = 1;
  int x = 0;

  *((char *)(&x)) = 1;
  if (x != 1)
  {
    fprintf(stderr, "expects little-endian architecture\n");
    ret = 0;
  }

  if (sizeof(int) != 4)
  {
    fprintf(stderr, "expects 32-bit int\n");
    ret = 0;
  }

  if (sizeof(short) != 2)
  {
    fprintf(stderr, "expects 16-bit short\n");
    ret = 0;
  }

  if ((sizeof(VivHeader) != 16) || (sizeof(VivDirEntr) != 12))
  {
    fprintf(stderr, "structs are not correctly packed\n");
    ret = 0;
  }

  if (kLibnfsvivFilenameMaxLen * 4 + 8 > kLibnfsvivBufferSize)
  {
    fprintf(stderr, "'kLibnfsvivBufferSize' is too small\n");
    ret = 0;
  }

  if (kLibnfsvivFilenameMaxLen > 256)
  {
    fprintf(stderr, "'kLibnfsvivFilenameMaxLen' exceeds 256 bytes\n");
    ret = 0;
  }

  return ret;
}
#endif  /* UVTDEBUG */

/* Assumes (viv_name). Assumes (outpath). Overwrites directory 'outpath'.
   Changes working directory to 'outpath'.

   If (request_file_idx > 0), extract file at given 1-based index.
   If (!request_file_name), extract file with given name. Overrides 'request_file_idx'.
*/
int LIBNFSVIV_Unviv(const char *viv_name, const char *outpath,
                    int request_file_idx, const char *request_file_name,
                    const int opt_dryrun, const int opt_strictchecks,
                    const int opt_verbose)
{
  int ret = 0;
  FILE *file = NULL;
  int viv_filesize;
  VivHeader viv_hdr;
  VivDirEntr *viv_directory = NULL;
  int count_dir_entries;
  int i = 0;

  if (opt_dryrun)
  {
    printf("Begin dry run\n");
  }

  for (;;)
  {
#ifndef _WIN32
    if (LIBNFSVIV_IsDirectory(viv_name))
    {
      fprintf(stderr, "Unviv: Cannot open directory as archive '%s'\n", viv_name);
      break;
    }
#endif
    file = fopen(viv_name, "rb");
    if (!file)
    {
      fprintf(stderr, "Unviv: Cannot open '%s'\n", viv_name);
      break;
    }

    printf("\nExtracting archive: %s\n", viv_name);
    printf("Extracting to: %s\n", outpath);

    {
      const size_t fs_ = LIBNFSVIV_GetFilesize(file);
      printf("Archive Size (parsed) = %d\n", (int)fs_);
      if (fs_ > INT_MAX)
      {
        fprintf(stderr, "Unviv: Filesize not supported ( > %d)\n", INT_MAX);
        break;
      }
      else if (fs_ < 16)
      {
        fprintf(stderr, "Unviv: Format error (file too small) %d\n", (int)fs_);
        break;
      }
      viv_filesize = (int)fs_;
    }

    /* Get header and validate */
    if (!LIBNFSVIV_GetVivHdr(&viv_hdr, file))
    {
      break;
    }
    printf("Archive Size (header) = %d\n", viv_hdr.filesize);
    printf("Directory Entries (header) = %d\n", viv_hdr.count_dir_entries);
    printf("Header Size (header) = %d\n", viv_hdr.header_size);

    LIBNFSVIV_FixVivHdr(&viv_hdr);

    if (!LIBNFSVIV_CheckVivHdr(viv_hdr, viv_filesize, opt_strictchecks))
    {
      break;
    }
    count_dir_entries = viv_hdr.count_dir_entries;  /* is non-negative here */

    viv_directory = (VivDirEntr *)malloc((size_t)(count_dir_entries + 1) * sizeof(*viv_directory));
    if (!viv_directory)
    {
      fprintf(stderr, "Unviv: Cannot allocate memory\n");
      break;
    }

    if (!LIBNFSVIV_GetVivDir(viv_directory, &count_dir_entries,
                             viv_filesize, viv_hdr, file, opt_strictchecks,
                             opt_verbose))
    {
      break;
    }

    if ((request_file_name) && (request_file_name[0] != '\0'))
    {
      request_file_idx = LIBNFSVIV_GetIdxFromFname(viv_directory,
                                                   file, viv_filesize,
                                                   count_dir_entries,
                                                   request_file_name);
      if (request_file_idx < 0)
      {
        break;
      }
    }

    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsDec(viv_directory, viv_hdr, count_dir_entries,
                              viv_filesize, file,
                              request_file_idx, request_file_name);
    }

    if (opt_dryrun)
    {
      printf("End dry run\n");
      ret = 1;
      break;
    }

    /* Extract archive */
#ifndef _WIN32
    if (!LIBNFSVIV_Exists(outpath))
    {
      fprintf(stderr, "Unviv: Path does not exist '%s'\n", outpath);
      break;
    }
    if (!LIBNFSVIV_IsDirectory(outpath))
    {
      fprintf(stderr, "Unviv: Expects directory '%s'\n", outpath);
      break;
    }
#endif
    if (chdir(outpath) != 0)
    {
#ifndef _WIN32
      fprintf(stderr, "Unviv: Cannot change working directory to '%s'\n", outpath);
#else
      fprintf(stderr, "Unviv: Cannot change working directory to '%s' (directory may not exist)\n", outpath);
#endif
      break;
    }

    if (request_file_idx != 0)
    {
      if ((request_file_idx < 0) || (request_file_idx > count_dir_entries))
      {
        fprintf(stderr, "Unviv: Requested idx (%d) out of bounds\n", request_file_idx);
        break;
      }

      if (!LIBNFSVIV_VivExtractFile(viv_directory[request_file_idx - 1],
                                    viv_filesize, file))
      {
        break;
      }
    }
    else
    {
      for (i = 0; i < count_dir_entries; ++i)
      {
        /* Continue extracting after a failure, unless strictchecks are enabled */
        if (!LIBNFSVIV_VivExtractFile(viv_directory[i], viv_filesize, file) && (opt_strictchecks))
        {
          break;
        }
      }
    }

    ret = 1;
    break;
  }  /* for (;;) */

  if (!opt_dryrun)
    printf("Number extracted: %d\n", i);

  if (file)
    fclose(file);
  if (viv_directory)
    free(viv_directory);

  return ret;
}

/* Assumes (viv_name). Overwrites file 'viv_name'. Skips unopenable infiles. */
int LIBNFSVIV_Viv(const char *viv_name,
                  char **infiles_paths, const int count_infiles,
                  const int opt_dryrun, const int opt_verbose)
{
  int ret = 1;
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
  {
    printf("Begin dry run\n");
  }

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
    infile_exists = (int *)calloc((size_t)(count_infiles + 1), sizeof(int));
    if (!infile_exists)
    {
      fprintf(stderr, "Viv: Cannot allocate memory\n");
      return 0;
    }
    for (i = 0; i < count_infiles; ++i)
    {
      if (LIBNFSVIV_Exists(infiles_paths[i]) && !LIBNFSVIV_IsDirectory(infiles_paths[i]))
      {
        infile_exists[i] = 1;
        ++count_infiles_exist;
      }
      else
      {
        printf("Cannot open file. Skipping '%s'\n", infiles_paths[i]);
      }
    }
#if defined(UVTVERBOSE) && UVTVERBOSE >= 1
    printf("viv:debug: infile_exists ");
    for (i = 0; i < count_infiles; ++i)
    {
      printf("%d ", infile_exists[i]);
    }
    printf("/%d\n", count_infiles);
#endif

    /* Set VIV directory */
    viv_directory = (VivDirEntr *)malloc((size_t)(count_infiles_exist + 1) * sizeof(*viv_directory));
    if (!viv_directory)
    {
      fprintf(stderr, "Viv: Cannot allocate memory\n");
      return 0;
    }
    if (!LIBNFSVIV_SetVivDirHeader(&viv_header, viv_directory,
                                   infiles_paths, count_infiles,
                                   &infile_exists, count_infiles_exist))
    {
      ret = 0;
      break;
    }

    if (opt_verbose)
    {
      LIBNFSVIV_PrintStatsEnc(viv_directory, viv_header,
                              infiles_paths, count_infiles,
                              &infile_exists, count_infiles_exist);
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
      ret = 0;
      break;
    }

    /* Write archive header to file */
    if (!LIBNFSVIV_WriteVivHeader(viv_header, file))
    {
      ret = 0;
      break;
    }
    if (!LIBNFSVIV_WriteVivDirectory(viv_directory,
                                     infiles_paths, count_infiles,
                                     &infile_exists, count_infiles_exist,
                                     file))
    {
      ret = 0;
      break;
    }
    hdr_size = (int)ftell(file);  /* used in format checks */

    if (hdr_size != viv_header.header_size)
    {
      fprintf(stderr, "Viv: output has invalid header size (%d!=%d)\n", hdr_size, viv_header.header_size);
      ret = 0;
      break;
    }

    /* Write infiles to file */
    for (i = 0, j = 0; i < count_infiles; ++i)
    {
      if (infile_exists[i] < 1)
      {
        continue;
      }
      if (!LIBNFSVIV_VivWriteFile(file, infiles_paths[i],
                                  viv_directory[j].filesize))
      {
        ret = 0;
        break;
      }
      ++j;
    }

    /* Validate */
    viv_filesize = LIBNFSVIV_GetFilesize(file);
    if (!LIBNFSVIV_CheckVivHdr(viv_header, viv_filesize, 1))
    {
      fprintf(stderr, "Viv: New archive failed format check (header)\n");
      ret = 0;
      break;
    }
    if (!LIBNFSVIV_CheckVivDir(viv_header, viv_directory,
                               hdr_size, viv_filesize,
                               count_infiles_exist, 1))
    {
      fprintf(stderr, "Viv: New archive failed format check (directory)\n");
      ret = 0;
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

  return ret;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* LIBNFSVIV_H_ */
