/* Selected actual XRT implementation, not stubs or reimplemented algorithms.
 * Supplies the complete native code-activity contract's runtime closure. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
