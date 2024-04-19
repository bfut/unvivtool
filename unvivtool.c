/*
  unvivtool.c - VIV/BIG decoder/encoder CLI
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>  /* GetModuleFileNameA */
#endif

#define UVTUTF8
#include "./libnfsviv.h"

static
void Usage(void)
{
  printf("Usage: unvivtool d [<options>...] <path/to/input.viv> [<path/to/output_directory>]\n"
         "       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...\n"
         "       unvivtool <path/to/input.viv>\n"
         "       unvivtool <paths/to/input_files>...\n"
         "\n");
  printf("Commands:\n"
         "  d            Decode and extract files from VIV/BIG archive\n"
         "  e            Encode files in new VIV/BIG archive\n"
         "\n");
  printf("Options:\n"
         "  -aot         decoder Overwrite mode: auto rename existing file\n"
         "  -dnl<N>      decode/encode, set fixed Directory eNtry Length (<N> >= 10)\n"
         "  -i<N>        decode file at 1-based Index <N>\n"
         "  -f<name>     decode File <name> (cAse-sEnsitivE) from archive, overrides -i\n"
         "  -fh          decode/encode to/from Filenames in base16/Hexadecimal\n"
         "  -fmt<format> encode to Format \"BIGF\" (default), \"BIGH\" or \"BIG4\" (w/o quotes)\n"
         "  -p           Print archive contents, do not write to disk (dry run)\n");
  printf("  -we          Write re-Encode command to path/to/input.viv.txt (keep files in order)\n"
         "  -v           print archive contents, Verbose\n");
  fflush(stdout);
}

int UVT_GetVivVersionFile(FILE *file)
{
  char buf[4];
  if (fread(buf, 1, 4, file) != 4)
    return 0;
  if (strncmp(buf, "BIG4", 4) == 0)
    return 4;
  if (strncmp(buf, "BIGF", 4) == 0)
    return 7;
  if (strncmp(buf, "BIGH", 4) == 0)
    return 8;
  return -1;
}

/* Returns 7 (BIGF), 8 (BIGH), 4 (BIG4), negative (unknown format), 0 (fread error) */
int UVT_GetVivVersion(const char *path)
{
  int retv = 0;
  FILE *file = fopen(path, "rb");
  if (file)
  {
    retv = UVT_GetVivVersionFile(file);
    fclose(file);
  }
  return retv;
}

#ifdef _WIN32
/* Should be safe enough with sz >= 260 * 4 */
void UVT_GetExePath(char *buf, const size_t sz)
{
  if (GetModuleFileName(NULL, buf, sz) > 1)
    LIBNFSVIV_BkwdToFwdSlash(buf);
}
#else
/* gcc -std=c89 requires sizeof(buf) >= 4096 to avoid buffer overflow */
void UVT_GetExePath(char *buf)
{
  char *ptr = realpath("/proc/self/exe", buf);
  if (!ptr)  buf[0] = '\0';
}
#endif

void CreateWENCFile(int *retv, const int argc, char **argv, const char *viv_name)
{
  char buf_[kLibnfsvivFilenameMaxLen] = {0};
  FILE *file_ = NULL;
  int i;
  memcpy(buf_, viv_name, LIBNFSVIV_min(strlen(viv_name), sizeof(buf_) - 1));
  if (!LIBNFSVIV_AppendFileEnding(buf_, sizeof(buf_), ".txt"))
  {
    fprintf(stderr, "Cannot use option '-we'\n");
    *retv = -1;
    return;
  }
  file_ = fopen(buf_, "w");
  if (!file_)
  {
    fprintf(stderr, "Cannot create '%s' (option -we)\n", buf_);
    *retv = -1;
    return;
  }
  printf("Writing re-Encoding command to '%s' (option -we)\n", buf_);
  buf_[0] = '\0';
  #ifdef _WIN32
  UVT_GetExePath(buf_, sizeof(buf_));
  #else
  UVT_GetExePath(buf_);
  #endif
  if (buf_[0])
  {
    fprintf(file_, "%s ", buf_);
    fprintf(file_, "e ");
    for (i = 2; i < argc; ++i)
    {
      const size_t sz = strlen(argv[i]);
      if (argv[i][0] == '-'
          && strcmp(argv[i], "-we")
          && (sz > 2 && (strncmp(argv[i], "-i", 2) || strncmp(argv[i], "-f", 2)))
          && (sz > 4 && (strncmp(argv[i], "-fmt", 4))))
        fprintf(file_, "%s ", argv[i]);
    }
    fflush(file_);
  }
  else  *retv = -1;
  fclose(file_);
}

int main(int argc, char **argv)
{
  int retv = 0;
  char viv_name[kLibnfsvivFilenameMaxLen] = {0};
  char *out_dir = NULL;
  char **infiles_paths = NULL;
  size_t infiles_paths_sz = 0;
  int count_infiles = 0;
  char *request_file_name = NULL;
  int request_file_idx = 0;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  char opt_requestfmt[5] = "BIGF";  /* viv() only */
  int opt_dryrun = 0;
  int opt_wenccommand = 0;
  int opt_printlvl = 0;
  int opt_overwrite = 0;
  int i;

#ifdef UVTUTF8
  printf("unvivtool " UVTVERS " - " UVTCOPYRIGHT "\n\n");
#else
  printf("unvivtool " UVTVERS " - " UVTCOPYRIGHT " | no-UTF8\n\n");
#endif

  if (argc < 2)
  {
    Usage();
    return 0;
  }

  /** Drag-and-drop mode
    always: argv[1] used to copy/derive viv_name

    decode if argv[1] has viv/big bytes
      needs viv_name
      mallocs and gets out_dir as parent_dir of viv_name
    else encode **argv to argv[1].viv
      needs infiles_paths
      gets viv_name from argv[1] with appended extension ".viv "
    no options
   */
  if (strlen(argv[1]) > 1)
  {
    memcpy(viv_name, argv[1], LIBNFSVIV_min(strlen(argv[1]), sizeof(viv_name) - 6));  /* leave 5 bytes for ".viv" */

    if (/* LIBNFSVIV_IsFile(viv_name) && */ UVT_GetVivVersion(viv_name) > 0)  /* decoder */
    {
      out_dir = (char *)calloc(kLibnfsvivFilenameMaxLen * sizeof(*out_dir), 1);
      if (!out_dir)
      {
        fprintf(stderr, "unvivtool: Memory allocation failed.\n");
        retv = -1;
      }
      else
      {
        memcpy(out_dir, argv[1], LIBNFSVIV_min(strlen(argv[1]), kLibnfsvivFilenameMaxLen - 1));
        LIBNFSVIV_GetParentDir(out_dir);
      }
    }
    else if (LIBNFSVIV_IsFile(viv_name) && !LIBNFSVIV_IsDir(viv_name))  /* encoder */
    {
      if (!LIBNFSVIV_AppendFileEnding(viv_name, sizeof(viv_name), ".viv"))
      {
        fprintf(stderr, "Unviv: Cannot append extension '.viv' to '%s'\n", viv_name);
        retv = -1;
      }
      infiles_paths_sz = 0;  /* not malloc'd */
      infiles_paths = &argv[1];
      count_infiles = argc - 1;
    }
    else
    {
      fprintf(stderr, "unvivtool: Invalid file or directory: '%s' (== '%s')\n", argv[1], viv_name);
      Usage();
      retv = -1;
    }
  }
  /** Command mode
    strlen(argv[1]) == 1
    argv[1] == 'd' || 'e'
    argv[i>=2] == viv_name
    argv[i>=3] == out_dir or infiles_paths
    and options

    decode:
      needs viv_name
      gets out_dir from args or as parent dir of viv_name
      opt_weenccommand and no dry-run: create file
  */
  else if (argc >= 3 && (argv[1][0] == 'd' || argv[1][0] == 'e'))
  {
    for (i = 2; i < argc; ++i)
    {
      if (argv[i][0] != '-')
      {
        memcpy(viv_name, argv[i], LIBNFSVIV_min(strlen(argv[i]), sizeof(viv_name) - 1));
        break;
      }
    }
    if (viv_name[0] == '\0')  { Usage(); retv = -1; }

    /* Decode: get output directory */
    if (retv == 0 && argv[1][0] == 'd')
    {
      out_dir = (char *)calloc(kLibnfsvivFilenameMaxLen * sizeof(*out_dir), 1);
      if (!out_dir)  { fprintf(stderr, "unvivtool: Memory allocation failed.\n"); retv = -1; }
      else
      {
        for (i = 2; i < argc; ++i)  /* out_dir from args */
        {
          if (argv[i][0] != '-' && strcmp(argv[i], viv_name))
          {
            memcpy(out_dir, argv[i], LIBNFSVIV_min(strlen(argv[i]), kLibnfsvivFilenameMaxLen - 1));
            break;
          }
        }
        if (out_dir[0] == '\0')  /*out_dir as parent dir of viv_name */
        {
          memcpy(out_dir, viv_name, strlen(viv_name) + 1);
          LIBNFSVIV_GetParentDir(out_dir);
        }
      }
    }  /* if 'd' */
    /* Encode: get input files paths */
    else if (retv == 0 && argv[1][0] == 'e')
    {
      infiles_paths = (char **)calloc((argc - 3) * sizeof(*infiles_paths), 1);
      infiles_paths_sz = (argc - 3) * sizeof(*infiles_paths);
      if (!infiles_paths)  { fprintf(stderr, "unvivtool: Memory allocation failed.\n"); retv = -1; }
      else
      {
        count_infiles = 0;
        for (i = 3; i < argc; ++i)
        {
          if (argv[i][0] != '-' && strcmp(argv[i], viv_name))
          {
            infiles_paths[count_infiles] = argv[i];
            ++count_infiles;
          }
        }
      }
    }  /* if 'e' */

    /** Get options
     */
    for (i = 2; i < argc && retv == 0; ++i)
    {
      if (argv[i][0] == '-')
      {
        const size_t sz = strlen(argv[i]);
        if (sz > 2)  /* only consider sufficiently long candidates */
        {
          const char *ptr = argv[i];
          if (sz > 4 && !strncmp(argv[i], "-dnl", 4))  /* fixed directory length (clamped) */
          {
            ptr += 4;
            opt_direnlenfixed = LIBNFSVIV_clamp(strtol(ptr, NULL, 10), 0, INT_MAX / 100);
            if (opt_direnlenfixed > 0)
            {
              opt_direnlenfixed = LIBNFSVIV_max(opt_direnlenfixed, 10);
              printf("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)\n", opt_direnlenfixed, opt_direnlenfixed, INT_MAX / 100);
            }
          }
          else if (/* sz > 2 && */ !request_file_name && !strncmp(argv[i], "-i", 2))
          {
            ptr += 2;
            request_file_idx = LIBNFSVIV_clamp(strtol(ptr, NULL, 10), 0, INT_MAX / 100);  /* 1-based index, 0 means no file requested */
            if (request_file_idx > 0)  printf("Requested file at index: %d\n", request_file_idx);
          }
          else if (/* sz > 2 && */ argv[1][0] == 'd' && !strncmp(argv[i], "-f", 2))  /* decode: request filename (overrides file idx request) */
          {
            ptr += 2;
            if (strlen(ptr) + 1 > kLibnfsvivFilenameMaxLen / 2)
            {
              fprintf(stderr, "Requested filename too long (max %d): len %d\n", kLibnfsvivFilenameMaxLen / 2, (int)strlen(ptr) + 1);
              retv = -1;
              break;
            }
            request_file_name = (char *)malloc((strlen(ptr) + 1) * sizeof(*request_file_name));
            if (!request_file_name)  { retv = -1; fprintf(stderr, "unvivtool: Memory allocation failed.\n"); break; }
            memcpy(request_file_name, ptr, strlen(ptr) + 1);
            printf("Requested file: %s\n", request_file_name);
            if (request_file_idx > 0)  printf("Overriding requested file index: %d\n", request_file_idx);
            request_file_idx = 0;  /* override option -i */
          }
          else if (sz >= 4 && argv[1][0] == 'e' && !strncmp(argv[i], "-fmt", 4))  /* encode: request encoding format */
          {
            ptr += 4;
            memcpy(opt_requestfmt, ptr, LIBNFSVIV_min(strlen(ptr) + 1, 5));
            if (strlen(ptr) + 1 != 5
                && (strncmp(opt_requestfmt, "BIGF", 5)
                    && strncmp(opt_requestfmt, "BIGH", 5)
                    && strncmp(opt_requestfmt, "BIG4", 5)))
            {
              Usage();
              retv = -1;
              break;
            }
            printf("Requested format: %.4s\n", opt_requestfmt);
          }
        }  /* (sz > 2) */
        if (!strcmp(argv[i], "-aot"))  { opt_overwrite = 1; }
        else if (!strcmp(argv[i], "-fh"))  { opt_filenameshex = 1; }
        else if (!strcmp(argv[i], "-p"))  { opt_dryrun = 1; opt_printlvl = 1; }
        else if (!strcmp(argv[i], "-v"))  { opt_printlvl = 1; }
        else if (!strcmp(argv[i], "-we"))  { opt_wenccommand = 1; }
        else  continue;
      }  /* if (argv[i][0] == '-') */
    }  /* Get options: for i */
  }  /* command mode */

  if (retv == 0 && argv[1][0] == 'd' && opt_wenccommand && !opt_dryrun)
    CreateWENCFile(&retv, argc, argv, viv_name);

  /** Decoder
   */
  if (retv == 0 && out_dir)
  {
    if (!LIBNFSVIV_Unviv(viv_name, out_dir,
                         request_file_idx, request_file_name,
                         opt_dryrun, opt_printlvl, opt_direnlenfixed,
                         opt_filenameshex, opt_wenccommand, opt_overwrite))
    {
      printf("Decoder failed.\n");
      retv = -1;
    }
    else
      printf("Decoder successful.\n");
  }

  /** Encoder
   */
  else if (retv == 0 && infiles_paths)
  {
    if (!LIBNFSVIV_Viv(viv_name,
                       infiles_paths, count_infiles,
                       opt_dryrun, opt_printlvl, opt_direnlenfixed,
                       opt_filenameshex, opt_requestfmt))
    {
      printf("Encoder failed.\n");
      retv = -1;
    }
    else
      printf("Encoder successful.\n");
  }

  /** Print usage
   */
  else if (retv == 0)
  {
    Usage();
    retv = -1;
  }

  if (out_dir)  free(out_dir);
  if (infiles_paths_sz > 0)  free(infiles_paths);

  return retv;
}
