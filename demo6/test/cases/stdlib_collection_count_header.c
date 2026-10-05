/* This ABI must compile with no XRT/Sequence declarations or private include
 * search paths. Actual call and shared-export tests are separate. */
#include "src/runtime/xl6_collection_count.h"
_Static_assert(_Generic(&xl6CollectionCountV1,
    int64_t (*)(const xvalue*): 1, default: 0), "exact count ABI");
_Static_assert(_Generic(&xl6CollectionCountIfPresentV1,
    bool (*)(const xvalue*, int64_t*): 1, default: 0), "exact present count ABI");
_Static_assert(_Generic(&xl6CollectionCountOrNullV1,
    xvalue* (*)(const xvalue*): 1, default: 0), "exact boxed count ABI");
int main(void) { return 0; }
