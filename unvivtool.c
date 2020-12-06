/*
  unvivtool.c - VIV/BIG decoder/encoder CLI
  unvivtool Copyright (C) 2020 Benjamin Futasz <https://github.com/bfut>

  Portions copyright other contributors, see below.
  You may not redistribute this program without its source code.

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

/**
  unvivtool.c - command-line tool for Windows, and Linux (GPLv3)
  libnfsviv.h - implements VIV/BIG decoding/encoding (zlib License)

  BUILD:
  - Linux:
      gcc -std=c89 -fPIE -fstack-clash-protection -fstack-protector-strong -D_FORTIFY_SOURCE=2 -s -O2 unvivtool.c -o unvivtool
  - Win32: cross-compile on Linux with MinGW
      i686-w64-mingw32-gcc -std=c89 -fstack-clash-protection -s -O2 -Xlinker --no-insert-timestamp unvivtool.c -o unvivtool.exe
 **/

#define UNVIVTOOL

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>  /* isdigit */

#include "libnfsviv.h"

#ifdef _WIN32
#define kUnvivtoolMaxPathLen 245
#else
#define kUnvivtoolMaxPathLen 4086
#endif

void Usage(void)
{
  fprintf(stdout, "\n"
                  "Usage: unvivtool e [<options>...] <output.viv> [<input_files>...]\n"
                  "       unvivtool d [<options>...] <input.viv> [<output_directory>]\n"
                  "\n"
                  "Commands:\n"
                  "  e             encode files in new archive\n"
                  "  d             decode and extract archive\n"
                  "\n");
  fprintf(stdout, "Options:\n"
                  "  -o            overwrite existing\n"
                  "  -p            print archive contents, do not write to disk\n"
                  "  -v            verbose\n"
                  "  -strict       extra format checks\n"
                  "  -fn <name>    decode file <name> (cAse-sEnsitivE) from archive (overrides -id)\n"
                  "  -id #         decode file at index (1-based)\n"
                  "  -fs #         decode single file with filesize (requires either -fn or -id)\n"
                  "  -fofs #       decode file at offset (requires either -fn or -id)\n"
                  );
}

/* Creates and returns new directory name. Increments name, if (increment) */
char *CreateOutfolder(char *name, const int increment)  /* modified from FSHTool Copyright (C) Denis Auroux 1998-2002 (GPLv2) */
{
  int i;
  int j;
  char *p;

  p = GetBasename(name);
  if (!IsSupportedName(p, (int)strlen(p)))
  {
    free(name);
    return 0;
  }

  rmdir(name);
  if (mkdir(name, (unsigned int)0777) != 0)
  {
    if (increment)
    {
      i = (int)strlen(name);
      name[i] = (char)'_';
      name[i + 2] = (char)'\0';

      for (j = 1; j <= 9; ++j)
      {
        name[i + 1] = (char)('0' + j);

        rmdir(name);
        if (mkdir(name, (unsigned int)0777) == 0)
          break;
      }
      if (j == 10)
      {
        fprintf(stderr, "\nCannot create incremented output directory %s ", name);
        name[i] = '\0';
        fprintf(stderr, "while %s already exists\n", name);
        free(name);
        return 0;
      }
    }
    /* else: keep existing */
  }

  return name;
}

/* Assumes extension length 3 */
char *IncrementFilename(char *name, const int i)  /* modified from FSHTool Copyright (C) Denis Auroux 1998-2002 (GPLv2) */
{
  FILE *f;
  int j;

  f = fopen(name, "rb");
  if (!f)
    return name;
  fclose(f);

  memmove(name + i + 2, name + i, (size_t)4);
  name[i] = '_';

  for (j = 1; j <= 9; ++j)
  {
    name[i + 1] = '0' + j;

    f = fopen(name, "rb");
    if (!f)
      break;
    fclose(f);
  }

  if (j == 10)
  {
    free(name);
    fprintf(stderr, "\nIncremented names already exist\n");
    return 0;
  }

  return name;
}

int main(int argc, char **argv)
{
  char *outpath;
  int count_options;
  int length;
  int i;
  int overwrite;
  char *p;
  char *p2;
  char request_file_name[kLibnfsvivFilenameMaxLen];
  int request_file_idx = 0;
  int request_file_size = 0;
  int request_file_offset = 0;

  fprintf(stdout, "=================================================================================\n"
                  "unvivtool 1.0+dev201206 - Copyright (C) 2020 Benjamin Futasz (GPLv3) - 2020-12-06\n\n");

  if (argc < 3)
  {
    Usage();
    return 0;
  }

  if (!SanityTest())
    return -1;

  overwrite = 0;
  libnfsviv_verbose = 0;
  libnfsviv_dryrun = 0;
  libnfsviv_strictchecks = 0;

  count_options = 0;

  memset(request_file_name, 0, (size_t)kLibnfsvivFilenameMaxLen);

  for (i = 2; i < argc; ++i)
  {
    p = argv[i];

    if (!strcmp(argv[i], "-fn"))
    {
      ++count_options;
      ++i;

      if (i < argc)
      {
        if (!sscanf(argv[i], "%s", request_file_name))
        {
          Usage();
          return -1;
        }

        fprintf(stdout, "Requested file: %s\n", request_file_name);
        ++count_options;
      }
      else
      {
        Usage();
        return -1;
      }
    }
    else if (!strcmp(argv[i], "-id"))
    {
      ++count_options;
      ++i;

      if (i < argc)
      {
        if (!sscanf(argv[i], "%d", &request_file_idx))
        {
          Usage();
          return -1;
        }

        fprintf(stdout, "Requested file at index: %d\n", request_file_idx);
        ++count_options;
      }
      else
      {
        Usage();
        return -1;
      }
    }
    else if (!strcmp(argv[i], "-fs"))
    {
      ++count_options;
      ++i;

      if (i < argc)
      {
        if (!sscanf(argv[i], "%d", &request_file_size))
        {
          Usage();
          return -1;
        }
        fprintf(stdout, "Requested file with size: %d\n", request_file_size);
        ++count_options;
      }
      else
      {
        Usage();
        return -1;
      }
    }
    else if (!strcmp(argv[i], "-fofs"))
    {
      ++count_options;
      ++i;

      if (i < argc)
      {
        if (!sscanf(argv[i], "%d", &request_file_offset))
        {
          Usage();
          return -1;
        }
        fprintf(stdout, "Requested file at offset: %d\n", request_file_offset);
        ++count_options;
      }
      else
      {
        Usage();
        return -1;
      }
    }
    else if (!strcmp(argv[i], "-o"))
    {
      overwrite = 1;
      ++count_options;
    }
    else if (!strcmp(argv[i], "-p"))
    {
      fprintf(stderr, "Begin dry run\n");

      libnfsviv_dryrun = 1;
      libnfsviv_verbose = 1;
      ++count_options;
    }
    else if (!strcmp(argv[i], "-strict"))
    {
      libnfsviv_strictchecks = 1;
      ++count_options;
    }
    else if (!strcmp(argv[i], "-v"))
    {
      libnfsviv_verbose = 1;
      ++count_options;
    }
    else if (p[0] == '-')
    {
      fprintf(stderr, "Invalid option '%s'\n", p);
      Usage();
      return -1;
    }
    else if (p[0] == '&' || p[0] == '(' || p[0] == ')' || p[0] == ';' ||
             p[0] == '|' || p[0] == '{' || p[0] == '}' || p[0] == '>' ||
             ((p[0] == '#') && (p[0] == ' ')))
    {
      argc = ++i;
      break;
    }
    else
      break;
  }

  /* e outfile name, d outfolder name */
  p = argv[count_options + 2];
  if (!p)
  {
    Usage();
    return -1;
  }
  length = (int)strlen(p);

    if (!libnfsviv_dryrun)
  {
    if (length < 5)
    {
      fprintf(stderr, "Filename '%s' too short\n", p);
      return 1;
    }
    if (p[length - 4] != '.')
    {
      fprintf(stderr, "Expects filename extension '.viv' or '.big' for %s\n", p);
      return -1;
    }
  }

  /* Encoder */
  if (!strcmp(argv[1], "e") && (argc > count_options + 3))
  {
    libnfsviv_strictchecks = 1;

    /* Set outpath from outfile */
    p = argv[count_options + 2];
    length = (int)strlen(p);

    if (length > kUnvivtoolMaxPathLen)
    {
        fprintf(stderr, "Output path exceeds length allowed by filesystem (%d)\n", length);
        return 1;
    }

    p2 = GetBasename(p);
    if (!IsSupportedName(p2, (int)strlen(p2)))
    {
      fprintf(stderr, "Please specify another output file\n");
      return 1;
    }

    outpath = (char *)malloc((size_t)(length + 3));
    if (!outpath)
    {
      fprintf(stderr, "Not enough memory (%d) main\n", length + 3);
      return -1;
    }

    memset(outpath, 0, (size_t)(length + 3));
    memcpy(outpath, p, (size_t)length);

    if (!overwrite && !libnfsviv_dryrun)
    {
      outpath = IncrementFilename(outpath, length - 4);
      if (!outpath)
      {
        fprintf(stderr, "Please specify another output file or overwrite with option '-o'\n");
        return -1;
      }
    }

    fprintf(stdout, "\n"
                    "Creating archive: %s\n", outpath);

    if (!Viv(outpath, &argv[count_options + 3], argc - count_options - 3))
    {
      fprintf(stdout, "Encoder failed.\n");
      free(outpath);
      return -1;
    }
    fprintf(stdout, "Encoder successful.\n");

    free(outpath);
  }

  /* Decoder */
  else if (!strcmp(argv[1], "d") && (argc > count_options + 2) )
  {
    /* Set outpath from optional outfolder or infile */
    p = argv[count_options + 3];

    if (!p)
      p = argv[count_options + 2];
    else if (!strcmp(p, "."))
      ++p;  /* outpath will be current working dir */

    length = (int)strlen(p);

    if (length == 0)
    {
      length = 220;

      outpath = (char *)malloc((size_t)length);
      if (!outpath)
      {
        fprintf(stderr, "Not enough memory (%d) main\n", length);
        return -1;
      }

      if (!getcwd(outpath, (size_t)length))
      {
        fprintf(stderr, "Cannot get current working directory\n");
        return -1;
      }
    }
    else
    {
      if (length > kUnvivtoolMaxPathLen)
      {
          fprintf(stderr, "Output path exceeds allowed length (%d)\n", length);
          return 1;
      }

      outpath = (char *)malloc((size_t)(length + 3));  /* + 3 for increment + nul */
      if (!outpath)
      {
        fprintf(stderr, "Not enough memory (%d) main\n", length + 3);
        return -1;
      }

      memset(outpath, 0, (size_t)(length + 3));
      strcpy(outpath, p);

      if (!argv[count_options + 3])
        outpath[length - 4] = '_';

      if (!libnfsviv_dryrun)
      {
        outpath = CreateOutfolder(outpath, !overwrite);
        if (!outpath)
        {
          fprintf(stderr, "Please specify a supported output directory or overwrite with option '-o'\n");
          return -1;
        }
      }
    }

    fprintf(stdout, "\n"
                    "Archive: %s\n", argv[count_options + 2]);
    fprintf(stdout, "Extracting to: %s\n", outpath);

    if (Unviv(argv[count_options + 2], outpath,
              request_file_idx, request_file_name,
              request_file_size, request_file_offset))
    {
      fprintf(stdout, "Decoder failed.\n");
      rmdir(outpath);  /* Try cleaning up. */
      free(outpath);
      return -1;
    }
    free(outpath);

    fprintf(stdout, "Decoder successful.\n");
  }

  /*   */
  else
    Usage();

  return 0;
}