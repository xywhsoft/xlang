#include <assert.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"

static bool rejectAssignment(xvalue* target, const xvalue* source)
{
    assert(target != NULL && source != NULL); return false;
}
static bool allocateAssignment(xvalue* target, const xvalue* source)
{
    xvalue* item;
    assert(target != NULL && source != NULL);
    item = xrtValueString(xrtStrView("restore-owned-allocation"));
    if (item == NULL) return false;
    xrtValueRelease(item); return true;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* target = xrtValueObject(); assert(target != NULL);
    for (unsigned i = 0; i < 2; ++i) {
        xvalue* output = target;
        xrtSetErrorKind(i ? XERR_MEMORY : XERR_VALUE);
        xerror* prior = xrtTakeError(); assert(prior != NULL);
        assert(xl6SerializeRestoreReadFinishV1(NULL, target, rejectAssignment, prior, &output));
        assert(output != NULL && xrtValueType(output) == XVALUE_NULL && xrtGetError() == prior);
        xrtValueRelease(output); xrtClearError();
    }
    xvalue* source = xrtValueObject(); assert(source != NULL);
    xvalue* output = target;
    assert(xl6SerializeRestoreReadFinishV1(source, target, rejectAssignment, NULL, &output));
    assert(output != NULL && xrtValueType(output) == XVALUE_NULL && xrtGetError() == NULL);
    xrtValueRelease(output);
    source = xrtValueObject(); assert(source != NULL);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6SerializeRestoreReadFinishV1(source, target, allocateAssignment, NULL, &output));
    assert(xrtMemDebugFailTriggered() && output == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear(); xrtClearError();
    source = xrtValueObject(); assert(source != NULL);
    assert(!xl6SerializeRestoreReadFinishV1(source, target, NULL, NULL, &output));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xrtValueRelease(target); assert(!xl6ProductMemoryEnd());
    xl6ProductMemoryVerify();
    puts("Typed restore status/error ownership balanced"); return 0;
}
