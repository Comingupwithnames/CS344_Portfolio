#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "libtree.h"

/* Convenient macro to get the length of an array (number of elements) */
#define arrlen(a) (sizeof(a) / sizeof *(a))

/* dprintf(...) can be used like printf to print diagnostic messages in the debug build. Does
 * nothing in release. This is how debugging with print statements is done -- conditional
 * compilation determined by a compile-time DEBUG macro. */
#ifdef DEBUG
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...) ((void)0)
#endif

/* AUTHOR: SKYELAR R CANN
 * LAST MODIFIED: 1/31/2023
 * COURSE NUMBER: CS 344
 * DESCRIPTION: This file will handle the file/directory information passed to it and print the 
 *              appropriate directories, files, and hidden files according to the passed option flags
 */

/* We will need to pass around file stat info quite a bit, so let's make a struct for this purpose.
 */
struct fileinfo {
  char *path;
  struct stat st;
};

/* A few helper functions to break up the program */
static int print_path_info(struct fileinfo finfo); /* Prints formatted file information */
static char *mode_string(mode_t mode);             /* Aka Permissions string */

/* These functions are used to get a list of files in a directory and sort them */
static int read_file_list(DIR *dirp, struct fileinfo **file_list, size_t *file_count);
static void free_file_list(struct fileinfo **file_list, size_t file_count);
static int filecmp(void const *lhs, void const *rhs);

/* Some file-scoped objects avoid having to pass things between functions */
static int depth;
static struct tree_options opts;
static int cur_dir = AT_FDCWD;

/* Here are our two main functions. tree_print is the externally linked function, accessible to
 * users of the library. tree_print_recurse is an internal recursive function. */
extern int tree_print(char const *path, struct tree_options opts);
static int tree_print_recurse(struct fileinfo finfo);

/* Simply sets up the initial recursion. Nothing for you to change here. */
extern int
tree_print(char const *path, struct tree_options _opts)
{
  opts = _opts;
  depth = 0;
  struct fileinfo finfo;
  if ((finfo.path = strdup(path)) == NULL) goto exit;
  if (fstatat(cur_dir, path, &(finfo.st), AT_SYMLINK_NOFOLLOW) == -1) goto exit;
  if (tree_print_recurse(finfo) == -1) goto exit;
exit:
  free(finfo.path);
  return errno ? -1 : 0;
}

/**
 * @brief Recursive function to print a directory and everything inside of it given that the 
 * appropriate flags are set 
 */
static int
tree_print_recurse(struct fileinfo finfo)
{
  int dir = -1, sav_dir = cur_dir;
  DIR *dirp = NULL;
  struct fileinfo *file_list = NULL;
  size_t file_count = 0;

  errno = 0;

  if((!opts.dirsonly || S_ISDIR(finfo.st.st_mode)) && depth != 0) //If we are handling a directory and depth is not zero, print indentation
  {
    for(int i = 0; i< depth * opts.indent; i++)// Print depth times our opts.indent spaces to correctly indent output 
    {
      putchar(' '); 
    }
  }

  if(S_ISDIR(finfo.st.st_mode)){ print_path_info(finfo); } // If we are handling a directory print its info
  else if(opts.dirsonly) { goto exit; } // If we reach here, we are handling a file and will exit if dirsonly is set
  else  // If we reach here, dirsonly is not set so we print the file's information and goto exit
  { 
    print_path_info(finfo);
    putchar('\n');
    goto exit;
  } 

  if ((dir = openat(cur_dir, finfo.path, O_RDONLY | O_CLOEXEC)) == -1 ||
      (dirp = fdopendir(dir)) == NULL) {
    if (errno == EACCES) {
      errno = 0; /* not an error, so reset errno! */
      printf(" [could not open directory %s]\n", finfo.path);
    }
    goto exit;
  }
  cur_dir = dir;



  if (read_file_list(dirp, &file_list, &file_count) == -1)
  {
    if(errno == EACCES) // If we get an EACCES error, print the appropriate error message and goto exit
    {
      errno = 0; // Reset errno since it may not be an error that requires exiting the program
      printf(" [could not open directory %s]\n", finfo.path);
    }
      goto exit;
  }
  if (putchar('\n') == EOF) goto exit;
  /* See QSORT(3) for info about this function. It's not super important. It just sorts the list of
   * files using the filesort() function, which is the part you need to finish. */
  qsort(file_list, file_count, sizeof *file_list, filecmp);

  ++depth;
  for (size_t i = 0; i < file_count; ++i) {
    struct fileinfo finfo = {file_list[i].path, file_list[i].st}; // Initialize finfo with our individual file paths and stat objects 
    if (fstatat(cur_dir, finfo.path, &(finfo.st), AT_SYMLINK_NOFOLLOW) == -1) goto exit;
    if (tree_print_recurse(finfo) == -1) goto exit; /* Recursive call */
  }
  --depth;
exit:
  if(dirp != NULL && S_ISDIR(finfo.st.st_mode)) { closedir(dirp); } // If our directory pointer exists and we are looking at a directory, free the directory pointer
  if(file_list != NULL) { free_file_list(&file_list,file_count); } // If our file list is not null, free it 
  cur_dir = sav_dir;
  return errno ? -1 : 0;
}

/**
 * @brief Helper function that prints formatted output of the modestring, username, groupname, file
 * size, and link target (for links).
 */
static int
print_path_info(struct fileinfo finfo)
{
  char sep = '[';
  if (opts.perms) {
    if (printf("%c%s", sep, mode_string(finfo.st.st_mode)) < 0) goto exit; // Use the mode_string() helper function to return a string representation of the perms
    sep = ' ';
  }
  if (opts.user) {
    /* If our printf() function returns a value less than zero because getpwuid() failed, exit */
    if (printf("%c%s", sep, getpwuid(finfo.st.st_uid)->pw_name) < 0) goto exit;
    sep = ' ';
  }
  if (opts.group) {
    /* If our printf() function returns anything less than zero through the getgrgid() function, exit*/
    if (printf("%c%s", sep, getgrgid(finfo.st.st_gid)->gr_name) < 0) goto exit;
    sep = ' ';
  }
  if (opts.size) {
    /*  If our printf() function returns a value less than zero because our file size does not exist, exit */
    if (printf("%c%jd", sep, finfo.st.st_size) < 0) goto exit;
    sep = ' ';
  }
  if (sep != '[')
    if (printf("] ") < 0) goto exit;
  if (printf("%s", finfo.path) < 0) goto exit;
  if (S_ISLNK(finfo.st.st_mode)) {
    char rp[PATH_MAX + 1] = {0};
    if (readlinkat(cur_dir, finfo.path, rp, PATH_MAX) == -1) goto exit;
    if (printf(" -> %s", rp) < 0) goto exit;
  }
exit:
  return errno ? -1 : 0;
}

/**
 * @brief File comparison function, used by qsort
 */
static int
filecmp(void const *_lhs, void const *_rhs)
{
  struct fileinfo const *lhs = _lhs, *rhs = _rhs;
  struct timespec const lt = lhs->st.st_mtim, rt = rhs->st.st_mtim;
  int retval = 0;
  switch (opts.sort) {
    case NONE:
      retval = 0;
      break;
    case ALPHA:
      retval = strcoll(lhs->path, rhs->path); // Default sorting, will sort in alphabetical order
      break; 
    case RALPHA:
      retval = strcoll(rhs->path, lhs->path); // Reverse sorting, will sort from Z->A
      break;
    case TIME: // Modification time sort, will sort by time last modified
      if (rt.tv_sec != lt.tv_sec) {
        retval = rt.tv_sec - lt.tv_sec;
      } else {
        retval = rt.tv_nsec - lt.tv_nsec;
      }
      break;
  }
  return retval;
}

/**
 * @brief Reads all files in a directory and populates a fileinfo array
 */
static int
read_file_list(DIR *dirp, struct fileinfo **file_list, size_t *file_count)
{
  for (;;) {
    errno = 0;
    struct dirent *de = readdir(dirp);
    if (de == NULL) break;

    /* Skip the "." and ".." subdirectories */
    if (strcoll(de->d_name, ".") == 0 || strcoll(de->d_name, "..") == 0) continue;

    if(!opts.all && !strncmp(de->d_name, ".", 1)) { continue; }; // If our all flag is set and if the first char of our dirname is '.', continue to counting it, otherwise skip over it

    ++(*file_count); // First file reached, increment our file count using its reference 
    (*file_list) = realloc((*file_list), sizeof *(*file_list) * (*file_count)); // Allocate memory for each file encountered dynamically using realloc
    (*file_list)[(*file_count) - 1].path = strdup(de->d_name); // Initialize the finfo path to be the directory/file name
    if (fstatat(cur_dir, de->d_name, &(*file_list)[(*file_count) - 1].st, AT_SYMLINK_NOFOLLOW) == -1) // Initialize the stat variable in the finfo struct
      break;
  }
  return errno ? -1 : 0;
}

/**
 * @brief Frees dynamically allocated file list (array of fileinfo objects)
 */
static void
free_file_list(struct fileinfo **file_list, size_t file_count)
{
  for (size_t i = 0; i < file_count; ++i) {
    free((*file_list)[i].path);
  }
  free(*file_list);
}

/**
 * @brief Returns a 9-character modestring for the given mode argument.
 */
static char *
mode_string(mode_t mode)
{
  static char str[11];
  if (S_ISREG(mode))
    str[0] = '-';
  else if (S_ISDIR(mode))
    str[0] = 'd';
  else if (S_ISBLK(mode))
    str[0] = 'b';
  else if (S_ISCHR(mode))
    str[0] = 'c';
  else if (S_ISLNK(mode))
    str[0] = 'l';
  else if (S_ISFIFO(mode))
    str[0] = 'p';
  else if (S_ISSOCK(mode))
    str[0] = 's';
  else
    str[0] = '.';
  str[1] = mode & S_IRUSR ? 'r' : '-';
  str[2] = mode & S_IWUSR ? 'w' : '-';
  str[3] = (mode & S_ISUID ? (mode & S_IXUSR ? 's' : 'S') : (mode & S_IXUSR ? 'x' : '-'));
  str[4] = mode & S_IRGRP ? 'r' : '-';
  str[5] = mode & S_IWGRP ? 'w' : '-';
  str[6] = (mode & S_ISGID ? (mode & S_IXGRP ? 's' : 'S') : (mode & S_IXGRP ? 'x' : '-'));
  str[7] = mode & S_IROTH ? 'r' : '-';
  str[8] = mode & S_IWOTH ? 'w' : '-';
  str[9] = (mode & S_ISVTX ? (mode & S_IXOTH ? 't' : 'T') : (mode & S_IXOTH ? 'x' : '-'));
  str[10] = '\0';
  return str;
}
