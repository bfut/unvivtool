/*
  libnfsviv.h
  Copyright (c) 2020 Benjamin Futasz <https://github.com/bfut>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 */

/**
  implements VIV/BIG decoding/encoding. unviv() decodes, viv() encodes.
 **/

#ifndef LIBNFSVIV_H
#define LIBNFSVIV_H

#include <stdio.h>
/* #include <stdlib.h> */
#include <malloc.h>
#include <string.h>
#include <ctype.h>  /* tolower */

#ifdef _WIN32
  #include <direct.h>
  #define rmdir _rmdir
  #define mkdir(a,b) _mkdir(a)
  #define chdir _chdir
#else
  #include <unistd.h>  /*POSIX chdir, rmdir */
  #include <sys/stat.h>  /* POSIX mkdir */
#endif  /* _WIN32 */

#define FILENAME_MAX_SIZE 100

int libnfsviv_verbose = 0;
int libnfsviv_print_content = 0;

typedef struct VivHeader {
  unsigned char BIGF[4];
  int filesize;
  int count_dir_entries;
  int header_size;  /* including entire vivdirectory */
} VivHeader;

typedef struct VivDirectory {
  int offset;
  int filesize;
  unsigned char filename[FILENAME_MAX_SIZE];  /* length includes NULL-terminator */
} VivDirectory;

static
int PrintOutFree4(const char *msg, void *ptr1, void *ptr2, void *ptr3, void *ptr4)
{
  fprintf(stderr, "%s", msg);
  if (ptr1) free(ptr1);
  if (ptr2) free(ptr2);
  if (ptr3) free(ptr3);
  if (ptr4) free(ptr4);
  return 0;
}

static
int PrintErrFree4(const char *msg, void *ptr1, void *ptr2, void *ptr3, void *ptr4)
{
  fprintf(stderr, "%s", msg);
  if (ptr1) free(ptr1);
  if (ptr2) free(ptr2);
  if (ptr3) free(ptr3);
  if (ptr4) free(ptr4);
  return -1;
}

static
int SwapEndian(int x)
{
  int swapped;
  swapped = ((x >> 24) & 0xff) |       /* move byte 3 to byte 0 */
            ((x << 8) & 0xff0000) |    /* move byte 1 to byte 2 */
            ((x >> 8) & 0xff00) |      /* move byte 2 to byte 1 */
            ((x << 24) & 0xff000000);  /* byte 0 to byte 3 */
  return swapped;
}

static
int FileRead(void* file, void* dst, size_t bytes)
{
    FILE* f;

    f = (FILE*)(file);
    return (int) fread(dst, 1, bytes, f);
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
  return (char *) ptr_last_slash ? ptr_last_slash + 1 : filename;
}

static
void libnfsviv_ParseVivDirectory(VivDirectory *viv_directory,
                       int *viv_dir_filename_sizes,
                       unsigned char *buffer, int count_dir_entries,
                       int viv_filesize)
{
  int pos;
  int curr_offset;
  char c;
  int i;

  curr_offset = 0x10;

  for (i = 0; i < count_dir_entries; ++i)
  {
    if (curr_offset + 8 > viv_filesize)
    {
      fprintf(stderr, "Format error (header has incorrect number of files 01).\n");
      free(viv_directory);
      viv_directory = 0;
      return;
    }

    viv_directory[i].offset = SwapEndian(* ((int *) (buffer + curr_offset)));
    viv_directory[i].filesize = SwapEndian(* ((int *) (buffer + curr_offset + 0x04)));

    curr_offset = curr_offset + 0x08;

    c = ' ';
    pos = 0;

    while ((c != 0) && (pos < FILENAME_MAX_SIZE))
    {
      c = * ((char *) (buffer + curr_offset + pos));
      viv_directory[i].filename[pos] = tolower(c);
      ++pos;

      if (curr_offset + pos > viv_filesize)
      {
        fprintf(stderr, "Format error (header has incorrect number of files 02).\n");
        free(viv_directory);
        viv_directory = 0;
        return;
      }
    }

    viv_dir_filename_sizes[i] = pos;

    if (pos >= FILENAME_MAX_SIZE)
    {
      viv_directory[i].filename[pos - 1] = '\0';
      fprintf(stderr, "VIV directory (filename '%s' exceeds character limit %d)\n", viv_directory[i].filename, FILENAME_MAX_SIZE);
      free(viv_directory);
      viv_directory = 0;
      return;
    }

    curr_offset = curr_offset + pos;
  }

  return;
}


/* Overwrites in directory 'outpath', if it exists */
int Unviv(const char *viv_name, const char *outpath)
{
  FILE *file;
  int viv_filesize;
  unsigned char *buffer;
  struct VivHeader *viv_header;
  struct VivDirectory *viv_dir;
  int *viv_dir_filename_sizes;
  int curr_offset;
  int *p;
  int contents_size;
  int i;

  if (!viv_name)
    return PrintErrFree4("Please specify an input archive\n", 0, 0, 0, 0);

  fprintf(stdout, "Reading archive %s\n", viv_name);

  /* Read VIV header */
  file = fopen(viv_name, "rb");
  if (!file)
  {
    fprintf(stderr, "File '%s' not found\n", viv_name);
    return -1;
  }

  viv_filesize = GetFilesize(file);
  if (viv_filesize < 16)
  {
    fclose(file);
    return PrintErrFree4("Format error (truncated header)\n", 0, 0, 0, 0);
  }

  viv_header = (struct VivHeader *) malloc(16);
  if (FileRead(file, viv_header, 16) != 16)
  {
    return PrintErrFree4("File read error (header)\n", viv_header, 0, 0, 0);
  }
  fclose(file);

  viv_header->count_dir_entries = SwapEndian(viv_header->count_dir_entries);
  viv_header->header_size = SwapEndian(viv_header->header_size);

  /* Format checks */
  if (strncmp((char *) viv_header->BIGF, "BIGF", 0x04) != 0)
    return PrintErrFree4("Format error (header missing BIGF)\n", viv_header, 0, 0, 0);
  if (SwapEndian(viv_header->filesize) != viv_filesize)
    return PrintErrFree4("Format error (header has incorrect filesize)\n", viv_header, 0, 0, 0);
  if (viv_header->header_size >= viv_filesize)
    return PrintErrFree4("Format error (header has incorrect headersize)\n", viv_header, 0, 0, 0);

  /* Read VIV archive */
  if (libnfsviv_verbose) fprintf(stdout, "VIV data (%d bytes)\n", viv_filesize);

  if (viv_header->count_dir_entries < 1)
    return PrintOutFree4("Empty archive\n", viv_header, 0, 0, 0);

  buffer = (unsigned char *) malloc(viv_filesize + 8192);

  file = fopen(viv_name, "rb");
  if (FileRead(file, buffer, viv_filesize) != viv_filesize)
  {
    fclose(file);
    return PrintErrFree4("File read error (VIV)\n", viv_header, buffer, 0, 0);
  }
  fclose(file);

  /* Get VIV directory */
  viv_dir = (VivDirectory *) malloc(sizeof(struct VivDirectory) * viv_header->count_dir_entries);
  viv_dir_filename_sizes = (int *) malloc(4 * viv_header->count_dir_entries);
  libnfsviv_ParseVivDirectory(viv_dir, viv_dir_filename_sizes,
                              buffer, viv_header->count_dir_entries, viv_filesize);
  if(!viv_dir)
    return PrintErrFree4("", viv_header, buffer, 0, viv_dir_filename_sizes);

  /* Check VIV directory */
  contents_size = 0;
  curr_offset = 0x10;
  for (i = 0; i < viv_header->count_dir_entries; ++i)
  {
    /* Get header size */
    curr_offset += viv_dir_filename_sizes[i] + 0x08;

    /* Add up archive content filesizes per directory */
    contents_size += viv_dir[i].filesize;

    p = &viv_dir[i].offset;
    if ((*(p) < viv_header->header_size) ||
        (*(p) + viv_dir[i].filesize > viv_filesize))
      return PrintErrFree4("Format error (offset out of bounds)\n", viv_header, buffer, viv_dir, viv_dir_filename_sizes);
  }

  if (viv_header->header_size != curr_offset)
    return PrintErrFree4("Format error (incorrect header size)\n", viv_header, buffer, viv_dir, viv_dir_filename_sizes);
  if (viv_filesize != curr_offset + contents_size)
    return PrintErrFree4("Format error  (dir has incorrect filesizes)\n", viv_header, buffer, viv_dir, viv_dir_filename_sizes);

  /* Print archive contents */
  if (libnfsviv_verbose)
  {
    fprintf(stdout, "Archive contains %u files\n", viv_header->count_dir_entries);

    for (i = 0; i < viv_header->count_dir_entries; ++i)
    {
      fprintf(stdout, "'%s' (%u bytes)\n", viv_dir[i].filename, viv_dir[i].filesize);
    }
  }

  /* End dry-run */
  if (libnfsviv_print_content)
    return PrintOutFree4("", viv_header, buffer, viv_dir, viv_dir_filename_sizes);

  /* Extract archive */
  if (chdir(outpath) != 0)
  {
    fprintf(stderr, "Cannot access directory '%s'\n", outpath);
    return PrintErrFree4("", viv_header, buffer, viv_dir, viv_dir_filename_sizes);
  }

  fprintf(stdout, "Extracting to directory %s\n", outpath);

  for (i = 0; i < viv_header->count_dir_entries; ++i)
  {
    curr_offset = viv_dir[i].offset;

    file = fopen((const char *) viv_dir[i].filename, "wb");
    if (!file)
      return PrintErrFree4("Cannot create output file", viv_header, buffer, viv_dir, viv_dir_filename_sizes);

    fwrite(buffer + curr_offset, 1, viv_dir[i].filesize, file);
    fclose(file);
  }

  return PrintOutFree4("", viv_header, buffer, viv_dir, viv_dir_filename_sizes);
}


/* Overwrites file 'viv_name', if it exists */
int Viv(const char *viv_name, const char **infiles_paths, const int count_dir_entries)
{
  FILE *infile;
  FILE *outfile;
  int viv_filesize;
  int offset;
  int curr_offset_header;
  unsigned char *outbuffer;
  char *p;
  char temp[32768];
  struct VivHeader *viv_header;
  struct VivDirectory *viv_directory;
  int *viv_dir_filename_sizes;
  int i;

  if (!infiles_paths || (count_dir_entries < 1))
  {
    fprintf(stderr, "Missing input files\n");
    return -1;
  }

  for (i = 0; i < count_dir_entries; ++i)
  {
    infile = fopen(infiles_paths[i], "rb");
    if (!infile)
    {
      fprintf(stderr, "Input file '%s' not found\n", infiles_paths[i]);
      return -1;
    }
    fclose(infile);
  }

  /* Initiate VIV header data */
  viv_filesize = 16;
  offset = 16;

  /* Create VIV directory, update VIV header */
  viv_directory = (VivDirectory *) malloc(sizeof(struct VivDirectory) * count_dir_entries);
  viv_dir_filename_sizes = (int *) malloc(4 * count_dir_entries);

  for (i = 0; i < count_dir_entries; ++i)
  {
    sprintf(temp, "%s", infiles_paths[i]);
    p = GetBasename(temp);

    viv_dir_filename_sizes[i] = strlen(p) + 1;  /* count nul */

    memcpy(viv_directory[i].filename, p, viv_dir_filename_sizes[i]);
    viv_directory[i].filename[viv_dir_filename_sizes[i] - 1] = 0;  /* NULL-terminated */

    infile = fopen(infiles_paths[i], "rb");
    viv_directory[i].filesize = GetFilesize(infile);
    fclose(infile);

    if (viv_directory[i].filesize < 0)
    {
      fprintf(stderr, "'%s' is not a file\n", infiles_paths[i]);
      free(viv_directory);
      free(viv_dir_filename_sizes);
      return -1;
    }

    /* dir entry: 4 + 4 + LEN + 1 */
    /* in file: size */
    viv_filesize += 8 + viv_dir_filename_sizes[i] + viv_directory[i].filesize;
    offset += 8 + viv_dir_filename_sizes[i];

    if (libnfsviv_verbose) fprintf(stdout, "'%s' (%d bytes)\n", temp, viv_directory[i].filesize);
  }

  /* Set VIV header */
  viv_header = (VivHeader *) malloc(16);
  memcpy((char *) viv_header->BIGF, "BIGF", 4);
  viv_header->filesize = SwapEndian(viv_filesize);
  viv_header->count_dir_entries = SwapEndian(count_dir_entries);
  viv_header->header_size = SwapEndian(offset);

  /* Write to outbuffer */
  outbuffer = (unsigned char *) malloc(viv_filesize);
  if (!outbuffer)
    return PrintErrFree4("Insufficient memory\n", 0, viv_header, viv_directory, viv_dir_filename_sizes);

  memcpy(outbuffer, viv_header, 16);
  curr_offset_header = 16;  /* begin of current directory entry */
  offset = SwapEndian(viv_header->header_size);  /* begin of current file */

  /* Insert file content, insert directory entry */
  for (i = 0; i < count_dir_entries; ++i)
  {
    viv_directory[i].offset = SwapEndian(offset);

    infile = fopen(infiles_paths[i], "rb");
    if (FileRead(infile, outbuffer + offset, viv_directory[i].filesize) != viv_directory[i].filesize)
    {
      fclose(infile);
      fprintf(stdout, "File read error '%s'\n", infiles_paths[i]);
      return PrintErrFree4("", outbuffer, viv_header, viv_directory, viv_dir_filename_sizes);
    }

    offset += viv_directory[i].filesize;

    viv_directory[i].filesize = SwapEndian(viv_directory[i].filesize);
    memcpy(outbuffer + curr_offset_header, &viv_directory[i], viv_dir_filename_sizes[i] + 8);

    curr_offset_header += viv_dir_filename_sizes[i] + 8;
  }

  if (libnfsviv_verbose) fprintf(stdout, "VIV data (%d bytes)\n", viv_filesize);

  if (libnfsviv_print_content)
    return PrintOutFree4("", outbuffer, viv_header, viv_directory, viv_dir_filename_sizes);

  /* Write to file */
  if (!viv_name)
  {
    fprintf(stderr, "Missing output path\n");
    return -1;
  }
  
  outfile = fopen(viv_name, "wb");
  if (!outfile)
  {
    fprintf(stderr, "Cannot create output file '%s'\n", viv_name);
    return -1;
  }

  fprintf(stdout, "Adding files to %s\n", viv_name);

  if (fwrite(outbuffer, 1, viv_filesize, outfile) != (size_t) viv_filesize)
  {
    fclose(outfile);
    return PrintErrFree4("File write error.\n", outbuffer, viv_header, viv_directory, viv_dir_filename_sizes);
  }
  fclose(outfile);

  PrintOutFree4("", outbuffer, viv_header, viv_directory, viv_dir_filename_sizes);
  return 0;
}

#endif  /* LIBNFSVIV_H */
