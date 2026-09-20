#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_ENGINE
#if defined(__linux__)
#define XRT_MODULE_NET_PORT_URING
#endif
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
