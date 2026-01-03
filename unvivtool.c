/*
  unvivtool.c - BIGF BIGH BIG4 0xFBC0 decoder/encoder CLI (commonly known as VIV/BIG)
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>  /* GetModuleFileNameA */
#endif

#ifndef SCL_DEBUG
#define SCL_DEBUG 0
#endif
#define UVTUTF8
#define UVTWWWW
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
         "  -x           decode/encode to/from filenames in base16/heXadecimal\n"
         "  -alf<N>      encoder ALigns File offsets to <N> (allows 0, 2, 4, 8, 16)\n");
  printf("  -fmt<format> encode to Format 'BIGF' (default), 'BIGH', 'BIG4', 'C0FB' or 'wwww' (w/o quotes)\n"
         "  -p           Print archive contents, do not write to disk (dry run)\n"
         "  -we          Write re-Encode command to path/to/input.viv.txt (keep files in order)\n"
         "  -v           print archive contents, Verbose\n");
  fflush(stdout);
}

#ifdef _WIN32
/* Should be safe enough with sz >= 256 * 4 */
static
void UVT_GetExePath(char *buf, const size_t sz)
{
  if (GetModuleFileName(NULL, buf, sz) > 1)
    LIBNFSVIV_BkwdToFwdSlash(buf);
}
#else
/* Assumes sizeof(buf) >= 4096 */
static
void UVT_GetExePath(char *buf)
{
  char *ptr = realpath("/proc/self/exe", buf);
  if (!ptr)  buf[0] = '\0';
}
#endif

static
int UVT_CreateWENCFile(const int argc, char **argv, const char *viv_name)
{
  char buf_[LIBNFSVIV_FilenameMaxLen];
  if (LIBNFSVIV_GetWENCPath(viv_name, buf_, sizeof(buf_)))
  {
    FILE *file_ = fopen(buf_, "w");
    if (file_)
    {
      printf("Writing re-Encoding command to '%s' (option -we)\n", buf_);

      buf_[0] = '\0';
      #ifdef _WIN32
      UVT_GetExePath(buf_, sizeof(buf_));
      #else
      UVT_GetExePath(buf_);
      #endif
      if (!!buf_[0])
      {
        int i;
        fprintf(file_, "%s ", buf_);
        fprintf(file_, "e ");
        for (i = 2; i < argc; i++)
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
      fclose(file_);
      return 0;
    }  /* if (file_) */
    else  fprintf(stderr, "Cannot create '%s' (option -we)\n", buf_);
  }  /* if (LIBNFSVIV_GetWENCPath ... */
  else  fprintf(stderr, "Cannot use option '-we'\n");
  return -1;
}

static
void UVT_RemoveWENCFile(const char *viv_name)
{
  char buf_[LIBNFSVIV_FilenameMaxLen];
  if (!LIBNFSVIV_GetWENCPath(viv_name, buf_, sizeof(buf_)) || remove(buf_) != 0)
    fprintf(stderr, "Cannot remove re-Encoding file\n");
}

int main(int argc, char **argv)
{
  int retv = 0;
  int i;
  char viv_name[LIBNFSVIV_FilenameMaxLen] = {0};  /* init's to {0} */
  char *out_dir = NULL;
  char **infiles_paths = NULL;
  size_t infiles_paths_sz = 0;
  int count_infiles = 0;
  char *request_file_name = NULL;
  int request_file_idx = 0;
  UVT_UnvivVivOpt opt;
  memset(&opt, 0, sizeof(opt));
  opt.requestendian = 0xE;
  memcpy(opt.requestfmt, "BIGF", 5);  /* Viv() only */

  SCL_assert(sizeof(UVT_DirEntr) == 16);
  SCL_assert(sizeof(UVT_Directory) == 64);

  printf("unvivtool " UVTVERS " - " UVTCOPYRIGHT " "
  #ifdef __cplusplus
         "|C++"
  #endif
  #ifndef UVTUTF8
         "|no-UTF8"
  #endif
  #ifndef UVTWWWW
         "|no-wwww"
  #endif
  #if SCL_DEBUG > 0
         "|debug"
  #endif
         "\n\n");

  if (argc < 2)
  {
    Usage();
    return 0;
  }

  /** Drag-and-drop mode
    always: argv[1] used to copy/derive viv_name

    decode if argv[1] has viv/big bytes
      needs viv_name
      malloc's and gets out_dir as parent_dir of viv_name
    else encode **argv to argv[1].viv
      needs infiles_paths
      gets viv_name from argv[1] with appended extension ".viv"
    no options
  */
  if (strlen(argv[1]) > 1)
  {
    char *viv_name_ptr = (char *)LIBNFSVIV_memccpy(viv_name, argv[1], '\0', sizeof(viv_name) - sizeof(".viv"));  /* leave 5 bytes for ".viv" */

    if (viv_name_ptr && /* LIBNFSVIV_IsFile(viv_name) && */ LIBNFSVIV_GetVivVersion_FromPath(viv_name) > 0)  /* decoder */
    {
      out_dir = (char *)calloc(LIBNFSVIV_FilenameMaxLen * sizeof(*out_dir), 1);
      if (out_dir && LIBNFSVIV_memccpy(out_dir, argv[1], '\0', LIBNFSVIV_FilenameMaxLen))
      {
        LIBNFSVIV_GetParentDir(out_dir);
      }
      else
      {
        fprintf(stderr, "unvivtool: Cannot get parent directory of input file\n");
        retv = -1;
      }
    }
    else if (viv_name_ptr && LIBNFSVIV_IsFile(viv_name) && !LIBNFSVIV_IsDir(viv_name))  /* encoder */
    {
      if (!LIBNFSVIV_memccpy(viv_name_ptr, ".viv", '\0', sizeof(".viv")))
      {
        fprintf(stderr, "unvivtool: Cannot append extension '.viv' to '%s'\n", viv_name);
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
      opt.weenccommand and no dry-run: create file
  */
  else if (argc >= 3 && (argv[1][0] == 'd' || argv[1][0] == 'e'))
  {
    for (i = 2; i < argc; i++)
    {
      if (argv[i][0] != '-' && LIBNFSVIV_memccpy(viv_name, argv[i], '\0', sizeof(viv_name)))  /* look for viv_name in args, skipping options */
        break;
    }
    if (viv_name[0] == '\0')  { Usage(); retv = -1; }

    /* Decode: get output directory */
    if (retv == 0 && argv[1][0] == 'd')
    {
      out_dir = (char *)malloc(LIBNFSVIV_FilenameMaxLen * sizeof(*out_dir));
      if (out_dir)
      {
        out_dir[0] = '\0';
        for (i = 2; i < argc; i++)  /* look for out_dir in args, but avoid viv_name */
        {
          if (argv[i][0] != '-' && strcmp(argv[i], viv_name) && LIBNFSVIV_memccpy(out_dir, argv[i], '\0', LIBNFSVIV_FilenameMaxLen))
            break;
        }
        if (out_dir[0] == '\0' && LIBNFSVIV_memccpy(out_dir, viv_name, '\0', LIBNFSVIV_FilenameMaxLen))  /*out_dir as parent dir of viv_name */
        {
          LIBNFSVIV_GetParentDir(out_dir);
        }
      }
      else  { fprintf(stderr, "unvivtool: Memory allocation failed.\n"); retv = -1; }
    }  /* if 'd' */
    /* Encode: get input files paths */
    else if (retv == 0 && argv[1][0] == 'e')
    {
      infiles_paths = (char **)calloc((argc - 3) * sizeof(*infiles_paths), 1);
      infiles_paths_sz = (argc - 3) * sizeof(*infiles_paths);
      if (infiles_paths)
      {
        count_infiles = 0;
        for (i = 3; i < argc; i++)
        {
          if (argv[i][0] != '-' && strcmp(argv[i], viv_name))
          {
            infiles_paths[count_infiles] = argv[i];
            ++count_infiles;
          }
        }
      }
      else  { fprintf(stderr, "unvivtool: Memory allocation failed.\n"); retv = -1; }
    }  /* if 'e' */

    /** Get options
    */
    for (i = 2; i < argc && retv == 0; i++)
    {
      if (argv[i][0] == '-')
      {
        const int sz = (int)strlen(argv[i]);
        if (sz > 2)  /* only consider sufficiently long candidates */
        {
          char *ptr = argv[i];
          if (sz > 4 && !strncmp(argv[i], "-dnl", 4))  /* fixed directory length (clamped) */
          {
            ptr += 4;
            opt.direnlenfixed = (int)strtol(ptr, NULL, 10);
            opt.direnlenfixed = LIBNFSVIV_Clamp_opt_direnlenfixed(opt.direnlenfixed, 1);
          }
          else if (/* sz > 2 && */ !request_file_name && !strncmp(argv[i], "-i", 2))  /* decode: request filename (overrides file idx request) */
          {
            ptr += 2;
            request_file_idx = LIBNFSVIV_clamp(strtol(ptr, NULL, 10), 0, INT_MAX / 100);  /* 1-based index, 0 means no file requested */
            if (request_file_idx > 0)  printf("Requested file at index: %d\n", request_file_idx);
          }
          else if (/* sz > 2 && */ argv[1][0] == 'd' && !strncmp(argv[i], "-f", 2))  /* decode: request filename (overrides file idx request) */
          {
            int ptr_len;
            ptr += 2;
            ptr_len = LIBNFSVIV_IsPrintString(ptr, sz - 2);
            if ((ptr_len != sz - 2) || (ptr_len + 1 > LIBNFSVIV_FilenameMaxLen / 2))
            {
              fprintf(stderr, "unvivtool: Requested filename is invalid (max %d): len %d\n", LIBNFSVIV_FilenameMaxLen / 2, ptr_len + 1);
              retv = -1;
              break;
            }
            request_file_name = (char *)malloc((ptr_len + 1) * sizeof(*request_file_name));
            if (!request_file_name)  { retv = -1; fprintf(stderr, "unvivtool: Memory allocation failed.\n"); break; }
            memcpy(request_file_name, ptr, ptr_len + 1);
            printf("Requested file: %s\n", request_file_name);
            if (request_file_idx > 0)  printf("Overriding requested file index: %d\n", request_file_idx);
            request_file_idx = 0;  /* override option -i */
          }
          else if (sz >= 4 && argv[1][0] == 'e' && !strncmp(argv[i], "-fmt", 4))  /* encode: request encoding format */
          {
            ptr += 4;
            LIBNFSVIV_memccpy(opt.requestfmt, ptr, '\0', sizeof(opt.requestfmt));
            opt.requestfmt[4] = '\0';
            if (!memcmp(opt.requestfmt, "C0FB", 4))  SCL_seri_uint(opt.requestfmt, 0x8000FBC0);
            if (LIBNFSVIV_GetVivVersion_FromBuf(opt.requestfmt) > 0)
              printf("Requested format: %.4s\n", SCL_deseri_uint(opt.requestfmt) != 0x8000FBC0 ? opt.requestfmt : "C0FB");
            else
            {
              Usage();
              retv = -1;
              break;
            }
          }
          else if (sz >= 4 && argv[1][0] == 'e' && !strncmp(argv[i], "-alf", 4))  /* encode: request file offsets */
          {
            ptr += 4;
            opt.alignfofs = (int)strtol(ptr, NULL, 10);
          }
        }  /* (sz > 2) */
        if (!strcmp(argv[i], "-aot"))  { opt.overwrite = 1; }
        else if (!strcmp(argv[i], "-x"))  { opt.filenameshex = 1; }
        else if (!strcmp(argv[i], "-p"))  { opt.dryrun = 1; opt.verbose = 1; }
        else if (!strcmp(argv[i], "-v"))  { opt.verbose = 1; }
        else if (!strcmp(argv[i], "-we"))  { opt.wenccommand = 1; }
        else  continue;
      }  /* if (argv[i][0] == '-') */
    }  /* Get options: for i */
  }  /* command mode */

  if (retv == 0 && argv[1][0] == 'd' && opt.wenccommand && !opt.dryrun)
    retv = UVT_CreateWENCFile(argc, argv, viv_name);

  /** Decoder
  */
  if (retv == 0 && out_dir)
  {
    opt.filenameshex = LIBNFSVIV_Fix_opt_filenameshex(opt.filenameshex, opt.direnlenfixed);
    if (!LIBNFSVIV_Unviv(viv_name, out_dir, request_file_idx, request_file_name, &opt))
    {
      printf("Decoder failed.\n");
      retv = -1;
      if (opt.wenccommand && !opt.dryrun)
        UVT_RemoveWENCFile(viv_name);  /* cleanup */
    }
    else
      printf("Decoder successful.\n");
  }

  /** Encoder
  */
  else if (retv == 0 && infiles_paths)
  {
    opt.filenameshex = LIBNFSVIV_Fix_opt_filenameshex(opt.filenameshex, opt.direnlenfixed);
    if (!LIBNFSVIV_Viv(viv_name, infiles_paths, count_infiles, &opt))
    {
      printf("Encoder failed.\n");
      retv = -1;
    }
    else
      printf("Encoder successful.\n");
  }

  /* Print usage */
  else if (retv == 0)
  {
    Usage();
    retv = -1;
  }

  free(out_dir);
  if (infiles_paths_sz > 0)  free(infiles_paths);  /* only malloc'd if (infiles_paths_sz > 0) */
  free(request_file_name);

  return retv;
}
