#ifndef XL6_TEST_STACK_TAKE_FOUND_H
#define XL6_TEST_STACK_TAKE_FOUND_H
#include "src/runtime/xl6_stack.h"
#include <assert.h>

/* Older presence-only vector assertions deliberately inspect the diagnostic
 * separately. Keep their projection in the test, not a production ABI shim. */
static bool xl6StackFoundTest(const xvalue* value, uint64_t identity,
    uint64_t element, unsigned int transfer, size_t size, const xrttype* type,
    int64_t depth, void* target, bool pop)
{
    bool found = false;
    bool ok = xl6StackTryTakeV2(value,identity,element,transfer,size,type,depth,target,pop,&found);
    assert(ok || !found);
    return found;
}
#endif
