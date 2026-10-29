/* Actual selected native Array closure. Deliberately excludes typed Stack. */
#define XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
