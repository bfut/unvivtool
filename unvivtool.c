/* unvivtool.c - VIV/BIG decoder/encoder CLI
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
#include <stdio.h>
#include <string.h>

#include "./libnfsviv.h"

static
void Usage(void)
{
  printf("Usage: unvivtool d [<options>...] <path/to/input.viv> <path/to/existing/output_directory>\n"
         "       unvivtool e [<options>...] <path/to/output.viv> <paths/to/input_files>...\n"
         "       unvivtool <path/to/input.viv>\n"
         "\n"
         "Commands:\n"
         "  d             decode and extract files from VIV/BIG archive\n"
         "  e             encode files in new VIV/BIG archive\n"
         "\n");
  printf("Options:\n"
         "  -i #          decode file at 1-based index #\n"
         "  -f <name>     decode file <name> (cAse-sEnsitivE) from archive, overrides -i\n"
         "  -p            print archive contents, do not write to disk (dry run)\n"
/*         "  -q            quiet mode\n" */
         "  -s            decoder strict mode, extra format checks, fail at first unsuccessful extraction\n"
         "  -v            verbose\n");
}

int main(int argc, char **argv)
{
  int retv = 0;
  char request_file_name[kLibnfsvivFilenameMaxLen * 4];
  int request_file_idx = 0;
  int opt_dryrun = 0;
  int opt_strictchecks = 0;
  int opt_printlvl = 0;
  int count_options = 0;
  char *ptr;
  int i;

  printf("unvivtool %s - Copyright (C) 2020-2022 Benjamin Futasz (GPLv3+)\n\n", UVTVERS);

#ifdef UVTDEBUG
  if (!LIBNFSVIV_SanityTest())
  {
    return -1;
  }
#endif

  if (argc < 2)
  {
    Usage();
    return 0;
  }

  memset(request_file_name, '\0', (size_t)(kLibnfsvivFilenameMaxLen * 4));

  /* Get options */
  for (i = 2; i < argc; ++i)
  {
    ptr = argv[i];
    if (*ptr == '-')
    {
      switch (*(++ptr))
      {
        case 'i':  /* request at index */
          ++count_options;
          ++i;

          if (i < argc)
          {
            if (!sscanf(argv[i], "%d", &request_file_idx))
            {
              Usage();
              return -1;
            }
            printf("Requested file at index: %d\n", request_file_idx);
            ++count_options;
          }
          else
          {
            Usage();
            return -1;
          }

          break;

        case 'f':  /* request filename */
          ++count_options;
          ++i;

          if (i < argc)
          {
            if ((int)strlen(argv[i]) + 1 > kLibnfsvivFilenameMaxLen * 4)
            {
              fprintf(stderr,
                      "Requested filename too long (max %d): len %d\n",
                      kLibnfsvivFilenameMaxLen, (int)strlen(argv[i]) + 1);
              return -1;
            }

            if (!sscanf(argv[i], "%1019s", request_file_name))
            {
              Usage();
              return -1;
            }

            printf("Requested file: %s\n", request_file_name);
            ++count_options;
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

        case 'q':  /* quiet mode */
          opt_printlvl = 0;
          ++count_options;
          break;

        case 's':  /* strict */
          opt_strictchecks = 1;
          ++count_options;
          break;

        case 'v':  /* verbose */
          opt_printlvl = 1;
          ++count_options;
          break;

        default:
          fprintf(stderr, "Ignoring unknown option '%s'\n", ptr - 1);
          break;
      }  /* switch */
    }
    else  /* no further options */
      break;
  }  /* for i */

  /* Decoder */
  if (!strcmp(argv[1], "d") && (argc > count_options + 3))
  {
    if (!LIBNFSVIV_Unviv(argv[count_options + 2], argv[count_options + 3],
                         request_file_idx, request_file_name,
                         opt_dryrun, opt_strictchecks, opt_printlvl))
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
                       opt_dryrun, opt_printlvl))
    {
      printf("Encoder failed.\n");
      retv = -1;
    }
    else
      printf("Encoder successful.\n");
  }

  /* Try Decoder for argv[1], decode to cwd */
  else if(LIBNFSVIV_Exists(argv[1]))
  {
    for (;;)
    {
      int fsize;
      char BIGF[4];
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

      if (fread(BIGF, (size_t)1, (size_t)4, file) != (size_t)4)
      {
        fclose(file);
        fprintf(stderr, "cli: File read error (cli)\n");
        retv = -1;
        break;
      }

      if (strncmp(BIGF, "BIGF", (size_t)4) != 0)
      {
        fclose(file);
        printf("cli: Format error (header missing BIGF)\n");
        retv = -1;
        break;
      }

      fclose(file);

      if (!LIBNFSVIV_Unviv(argv[1], ".",
                           request_file_idx, request_file_name,
                           opt_dryrun, opt_strictchecks, opt_printlvl))
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
