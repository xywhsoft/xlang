#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    const xrttype* type=XL6_TEST_ENUM_DESCRIPTOR();
    xvalue* owner=xl6ArrayNumericEnumCreateV1(1,2,type,XL6_TEST_ENUM_RULE,true,3); assert(owner);
    assert(xl6ArrayResizeV1(owner,1,2,1,type->Size,type,7));
    xrtValueRelease(owner); xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    puts("one selected enum nullable descriptor, no converter, balanced"); return 0;
}
