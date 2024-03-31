/* unvivtool.c - VIV/BIG decoder/encoder CLI
   unvivtool Copyright (C) 2020-2024 Benjamin Futasz <https://github.com/bfut>

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

#define UVTVUTF8
#include "./libnfsviv.h"

static
void Usage()
{
  printf("Usage: unvivtool d [<options>...] <path/to/input.viv> <path/to/existing/output_directory>\n"
         "       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...\n"
         "       unvivtool <path/to/input.viv>\n"
         "\n"
         "Commands:\n"
         "  d             Decode and extract files from VIV/BIG archive\n"
         "  e             Encode files in new VIV/BIG archive\n"
         "\n");
  printf("Options:\n"
         "  -dnl #        decode/encode, set fixed Directory eNtry Length (>= 10)\n"
         "  -i #          decode file at 1-based Index #\n"
         "  -f <name>     decode file <name> (cAse-sEnsitivE) from archive, overrides -i\n"
         "  -fh           decode/encode to/from Filenames in Hexadecimal\n"
         "  -fmt <format> encode 'BIGF' (default), 'BIGH' or 'BIG4'\n"
         "  -p            Print archive contents, do not write to disk (dry run)\n");
  printf("  -we           Write re-Encode command to path/to/input.viv.txt (keep files in order)\n"
         "  -v            Print archive contents, verbose\n");
}

int main(int argc, char **argv)
{
  int retv = 0;
  char request_file_name[kLibnfsvivFilenameMaxLen * 4] = {0};
  int request_file_idx = 0;
  int opt_direnlenfixed = 0;
  int opt_filenameshex = 0;
  char opt_requestfmt[5] = "BIGF";  /* viv() only */
  int opt_dryrun = 0;
  int opt_wenccommand = 0;
  int opt_printlvl = 0;
  int count_options = 0;
  char *ptr;
  int i;

  printf("unvivtool " UVTVERS " - Copyright (C) 2020-2024 Benjamin Futasz (GPLv3+)\n\n");

  if (argc < 2)
  {
    Usage();
    return 0;
  }

  /* Get options */
  for (i = 2; i < argc; ++i)
  {
    const int _sz = strlen(argv[i]);
    ptr = argv[i];
    if (*ptr == '-')
    {
      switch (*(++ptr))
      {
        case 'd':
          if (i + 1 < argc && (_sz == 4) && !strncmp(argv[i], "-dnl", 5))  /* fixed directory length (clamped) */
          {
            ++i;

            opt_direnlenfixed = LIBNFSVIV_clamp(strtol(argv[i], NULL, 10), 0, INT_MAX / 100);
            if (opt_direnlenfixed > 0)
            {
              opt_direnlenfixed = LIBNFSVIV_max(opt_direnlenfixed, 10);
              printf("Setting fixed directory entry length: %d (0x%x) (clamped to 0xA,0x%x)\n", opt_direnlenfixed, opt_direnlenfixed, INT_MAX / 100);
            }
            count_options += 2;
          }
          else
          {
            Usage();
            return -1;
          }
          break;

        case 'i':  /* request at index (clamped) */
          if (i + 1 < argc)
          {
            ++i;

            request_file_idx = LIBNFSVIV_clamp(strtol(argv[i], NULL, 10), 0, INT_MAX / 100);
            printf("Requested file at index: %d\n", request_file_idx);
            count_options += 2;
          }
          else
          {
            Usage();
            return -1;
          }
          break;

        case 'f':
          if (i + 1 < argc && (_sz == 2) && !strncmp(argv[i], "-f", 3))  /* request filename */
          {
            ++i;
            if (strlen(argv[i]) + 1 > sizeof(request_file_name))
            {
              fprintf(stderr, "Requested filename too long (max %d): len %d\n", kLibnfsvivFilenameMaxLen, (int)strlen(argv[i]) + 1);
              return -1;
            }
            memcpy(request_file_name, argv[i], LIBNFSVIV_min(strlen(argv[i]) + 1, sizeof(request_file_name) - 1));
            printf("Requested file: %s\n", request_file_name);
            count_options += 2;
          }
          else if (_sz == 3 && !strncmp(argv[i], "-fh", 4))  /* filenames as hex */
          {
            opt_filenameshex = 1;
            ++count_options;
          }
          else if (i + 1 < argc && (_sz == 4) && !strncmp(argv[i], "-fmt", 5))  /* encode: request encoding format */
          {
            ++i;
            if (!strcmp(argv[1], "e"))
            {
              memcpy(opt_requestfmt, argv[i], LIBNFSVIV_min(strlen(argv[i]) + 1, 5));
              if (strlen(argv[i]) + 1 != 5
                  && (strncmp(opt_requestfmt, "BIGF", 5)
                      && strncmp(opt_requestfmt, "BIGH", 5)
                      && strncmp(opt_requestfmt, "BIG4", 5)))
              {
                Usage();
                return -1;
              }
              printf("Requested format: %.4s\n", opt_requestfmt);
            }
            count_options += 2;
          }
          else
          {
            Usage();
            return -1;
          }
          break;

        case 'p':  /* dry run */
          opt_dryrun = 1;
          opt_printlvl = 1;
          ++count_options;
          break;

        case 'v':  /* verbose */
          opt_printlvl = 1;
          ++count_options;
          break;

        case 'w':
          if (_sz == 3 && !strncmp(argv[i], "-we", 4))  /* write re-encode command to input.viv.txt */
          {
            opt_wenccommand = 1;
            ++count_options;
          }
          else
          {
            Usage();
            return -1;
          }
          break;

        default:
          fprintf(stderr, "Ignoring unknown option '%s'\n", ptr - 1);
          ++count_options;
          break;
      }  /* switch */
    }
    else  /* no further options */
      break;
  }  /* for i */

  /* Decoder */
  if (!strcmp(argv[1], "d") && (argc > count_options + 3))
  {
    if (opt_wenccommand && !opt_dryrun)  /* Option: Write re-Encode command to file */
    {
      FILE *file_ = NULL;
      const int size_ = strlen(argv[count_options + 2]);  /* does not count null */
      char buf_[kLibnfsvivFilenameMaxLen * 4] = {0};

      for (;;)
      {
        if (size_ > (int)sizeof(buf_) - 5)
        {
          fprintf(stderr, "Cannot use option '-we' (input path too long)\n");
          break;
        }

        file_ = fopen(argv[count_options + 2], "r");
        if (!file_)
        {
          fprintf(stderr, "Cannot open '%s'\n", argv[count_options + 2]);
          retv = -1;
          break;
        }
        fclose(file_);

        memcpy(buf_, argv[count_options + 2], size_);
        memcpy(buf_ + size_, ".txt", 4);

        file_ = fopen(buf_, "w+");
        if (!file_)
        {
          fprintf(stderr, "Cannot create '%s' (option -we)\n", buf_);
          break;
        }
        printf("Writing re-Encoding command to '%s' (option -we)\n", buf_);

        for (i = 0; i < 2 + count_options; ++i)
        {
          const int _sz = strlen(argv[i]);
          if (_sz == 1 && !strcmp(argv[i], "d"))
            fprintf(file_, "e ");
          else if (_sz == 2)
          {
            if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "-f"))
            {
              ++i;
              continue;
            }
            else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-we"))
              continue;
          }
          else
            fprintf(file_, "%s ", argv[i]);
        }

        fflush(file_);
        fclose(file_);

        break;
      }  /* for (;;) */
    }  /* if (opt_wenccommand) */

    if (retv != 0 ||
        !LIBNFSVIV_Unviv(argv[count_options + 2], argv[count_options + 3],
                         request_file_idx, request_file_name,
                         opt_dryrun, opt_printlvl, opt_direnlenfixed,
                         opt_filenameshex, opt_wenccommand))
    {
      printf("Decoder failed.\n");
      retv = -1;
    }
    else
      printf("Decoder successful.\n");
  }

  /* Encoder */
  else if (!strcmp(argv[1], "e") && (argc > count_options + 3))
  {
    if (!LIBNFSVIV_Viv(argv[count_options + 2],
                       &argv[count_options + 3], argc - count_options - 3,
                       opt_dryrun, opt_printlvl, opt_direnlenfixed,
                       opt_filenameshex, opt_requestfmt))
    {
      printf("Encoder failed.\n");
      retv = -1;
    }
    else
      printf("Encoder successful.\n");
  }

  /* Try Decoder for argv[1], decode to cwd */
  else if (LIBNFSVIV_Exists(argv[1]))
  {
    for (;;)
    {
      int fsize;
      char format[4];
      FILE *file = fopen(argv[1], "rb");

      if (!file)
      {
        fclose(file);
        retv = -1;
        break;
      }

      fsize = LIBNFSVIV_GetFilesize(file);
      if ((fsize > 0) && (fsize < 0x4))
      {
        fclose(file);
        break;
      }

      if (fread(format, 1, 4, file) != 4)
      {
        fclose(file);
        fprintf(stderr, "cli: File read error (cli)\n");
        retv = -1;
        break;
      }

      fclose(file);

      if (retv != 0 ||
          !LIBNFSVIV_Unviv(argv[1], ".",
                           request_file_idx, request_file_name,
                           opt_dryrun, opt_printlvl, opt_direnlenfixed,
                           opt_filenameshex, opt_wenccommand))
      {
        printf("Decoder failed.\n");
        retv = -1;
      }
      else
        printf("Decoder successful.\n");

      break;
    }
  }

  /* Print usage */
  else
  {
    Usage();
    retv = -1;
  }

  return retv;
}
