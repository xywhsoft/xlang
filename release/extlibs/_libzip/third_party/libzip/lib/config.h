#ifndef HAD_CONFIG_H
#define HAD_CONFIG_H

#include "zipconf.h"

#define HAVE_STDBOOL_H 1
#define HAVE_SNPRINTF 1
#define HAVE_STRTOLL 1
#define HAVE_STRTOULL 1
#define SIZEOF_SIZE_T 8
#define SIZEOF_OFF_T 8

#if defined(_WIN32) || defined(_WIN64)
#define HAVE__CLOSE 1
#define HAVE__DUP 1
#define HAVE__FDOPEN 1
#define HAVE__FILENO 1
#define HAVE__FSEEKI64 1
#define HAVE__FSTAT64 1
#define HAVE__SETMODE 1
#define HAVE__SNPRINTF_S 1
#define HAVE__SNWPRINTF_S 1
#define HAVE__STAT64 1
#define HAVE__STRDUP 1
#define HAVE__STRICMP 1
#define HAVE__STRTOI64 1
#define HAVE__STRTOUI64 1
#define HAVE__UNLINK 1
#define HAVE_LOCALTIME_S 1
#define HAVE_FSEEKO 1
#define HAVE_FTELLO 1
#else
#define HAVE_DIRENT_H 1
#define HAVE_FSEEKO 1
#define HAVE_FTELLO 1
#define HAVE_LOCALTIME_R 1
#define HAVE_STRCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_UNISTD_H 1
#endif

#define PACKAGE "libzip"
#define VERSION "1.11.4"

#endif
