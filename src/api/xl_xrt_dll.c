#ifndef BUILD_DLL
#define BUILD_DLL
#endif

#define XRT_BUILD_SHARED
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "xl_foreign_error.c"
