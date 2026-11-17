#include "foreign_result_snapshot.h"
#include <assert.h>
static xvalue* held;
static int calls,live,drops;
static xmemdebugsnapshot baseline;
void result_store(xvalue* value)
{
    xvalue* previous=held;
    held=value ? xrtValueRetain(value):NULL;
    xrtValueRelease(previous);
}
xvalue* result_get(void) { ++calls;return held ? xrtValueRetain(held):NULL; }
xvalue* result_move(void) { ++calls;xvalue* value=held;held=NULL;return value; }
xvalue* result_echo(xvalue* value) { ++calls;return value ? xrtValueRetain(value):NULL; }
int result_pair(xvalue** a,xvalue** b)
{
    ++calls;*a=held ? xrtValueRetain(held):NULL;*b=held ? xrtValueRetain(held):NULL;
    return 0;
}
int result_mixed(xvalue** a,const char** text,xvalue** b,int fail)
{
    result_pair(a,b);*text="borrowed";
    return fail ? -91:0;
}
static ResultReceiver receiver={result_receiver_get};
ResultReceiver* result_receiver(void) { return &receiver; }
xvalue* result_receiver_get(ResultReceiver* input) { assert(input==&receiver);return result_get(); }
int result_receiver_output(ResultReceiver* input,xvalue** value)
{ assert(input==&receiver);*value=result_get();return 0; }
int result_calls(void) { return calls; }
int result_same(xvalue* value) { return value==held; }
static void opaqueDrop(ptr raw,ptr user)
{
    (void)raw;assert(live>0);--live;++drops;
    if(user)xrtSetErrorKind(XERR_STATE);
}
static bool opaqueClone(ptr raw,ptr* output,ptr user)
{ (void)raw;(void)output;(void)user;assert(!"strict snapshot must never Clone opaque values");return false; }
xvalue* result_opaque(int failDrop)
{
    static const xvaluehandleops ops={opaqueClone,opaqueDrop,NULL,NULL};
    ptr raw=&live;xvalue* value=xrtValueHandleTake(&raw,&ops,failDrop ? &drops:NULL);
    if(value)++live;
    return value;
}
int result_opaque_live(void) { return live; }
int result_drops(void) { return drops; }
void result_clear(void) { xvalue* value=held;held=NULL;xrtValueRelease(value); }
void result_budget(int allowance)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    if(allowance<0)xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}
int result_triggered(void) { return xrtMemDebugFailTriggered(); }
void result_mark(void) { xrtMemDebugSnapshot(&baseline); }
int result_balanced(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    return baseline.LiveCount==after.LiveCount && baseline.LiveBytes==after.LiveBytes;
}
