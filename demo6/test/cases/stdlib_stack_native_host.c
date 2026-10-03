#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_ARGS UINT64_C(101),UINT64_C(201),2,sizeof(xstrview),xrtTypeStringView()
#define REF_ARGS UINT64_C(102),UINT64_C(202),3,sizeof(xvalue*),xrtTypeValue()
#define FLOAT_ARGS UINT64_C(103),UINT64_C(203),1,sizeof(double),xrtTypeFloat64()
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static void exact(xstrview text)
{ assert(text.Size == 3 && !memcmp(text.Data,"a\0b",3)); }
static void memoryFault(const char* operation, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* stack = xl6StackCreateV1(TEXT_ARGS); assert(stack);
    xstrview item = {"a\0b",3}, output = {xrtStrDup("unchanged"),9};
    assert(output.Data && xl6StackReserveV1(stack,TEXT_ARGS,8));
    assert(xl6StackPushV1(stack,TEXT_ARGS,&item));
    assert(xrtMemDebugFailAfter(point));
    xvalue* created = NULL; bool ok = false;
    if (!strcmp(operation,"create")) ok = (created=xl6StackCreateV1(TEXT_ARGS)) != NULL;
    else if (!strcmp(operation,"reserve")) ok = xl6StackReserveV1(stack,TEXT_ARGS,32768);
    else if (!strcmp(operation,"push")) ok = xl6StackPushV1(stack,TEXT_ARGS,&item);
    else if (!strcmp(operation,"peek")) ok = xl6StackPeekAtV1(stack,TEXT_ARGS,0,&output);
    else if (!strcmp(operation,"clone")) ok = (created=xl6StackCloneV1(stack,TEXT_ARGS)) != NULL;
    else if (!strcmp(operation,"trim")) ok = xl6StackTrimV1(stack,TEXT_ARGS);
    else abort();
    bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit && (ok || xrtErrorKind(xrtGetError()) != XERR_MEMORY))
        fprintf(stderr,"%s point=%zu kind=%d ok=%d\n",operation,point,(int)xrtErrorKind(xrtGetError()),ok);
    assert(hit ? !ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY : ok && !xrtGetError());
    xerror* failure = xrtTakeError();
    assert(xl6StackSizeV1(stack,TEXT_ARGS,false) == (ok && !strcmp(operation,"push") ? 2 : 1));
    if (hit && !strcmp(operation,"peek"))
        assert(output.Size == 9 && !memcmp(output.Data,"unchanged",9));
    assert(xl6StackPeekAtV1(stack,TEXT_ARGS,0,&output)); exact(output);
    xrtTypeDropValue(xrtTypeStringView(),&output);
    xrtSetErrorTake(failure); const xerror* original = xrtGetError();
    xrtValueRelease(created); xrtValueRelease(stack);
    assert(xrtGetError() == original); xrtClearError(); balanced(&before);
    printf("stack %s point=%zu hit=%d balanced\n",operation,point,hit);
}
static xvalue* releaseDuringEquality;
static int equalityCalls;
static bool equalHandle(void* left, void* right, void* user)
{
    (void)left; (void)right; (void)user; ++equalityCalls;
    xl6StackClearV1(releaseDuringEquality,REF_ARGS);
    assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
    xvalue* owner = releaseDuringEquality; releaseDuringEquality = NULL;
    xrtValueRelease(owner); return true;
}
static bool cloneHandle(void* source, void** target, void* user)
{ (void)user; *target = source; return true; }
static void dropHandle(void* source, void* user)
{ (void)source; (void)user; }
static uint64_t hashHandle(void* source, void* user)
{ (void)source; (void)user; return 1; }
static const xvaluehandleops equalityOps = {cloneHandle,dropHandle,hashHandle,equalHandle};
static void vectors(void)
{
    xvalue* stack = xl6StackCreateV1(TEXT_ARGS); assert(stack);
    xstrview input = {"a\0b",3}, output = {0};
    assert(xl6StackPushV1(stack,TEXT_ARGS,&input));
    assert(xl6StackPeekAtV1(stack,TEXT_ARGS,0,&output)); exact(output);
    xvalue* clone = xl6StackCloneV1(stack,TEXT_ARGS); assert(clone);
    assert(xl6StackEqualsV1(stack,clone,TEXT_ARGS));
    assert(xl6StackPopOrPeekV1(stack,TEXT_ARGS,&output,false)); exact(output);
    assert(!xl6StackPopOrPeekV1(stack,TEXT_ARGS,&output,false)); exact(output); assert(!xrtGetError());
    assert(!xl6StackPeekAtV1(stack,TEXT_ARGS,-1,&output)); assert(!xrtGetError());
    assert(!xl6StackReserveV1(stack,TEXT_ARGS,-1)); assert(xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    assert(!xl6StackPushV1(stack,101,999,2,sizeof(xstrview),xrtTypeStringView(),&input));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(!xl6StackCreateV1(104,204,1,sizeof(xvalue*),xrtTypeValue()));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xrtTypeDropValue(xrtTypeStringView(),&output); xrtValueRelease(stack); xrtValueRelease(clone);

    xvalue* refs = xl6StackCreateV1(REF_ARGS); assert(refs);
    xvalue* object = xrtValueArray(); assert(object);
    assert(xl6StackPushV1(refs,REF_ARGS,&object));
    xvalue* observed = NULL;
    assert(xl6StackPeekAtV1(refs,REF_ARGS,0,&observed)); assert(observed == object);
    xvalue* refsClone = xl6StackCloneV1(refs,REF_ARGS); assert(refsClone);
    xrtValueRelease(object); xrtValueRelease(observed); observed = NULL;
    assert(xl6StackPopOrPeekV1(refsClone,REF_ARGS,&observed,false));
    assert(xrtValueArrayAppendNew(observed,xrtValueInt(71)));
    xvalue* other = NULL; assert(xl6StackPeekAtV1(refs,REF_ARGS,0,&other));
    assert(other == observed && xrtValueCount(other) == 1);
    xrtValueRelease(other); xrtValueRelease(observed); xrtValueRelease(refs); xrtValueRelease(refsClone);

    xvalue* a = xl6StackCreateV1(FLOAT_ARGS); xvalue* b = xl6StackCreateV1(FLOAT_ARGS);
    double plus = 0.0, minus = -0.0; assert(a && b);
    assert(xl6StackPushV1(a,FLOAT_ARGS,&plus) && xl6StackPushV1(b,FLOAT_ARGS,&minus));
    assert(xl6StackEqualsV1(a,b,FLOAT_ARGS)); xrtValueRelease(a); xrtValueRelease(b);

    /* Real XRT handle equality invokes a user callback. Both wrappers must
     * remain pinned even when that callback drops the caller's last owner. */
    a = xl6StackCreateV1(REF_ARGS); b = xl6StackCreateV1(REF_ARGS); assert(a && b);
    int x = 1, y = 2;
    void* px = &x; void* py = &y;
    xvalue* first = xrtValueHandleTake(&px,&equalityOps,NULL);
    xvalue* second = xrtValueHandleTake(&py,&equalityOps,NULL); assert(first && second);
    assert(xl6StackPushV1(a,REF_ARGS,&first) && xl6StackPushV1(b,REF_ARGS,&second));
    xrtValueRelease(first); xrtValueRelease(second); releaseDuringEquality = a;
    assert(xl6StackEqualsV1(a,b,REF_ARGS)); assert(equalityCalls == 1 && !releaseDuringEquality);
    xrtValueRelease(b); assert(!xrtGetError());
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc == 3) { memoryFault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc == 1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    vectors(); balanced(&before); puts("stack native vectors balanced"); return 0;
}
