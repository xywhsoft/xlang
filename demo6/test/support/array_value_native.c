/* Actual projection closure, separate from storage-only/cursor acceptance. */
#define XRUNTIME_MODULE_TYPED_ARRAY_VALUE
#ifdef XL6_TEST_ERASED_STACK
#define XRUNTIME_MODULE_TYPED_STACK
#endif
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_ONCE
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
