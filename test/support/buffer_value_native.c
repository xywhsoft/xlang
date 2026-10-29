/* Real selected XRT implementation, not mocked allocation or Buffer APIs. */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_THREAD
#define XRT_MODULE_ONCE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
