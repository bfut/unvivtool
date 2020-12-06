/*
  libnfsviv.h
  Copyright (C) 2020 and later Benjamin Futasz <https://github.com/bfut>

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

/**
  implements VIV/BIG decoding/encoding. unviv() decodes, viv() encodes.
 **/

#ifndef LIBNFSVIV_H
#define LIBNFSVIV_H


#include <stdio.h>
#include <stdlib.h>  /* free, malloc, ... */
#include <string.h>  /* strlen, memset, ... */
#include <ctype.h>   /* isalpha */

#ifdef _WIN32
  #include <direct.h>
  #define chdir _chdir
  #define getcwd _getcwd
  #define mkdir(a,b) _mkdir(a)
  #define rmdir _rmdir
#else
  #include <unistd.h>    /* POSIX chdir, getcwd, rmdir */
  #include <sys/stat.h>  /* POSIX mkdir */
#endif  /* _WIN32 */

#define kLibnfsvivBufferSize 8192
#define kLibnfsvivFilenameMaxLen 255

int libnfsviv_verbose = 0;
int libnfsviv_dryrun = 0;
int libnfsviv_strictchecks = 0;

/* BIGF, filesize, count_dir_entries, header_size */
typedef struct VivHeader {
  unsigned char BIGF[4];
  int filesize;
  int count_dir_entries;
  int header_size;  /* includes VIV directory. filename lengths include nul */
} VivHeader;

/* offset, filesize, ofs_begin_filename */
typedef struct VivDirEntr{
  int offset;
  int filesize;
  int ofs_begin_filename;
} VivDirEntr;


/* utils -------------------------------------------------------------------- */

static
int SwapEndian(int x)
{
  return ((x >> 24) & 0xff) |       /* move byte 3 to byte 0 */
         ((x << 8) & 0xff0000) |    /* move byte 1 to byte 2 */
         ((x >> 8) & 0xff00) |      /* move byte 2 to byte 1 */
         ((x << 24) & 0xff000000);  /* move byte 0 to byte 3 */
}

static
int Min(const int a, const int b)
{
  if (a < b)
    return a;
  else
    return b;
}

static
int Sign(const int x) {
    return (x > 0) - (x < 0);
}

static
int GetFilesize(FILE *file)
{
  int filesize;

  fseek(file, (long)0, SEEK_END);
  filesize = (int)ftell(file);
  rewind(file);

  return filesize;
}

/* 'path/to/file.ext' returns 'file.ext' */
static
char *GetBasename(char *filename)
{
  char *ptr_last_slash;

  ptr_last_slash = strrchr(filename, '/');

  if (ptr_last_slash)
    return ptr_last_slash + 1;
  else
    return filename;
}

static
int IsSupportedName(const char *name, int len)
{
  int valid_len;
  const char *legal_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ._-";
  char name_loc[kLibnfsvivFilenameMaxLen];
  char *p;
  int i;
  static
  char const *reserved_names[22] = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
  };

  if (len > kLibnfsvivFilenameMaxLen - 1)
    {
      fprintf(stderr, "Name of is too long (length %d, max. %d)\n",
              len, kLibnfsvivFilenameMaxLen - 1);
      return 0;
    }

  valid_len = (int)strspn(name, legal_chars);
  if(valid_len != len)
  {
    fprintf(stderr, "Unsupported name '%s', must only contain the following characters %s\n",
            name, legal_chars);
    return 0;
  }

  /* always: name[len] == '\0' */
  switch (name[len - 1])
  {
    case ((char)'.'): case ((char)','): case ((char)';'): case ((char)' '):

      fprintf(stderr, "Windows-incompatible name '%s' (last character '%c')\n",
              name, name[len - 1]);
      return 0;
  }

  memset(name_loc, '\0', kLibnfsvivFilenameMaxLen);
  memcpy(name_loc, name, len);

  p = strrchr(name_loc, '.');
  if (p)
    p[0] = '\0';

  p = name_loc;
  while (--len)
  {
    p[0] = (char)toupper(p[0]);
    ++p;
  }

  for (i = 0; i < 22; ++i)
  {
    if (!strcmp(reserved_names[i], name_loc))
    {
      fprintf(stderr, "Windows-incompatible name '%s'\n", name);
      return 0;
    }
  }

  return 1;
}


/* verbose ------------------------------------------------------------------ */

static
void PrintStatisticsDec(VivDirEntr *viv_dir, const VivHeader viv_hdr,
                        const int count_dir_entries, const int viv_filesize,
                        FILE *file,
                        const int request_file_idx, const char *request_file_name)
{
  int curr_chunk_size;
  int i;
  int contents_size = 0;
  int hdr_size;
  int chunk_size = Min(viv_filesize,
                       viv_dir[count_dir_entries - 1].ofs_begin_filename +
                       kLibnfsvivFilenameMaxLen + 1);
  unsigned char *tmpbuf = (unsigned char *)malloc((size_t)chunk_size);
  if (!tmpbuf)
  {
    fprintf(stderr, "Not enough memory (PrintStatisticsDec) (%lu)\n", (unsigned long)(size_t)chunk_size);
    return;
  }

  fprintf(stdout, "Header Size (header) = %d\n", viv_hdr.header_size);
  fprintf(stdout, "Directory Entries (parsed) = %d\n", count_dir_entries);
  fprintf(stdout, "Archive Size (header) = %d\n", viv_hdr.filesize);
  fprintf(stdout, "Buffer = %d\n", kLibnfsvivBufferSize);

  if (request_file_idx)
    fprintf(stdout, "Requested file idx = %d\n", request_file_idx);

  if (*request_file_name)
    fprintf(stdout, "Requested file = %s\n", request_file_name);

  if (count_dir_entries > 0)
  {
    for (i = 0; i < count_dir_entries; ++i)
    {
      contents_size += viv_dir[i].filesize;
    }

    if (fread(tmpbuf, (size_t)1, (size_t)chunk_size, file) != (size_t)chunk_size)
    {
      fprintf(stderr, "File read error (print stats)\n");
      free(tmpbuf);
      return;
    }

    hdr_size = viv_dir[count_dir_entries - 1].ofs_begin_filename + (int)strlen((char *)(tmpbuf + viv_dir[count_dir_entries - 1].ofs_begin_filename)) + 1;

    fprintf(stdout, "\nPrinting VIV directory:\n"
                    "\n"
                    "   id       Offset Gap         Size Len  Name\n"
                    " ---- ------------ --- ------------ ---  -----------------------\n");
    curr_chunk_size = 0;

    fprintf(stdout, " %4d   %10d %3d   %10d %3d  %s\n", 1, viv_dir[0].offset, viv_dir[0].offset - hdr_size, viv_dir[0].filesize, (int)strlen((char *)(tmpbuf + viv_dir[0].ofs_begin_filename)) + 1, (char *)(tmpbuf + viv_dir[0].ofs_begin_filename));
    for (i = 1; i < count_dir_entries; ++i)
    {
      curr_chunk_size = viv_dir[i].offset - viv_dir[i - 1].offset - viv_dir[i - 1].filesize;
      fprintf(stdout, " %4d   %10d %3d   %10d %3d  %s\n", i + 1, viv_dir[i].offset, curr_chunk_size, viv_dir[i].filesize, (int)strlen((char *)(tmpbuf + viv_dir[i].ofs_begin_filename)) + 1, (char *)(tmpbuf + viv_dir[i].ofs_begin_filename));
    }
    fprintf(stdout, " ---- ------------ --- ------------ ---  -----------------------\n"
                    "        %10d       %10d      %d files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, contents_size, count_dir_entries);
  }

  free(tmpbuf);
}  /* PrintStatisticsDec() */

static
void PrintStatisticsEnc(VivDirEntr *viv_dir, const VivHeader viv_hdr,
                        const int count_dir_entries,
                        char **infiles_paths)
{
  int i;

  fprintf(stdout, "Buffer = %d\n", kLibnfsvivBufferSize);
  fprintf(stdout, "Header Size = %d\n", viv_hdr.header_size);
  fprintf(stdout, "Directory Entries = %d\n", viv_hdr.count_dir_entries);
  fprintf(stdout, "Archive Size = %d\n", viv_hdr.filesize);

  if (count_dir_entries > 0)
  {
    fprintf(stdout, "\n"
                    "   id       Offset         Size Len  Name\n"
                    " ---- ------------ ------------ ---  -----------------------\n");

    for (i = 0; i < count_dir_entries; ++i)
    {
      fprintf(stdout, " %4d   %10d   %10d %3d  %s\n", i + 1, viv_dir[i].offset, viv_dir[i].filesize, (int)strlen(GetBasename(infiles_paths[i])) + 1, GetBasename(infiles_paths[i]));
    }
    fprintf(stdout, " ---- ------------ ------------ ---  -----------------------\n"
                    "        %10d   %10d      %d files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, viv_hdr.filesize - viv_hdr.header_size, count_dir_entries);
  }
}  /* PrintStatisticsEnc() */


/* validate ----------------------------------------------------------------- */

static
int CheckVivHeader(const VivHeader viv_header, const int viv_filesize)
{
    if (strncmp((char *)viv_header.BIGF, "BIGF", 0x04) != 0)
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

    if (libnfsviv_strictchecks && (viv_header.filesize != viv_filesize))
    {
      fprintf(stderr, "Strict Format error (header filesize != filesize)\n");
      return 0;
    }

  return 1;
}  /* CheckVivHeader */

static
int CheckVivDirectory(const VivHeader viv_header, VivDirEntr *viv_dir,
                      const int hdr_size, const int viv_filesize,
                      const int count_dir_entries)
{
  int contents_size = 0;
  int i;
  int *p;
  int *p_prev;

  if (viv_header.count_dir_entries != count_dir_entries )
  {
    fprintf(stdout, "Warning: header has incorrect number of directory entries "
                    "(%d files listed, %d files found)\n",
                    viv_header.count_dir_entries, count_dir_entries);
  }

  /* :HS, :PU allow >= truth */
  if ((viv_header.count_dir_entries < 1) || (count_dir_entries < 1))
  {
    fprintf(stdout, "Empty archive "
                    "(%d files listed, %d files found)\n",
                    viv_header.count_dir_entries, count_dir_entries);
    return 1;
  }

  /* Validate file offsets, cumulate filesizes */
  if (count_dir_entries > 0)
  {
    i = 0;

    contents_size += viv_dir[i].filesize;

    p = &viv_dir[i].offset;
    if ((*(p) < hdr_size) ||
        (*(p) < viv_header.header_size) ||
        (*(p) + viv_dir[i].filesize > viv_filesize))
    {
      fprintf(stderr, "Format error (offset out of bounds) (file %d) %d\n", i, *(p));
      return 0;
    }

    for (i = 1; i < count_dir_entries; ++i)
    {
      contents_size += viv_dir[i].filesize;

      p_prev = p;
      p = &viv_dir[i].offset;

      if (*(p) - *(p_prev) < viv_dir[i - 1].filesize)
      {
        fprintf(stderr, "Format error (file %d overlaps file %d) %d\n",
                        i - 1, i, *(p_prev) + viv_dir[i - 1].filesize - *(p));
        return 0;
      }

      if ((*(p) < hdr_size) ||
          (*(p) < viv_header.header_size) ||
          (*(p) + viv_dir[i].filesize > viv_filesize))
      {
        fprintf(stderr, "Format error (offset out of bounds) (file %d) %d\n", i, *(p));
        return 0;
      }
    }
  }

  /* Normally, should be equal. See strictchecks section.
     counterexample: official DLC walm/car.viv broken header alleges gaps
     between contained files */
  if (viv_dir[0].offset + contents_size > viv_filesize)
  {
    fprintf(stderr, "Format error (Viv directory filesizes too large)\n");
    return 0;
  }

  if (libnfsviv_strictchecks)
  {
    if (viv_dir[0].offset + contents_size != viv_filesize)
    {
      fprintf(stderr, "Strict Format error (Viv directory has invalid filesizes)\n");
      return 0;
    }

    /* :HS, :PU allow >= truth */
    if (viv_header.count_dir_entries != count_dir_entries)
    {
      fprintf(stderr, "Strict Format error (Viv header has incorrect number of directory entries)\n");
      return 0;
    }
  }
  else
  {
    if (viv_dir[0].offset + contents_size != viv_filesize)
    {
      fprintf(stderr, "Strict Format warning (Viv directory has invalid filesizes)\n");
    }

    /* :HS, :PU allow >= truth */
    if (viv_header.count_dir_entries != count_dir_entries)
    {
      fprintf(stderr, "Strict Format warning (Viv header has incorrect number of directory entries)\n");
    }
  }

  return 1;
}  /* CheckVivDirectory() */


/* decode ------------------------------------------------------------------- */

/* Assumes ftell(file) == 0
   Returns 1, if Viv header can be read and passes checks. Else, return 0. */
static
int GetVivHeader(VivHeader *viv_hdr, FILE *file, const int viv_filesize)
{
  if (viv_filesize < 16)
  {
    fprintf(stderr, "Format error (file too small)\n");
    return 0;
  }

  if (fread(viv_hdr, (size_t)1, (size_t)16, file) != (size_t)16)
  {
    fprintf(stderr, "File read error (header)\n");
    return 0;
  }

  viv_hdr->filesize          = SwapEndian(viv_hdr->filesize);
  viv_hdr->count_dir_entries = SwapEndian(viv_hdr->count_dir_entries);
  viv_hdr->header_size       = SwapEndian(viv_hdr->header_size);

  if (!CheckVivHeader(*viv_hdr, viv_filesize))
    return 0;

  return 1;
}  /* GetVivHeader() */

/* Assumes (*viv_dir) and (buffer). Assumes (*count_dir_entries >= true value).
   Returns boolean.

   It is thus possible to always search for more entries than are listed in the
   header, but this may be unsafe.
   */
int GetVivDirectory(VivDirEntr **viv_dir, int *count_dir_entries,
                    const int viv_filesize, const VivHeader viv_header,
                    unsigned char *buffer, FILE *file)
{
  int len;
  int curr_offset_file;  /* at return, will be true unpadded header size */
  int curr_offset_buffer;
  int curr_chunk_size;
  int i;
  char *c;

  /* Init */
  curr_offset_file = 0x10;
  curr_offset_buffer = 0x10;
  (*viv_dir)[0].offset = viv_filesize;
  i = 0;

  while ((curr_offset_file < (*viv_dir)[0].offset) && (i < *count_dir_entries))
  {
    /* Read next chunk */
    curr_chunk_size = Min(kLibnfsvivBufferSize, viv_filesize - curr_offset_file);
    fseek(file, (long)curr_offset_file, SEEK_SET);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, file) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error at %d (directory)\n", curr_offset_file);
      return 0;
    }

    curr_offset_buffer = 0;

    /* Get entries safely within chunk
       (max. DirEntry size = 8 + kLibnfsvivFilenameMaxLen) */
    for ( ; i < *count_dir_entries; ++i)
    {
      (*viv_dir)[i].offset   = SwapEndian(*((int *)(buffer + curr_offset_buffer)));
      (*viv_dir)[i].filesize = SwapEndian(*((int *)(buffer + curr_offset_buffer + 0x04)));

      curr_offset_buffer += 0x08;
      curr_offset_file += 0x08;

      (*viv_dir)[i].ofs_begin_filename = curr_offset_file;

      /* We expect a string here (filename). If no string can be found,
         we are a past the directory. Then the previous entry ended the
         directory.
         Hence COUNT_DIR_ENTRIES = i */
      len = 1;
      c = (char *)(buffer + curr_offset_buffer - 1);

      if (!isalpha(*(c + 1)))
      {
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      while (*(++c) && (curr_offset_buffer + len < viv_filesize))
        ++len;

      /* Not null-terminated and EOF reached? Then is not a directory entry */
      if (curr_offset_buffer + len > viv_filesize)
      {
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      /* Found a string */
      if (!IsSupportedName((char *)(buffer + curr_offset_buffer), len - 1))
      {
        *count_dir_entries = i;  /* breaks while loop */
        break;
      }

      curr_offset_buffer += len;
      curr_offset_file += len;

      /* Does next directory entry fit into current chunk? (including nul) */
      if ((8 + kLibnfsvivFilenameMaxLen > (int)ftell(file) - curr_offset_file) ||
          !(i < *count_dir_entries))
      {
        ++i;
        break;
      }
    }  /* for */
  }  /* while */

  *count_dir_entries = i;

  if (!CheckVivDirectory(viv_header, *viv_dir,
                         curr_offset_file, viv_filesize, *count_dir_entries))
    return 0;

  return 1;
}  /* GetVivDirectory */

/* Accepts a directory entry, extracts the described file. Returns boolean. */
static
int VivExtractFile(const VivDirEntr viv_dir, const int viv_filesize,
                   FILE *infile, unsigned char *buffer)
{
  int curr_chunk_size;
  int curr_offset;
  FILE *outfile;

  /* Get outfilename */
  curr_offset = viv_dir.ofs_begin_filename;
  curr_chunk_size = Min(kLibnfsvivBufferSize, viv_filesize);

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
    fclose(outfile);
    return 0;
  }

  curr_offset = viv_dir.offset;
  fseek(infile, (long)curr_offset, SEEK_SET);

  while(curr_offset < viv_dir.offset + viv_dir.filesize)
  {
    curr_chunk_size = Min(kLibnfsvivBufferSize, viv_dir.offset + viv_dir.filesize - curr_offset);

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
}  /* VivExtractFile() */

/* Accepts a directory entry, and optionally non-zero parameters 'custom_offset',
   'custom_filesize'.
   Extracts the requested file at 'custom_offset' of size 'custom_filesize', if
   given. Returns boolean. */
static
int VivExtractFileCustom(const VivDirEntr viv_dir, const int viv_filesize,
                         FILE* infile, unsigned char *buffer,
                         const int custom_offset, const int custom_filesize)
{
  VivDirEntr viv_dir_custom;

  if (custom_offset)
    viv_dir_custom.offset = custom_offset;
  else
    viv_dir_custom.offset = viv_dir.offset;

  if (custom_filesize)
    viv_dir_custom.filesize = custom_filesize;
  else
    viv_dir_custom.filesize = viv_dir.filesize;

  viv_dir_custom.ofs_begin_filename = viv_dir.ofs_begin_filename;

  return VivExtractFile(viv_dir_custom, viv_filesize, infile, buffer);

}  /* VivExtractFileCustom() */

/** Returns 1-based directory entry index for given filename, -1 if it does not
    exist. **/
static
int FindDirIdxFromFilename(const VivDirEntr *viv_dir,
                           FILE* infile, const int infilesize,
                           unsigned char *buffer, const int count_dir_entries,
                           const char *request_file_name)
{
  int i;
  int chunk_size;

  for (i = 0; i < count_dir_entries; ++i)
  {
    fseek(infile, (long)viv_dir[i].ofs_begin_filename, SEEK_SET);

    chunk_size = Min(infilesize - viv_dir[i].ofs_begin_filename, kLibnfsvivFilenameMaxLen + 1);

    if (fread(buffer, (size_t)1, (size_t)chunk_size, infile) != (size_t)chunk_size)
    {
      fprintf(stderr, "File read error (find index)\n");
      break;
    }

    if (!strcmp((char *)buffer, request_file_name))
      return i + 1;
  }

  fprintf(stderr, "Cannot find requested file in archive (filename is cAse-sEnsitivE)\n");

  return -1;
}  /* FindDirIdxFromFilename */


/* encode ------------------------------------------------------------------- */

/* Skips invalid paths and accordingly corrects 'count_dir_entries' */
static
int SetVivDirectoryHeader(VivHeader *viv_hdr, VivDirEntr **viv_dir,
                          char **infiles_paths, int *count_dir_entries)
{
  int retv = 1;
  FILE *file;
  int curr_offset;
  int i;
  int j;
  char *name;
  int len;

  viv_hdr->filesize = 0;
  curr_offset = 16;

  for (i = 0; (i < *count_dir_entries) && (retv); ++i)
  {
    file = fopen(infiles_paths[i], "rb");

    if (!file || (GetFilesize(file) < 1))
    {
      if (file)
        fclose(file);

      fprintf(stdout, "Cannot open file '%s' Skipping...\n", infiles_paths[i]);

      for (j = i; j < *count_dir_entries - 1; ++j)
      {
        infiles_paths[j] = infiles_paths[j + 1];
      }

      --*count_dir_entries;
      --i;

      continue;
    }

    (*viv_dir)[i].filesize = GetFilesize(file);
    viv_hdr->filesize += (*viv_dir)[i].filesize;

    curr_offset += 8;
    (*viv_dir)[i].ofs_begin_filename = curr_offset;

    name = GetBasename(infiles_paths[i]);
    len = strlen(name);
    retv = IsSupportedName(name, len);

    curr_offset += len + 1;

    fclose(file);
  }

  if (retv)
  {
    (*viv_dir)[0].offset = curr_offset;
    for (i = 1; i < *count_dir_entries; ++i)
    {
      (*viv_dir)[i].offset = (*viv_dir)[i - 1].offset + (*viv_dir)[i - 1].filesize;
    }

    memcpy(viv_hdr->BIGF, "BIGF", (size_t)4);
    viv_hdr->filesize          = viv_hdr->filesize + curr_offset;
    viv_hdr->count_dir_entries = *count_dir_entries;
    viv_hdr->header_size       = curr_offset;
  }

  return retv;
}  /* SetVivDirectoryHeader() */

static
int WriteVivHeader(VivHeader viv_hdr, FILE *file)
{
  int err = 0;

  viv_hdr.filesize          = SwapEndian(viv_hdr.filesize);
  viv_hdr.count_dir_entries = SwapEndian(viv_hdr.count_dir_entries);
  viv_hdr.header_size       = SwapEndian(viv_hdr.header_size);

  err += fprintf(file, "%4s", viv_hdr.BIGF);
  err += (int)fwrite(&viv_hdr.filesize, (size_t)1, (size_t)4, file);
  err += (int)fwrite(&viv_hdr.count_dir_entries, (size_t)1, (size_t)4, file);
  err += (int)fwrite(&viv_hdr.header_size, (size_t)1, (size_t)4, file);

  if (err != 16)
  {
    fprintf(stderr, "Warning: File write error (header)");
    return 0;
  }

  return 1;
}

static
int WriteVivDirectory(VivDirEntr *viv_directory, char **infiles_paths,
                      const int count_infiles, FILE *file)
{
  int val;
  int i;
  int err = 0;

  for (i = 0; i < count_infiles; ++i)
  {
    val = SwapEndian(viv_directory[i].offset);
    err += (int)fwrite(&val, (size_t)1, (size_t)4, file);

    val = SwapEndian(viv_directory[i].filesize);
    err += (int)fwrite(&val, (size_t)1, (size_t)4, file);

    /* fprintf is supposed to print at least one character */
    err *= Sign(fprintf(file, "%s%c", GetBasename(infiles_paths[i]), '\0'));
  }

  if (err != count_infiles * 8)
  {
    fprintf(stderr, "Warning: File write error (directory)");
    return 0;
  }

  return 1;
}

/* Buffered write from file at infile_path to outfile. */
static
int VivWriteFile(FILE *outfile, const char *infile_path, const int infile_size,
                 unsigned char *buffer)
{
  int retv = 1;
  int curr_ofs;
  int curr_chunk_size = kLibnfsvivBufferSize;

  FILE *infile = fopen(infile_path, "rb");
  if (!infile)
  {
    fprintf(stderr, "Cannot open file '%s' (infile)\n", infile_path);
    return 0;
  }

  while(curr_chunk_size > 0)
  {
    curr_ofs = ftell(infile);
    curr_chunk_size = Min(kLibnfsvivBufferSize, infile_size - curr_ofs);

    if (fread(buffer, (size_t)1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error at %d in '%s' (infile)\n", curr_ofs, infile_path);
      retv = 0;
      break;
    }

    if (fwrite(buffer, (size_t)1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File write error (encode) %d\n", curr_chunk_size);
      retv = 0;
      break;
    }
  }

  fclose(infile);
  return retv;
}


/* api ---------------------------------------------------------------------- */

int SanityTest(void)
{
  int x = 0;

  *((char *)(&x)) = 1;
  if (x != 1)
  {
    fprintf(stderr, "architecture is not little-endian\n");
    return 0;
  }

  if ((int)sizeof(int) != 4)
  {
    fprintf(stderr, "int is not 32-bit\n");
    return 0;
  }

  if ((int)sizeof(short) != 2)
  {
    fprintf(stderr, "short is not 16-bit\n");
    return 0;
  }

  if (((int)sizeof(struct VivHeader) != 16) || ((int)sizeof(struct VivDirEntr) != 12))
  {
    fprintf(stderr, "structs are not correctly packed\n");
    return 0;
  }

  if (kLibnfsvivFilenameMaxLen + 8 > kLibnfsvivBufferSize)
  {
    fprintf(stderr, "buffer is too small\n");
    return 0;
  }

  if (kLibnfsvivFilenameMaxLen > 255)
  {
    fprintf(stderr, "maximum filename length exceeds 255\n");
    return 0;
  }

  return 1;
}

/* Assumes (viv_name). Assumes 'outpath' is allowed name.
   Overwrites in directory 'outpath', if it exists.

   If optional 'request_file_idx' is non-zero, extract file at given 1-based
   index.
   If optional 'request_file_name' is non-NULL, extract file with given name.

   If either 'request_file_idx' or 'request_file_name' are given, further
   specify with optional 'request_file_size', and 'request_file_offset'.

   Defaults to values from the respective directory entry.
 */
int Unviv(const char *viv_name, const char *outpath,
          int request_file_idx, const char *request_file_name,
          const int request_file_size, const int request_file_offset)
{
  int retv = 0;
  FILE *file;
  int viv_filesize;
  VivHeader viv_header;
  VivDirEntr *viv_directory;
  unsigned char buffer[kLibnfsvivBufferSize];
  int count_dir_entries;
  int i;

  file = fopen(viv_name, "rb");
  if (!file)
  {
    fprintf(stderr, "File '%s' not found\n", viv_name);
    return -1;
  }

  viv_filesize = GetFilesize(file);

  if (!(GetVivHeader(&viv_header, file, viv_filesize)))
  {
    fclose(file);
    return -1;
  }

  /* viv_header.count_dir_entries is non-negative here */
  count_dir_entries = viv_header.count_dir_entries;

  fprintf(stdout, "Archive Size (parsed) = %d\n", viv_filesize);
  fprintf(stdout, "Directory Entries (header) = %d\n", count_dir_entries);

  viv_directory = (VivDirEntr *)malloc((size_t)((int)sizeof(*viv_directory) * count_dir_entries));
  if (!viv_directory)
  {
    fprintf(stderr, "Not enough memory (%lu)\n", (unsigned long)((size_t)((int)sizeof(*viv_directory) * count_dir_entries)));
    fclose(file);
    return -1;
  }

  for (;;)
  {
    if (!GetVivDirectory(&viv_directory, &count_dir_entries,
                         viv_filesize, viv_header,
                         buffer, file))
    {
      retv = -1;
      break;
    }

    if (*request_file_name)
    {
      rewind(file);
      request_file_idx = FindDirIdxFromFilename(viv_directory, file,
                                                viv_filesize,
                                                buffer, count_dir_entries,
                                                request_file_name);

      if (request_file_idx < 0)
      {
        retv = -1;
        break;
      }
    }

    if (libnfsviv_verbose)
    {
      rewind(file);
      PrintStatisticsDec(viv_directory, viv_header, count_dir_entries,
                         viv_filesize, file,
                         request_file_idx, request_file_name);
    }

    if (libnfsviv_dryrun)
    {
      fprintf(stderr, "End dry run\n");
      break;
    }

    if (chdir(outpath) != 0)
    {
      fprintf(stderr, "Cannot access directory '%s'\n", outpath);
      retv = -1;
      break;
    }

    rewind(file);

    if (request_file_idx)
    {
      if (request_file_idx - 1 < count_dir_entries)
      {
        if (!VivExtractFileCustom(viv_directory[request_file_idx - 1],
                                  viv_filesize, file, buffer,
                                  request_file_offset, request_file_size))
        {
          retv = -1;
          break;
        }
      }
    }
    else
    {
      /* Continue extracting after a failure unless strictchecks are enabled. */
      for (i = 0; i < count_dir_entries;++i)
      {
        if (!VivExtractFile(viv_directory[i], viv_filesize, file, buffer) &&
            (libnfsviv_strictchecks))
        {
          retv = -1;
          break;
        }
      }
    }

    break;
  }

  fclose(file);
  free(viv_directory);

  return retv;
}

/* Assumes (viv_name). Assumes 'viv_name' is allowed name.
   Overwrites file 'viv_name', if it exists. */
int Viv(const char *viv_name, char **infiles_paths, int count_infiles)
{
  int retv = 1;
  FILE *file;
  VivHeader viv_header;
  VivDirEntr *viv_directory;
  unsigned char buffer[kLibnfsvivBufferSize];
  int i;
  int hdr_size;

  fprintf(stdout, "Number of files to encode = %d\n", count_infiles);
  viv_directory = (VivDirEntr *)malloc((size_t)((int)sizeof(*viv_directory) * count_infiles));
  if (!viv_directory)
  {
    fprintf(stderr, "Not enough memory (%lu)\n", (unsigned long)((size_t)sizeof(*viv_directory) * (size_t)count_infiles));
    return 0;
  }

  if (!SetVivDirectoryHeader(&viv_header, &viv_directory,
                             infiles_paths, &count_infiles))
  {
    free(viv_directory);
    return 0;
  }

  if (libnfsviv_verbose)
  {
    PrintStatisticsEnc(viv_directory, viv_header, count_infiles, infiles_paths);
  }

  if (libnfsviv_dryrun)
  {
    fprintf(stderr, "End dry run\n");
    free(viv_directory);
    return 1;
  }

  file = fopen(viv_name, "wb");
  if (!file)
  {
    fprintf(stderr, "Cannot create output file '%s'\n", viv_name);
    free(viv_directory);
    return 0;
  }

  for (;;)
  {
    if (!(retv = WriteVivHeader(viv_header, file)))
      break;

    if (!(retv = WriteVivDirectory(viv_directory, infiles_paths,
                                   count_infiles, file)))
      break;

    hdr_size = (int)ftell(file);
    /** TODO: optional legacy mode (pad directory s.t. its length is divisible
              by 4) **/

    for (i = 0; i < count_infiles; ++i)
    {
      if (!(retv = VivWriteFile(file, infiles_paths[i], viv_directory[i].filesize,
                        buffer)))
        break;
    }

    if (!(retv = CheckVivHeader(viv_header, GetFilesize(file))))
    {
      fprintf(stderr, "Something may be wrong with the new archive\n");
      break;
    }

    if (!(retv = CheckVivDirectory(viv_header, viv_directory,
                           hdr_size, GetFilesize(file), count_infiles)))
    {
      fprintf(stderr, "Something may be wrong with the new archive\n");
      break;
    }

    break;
  }

  fclose(file);
  free(viv_directory);

  return retv;
}

#endif  /* LIBNFSVIV_H */