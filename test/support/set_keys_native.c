/* Actual selected supplier bodies, not a test container or mirrored ABI. */
#define XRUNTIME_MODULE_TYPED_SET
#define XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_ONCE
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
