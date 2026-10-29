#define XRT_MODULE_LOGGER_PRINTF
#define XRT_MODULE_LOGGER_FORMAT_TEXT_BUFFER
#define XRT_MODULE_LOGGER_FORMAT_JSON_BUFFER
#define XRT_MODULE_VALUE
#define XRT_MODULE_THREAD
#define XRT_MODULE_ONCE
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
