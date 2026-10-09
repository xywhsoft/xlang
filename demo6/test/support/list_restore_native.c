/* Actual parser/writer and typed collection closure; no test storage, parser
 * or import implementation substitutes. Graph remains optional. */
#define XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_WRITE
#define XRT_MODULE_XLON_READ
#define XRT_MODULE_XLON_WRITE
#if defined(XL6_TEST_RESTORE_PLAN)
#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE_GRAPH
#endif
#if defined(XL6_TEST_LIST_GRAPH)
#define XRT_MODULE_VALUE_GRAPH
#endif
#include "test/support/list_native.c"
