/* Same selected native storage closure plus the REAL shared graph engine and
 * format writers. No graph memo, storage or container substitute in tests. */
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_JSON_WRITE
#define XRT_MODULE_XLON_WRITE
#include "test/support/list_native.c"
