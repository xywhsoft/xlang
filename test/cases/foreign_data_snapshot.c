#include "foreign_data_snapshot.h"
#include <assert.h>
#include <string.h>
static xvalue* saved;
static int calls;
static xmemdebugsnapshot baseline;
static int opaqueLive;
static void opaqueDrop(ptr raw,ptr user) { (void)raw;(void)user;assert(opaqueLive>0);--opaqueLive; }
static bool opaqueClone(ptr raw,ptr* output,ptr user)
{ (void)raw;(void)output;(void)user;assert(!"snapshot must not invoke opaque Clone");return false; }
xvalue* snapshot_opaque(void)
{
    static const xvaluehandleops ops={opaqueClone,opaqueDrop,NULL,NULL};ptr raw=&calls;
    xvalue* value=xrtValueHandleTake(&raw,&ops,NULL);
    if(value)++opaqueLive;
    return value;
}
int snapshot_opaque_live(void) { return opaqueLive; }
int snapshot_store(xvalue* value)
{
    ++calls;
    xvalue* next=xrtValueRetain(value);
    assert(next);
    xrtValueRelease(saved);saved=next;
    return 0;
}
int snapshot_pair(xvalue* value,int tag)
{ assert(tag==42); return snapshot_store(value); }
xvalue* snapshot_owned(xvalue* value)
{ ++calls; return xrtValueRetain(value); }
int snapshot_const(const xvalue* value)
{ return snapshot_store((xvalue*)value); }
int snapshot_failing_store(xvalue* value)
{ snapshot_store(value); return -91; }
int snapshot_two(xvalue* first,xvalue* second)
{
    /* Each explicitly marked input owns its own data projection. */
    assert(first && second && first!=second);
    return snapshot_store(first);
}
int snapshot_pack(const SnapshotPack* value)
{ assert(value && value->tag==42); return snapshot_store(value->value); }
int snapshot_receiver_store(SnapshotReceiver* receiver,xvalue* value);
static SnapshotReceiver receiver={snapshot_receiver_store};
SnapshotReceiver* snapshot_receiver(void) { return &receiver; }
int snapshot_receiver_store(SnapshotReceiver* input,xvalue* value)
{ assert(input==&receiver); return snapshot_store(value); }
int snapshot_calls(void) { return calls; }
int snapshot_same(xvalue* value) { return saved==value; }
int snapshot_number(void)
{
    int64 n=0;
    assert(xrtValueGetInt(xrtValueObjectGet(saved,XRT_STR_LITERAL("n")),&n));
    return (int)n;
}
int snapshot_public(void)
{
    assert(saved && xrtValueType(saved)==XVALUE_OBJECT && !xrtValueTypeId(saved));
    assert(xrtValueCount(saved)==3);
    assert(xrtValueObjectGet(saved,XRT_STR_LITERAL("name")));
    assert(xrtValueObjectGet(saved,XRT_STR_LITERAL("inherited")));
    assert(xrtValueObjectGet(saved,XRT_STR_LITERAL("visible")));
    assert(!xrtValueObjectGet(saved,XRT_STR_LITERAL("secret")));
    assert(!xrtValueObjectGet(saved,XRT_STR_LITERAL("resource")));
    assert(!xrtValueObjectGet(saved,XRT_STR_LITERAL("computed")));
    assert(!xrtValueObjectGet(saved,XRT_STR_LITERAL("$xlir.class.runtime-type")));
    return 1;
}
int snapshot_array(void)
{
    assert(saved && xrtValueType(saved)==XVALUE_ARRAY && xrtValueCount(saved)==2);
    assert(xrtValueArrayGet(saved,0)==xrtValueArrayGet(saved,1));
    return 1;
}
int snapshot_sequence(void)
{ assert(saved && xrtValueType(saved)==XVALUE_ARRAY && xrtValueCount(saved)==2);return 1; }
int snapshot_scalar(int mode)
{
    if(mode==0)assert(saved && xrtValueType(saved)==XVALUE_NULL);
    if(mode==1){xstrview view;assert(xrtValueGetString(saved,&view) && view.Size==3 && !memcmp(view.Data,"a\0b",3));}
    if(mode==2){xbytesview view;assert(xrtValueGetBytes(saved,&view) && view.Size==2 && !memcmp(view.Data,"\0\xff",2));}
    return 1;
}
void snapshot_clear(void) { xrtValueRelease(saved);saved=NULL; }
void snapshot_budget(int allowance)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    if(allowance<0)xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}
int snapshot_triggered(void) { return xrtMemDebugFailTriggered(); }
void snapshot_mark(void) { xrtMemDebugSnapshot(&baseline); }
int snapshot_balanced(void)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    return baseline.LiveCount==after.LiveCount && baseline.LiveBytes==after.LiveBytes;
}
