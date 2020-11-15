/*
  libnfsviv.h - 2020-11-15
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
  implements Viv/Big decoding/encoding. unviv() decodes, viv() encodes.
 **/

#ifndef LIBNFSVIV_H
#define LIBNFSVIV_H


#include <stdio.h>
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

#define kFileNameMaxSize 56      /* known max. len=41 */
#define kLibnfsvivChunkSize 8192


int libnfsviv_verbose = 0;
int libnfsviv_dryrun = 0;
int libnfsviv_nochecks = 0;
int libnfsviv_strictchecks = 0;


/* BIGF, filesize, count_dir_entries, header_size */
typedef struct VivHeader {
  unsigned char BIGF[4];
  int filesize;
  int count_dir_entries;
  int header_size;  /* includes vivdirectory */
} VivHeader;

typedef struct VivDirEntry {
  int offset;
  int filesize;
  unsigned char filename[kFileNameMaxSize];
} VivDirEntry;

/* known max. entries=593  */
typedef struct VivStruct {
  VivHeader header;           /* len=  16  */
  int filename_lengths[764];  /* len=3056  */ /* strlen(VivDirEntry.filename) + 1 */
  VivDirEntry directory[764]; /* len= 764 * (8 + kFileNameMaxSize) */
} VivStruct;


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
  /* return (char *)ptr_last_slash ? ptr_last_slash + 1 : filename; */
}

static
int PrintFree4Ret(const int retv, void *stream, const char *msg, void *ptr1, void *ptr2, void *ptr3, void *ptr4)
{
  fprintf((FILE *)stream, "%s", msg);

  if (ptr1) free(ptr1);
  if (ptr2) free(ptr2);
  if (ptr3) free(ptr3);
  if (ptr4) free(ptr4);

  return retv;
}

static
int Max(int a, int b)
{
  return a > b ? a : b;
}

static
int Min(int a, int b)
{
  return a < b ? a : b;
}

#if 0
static
int IncrUntilDiv4(const int a)
{
  int incr = 0;

  while((a + incr) % 4)
    ++incr;

  return incr;
}
#endif

static
int SwapEndian(int x)
{
  return ((x >> 24) & 0xff) |       /* move byte 3 to byte 0 */
         ((x << 8) & 0xff0000) |    /* move byte 1 to byte 2 */
         ((x >> 8) & 0xff00) |      /* move byte 2 to byte 1 */
         ((x << 24) & 0xff000000);  /* byte 0 to byte 3 */
}

static
int FileRead(void* file, void* dst, size_t bytes)
{
    FILE* f;

    f = (FILE*)(file);

    return (int)fread(dst, 1, bytes, f);
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


/* Parses Viv directory. Updates true unpadded header size,
   and true number of directory entries. */
static
int GetVivDirectory(VivDirEntry **viv_dir, int **viv_dir_filename_sizes,
                      int *viv_headersize, int *count_dir_entries,
                      unsigned char *buffer, FILE *file,
                      const int viv_filesize)
{
  int len;
  int curr_offset_file;
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
    curr_chunk_size = Min(kLibnfsvivChunkSize, viv_filesize - curr_offset_file);

    fseek(file, curr_offset_file, SEEK_SET);
    if (FileRead(file, buffer, curr_chunk_size) != curr_chunk_size)
    {
      fprintf(stdout, "File read error at %d\n", curr_offset_file);
      return -1;
    }

    curr_offset_buffer = 0;

    /* Get entries safely within chunk (max. DirEntry size = 8 + kFileNameMaxSize */
    for ( ; i < *count_dir_entries; ++i)
    {
      (*viv_dir)[i].offset = SwapEndian(*((int *)(buffer + curr_offset_buffer)));
      (*viv_dir)[i].filesize = SwapEndian(*((int *)(buffer + curr_offset_buffer + 0x04)));

      curr_offset_buffer += 0x08;
      curr_offset_file += 0x08;

      len = 1;
      c = (char *)(buffer + curr_offset_buffer - 1);
      while (*(++c) && (len < kFileNameMaxSize) && (curr_offset_buffer + len < viv_filesize))
        ++len;

      if (!(len < kFileNameMaxSize))
      {
        fprintf(stderr, "DirEntry filename at %d exceeds length (%d)\n", curr_offset_file, kFileNameMaxSize);
        return -1;
      }

      if ((curr_offset_buffer + len > viv_filesize) && !libnfsviv_nochecks)
      {
        fprintf(stderr, "Format error (header not null-terminated)\n");
        return -1;
      }

      (*viv_dir_filename_sizes)[i] = len;
      memcpy( (*viv_dir)[i].filename , buffer + curr_offset_buffer, len);

      curr_offset_buffer += len;
      curr_offset_file += len;

      /* Next directory entry fits into current chunk?  */
      if ((8 + kFileNameMaxSize > ftell(file) - curr_offset_file) || !(i < *count_dir_entries))
      {
        ++i;
        break;
      }
    }
  }

  *viv_headersize = curr_offset_file;
  *count_dir_entries = i;

  return 0;
}

static
int CheckVivHeader(const VivHeader viv_header, const int viv_filesize)
{
    if (strncmp((char *)viv_header.BIGF, "BIGF", 0x04) != 0)
    {
      fprintf(stderr, "Format error (header missing BIGF)\n");
      return -1;
    }

    if (viv_header.header_size > viv_filesize)
    {
      fprintf(stderr, "Format error (headersize > filesize)\n");
      return -1;
    }

    if (libnfsviv_strictchecks && (viv_header.filesize != viv_filesize))
    {
      fprintf(stderr, "Strict Format error (header filesize != filesize)\n");
      return -1;
    }

  return 0;
}

static
int CheckVivDirectory(int *contents_size,
                      const VivHeader viv_header, VivDirEntry *viv_dir,
                      const int *viv_dir_filename_sizes, const int viv_filesize,
                      const int count_dir_entries)
{
  int curr_offset = 0x10;
  int i;
  int *p;

  *contents_size = 0;

  if ((viv_header.count_dir_entries < 1) && (count_dir_entries < 1))
  {
    fprintf(stdout, "Empty archive (%d files)\n", viv_header.count_dir_entries);
    return -1;
  }

  for (i = 0; i < count_dir_entries; ++i)
  {
    curr_offset += viv_dir_filename_sizes[i] + 0x08;  /* measure header size */
    *contents_size += viv_dir[i].filesize;  /* Add up archive content filesizes per directory */

    p = &viv_dir[i].offset;
    if ((*(p) < viv_header.header_size) ||
        (*(p) + viv_dir[i].filesize > viv_filesize))
    {
      fprintf(stderr, "Format error (offset out of bounds)\n");
      return -1;
    }
  }

  if (viv_dir[0].offset + *contents_size > viv_filesize)
  {
    fprintf(stderr, "Format error (Viv directory has invalid filesizes)\n");
    return -1;
  }

  if (libnfsviv_strictchecks)
  {
    /* NFS:HS, NFS:PU allow >= truth */
    if (viv_header.count_dir_entries > count_dir_entries)
    {
      fprintf(stderr, "Strict Format error (Viv directory has invalid number of directory entries)\n");
      return -1;
    }
  }

  return 0;
}

static
void PrintInfoVivDirectory(VivDirEntry *viv_dir,
                           int *viv_dir_filename_sizes, const int viv_headersize,
                           const int count_dir_entries,
                           const int viv_contents_size)
{
  int curr_chunk_size;
  int i;

  fprintf(stdout, "Header (unpadded) = %d\n", viv_headersize);
  fprintf(stdout, "Directory Entries (parsed) = %d\n", count_dir_entries);
  fprintf(stdout, "Buffer = %d\n", kLibnfsvivChunkSize);

  if (count_dir_entries > 0)
  {
    fprintf(stdout, "\n"
                    "   id       Offset Gap         Size Len  Name\n"
                    " ---- ------------ --- ------------ ---  -----------------------\n");
    curr_chunk_size = 0;

    fprintf(stdout, " %4d   %10u %3u   %10u %3u  %s\n", 1, viv_dir[0].offset, viv_dir[0].offset - viv_headersize, viv_dir[0].filesize, viv_dir_filename_sizes[0], viv_dir[0].filename);
    for (i = 1; i < count_dir_entries; ++i)
    {
      curr_chunk_size = viv_dir[i].offset - viv_dir[i - 1].offset - viv_dir[i - 1].filesize;
      fprintf(stdout, " %4d   %10u %3u   %10u %3u  %s\n", i + 1, viv_dir[i].offset, curr_chunk_size, viv_dir[i].filesize, viv_dir_filename_sizes[i], viv_dir[i].filename);
    }
    fprintf(stdout, " ---- ------------ --- ------------ ---  -----------------------\n"
                    "        %10u       %10u      %u files\n", viv_dir[count_dir_entries - 1].offset + viv_dir[count_dir_entries - 1].filesize, viv_contents_size, count_dir_entries);
  }
}


int VivExtractFile(VivDirEntry viv_dir, FILE* infile, unsigned char *buffer)
{
  int curr_chunk_size;
  int curr_offset = viv_dir.offset;
  FILE *outfile = fopen((const char *)viv_dir.filename, "wb");

  if (!outfile)
  {
    fprintf(stdout, "Cannot create output file");
    fclose(outfile);
    return -1;
  }

  fseek(infile, viv_dir.offset, SEEK_SET);

#if 0
  fprintf(stderr, "VivExtractFile %27s %ld\n", (const char *)viv_dir.filename, ftell(infile));
#endif

  while(curr_offset < viv_dir.offset + viv_dir.filesize)
  {
    curr_chunk_size = Min(kLibnfsvivChunkSize, viv_dir.offset + viv_dir.filesize - curr_offset);

#if 0
    fprintf(stderr, "ofs %10d  buf %d\n", curr_offset, curr_chunk_size);
#endif

    if (FileRead(infile, buffer, (size_t)curr_chunk_size) != curr_chunk_size)
    {
      fprintf(stderr, "File read error (archive)\n");
      fclose(outfile);
      return -1;
    }

    if (fwrite(buffer, 1, (size_t)curr_chunk_size, outfile) != (size_t)curr_chunk_size)
    {
      fprintf(stderr, "File write error (output)\n");
      fclose(outfile);
      return -1;
    }

    curr_offset += curr_chunk_size;
  }

  fclose(outfile);

  return 0;
}


/* Assumes (viv_name). Overwrites in directory 'outpath', if it exists */
int Unviv(const char *viv_name, const char *outpath)
{
  FILE *infile;
  unsigned char buffer[kLibnfsvivChunkSize];
  VivHeader viv_header;
  VivDirEntry *viv_dir;
  int *viv_dir_filename_sizes;
  int viv_filesize;       /* true value */
  int viv_headersize;     /* true value */
  int count_dir_entries;  /* true value */
  int viv_contents_size;  /* cumulated per header */
  int i;

  fprintf(stdout, "Reading archive: %s\n", viv_name);


  /* Get Viv header */
  infile = fopen(viv_name, "rb");
  if (!infile)
  {
    fprintf(stderr, "File '%s' not found\n", viv_name);
    return -1;
  }

  viv_filesize = GetFilesize(infile);
  if (FileRead(infile, &viv_header, 16) != 16)
  {
    fclose(infile);
    fprintf(stderr, "File read error (header)\n");
    return -1;
  }
  fclose(infile);

  viv_header.filesize          = SwapEndian(viv_header.filesize);  /* verbose, and strict checks */
  viv_header.count_dir_entries = SwapEndian(viv_header.count_dir_entries);
  viv_header.header_size       = SwapEndian(viv_header.header_size);

  fprintf(stdout, "Archive Size = %d\n", viv_filesize);
  if (libnfsviv_verbose)
  {
    fprintf(stdout, "Archive Size (per header) = %d\n", viv_header.filesize);
    fprintf(stdout, "Directory Entries (per header) = %d\n", viv_header.count_dir_entries);
    fprintf(stdout, "Header (per header) = %d\n", viv_header.header_size);
  }

  if (!libnfsviv_nochecks)
  {
    if(CheckVivHeader(viv_header, viv_filesize))
      return -1;
  }
#if 1
  else
  {
    /* avoid gigabytes of allocation */
    if (10 * viv_header.count_dir_entries >= viv_filesize)
    {
      fprintf(stdout, "Invalid file\n");
      return -1;
    }
  }
#endif


  /* Get Viv directory */
  viv_dir = (VivDirEntry *)malloc(sizeof(*viv_dir) * viv_header.count_dir_entries);
  viv_dir_filename_sizes = (int *)malloc(4 * viv_header.count_dir_entries);
  if (!viv_dir || !viv_dir_filename_sizes)
  {
    fprintf(stderr, "Not enough memory\n");
    return -1;
  }

  if (libnfsviv_verbose)
  {
    fprintf(stdout, "Header (allocated) = %lu\n",
            sizeof(*viv_dir) * viv_header.count_dir_entries +
            (unsigned long)(4 * viv_header.count_dir_entries + 16));
  }

  count_dir_entries = viv_header.count_dir_entries;

  infile = fopen(viv_name, "rb");
  if (GetVivDirectory(&viv_dir, &viv_dir_filename_sizes,
                      &viv_headersize, &count_dir_entries,
                      (unsigned char *)&buffer, infile,
                      viv_filesize))
  {
    fclose(infile);
    free(viv_dir);
    free(viv_dir_filename_sizes);
    return -1;
  }
  fclose(infile);

  if (!libnfsviv_nochecks)
  {
    if(CheckVivDirectory(&viv_contents_size,
                         viv_header, viv_dir, viv_dir_filename_sizes,
                         viv_filesize, count_dir_entries))
    {
      free(viv_dir);
      free(viv_dir_filename_sizes);
      return -1;
    }
  }

  if (libnfsviv_verbose)
  {
    PrintInfoVivDirectory(viv_dir, viv_dir_filename_sizes,
                          viv_headersize, count_dir_entries, viv_contents_size);
  }


  /* End dry run */
  if (libnfsviv_dryrun)
  {
    free(viv_dir);
    free(viv_dir_filename_sizes);
    return 0;
  }


  /* Extract archive */
  infile = fopen(viv_name, "rb");

  if (chdir(outpath) != 0)
  {
    fprintf(stderr, "Cannot access directory '%s'\n", outpath);
    fclose(infile);
    free(viv_dir);
    free(viv_dir_filename_sizes);
    return -1;
  }

  for (i = 0; i < count_dir_entries; ++i)
  {
    if (VivExtractFile(viv_dir[i], infile, (unsigned char *)&buffer))
      break;
  }

  fclose(infile);

  free(viv_dir);
  free(viv_dir_filename_sizes);

  return 0;
}


/* Overwrites file 'viv_name', if it exists */
int Viv(const char *viv_name, const char **infiles_paths, const int count_dir_entries)
{
  struct VivHeader *viv_header;
  struct VivDirEntry *viv_dir;
  int *viv_dir_filename_sizes;
  FILE *infile;
  FILE *outfile;
  int viv_filesize;
  int offset;
  int curr_offset_header;
  unsigned char *outbuffer;
  unsigned char *outbuffer_LEGACY;
  char *p;
  char temp[32768];
  int mem[2];
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
  viv_dir = (VivDirEntry *) malloc(sizeof(struct VivDirEntry) * count_dir_entries);
  viv_dir_filename_sizes = (int *) malloc(4 * count_dir_entries);

  memset(viv_dir, '\0', sizeof(struct VivDirEntry) * count_dir_entries);  /* padded at end */
  mem[1] = 0;

  for (i = 0; i < count_dir_entries; ++i)
  {
    sprintf(temp, "%s", infiles_paths[i]);
    p = GetBasename(temp);

    viv_dir_filename_sizes[i] = strlen(p) + 1;  /* count nul */

    if (viv_dir_filename_sizes[i] > kFileNameMaxSize)
    {
      fprintf(stderr, "Name exceeds limit: %s\n", p);
    }

    memcpy(viv_dir[i].filename, p, viv_dir_filename_sizes[i]);

    infile = fopen(infiles_paths[i], "rb");
    viv_dir[i].filesize = GetFilesize(infile);
    fclose(infile);

    if (viv_dir[i].filesize < 0)
    {
      fprintf(stderr, "'%s' is not a file\n", infiles_paths[i]);
      free(viv_dir);
      free(viv_dir_filename_sizes);
      return -1;
    }

    /* dir entry: 4 + 4 + LEN + 1 */
    /* in file: size */
    viv_filesize += 8 + viv_dir_filename_sizes[i] + viv_dir[i].filesize;
    offset += 8 + viv_dir_filename_sizes[i];

    if (libnfsviv_verbose) fprintf(stdout, "'%s' (%d bytes)\n", temp, viv_dir[i].filesize);
  }

  /* Set VIV header */
  viv_header = (VivHeader *)malloc(16);
  memcpy((char *)viv_header->BIGF, "BIGF", 4);

  /* Make viv directory size divisible by 4 ?
   * Sometimes seen in NFS3 (walm), NFSPU (camera.viv), NFSHS. Not seen in MCO. Not necessary for NFS3 etc.
   */
  mem[0] = 0; /* IncrUntilDiv4(offset); */

  viv_header->filesize = viv_filesize + mem[0];
  viv_header->count_dir_entries = count_dir_entries;
  viv_header->header_size = offset + mem[0];

  outbuffer = (unsigned char *)malloc(Max(viv_header->header_size, kLibnfsvivChunkSize));
  if (!outbuffer)
    return PrintFree4Ret(-1, stderr, "Insufficient memory\n", 0, viv_header, viv_dir, viv_dir_filename_sizes);


  viv_header->filesize = SwapEndian(viv_header->filesize);
  viv_header->count_dir_entries = SwapEndian(viv_header->count_dir_entries);
  viv_header->header_size = SwapEndian(viv_header->header_size);

  /* Write to outbuffer_LEGACY */
  outbuffer_LEGACY = (unsigned char *)malloc(viv_filesize);
  if (!outbuffer_LEGACY)
    return PrintFree4Ret(-1, stderr, "Insufficient memory\n", 0, viv_header, viv_dir, viv_dir_filename_sizes);

  memcpy(outbuffer_LEGACY, viv_header, 16);
  curr_offset_header = 16;  /* begin of current directory entry */
  offset = SwapEndian(viv_header->header_size);  /* begin of current file */

  /* Insert file content, insert directory entry */
  for (i = 0; i < count_dir_entries; ++i)
  {
    viv_dir[i].offset = SwapEndian(offset);

    infile = fopen(infiles_paths[i], "rb");
    if (FileRead(infile, outbuffer_LEGACY + offset, viv_dir[i].filesize) != viv_dir[i].filesize)
    {
      fclose(infile);
      fprintf(stdout, "File read error '%s'\n", infiles_paths[i]);
      return PrintFree4Ret(-1, stderr, "", outbuffer_LEGACY, viv_header, viv_dir, viv_dir_filename_sizes);
    }

    offset += viv_dir[i].filesize;

    viv_dir[i].filesize = SwapEndian(viv_dir[i].filesize);
    memcpy(outbuffer_LEGACY + curr_offset_header, &viv_dir[i], viv_dir_filename_sizes[i] + 8);

    curr_offset_header += viv_dir_filename_sizes[i] + 8;
  }

  if (libnfsviv_verbose) fprintf(stdout, "VIV data (%d bytes)\n", viv_filesize);

  /* End dry-run */
  if (libnfsviv_dryrun)
    return PrintFree4Ret(0, stdout, "", outbuffer_LEGACY, viv_header, viv_dir, viv_dir_filename_sizes);

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

  if (fwrite(outbuffer_LEGACY, 1, viv_filesize, outfile) != (size_t) viv_filesize)
  {
    fclose(outfile);
    return PrintFree4Ret(-1, stderr, "File write error.\n", outbuffer_LEGACY, viv_header, viv_dir, viv_dir_filename_sizes);
  }
  fclose(outfile);

  return PrintFree4Ret(0, stdout, "", outbuffer_LEGACY, viv_header, viv_dir, viv_dir_filename_sizes);
}

#endif  /* LIBNFSVIV_H */
