#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"

/* 该消费者只用容器/Unicode/内存调试，禁止悄悄重新拉入图算法。 */
#ifdef XRT_FEATURE_VALUE_GRAPH
#error CLI tests must not require VALUE_GRAPH
#endif
