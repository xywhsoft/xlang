/* Actual open-index closure. Unlike the general List test closure it has no
 * Stack implementation: open Array/List/Dict indexing must not retain it. */
#define XRUNTIME_MODULE_TYPED_LIST_VALUE
#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD
#define XRUNTIME_MODULE_TYPED_DICT
#define XRUNTIME_MODULE_TYPED_ARRAY_VALUE
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_MODULE_ONCE
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
