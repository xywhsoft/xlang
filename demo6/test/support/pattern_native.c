/* Actual pattern/string dependency closure, instrumentable independently
 * of the resident DLL. No substitute matcher or allocator implementation. */
#define XRT_MODULE_PATTERN
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
