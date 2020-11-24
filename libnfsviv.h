/*
  libnfsviv.h - 2020-11-24
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
#include <malloc.h>
#include <string.h>  /* memset */
#include <ctype.h>   /* isalpha */

#ifdef _WIN32
  #include <direct.h>
  #define rmdir _rmdir
  #define mkdir(a,b) _mkdir(a)
  #define chdir _chdir
#else
  #include <unistd.h>    /* POSIX chdir, rmdir */
  #include <sys/stat.h>  /* POSIX mkdir */
#endif  /* _WIN32 */

#define kLibnfsvivBufferSize 8192
#define kLibnfsvivFilenameMaxLen 255


int libnfsviv_verbose = 0;
int libnfsviv_dryrun = 0;
int libnfsviv_nochecks = 0;
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
int GetFilesize(FILE *file)
{
  int filesize;

  fseek(file, 0, SEEK_END);
  filesize = ftell(file);
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
                       viv_dir[count_dir_entries].ofs_begin_filename +
                       kLibnfsvivFilenameMaxLen + 1);
  unsigned char *tmpbuf = (unsigned char *)malloc(chunk_size);

  fprintf(stdout, "Header Size (header) = %d\n", viv_hdr.header_size);
  fprintf(stdout, "Directory Entries (parsed) = %d\n", count_dir_entries);
  fprintf(stdout, "Directory Entries (header) = %d\n", viv_hdr.count_dir_entries);
  fprintf(stdout, "Archive Size (parsed) = %d\n", viv_filesize);
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

    if (fread(tmpbuf, 1, (size_t)chunk_size, file) != (size_t)chunk_size)
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

    fprintf(stdout, " %4d   %10u %3u   %10u %3u  %s\n", 1, viv_dir[0].offset, viv_dir[0].offset - hdr_size, viv_dir[0].filesize, (int)strlen((char *)(tmpbuf + viv_dir[0].ofs_begin_filename)) + 1, (char *)(tmpbuf + viv_dir[0].ofs_begin_filename));
    for (i = 1; i < count_dir_entries; ++i)
    {
      curr_chunk_size = viv_dir[i].offset - viv_dir[i - 1].offset - viv_dir[i - 1].filesize;
      fprintf(stdout, " %4d   %10u %3u   %10u %3u  %s\n", i + 1, viv_dir[i].offset, curr_chunk_size, viv_dir[i].filesize, (int)strlen((char *)(tmpbuf + viv_dir[i].ofs_begin_filename)) + 1, (char *)(tmpbuf + viv_dir[i].ofs_begin_filename));
    }
    fprintf(stdout, " ---- ------------ --- ------------ ---  -----------------------\n"
                    "        %10u       %10u      %u files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, contents_size, count_dir_entries);
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
      fprintf(stdout, " %4d   %10u   %10u %3u  %s\n", i + 1, viv_dir[i].offset, viv_dir[i].filesize, (int)strlen(GetBasename(infiles_paths[i])) + 1, GetBasename(infiles_paths[i]));
    }
    fprintf(stdout, " ---- ------------ ------------ ---  -----------------------\n"
                    "        %10u   %10u      %u files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, viv_hdr.filesize - viv_hdr.header_size, count_dir_entries);
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
}

static
int CheckVivDirectory(const VivHeader viv_header, VivDirEntr *viv_dir,
                      const int hdr_size, const int viv_filesize,
                      const int count_dir_entries)
{
  int contents_size;
  int i;
  int *p;

  if ((viv_header.count_dir_entries < 1) && (count_dir_entries < 1))
  {
    fprintf(stdout, "Empty archive (%d files)\n", viv_header.count_dir_entries);
    return 1;
  }

  contents_size = 0;
  for (i = 0; i < count_dir_entries; ++i)
  {
    contents_size += viv_dir[i].filesize;

    p = &viv_dir[i].offset;
    if ((*(p) < hdr_size) ||
        (*(p) < viv_header.header_size) ||
        (*(p) + viv_dir[i].filesize > viv_filesize))
    {
      fprintf(stderr, "Format error (offset out of bounds) (file %d)\n", i);
      return 0;
    }
  }

  /* Normally, should be equal. See strictchecks section.
     counterexample: NFS3 /walm/car.viv has gaps between contained files */
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

    /* NFS:HS, NFS:PU allow >= truth */
    if (viv_header.count_dir_entries != count_dir_entries)
    {
      fprintf(stderr, "Strict Format error (Viv header has invalid number of directory entries)\n");
      return 0;
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
  if (fread(viv_hdr, 1, (size_t) 16, file) != 16)
    return 0;

  viv_hdr->filesize          = SwapEndian(viv_hdr->filesize);
  viv_hdr->count_dir_entries = SwapEndian(viv_hdr->count_dir_entries);
  viv_hdr->header_size       = SwapEndian(viv_hdr->header_size);

  if (!libnfsviv_nochecks)
  {
    if (!CheckVivHeader(*viv_hdr, viv_filesize))
      return 0;
  }

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
    fseek(file, curr_offset_file, SEEK_SET);

    if (fread(buffer, 1, (size_t)curr_chunk_size, file) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error at %d\n", curr_offset_file);
      return 0;
    }

    curr_offset_buffer = 0;

    /* Get entries safely within chunk (max. DirEntry size = 8 + kLibnfsvivFilenameMaxLen) */
    for ( ; i < *count_dir_entries; ++i)
    {
      (*viv_dir)[i].offset = SwapEndian(*((int *)(buffer + curr_offset_buffer)));
      (*viv_dir)[i].filesize = SwapEndian(*((int *)(buffer + curr_offset_buffer + 0x04)));

      curr_offset_buffer += 0x08;
      curr_offset_file += 0x08;

      (*viv_dir)[i].ofs_begin_filename = curr_offset_file;

      /* We expect a string here (filename). If no string can be found,
         we are a past the directory. Then the previous entry ended the directory.
         Hence COUNT_DIR_ENTRIES = i */
      len = 1;
      c = (char *)(buffer + curr_offset_buffer - 1);

      if (!isalpha(*(c + 1)))
      {
        *count_dir_entries = i;
        break;
      }

      while (*(++c) && (curr_offset_buffer + len < viv_filesize))
        ++len;

      if ((curr_offset_buffer + len > viv_filesize) && !libnfsviv_nochecks)
      {
        *count_dir_entries = i;
        break;
      }

      curr_offset_buffer += len;
      curr_offset_file += len;

      /* Next directory entry fits into current chunk? Len >= 2 (includes nul) */
      if ((8 + kLibnfsvivFilenameMaxLen > ftell(file) - curr_offset_file) ||
          !(i < *count_dir_entries))
      {
        ++i;
        break;
      }
    }  /* for */
  }  /* while */

  *count_dir_entries = i;

  if (!libnfsviv_nochecks)
  {
    if (!CheckVivDirectory(viv_header, *viv_dir,
                           curr_offset_file, viv_filesize, *count_dir_entries))
      return 0;
  }

  return 1;
}  /* GetVivDirectory */

/* Accepts a directory entry, extracts the described file. Returns boolean. */
static
int VivExtractFile(const VivDirEntr viv_dir, const int viv_filesize,
                   FILE* infile, unsigned char *buffer)
{
  int curr_chunk_size;
  int curr_offset;
  FILE *outfile;

  /* Get outfilename */
  curr_offset = viv_dir.ofs_begin_filename;
  curr_chunk_size = Min(kLibnfsvivBufferSize, viv_filesize);

  fseek(infile, curr_offset, SEEK_SET);

  if (fread(buffer, 1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
  {
    fprintf(stderr, "File read error at %d\n", curr_offset);
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
  fseek(infile, curr_offset, SEEK_SET);

  while(curr_offset < viv_dir.offset + viv_dir.filesize)
  {
    curr_chunk_size = Min(kLibnfsvivBufferSize, viv_dir.offset + viv_dir.filesize - curr_offset);

    if (fread(buffer, 1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error (archive)\n");
      fclose(outfile);
      return 0;
    }

    if (fwrite(buffer, 1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
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
    exist.
    TODO: reimplement CustomFileExtractor, avoid triple-reading the header  **/
static
int FindDirIdxFromFilename(const VivDirEntr *viv_dir,
                           FILE* infile, unsigned char *buffer,
                           const int count_dir_entries,
                           const char *request_file_name)
{
  int i;

  for (i = 0; i < count_dir_entries; ++i)
  {
    fseek(infile, viv_dir[i].ofs_begin_filename, SEEK_SET);

    if (fread(buffer, 1, kLibnfsvivFilenameMaxLen + 1, infile) !=
      kLibnfsvivFilenameMaxLen + 1)
    {
      fprintf(stderr, "File read error\n");
      break;
    }

    if (!strcmp((char *)buffer, request_file_name))
      return i + 1;
  }

  fprintf(stderr, "Cannot find requested file in archive\n");

  return -1;
}  /* FindDirIdxFromFilename */


/* encode ------------------------------------------------------------------- */

/* Skips invalid paths and accordingly corrects 'count_dir_entries' */
static
void SetVivDirectoryHeader(VivHeader *viv_hdr, VivDirEntr **viv_dir,
                           char **infiles_paths, int *count_dir_entries)
{
  FILE *file;
  int curr_offset;
  int i;
  int j;

  viv_hdr->filesize = 0;
  curr_offset = 16;

  for (i = 0; i < *count_dir_entries; ++i)
  {
    file = fopen(infiles_paths[i], "rb");

    if ((!file) || (GetFilesize(file) < 1))
    {
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

    curr_offset += strlen(GetBasename(infiles_paths[i])) + 1;

    fclose(file);
  }

  (*viv_dir)[0].offset = curr_offset;
  for (i = 1; i < *count_dir_entries; ++i)
  {
    (*viv_dir)[i].offset = (*viv_dir)[i - 1].offset + (*viv_dir)[i - 1].filesize;
  }

  memcpy(viv_hdr->BIGF, "BIGF", 4);
  viv_hdr->filesize          = viv_hdr->filesize + curr_offset;
  viv_hdr->count_dir_entries = *count_dir_entries;
  viv_hdr->header_size       = curr_offset;
}  /* SetVivDirectoryHeader() */

static
void WriteVivHeader(VivHeader viv_hdr, FILE *file)
{
  viv_hdr.filesize          = SwapEndian(viv_hdr.filesize);
  viv_hdr.count_dir_entries = SwapEndian(viv_hdr.count_dir_entries);
  viv_hdr.header_size       = SwapEndian(viv_hdr.header_size);

  fprintf(file, "%4s", viv_hdr.BIGF);
  fwrite(&viv_hdr.filesize, 1, 4, file);
  fwrite(&viv_hdr.count_dir_entries, 1, 4, file);
  fwrite(&viv_hdr.header_size, 1, 4, file);
}

static
void WriteVivDirectory(VivDirEntr *viv_directory, char **infiles_paths,
                       const int count_infiles, FILE *file)
{
  int val;
  int i;

  for (i = 0; i < count_infiles; ++i)
  {
    val = SwapEndian(viv_directory[i].offset);
    fwrite(&val, 1, 4, file);

    val = SwapEndian(viv_directory[i].filesize);
    fwrite(&val, 1, 4, file);

    fprintf(file, "%s%c", GetBasename(infiles_paths[i]), '\0');
  }
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
    fprintf(stderr, "Cannot open file '%s'\n", infile_path);
    return 0;
  }

  while(curr_chunk_size > 0)
  {
    curr_ofs = ftell(infile);
    curr_chunk_size = Min(kLibnfsvivBufferSize, infile_size - curr_ofs);

    if (fread(buffer, 1, (size_t)curr_chunk_size, infile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File read error at %d in '%s'\n", curr_ofs, infile_path);
      retv = 0;
      break;
    }

    fwrite(buffer, 1, curr_chunk_size, outfile);
  }

  fclose(infile);
  return retv;
}


/* api ---------------------------------------------------------------------- */

/* Assumes (viv_name). Overwrites in directory 'outpath', if it exists.

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

  /* Sanity check */
  if (kLibnfsvivFilenameMaxLen + 8 > kLibnfsvivBufferSize)
  {
    fprintf(stderr, "Buffer too small\n");
    return -1;
  }

  file = fopen(viv_name, "rb");
  if (!file)
  {
    fprintf(stderr, "File '%s' not found\n", viv_name);
    return -1;
  }

  viv_filesize = GetFilesize(file);

  if (!(GetVivHeader(&viv_header, file, viv_filesize)))
  {
    fprintf(stderr, "File read error (header)\n");
    fclose(file);
    return -1;
  }

  count_dir_entries = viv_header.count_dir_entries;
  viv_directory = (VivDirEntr *)malloc(sizeof(*viv_directory) * count_dir_entries);
  if (!viv_directory)
  {
    fprintf(stderr, "Not enough memory\n");
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

/* Assumes (viv_name). Overwrites file 'viv_name', if it exists. */
int Viv(const char *viv_name, char **infiles_paths, int count_infiles)
{
  int retv = 0;
  FILE *file;
  VivHeader viv_header;
  VivDirEntr *viv_directory;
  unsigned char buffer[kLibnfsvivBufferSize];
  int i;

  viv_directory = (VivDirEntr *)malloc(sizeof(*viv_directory) * count_infiles);
  if (!viv_directory)
  {
    fprintf(stderr, "Not enough memory\n");
    return -1;
  }

  SetVivDirectoryHeader(&viv_header, &viv_directory,
                          infiles_paths, &count_infiles);

  if (libnfsviv_verbose)
  {
    PrintStatisticsEnc(viv_directory, viv_header, count_infiles,
                        infiles_paths);
  }

  if (libnfsviv_dryrun)
  {
    fprintf(stderr, "End dry run\n");
    free(viv_directory);
    return 0;
  }

  file = fopen(viv_name, "wb");
  if (!file)
  {
    fprintf(stderr, "Cannot create output file '%s'\n", viv_name);
    free(viv_directory);
    return -1;
  }

  for (;;)
  {
    WriteVivHeader(viv_header, file);
    WriteVivDirectory(viv_directory, infiles_paths, count_infiles, file);
    /** TODO: optional legacy mode.  add padding to directory until its length
              is divisible by 4, see NFS3, NFS:HS **/

    for (i = 0; i < count_infiles; ++i)
    {
      if (!VivWriteFile(file, infiles_paths[i], viv_directory[i].filesize,
                        buffer))
      {
        retv = -1;
        break;
      }
    }

    break;
  }

  fclose(file);
  free(viv_directory);

  return retv;
}

#endif  /* LIBNFSVIV_H */
