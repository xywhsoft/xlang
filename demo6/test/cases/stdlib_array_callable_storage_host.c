#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include <assert.h>
int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    xvalue* value=xl6SequenceCallableCreateV1(XL6_SEQUENCE_ARRAY_V1,11,22,33,true); assert(value);
    assert(xl6ArrayResizeV1(value,11,22,3,sizeof(xvalue*),xrtTypeValue(),4));
    xvalue* item=NULL;
    xl6ArrayReadV1(value,11,22,3,sizeof(xvalue*),xrtTypeValue(),3,&item);
    assert(!item && !xrtGetError()); xrtValueRelease(value); xrtMemDebugSnapshot(&after);
    assert(before.LiveBytes==after.LiveBytes && before.LiveCount==after.LiveCount);
    return 0;
}
