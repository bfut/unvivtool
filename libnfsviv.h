/*
  libnfsviv.h - implements VIV/BIG decoding/encoding
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
#include <unistd.h>
#endif

#define UVTVERS "1.9"

#ifndef __cplusplus
enum { kLibnfsvivBufferSize = 4096 };
enum { kLibnfsvivFilenameMaxLen = 256 };
#else
static const int kLibnfsvivBufferSize = 4096;
static const int kLibnfsvivFilenameMaxLen = 256;
#endif

/* BIGF, filesize, count_dir_entries, header_size */
typedef struct {
  char BIGF[4];
  int filesize;
  int count_dir_entries;
  int header_size;  /* includes VIV directory. filename lengths include nul */
} VivHeader;

/* offset, filesize, ofs_begin_filename */
typedef struct {
  int offset;
  int filesize;
  int ofs_begin_filename;
} VivDirEntr;

/* misc --------------------------------------------------------------------- */

static
int LIBNFSVIV_SwapEndian(const int x)
{
  return ((x >> 24) & 0x000000ff) | ((x << 24) & 0xff000000) |
         ((x << 8) & 0xff0000) | ((x >> 8) & 0x00ff00);
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

/* Memsets 'src' to '\\0' */
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
#endif  /* _WIN32 */

/* 'path/to/file.ext' returns 'file.ext' */
char *LIBNFSVIV_GetBasename(char *filename)
{
  char *ptr;

#ifdef _WIN32
  LIBNFSVIV_BkwdToFwdSlash(filename);
#endif  /* _WIN32 */

  if ((ptr = strrchr(filename, '/')))
    return ptr + 1;
  else
    return filename;
}


/* verbose ------------------------------------------------------------------ */

/* Assumes that 'file' is valid VIV data. */
static
void LIBNFSVIV_INTERNAL_PrintStatsDec(
      const VivDirEntr *viv_dir, const VivHeader viv_hdr,
      const int count_dir_entries, const int viv_filesize,
      FILE *file,
      const int request_file_idx, const char *request_file_name)
{
  int curr_chunk_size;
  int i;
  int contents_size = 0;
  int hdr_size;
  int chunk_size = LIBNFSVIV_Min(
                     viv_filesize,
                     viv_dir[count_dir_entries - 1].ofs_begin_filename + kLibnfsvivFilenameMaxLen * 4);
  unsigned char *buffer;
  char filename[kLibnfsvivFilenameMaxLen * 4];
  int len;

  /* (1<<22) - (16 + (8*255)*2048) = 16368 */
  if (chunk_size > (1<<22))
  {
    fprintf(stderr, "Header purports to be greater than 4MB\n");
    return;
  }
  else if (chunk_size < 1)
  {
    fprintf(stderr, "Empty file\n");
    return;
  }

  printf("Archive Size (header) = %d\n", viv_hdr.filesize);
  printf("Header Size (header) = %d\n", viv_hdr.header_size);
  printf("Directory Entries (parsed) = %d\n", count_dir_entries);
  if (request_file_idx)
    printf("Requested file idx = %d\n", request_file_idx);
  if (request_file_name)
  {
    if (request_file_name[0] != '\0')
      printf("Requested file = %.*s\n",
             kLibnfsvivFilenameMaxLen * 4 - 1, request_file_name);
  }

  printf("Buffer = %d\n", kLibnfsvivBufferSize);

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
}  /* LIBNFSVIV_INTERNAL_PrintStatsDec() */

static
void LIBNFSVIV_INTERNAL_PrintStatsEnc(VivDirEntr *viv_dir, const VivHeader viv_hdr,
                                      const int count_dir_entries, char **infiles_paths)
{
  int i;

  printf("Buffer = %d\n", kLibnfsvivBufferSize);
  printf("Header Size = %d\n", viv_hdr.header_size);
  printf("Directory Entries = %d\n", viv_hdr.count_dir_entries);
  printf("Archive Size = %d\n", viv_hdr.filesize);

  if (count_dir_entries > 0)
  {
    printf("\n"
           "   id       Offset         Size Len  Name\n"
           " ---- ------------ ------------ ---  -----------------------\n");

    for (i = 0; i < count_dir_entries; ++i)
    {
      printf(" %4d   %10d   %10d %3d  %s\n", i + 1, viv_dir[i].offset, viv_dir[i].filesize, (int)strlen(LIBNFSVIV_GetBasename(infiles_paths[i])) + 1, LIBNFSVIV_GetBasename(infiles_paths[i]));
    }
    printf(" ---- ------------ ------------ ---  -----------------------\n"
           "        %10d   %10d      %d files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, viv_hdr.filesize - viv_hdr.header_size, count_dir_entries);
  }
}  /* LIBNFSVIV_INTERNAL_PrintStatsEnc() */


/* validate ----------------------------------------------------------------- */

static
int LIBNFSVIV_INTERNAL_CheckVivHeader(const VivHeader viv_header,
                                      const int viv_filesize,
                                      const int opt_strictchecks)
{
    if (strncmp(viv_header.BIGF, "BIGF", (size_t)4) != 0)
    {
      fprintf(stderr, "Format error (header missing BIGF)\n");
      return 0;
    }

    if (viv_header.count_dir_entries < 0)
    {
      fprintf(stderr, "Format error (number of directory entries < 0) %d\n", viv_header.count_dir_entries);
      return 0;
    }

    if (viv_header.count_dir_entries * 12 + 16 > viv_filesize)
    {
      fprintf(stderr, "Format error (invalid number of directory entries) (%d) %d\n", viv_header.count_dir_entries, viv_filesize);
      return 0;
    }

    if (viv_header.header_size > viv_header.count_dir_entries * (8 + kLibnfsvivFilenameMaxLen) + 16)
    {
      fprintf(stderr, "Format error (invalid headersize) (%d) %d\n", viv_header.header_size, viv_header.count_dir_entries);
      return 0;
    }

    if (viv_header.header_size > viv_filesize)
    {
      fprintf(stderr, "Format error (headersize > filesize)\n");
      return 0;
    }

    if (opt_strictchecks && (viv_header.filesize != viv_filesize))
    {
      fprintf(stderr, "Strict Format error (header filesize != filesize)\n");
      return 0;
    }

  return 1;
}  /* LIBNFSVIV_INTERNAL_CheckVivHeader */

static
int LIBNFSVIV_INTERNAL_CheckVivDir(
      const VivHeader viv_header, const VivDirEntr *viv_dir,
      const int hdr_size, const int viv_filesize,
      const int count_dir_entries,
      const int opt_strictchecks)
{
  int retv = 1;
  int contents_size = 0;
  int ofs_now;
  int ofs_prev;
  int i;

  if (viv_header.count_dir_entries != count_dir_entries )
  {
    printf("Warning: header has incorrect number of directory entries "
           "(%d files listed, %d files found)\n",
           viv_header.count_dir_entries, count_dir_entries);
  }

  /* :HS, :PU allow >= truth */
  if ((viv_header.count_dir_entries < 1) || (count_dir_entries < 1))
  {
    printf("Archive is empty "
           "(%d files listed, %d files found)\n",
           viv_header.count_dir_entries, count_dir_entries);
    return 1;
  }

  /* Validate file offsets, sum filesizes */
  i = 0;

  contents_size += viv_dir[i].filesize;

  ofs_now = viv_dir[i].offset;
  if ((ofs_now < hdr_size) ||
      (ofs_now < viv_header.header_size) ||
      (ofs_now + viv_dir[i].filesize > viv_filesize))
  {
    fprintf(stderr, "Format error (offset out of bounds) (file %d) %d\n", i, ofs_now);
    return 0;
  }

  for (i = 1; i < count_dir_entries; ++i)
  {
    contents_size += viv_dir[i].filesize;

    ofs_prev = ofs_now;
    ofs_now = viv_dir[i].offset;

    if (ofs_now - ofs_prev < viv_dir[i - 1].filesize)
    {
      fprintf(stderr, "Format error (file %d overlaps file %d) %d\n",
                      i - 1, i, ofs_prev + viv_dir[i - 1].filesize - ofs_now);
      return 0;
    }

    if ((ofs_now < hdr_size) ||
        (ofs_now < viv_header.header_size) ||
        (ofs_now + viv_dir[i].filesize > viv_filesize))
    {
      fprintf(stderr, "Format error (offset out of bounds) (file %d) %d\n", i, ofs_now);
      return 0;
    }
  }  /* for i */

  /* Normally, should be equal. See strict checks below.
     counterexample: official DLC walm/car.viv broken header purports gaps
     between contained files */
  if (viv_dir[0].offset + contents_size > viv_filesize)
  {
    fprintf(stderr, "Format error (Viv directory filesizes too large)\n");
    retv = 0;
  }

  if (viv_dir[0].offset + contents_size != viv_filesize)
  {
    if (opt_strictchecks == 1)
    {
      fprintf(stderr, "Strict Format error (Viv directory filesizes do not match archive size)\n");
      retv = 0;
    }
    else
      fprintf(stderr, "Strict Format warning (Viv directory filesizes do not match archive size)\n");
  }

  /* :HS, :PU allow >= truth */
  if (viv_header.count_dir_entries != count_dir_entries)
  {
    if (opt_strictchecks == 1)
    {
      fprintf(stderr, "Strict Format error (Viv header has incorrect number of directory entries)\n");
      retv = 0;
    }
    else
      fprintf(stderr, "Strict Format warning (Viv header has incorrect number of directory entries)\n");
  }

  return retv;
}  /* LIBNFSVIV_INTERNAL_CheckVivDir() */


/* decode ------------------------------------------------------------------- */

/* Assumes ftell(file) == 0
   Returns 1, if Viv header can be read and passes checks. Else, return 0. */
static
int LIBNFSVIV_INTERNAL_GetVivHeader(VivHeader *viv_hdr, FILE *file,
                                    const int viv_filesize,
                                    const int opt_strictchecks)
{
  unsigned char buffer[16];

  if (viv_filesize < 16)
  {
    fprintf(stderr, "Format error (file too small)\n");
    return 0;
  }

  if (fread(buffer, (size_t)1, (size_t)16, file) != (size_t)16)
  {
    fprintf(stderr, "File read error (header)\n");
    return 0;
  }

  memcpy(viv_hdr->BIGF,               buffer,      (size_t)4);
  memcpy(&viv_hdr->filesize,          buffer + 4,  (size_t)4);
  memcpy(&viv_hdr->count_dir_entries, buffer + 8,  (size_t)4);
  memcpy(&viv_hdr->header_size,       buffer + 12, (size_t)4);

  viv_hdr->filesize          = LIBNFSVIV_SwapEndian(viv_hdr->filesize);
  viv_hdr->count_dir_entries = LIBNFSVIV_SwapEndian(viv_hdr->count_dir_entries);
  viv_hdr->header_size       = LIBNFSVIV_SwapEndian(viv_hdr->header_size);

  if (!LIBNFSVIV_INTERNAL_CheckVivHeader(*viv_hdr, viv_filesize, opt_strictchecks))
    return 0;

  return 1;
}  /* LIBNFSVIV_INTERNAL_GetVivHeader() */

/* Assumes (viv_dir). Assumes (*count_dir_entries >= true value). Returns boolean.
   */
int LIBNFSVIV_INTERNAL_GetVivDir(VivDirEntr *viv_dir, int *count_dir_entries,
                                 const int viv_filesize,
                                 const VivHeader viv_header, FILE *file,
                                 const int opt_strictchecks)
{
  unsigned char buffer[kLibnfsvivBufferSize];
  int len;
  int curr_offset_file = 0x10;  /* at return, will be true unpadded header size */
  int curr_offset_buffer;
  int curr_chunk_size;
  int i = 0;
  unsigned char *ptr;
  char tmp;

  memset(buffer, '\0', (size_t)kLibnfsvivBufferSize);

  viv_dir[0].offset = viv_filesize;
  while ((curr_offset_file < viv_dir[0].offset) && (i < *count_dir_entries))
  {
    /* Read next chunk */
    curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, viv_filesize - curr_offset_file);
    fseek(file, (long)curr_offset_file, SEEK_SET);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, file) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error at %d (directory)\n", curr_offset_file);
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

      curr_offset_buffer += 0x08;
      curr_offset_file   += 0x08;

      viv_dir[i].ofs_begin_filename = curr_offset_file;

      /* Expect a string here (filename). If no string can be found, offset
         is definitely past the directory. Then the previous entry ended the
         directory, and while is ended. */
      len = 1;
      ptr = buffer + curr_offset_buffer - 1;

      memcpy(&tmp, ptr + 1, (size_t)1);
      if (!isprint(tmp))  /* had isalpha() which fails in respective cases */
      {
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      while (*(++ptr) && (curr_offset_buffer + len < viv_filesize))
        ++len;

      /* Not a string and EOF reached? Not a directory entry. */
      if (curr_offset_buffer + len > viv_filesize)
      {
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

  if (!LIBNFSVIV_INTERNAL_CheckVivDir(viv_header, viv_dir, curr_offset_file,
                                      viv_filesize, *count_dir_entries,
                                      opt_strictchecks))
  {
    return 0;
  }

  return 1;
}  /* LIBNFSVIV_INTERNAL_GetVivDir */

/* Accepts a directory entry, extracts the described file. Returns boolean. */
static
int LIBNFSVIV_INTERNAL_VivExtractFile(const VivDirEntr viv_dir,
                                      const int viv_filesize, FILE *infile)
{
  unsigned char buffer[kLibnfsvivBufferSize];
  int curr_chunk_size;
  int curr_offset;
  FILE *outfile;

  memset(buffer, '\0', (size_t)kLibnfsvivBufferSize);

  /* Get outfilename */
  curr_offset = viv_dir.ofs_begin_filename;
  curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, viv_filesize);

  fseek(infile, (long)curr_offset, SEEK_SET);

  if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
  {
    fprintf(stderr, "File read error at %d (extract outfilename)\n", curr_offset);
    return 0;
  }

  /* Create outfile */
  outfile = fopen((const char *)buffer, "wb");
  if (!outfile)
  {
    fprintf(stderr, "Cannot create output file '%s'\n", (const char *)buffer);
    return 0;
  }

  curr_offset = viv_dir.offset;
  fseek(infile, (long)curr_offset, SEEK_SET);

  while (curr_offset < viv_dir.offset + viv_dir.filesize)
  {
    curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, viv_dir.offset + viv_dir.filesize - curr_offset);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error (archive)\n");
      fclose(outfile);
      return 0;
    }

    if (fwrite(buffer, (size_t)1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File write error (output)\n");
      fclose(outfile);
      return 0;
    }

    curr_offset += curr_chunk_size;
  }

  fclose(outfile);

  return 1;
}  /* LIBNFSVIV_INTERNAL_VivExtractFile() */


/** Assumes (request_file_name).
    Returns 1-based directory entry index for given filename, -1 if it does not
    exist, 0 on error. **/
static
int LIBNFSVIV_INTERNAL_GetIdxFromFname(const VivDirEntr *viv_dir,
                                       FILE* infile, const int infilesize,
                                       const int count_dir_entries,
                                       const char *request_file_name)
{
  int retv = -1;
  int i;
  int chunk_size;
  char buffer[kLibnfsvivBufferSize];

  if (strlen(request_file_name) + 1 > kLibnfsvivFilenameMaxLen * 4)
  {
    fprintf(stderr, "Requested filename is too long\n");
    return 0;
  }

  memset(buffer, '\0', (size_t)kLibnfsvivBufferSize);

  for (i = 0; i < count_dir_entries; ++i)
  {
    fseek(infile, (long)viv_dir[i].ofs_begin_filename, SEEK_SET);
    chunk_size = LIBNFSVIV_Min(infilesize - viv_dir[i].ofs_begin_filename, kLibnfsvivFilenameMaxLen * 4);

    if (fread(buffer, (size_t)1, (size_t)chunk_size, infile) != (size_t)chunk_size)
    {
      fprintf(stderr, "File read error (find index)\n");
      retv = 0;
      break;
    }

    if (!strcmp(buffer, request_file_name))
      return i + 1;
  }

  fprintf(stderr, "Cannot find requested file in archive (filename is cAse-sEnsitivE)\n");
  return retv;
}  /* LIBNFSVIV_INTERNAL_GetIdxFromFname */


/* encode ------------------------------------------------------------------- */

/* Skips invalid paths and accordingly corrects 'count_dir_entries' */
static
void LIBNFSVIV_INTERNAL_SetVivDirHeader(VivHeader *viv_hdr, VivDirEntr *viv_dir,
                                        char **infiles_paths,
                                        int *count_dir_entries)
{
  FILE *file;
  int curr_offset;
  int i;
  int j;
  char *name;
  int len;
  int filesize;

  viv_hdr->filesize = 0;
  curr_offset = 16;

  for (i = 0; i < *count_dir_entries; ++i)
  {
    name = LIBNFSVIV_GetBasename(infiles_paths[i]);
    len = (int)strlen(name);

    filesize = -1;

    file = fopen(infiles_paths[i], "rb");
    if (!file)
    {
      printf("Cannot open file. Skipping '%s'\n", infiles_paths[i]);
    }
    else
    {
      filesize = LIBNFSVIV_GetFilesize(file);
      fclose(file);
    }

    if (filesize < 0)
    {
      for (j = i; j < *count_dir_entries - 1; ++j)
      {
        infiles_paths[j] = infiles_paths[j + 1];
      }

      --*count_dir_entries;
      --i;
      continue;
    }

    viv_dir[i].filesize = filesize;
    viv_hdr->filesize += filesize;

    curr_offset += 8;
    viv_dir[i].ofs_begin_filename = curr_offset;

    curr_offset += len + 1;
  }

  viv_dir[0].offset = curr_offset;
  for (i = 1; i < *count_dir_entries; ++i)
  {
    viv_dir[i].offset = viv_dir[i - 1].offset + viv_dir[i - 1].filesize;
  }

  memcpy(viv_hdr->BIGF, "BIGF", (size_t)4);
  viv_hdr->filesize          = viv_hdr->filesize + curr_offset;
  viv_hdr->count_dir_entries = *count_dir_entries;
  viv_hdr->header_size       = curr_offset;
}  /* LIBNFSVIV_INTERNAL_SetVivDirHeader() */

static
int LIBNFSVIV_INTERNAL_WriteVivHeader(VivHeader viv_hdr, FILE *file)
{
  int retv = 1;
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
    fprintf(stderr, "Warning: File write error (header)\n");
    retv = 0;
  }

  return retv;
}  /* LIBNFSVIV_INTERNAL_WriteVivHeader */

static
int LIBNFSVIV_INTERNAL_WriteVivDirectory(
                                VivDirEntr *viv_directory, char **infiles_paths,
                                const int count_infiles, FILE *file)
{
  int val;
  int i;
  int err = 0;

  for (i = 0; i < count_infiles; ++i)
  {
    val = LIBNFSVIV_SwapEndian(viv_directory[i].offset);
    err += (int)fwrite(&val, (size_t)1, (size_t)4, file);

    val = LIBNFSVIV_SwapEndian(viv_directory[i].filesize);
    err += (int)fwrite(&val, (size_t)1, (size_t)4, file);

    /* nul is always printed */
    err *= LIBNFSVIV_Sign(
          fprintf(file, "%s%c", LIBNFSVIV_GetBasename(infiles_paths[i]), '\0'));

    fflush(file);
  }

  if (err != count_infiles * 8)
  {
    fprintf(stderr, "File write error (directory)\n");
    return 0;
  }

  return 1;
}  /* LIBNFSVIV_INTERNAL_WriteVivDirectory */

/* Chunked write from file at infile_path to outfile. */
static
int LIBNFSVIV_INTERNAL_VivWriteFile(FILE *outfile, const char *infile_path,
                                    const int infile_size)
{
  int retv = 1;
  unsigned char buffer[kLibnfsvivBufferSize];
  int curr_ofs;
  int curr_chunk_size = kLibnfsvivBufferSize;
  FILE *infile = fopen(infile_path, "rb");
  if (!infile)
  {
    fprintf(stderr, "Cannot open file '%s' (infile)\n", infile_path);
    return 0;
  }

  while (curr_chunk_size > 0)
  {
    curr_ofs = ftell(infile);
    curr_chunk_size = LIBNFSVIV_Min(kLibnfsvivBufferSize, infile_size - curr_ofs);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error at %d in '%s' (infile)\n", curr_ofs, infile_path);
      retv = 0;
      break;
    }

    if (fwrite(buffer, (size_t)1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File write error at %d (outfile)\n", curr_chunk_size);
      retv = 0;
      break;
    }
  }

  fclose(infile);
  return retv;
}  /* LIBNFSVIV_INTERNAL_VivWriteFile */


/* api ---------------------------------------------------------------------- */

#if 0
int LIBNFSVIV_SanityTest(void)  /* src: D. Auroux fshtool.c [2002]*/
{
  int retv = 1;
  int x = 0;

  *((char *)(&x)) = 1;
  if (x != 1)
  {
    fprintf(stderr, "expects little-endian architecture\n");
    retv = 0;
  }

  if (sizeof(int) != 4)
  {
    fprintf(stderr, "expects 32-bit int\n");
    retv = 0;
  }

  if (sizeof(short) != 2)
  {
    fprintf(stderr, "expects 16-bit short\n");
    retv = 0;
  }

  if ((sizeof(VivHeader) != 16) || (sizeof(VivDirEntr) != 12))
  {
    fprintf(stderr, "structs are not correctly packed\n");
    retv = 0;
  }

  if (kLibnfsvivFilenameMaxLen * 4 + 8 > kLibnfsvivBufferSize)
  {
    fprintf(stderr, "'kLibnfsvivBufferSize' is too small\n");
    retv = 0;
  }

  if (kLibnfsvivFilenameMaxLen > 256)
  {
    fprintf(stderr, "'kLibnfsvivFilenameMaxLen' exceeds 256 bytes\n");
    retv = 0;
  }

  return retv;
}
#endif

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
  int retv = 1;
  FILE *file = NULL;
  int viv_filesize;
  VivHeader viv_header;
  VivDirEntr *viv_directory = NULL;
  int count_dir_entries;
  int i;

  if (opt_dryrun)
    printf("Begin dry run\n");

  for (;;)
  {
    file = fopen(viv_name, "rb");
    if (!file)
    {
      fprintf(stderr, "File '%s' not found\n", viv_name);
      retv = 0;
      break;
    }

    printf("\nExtracting archive: %s\n", viv_name);
    printf("Extracting to: %s\n", outpath);

    /* Get header and validate */
    viv_filesize = LIBNFSVIV_GetFilesize(file);
    if (!(LIBNFSVIV_INTERNAL_GetVivHeader(&viv_header, file, viv_filesize,
                                          opt_strictchecks)))
    {
      fclose(file);
      retv = 0;
      break;
    }

    count_dir_entries = viv_header.count_dir_entries;  /* is non-negative here */

    printf("Archive Size (parsed) = %d\n", viv_filesize);
    printf("Directory Entries (header) = %d\n", count_dir_entries);

    /* (1<<22) - (16 + (8*255)*2048) = 16368 */
    if (count_dir_entries > 2048)
    {
      fprintf(stderr, "Number of purported directory entries not supported and likely invalid\n");
      retv = 0;
      break;
    }

    viv_directory = (VivDirEntr *)malloc((size_t)(count_dir_entries + 1) * sizeof(*viv_directory));
    if (!viv_directory)
    {
      fprintf(stderr, "Cannot allocate memory\n");
      retv = 0;
      break;
    }

    if (!LIBNFSVIV_INTERNAL_GetVivDir(viv_directory, &count_dir_entries,
                                      viv_filesize, viv_header,
                                      file, opt_strictchecks))
    {
      retv = 0;
      break;
    }

    if (request_file_name)
    {
      if (request_file_name[0] != '\0')
      {
        request_file_idx = LIBNFSVIV_INTERNAL_GetIdxFromFname(
                            viv_directory, file,
                            viv_filesize,
                            count_dir_entries,
                            request_file_name);

        if (request_file_idx < 0)
        {
          retv = 0;
          break;
        }
      }
    }

    if (opt_verbose)
    {
      LIBNFSVIV_INTERNAL_PrintStatsDec(
        viv_directory, viv_header, count_dir_entries,
        viv_filesize, file,
        request_file_idx, request_file_name);
    }

    if (opt_dryrun)
    {
      printf("End dry run\n");
      break;
    }

    /* Extract archive */
    if (chdir(outpath) != 0)
    {
      fprintf(stderr, "Cannot change working directory to '%s' "
                      "(Does the directory exist?)\n", outpath);
      retv = 0;
      break;
    }

    if (request_file_idx != 0)
    {
      if ((request_file_idx < 0) || (request_file_idx > count_dir_entries))
      {
        fprintf(stderr, "Requested idx (%d) out of bounds\n", request_file_idx);
        retv = 0;
        break;
      }

      if (!LIBNFSVIV_INTERNAL_VivExtractFile(viv_directory[request_file_idx - 1],
                                             viv_filesize, file))
      {
        retv = 0;
        break;
      }
    }
    else
    {
      for (i = 0; i < count_dir_entries; ++i)
      {
        /* Continue extracting after a failure, unless strictchecks are enabled */
        if (!LIBNFSVIV_INTERNAL_VivExtractFile(viv_directory[i], viv_filesize, file) &&
            (opt_strictchecks))
        {
          retv = 0;
          break;
        }
      }
    }

    break;
  }  /* for (;;) */

  if (file)
    fclose(file);
  if (viv_directory)
    free(viv_directory);

  return retv;
}

/* Assumes (viv_name). Overwrites file 'viv_name'. Skips unopenable infiles. */
int LIBNFSVIV_Viv(const char *viv_name,
                  char **infiles_paths, int count_infiles,
                  const int opt_dryrun, const int opt_verbose)
{
  int retv = 1;
  FILE *file = NULL;
  int viv_filesize;
  VivHeader viv_header;
  VivDirEntr *viv_directory = NULL;
  int i;
  int hdr_size;

  if (opt_dryrun)
    printf("Begin dry run\n");

  printf("\nCreating archive: %s\n", viv_name);
  printf("Number of files to encode = %d\n", count_infiles);

  /* (1<<22) - (16 + (8*255)*2048) = 16368 */
  if (count_infiles > 2048)
  {
    fprintf(stderr, "Number of files to encode not supported, too large\n");
    return 0;
  }
  else if (count_infiles < 1)
    return 1;

  for (;;)
  {
    /* Set VIV directory */
    viv_directory = (VivDirEntr *)malloc((size_t)count_infiles * sizeof(*viv_directory));
    if (!viv_directory)
    {
      fprintf(stderr, "Cannot allocate enough memory\n");
      return 0;
    }

    LIBNFSVIV_INTERNAL_SetVivDirHeader(&viv_header, viv_directory,
                                       infiles_paths, &count_infiles);

    if (opt_verbose)
    {
      LIBNFSVIV_INTERNAL_PrintStatsEnc(viv_directory, viv_header,
                                       count_infiles, infiles_paths);
    }

    if (opt_dryrun)
    {
      fprintf(stderr, "End dry run\n");
      break;
    }

    file = fopen(viv_name, "wb");
    if (!file)
    {
      fprintf(stderr, "Cannot create output file '%s'\n", viv_name);
      retv = 0;
      break;
    }

    /* Write archive header to file */
    if (!LIBNFSVIV_INTERNAL_WriteVivHeader(viv_header, file))
    {
      retv = 0;
      break;
    }
    if (!LIBNFSVIV_INTERNAL_WriteVivDirectory(viv_directory, infiles_paths,
                                              count_infiles, file))
    {
      retv = 0;
      break;
    }
    hdr_size = (int)ftell(file);  /* used in format checks */

    /* Write infiles to file */
    for (i = 0; i < count_infiles; ++i)
    {
      if (!LIBNFSVIV_INTERNAL_VivWriteFile(file, infiles_paths[i],
                                           viv_directory[i].filesize))
      {
        retv = 0;
        break;
      }
    }

    /* Validate */
    viv_filesize = LIBNFSVIV_GetFilesize(file);
    if (!LIBNFSVIV_INTERNAL_CheckVivHeader(viv_header, viv_filesize, 1))
    {
      fprintf(stderr, "New archive failed format check (header)\n");
      retv = 0;
      break;
    }
    if (!LIBNFSVIV_INTERNAL_CheckVivDir(viv_header, viv_directory,
                                        hdr_size, viv_filesize,
                                        count_infiles, 1))
    {
      fprintf(stderr, "New archive failed format check (directory)\n");
      retv = 0;
      break;
    }

    break;
  }  /* for (;;) */

  if (file)
    fclose(file);
  if (viv_directory)
    free(viv_directory);

  return retv;
}

#endif  /* LIBNFSVIV_H_ */
