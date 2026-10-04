/* Actual Array/Stack slots. Operation status, presence, and diagnostics are
 * independent channels; no generated-code patches or mock collection. */
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define LAYOUT UINT64_C(701),UINT64_C(702),1,sizeof(int64_t),xrtTypeInt64()
#define TEXT UINT64_C(703),UINT64_C(704),2,sizeof(xstrview),xrtTypeStringView()
#define REF UINT64_C(705),UINT64_C(706),3,sizeof(xvalue*),xrtTypeValue()
static unsigned int Checks;
static void error(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError())==kind); xrtClearError(); ++Checks; }
static void vectors(void)
{
    xvalue* array=xl6ArrayCreateV1(LAYOUT);
    xvalue* stack=xl6StackCreateV1(LAYOUT); assert(array && stack);
    int64_t target=73, item=19; bool found=true;
    assert(xl6StackTryTakeV2(stack,LAYOUT,0,&target,true,&found) && !found && target==73 && !xrtGetError()); ++Checks;
    found=true;
    assert(xl6StackTryTakeV2(stack,LAYOUT,INT64_MAX,&target,false,&found) && !found && target==73 && !xrtGetError()); ++Checks;
    assert(!xl6StackTakeV1(stack,LAYOUT,0,&target,true) && target==73); error(XERR_RANGE);
    found=true;
    assert(!xl6StackTryTakeV2(stack,LAYOUT,-1,&target,false,&found) && !found && target==73); error(XERR_RANGE);
    found=true;
    assert(!xl6StackTryTakeV2(stack,LAYOUT,1,&target,true,&found) && !found && target==73); error(XERR_ARGUMENT);
    assert(!xl6StackTryTakeV2(stack,LAYOUT,0,&target,false,NULL) && target==73); error(XERR_ARGUMENT);
    found=true;
    assert(!xl6StackTryTakeV2(stack,LAYOUT,0,NULL,false,&found) && !found); error(XERR_ARGUMENT);
    assert(!xl6ArrayReadV1(array,LAYOUT,0,&target) && target==73); error(XERR_RANGE);
    assert(!xl6ArrayTakeV1(array,LAYOUT,0,&target) && target==73); error(XERR_RANGE);
    assert(!xl6ArrayReadV1(array,LAYOUT,0,NULL)); error(XERR_ARGUMENT);
    assert(!xl6ArrayTakeV1(array,LAYOUT,0,NULL)); error(XERR_ARGUMENT);
    found=true;
    assert(!xl6StackTryTakeV2(array,LAYOUT,0,&target,false,&found) && !found && target==73); error(XERR_ARGUMENT);
    assert(!xl6ArrayReadV1(stack,LAYOUT,0,&target) && target==73); error(XERR_ARGUMENT);
    assert(xl6ArrayPushV1(array,LAYOUT,&item) && xl6StackPushV1(stack,LAYOUT,&item));
    /* A prior diagnostic does not change a successful operation into a false
     * status. Language calls enter with no pending error; native callers may
     * use this to carry a pending error through a successful read. */
    xrtSetErrorKind(XERR_STATE); const xerror* pending=xrtGetError(); assert(pending);
    assert(xl6ArrayReadV1(array,LAYOUT,0,&target) && target==item && xrtGetError()==pending); ++Checks;
    assert(xl6StackTryTakeV2(stack,LAYOUT,0,&target,false,&found) && found && target==item && xrtGetError()==pending); ++Checks;
    assert(xl6ArrayTakeV1(array,LAYOUT,0,&target) && target==item && xrtGetError()==pending); ++Checks;
    assert(xl6StackTakeV1(stack,LAYOUT,0,&target,true) && target==item && xrtGetError()==pending); ++Checks;
    xrtClearError(); xrtValueRelease(array); xrtValueRelease(stack);

    /* Found=true with a physically NULL payload is distinct from missing. */
    xvalue* refs=xl6StackCreateV1(REF); xvalue* nil=NULL; assert(refs && xl6StackPushV1(refs,REF,&nil));
    found=false; target=73;
    assert(xl6StackTryTakeV2(refs,REF,0,&nil,true,&found) && found && !nil && !xrtGetError()); ++Checks;
    assert(xl6StackTryTakeV2(refs,REF,0,&nil,true,&found) && !found && !nil && !xrtGetError()); ++Checks;
    xrtValueRelease(refs);

    array=xl6ArrayCreateV1(TEXT); stack=xl6StackCreateV1(TEXT);
    xstrview text={"a\0b",3}, output={0}; assert(array && stack);
    assert(xl6ArrayPushV1(array,TEXT,&text) && xl6StackPushV1(stack,TEXT,&text));
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayReadV1(array,TEXT,0,&output) && !output.Data && !output.Size);
    assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); error(XERR_MEMORY);
    found=true; assert(xrtMemDebugFailAfter(0));
    assert(!xl6StackTryTakeV2(stack,TEXT,0,&output,false,&found) && !found && !output.Data && !output.Size);
    assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); error(XERR_MEMORY);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6StackTakeV1(stack,TEXT,0,&output,false) && !output.Data && !output.Size);
    assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); error(XERR_MEMORY);
    assert(xl6ArraySizeV1(array,TEXT,false)==1 && xl6StackSizeV1(stack,TEXT,false)==1);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ArrayTakeV1(array,TEXT,0,&output) && output.Size==3 && !memcmp(output.Data,"a\0b",3));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++Checks;
    xrtTypeDropValue(xrtTypeStringView(),&output);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6StackTryTakeV2(stack,TEXT,0,&output,true,&found) && found && output.Size==3 && !memcmp(output.Data,"a\0b",3));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++Checks;
    xrtTypeDropValue(xrtTypeStringView(),&output);
    xrtValueRelease(array); xrtValueRelease(stack);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after;
    xrtMemDebugSnapshot(&before); vectors(); xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("sequence take status: %u vectors, balanced\n",Checks); return 0;
}
