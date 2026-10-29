/* The native implementation has no language facade exports. Each audited
 * language bridge is a separately selected resident component. */
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_foreign_error.c"
