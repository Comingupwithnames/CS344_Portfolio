/* This file serves as a testbench for your library. You do not 
 * need to modify it.
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "libtree.h"

int
main(int argc, char *argv[])
{
  struct tree_options opts = {.indent = 2, .sort = ALPHA};
  char const *optstring = "+adpugsrtUhi:";
  for (char c; (c = getopt(argc, argv, optstring)) != -1;) {
    switch (c) {
      case 'a':
        opts.all = true;
        break;
      case 'd':
        opts.dirsonly = true;
        break;
      case 'p':
        opts.perms = true;
        break;
      case 'u':
        opts.user = true;
        break;
      case 'g':
        opts.group = true;
        break;
      case 's':
        opts.size = true;
        break;
      case 'r':
        opts.sort = RALPHA;
        break;
      case 't':
        opts.sort = TIME;
        break;
      case 'U':
        opts.sort = NONE;
        break;
      case 'i': {
        char *end = optarg;
        long int i = strtol(optarg, &end, 10);
        if (*optarg != '\0' && *end == '\0')
          opts.indent = i;
        else
          err(errno = EINVAL, "%s", optarg);
        break;
      }
      case 'h':
        fprintf(stderr, 
            "%s [OPTION]... [DIRECTORY]...\n\n"
            "Recursively list all files found in DIRECTORYs. If no DIRECTORY is provided, the current directory is used.\n"
            "By default, output is sorted alphabetically.\n"
            "\n"
            "LISTING OPTIONS\n"
            "  -a    Print hidden files. By default, tree does not print hidden files beginning with a dot ('.') character.\n"
            "        The filesystem constructs `.' and `..' are never printed even with the -a option.\n"
            "  -d    Print only directories, no files.\n"
            "\n"
            "FILE OPTIONS\n"
            "  -p    Print permissions according to the mode string format specified for `ls' according to POSIX.\n"
            "  -u    Print the username, or UID # if no username is available, of the file.\n"
            "  -g    Print the group name, or GID # if no group name is available, of the file.\n"
            "  -s    Print the size of each file in bytes.\n"
            "\n"
            "SORTING OPTIONS (default: alphabetic sorting)\n"
            "  -r    Sort the output in reverse alphabetic order.\n"
            "  -t    Sort the output by last modification time instead of alphabetically.\n"
            "  -U    Do not sort. List files according to directory order.\n"
            "\n"
            "  -h    Print this message\n", argv[0]
            );
        exit(1);
      case '?':
        fprintf(stderr, "Usage: %s [-adpugsrtUh] [path...]\n", argv[0]);
        exit(1);
    }
  }

#ifdef DEBUG
#define boolstr(b) (b ? "true" : "false")
  fprintf(stderr,
          "opts = {\n"
          "  .all      = %5s, /* print hidden '.' files */\n"
          "  .dirsonly = %5s, /* list directories only */\n"
          "  .perms    = %5s, /* print file type and permissions */ \n"
          "  .user     = %5s, /* print the username of the file */\n"
          "  .group    = %5s, /* print the group name of file */\n"
          "  .size     = %5s, /* print file size in bytes */\n"
          "  .sort     = %5s, /* sorting method to use */\n"
          "  .indent   = %5d, /* indent size */"
          "};\n",
          boolstr(opts.all), boolstr(opts.dirsonly), boolstr(opts.perms), boolstr(opts.user),
          boolstr(opts.group), boolstr(opts.size),
          (char *[]){"NONE", "ALPHA", "RALPHA", "TIME"}[opts.sort], opts.indent);
#endif

  if (optind < argc) {
    for (int i = optind; i < argc; ++i) {
      if (tree_print(argv[i], opts) == -1) err(errno, "printing tree for %s", argv[i]);
    }
  } else {
    if (tree_print("./", opts) == -1) err(errno, "printing tree for ./");
  }
}
