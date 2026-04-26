#ifndef XL_APP_PATH_H
#define XL_APP_PATH_H

#include "xl_common.h"
#include "xl_path.h"

int xlAppPathGetExecutable(char* sOut, size_t iOutSize);
int xlAppPathGetDirectory(char* sOut, size_t iOutSize);
int xlAppPathGetParentDirectory(char* sOut, size_t iOutSize);

#endif
