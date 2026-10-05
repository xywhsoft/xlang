#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    const xrttype* type=XL6_TEST_NATIVE_C_DESCRIPTOR();
    xvalue* owner=xl6ArrayNullableCScalarCreateV1(1,2,type,XL6_TEST_NATIVE_C_BASE(),3); assert(owner);
    assert(xl6ArrayResizeV1(owner,1,2,1,type->Size,type,7));
    assert(xl6ArraySizeV1(owner,1,2,1,type->Size,type,false)==7);
    xrtValueRelease(owner); xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    puts("one selected resident nullable descriptor, no converter or foreign callback provider, balanced"); return 0;
}
