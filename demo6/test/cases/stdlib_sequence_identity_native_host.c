#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

#define OWNER UINT64_C(9201)
#define ELEMENT UINT64_C(9202)
#define LAYOUT(type,transfer) OWNER,ELEMENT,transfer,(type)->Size,type
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static uint64 hash(const xvalue* value)
{ uint64 result=UINT64_MAX; assert(xrtValueHash(value,&result)); return result; }
static void equal(const xvalue* a,const xvalue* b,const xrttype* type,unsigned transfer,bool expected)
{
    bool actual=xl6ArrayEqualsV1(a,b,LAYOUT(type,transfer));
    if (actual!=expected || xrtGetError()) fprintf(stderr,"direct equal type=%.*s transfer=%u expected=%d actual=%d error=%d\n",
        (int)type->Name.Size,type->Name.Data,transfer,expected,actual,(int)xrtErrorKind(xrtGetError()));
    assert(actual==expected && !xrtGetError());
    assert(xrtValueEqual(a,b)==expected && !xrtGetError());
    if (expected) assert(hash(a)==hash(b));
}
static xvalue* primitive(const xrttype* type,unsigned transfer,unsigned kind,bool nullable)
{
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,transfer,type->Size,type,
        kind,nullable,nullable ? ELEMENT+1 : ELEMENT);
}
static void scalar(void)
{
    const xrttype* type=xrtTypeInt64();
    xvalue* a=primitive(type,1,XL6_VALUE_ADMIT_I8,false);
    xvalue* b=primitive(type,1,XL6_VALUE_ADMIT_I8,false); assert(a && b);
    equal(a,b,type,1,true);
    assert(xl6ArrayReserveV1(b,LAYOUT(type,1),100));
    int64_t first=7,second=8; assert(xl6ArrayPushV1(a,LAYOUT(type,1),&first));
    assert(xl6ArrayPushV1(b,LAYOUT(type,1),&first)); equal(a,b,type,1,true);
    assert(xl6ArrayPushV1(b,LAYOUT(type,1),&second)); equal(a,b,type,1,false);
    assert(xl6ArrayPushV1(a,LAYOUT(type,1),&second)); equal(a,b,type,1,true);
    assert(xl6ArraySwapV1(b,LAYOUT(type,1),0,1)); equal(a,b,type,1,false);
    assert(xl6ArrayReverseV1(b,LAYOUT(type,1))); equal(a,b,type,1,true);
    xvalue* clone=xl6ArrayCloneV1(a,LAYOUT(type,1)); assert(clone); equal(a,clone,type,1,true);
    /* Same physical/logical IDs do not authorize a different sealed range. */
    xvalue* wider=primitive(type,1,XL6_VALUE_ADMIT_I16,false); assert(wider);
    assert(xl6ArrayPushV1(wider,LAYOUT(type,1),&first) && xl6ArrayPushV1(wider,LAYOUT(type,1),&second));
    equal(a,wider,type,1,false);
    XL6_Sequence* pin=xl6SequenceBeginV1(a,LAYOUT(type,1)); assert(pin);
    uint64 output=23; assert(!xrtValueHash(a,&output) && output==23); error(XERR_STATE);
    assert(!xrtValueEqual(a,b)); error(XERR_STATE); xl6SequenceEndV1(pin);
    equal(a,b,type,1,true);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    assert(xrtValueEqual(a,b) && xrtGetError()==prior);
    (void)hash(a); assert(xrtGetError()==prior);
    assert(xl6ArrayEqualsV1(a,b,LAYOUT(type,1)) && xrtGetError()==prior); xrtClearError();
    /* Keys remain stable while stored; this is not a freeze/snapshot API. */
    xvalue* keys=xrtValueSet(); assert(keys && xrtValueSetAdd(keys,a) && xrtValueSetAdd(keys,clone));
    assert(xrtValueCount(keys)==1 && xrtValueSetHas(keys,b)); xrtValueRelease(keys);
    xvalue* stack=xl6StackCreateV1(LAYOUT(type,1)); assert(stack);
    assert(xl6StackPushV1(stack,LAYOUT(type,1),&first) && xl6StackPushV1(stack,LAYOUT(type,1),&second));
    assert(!xrtValueEqual(a,stack) && !xrtGetError());
    xvalue* stackCopy=xl6StackCloneV1(stack,LAYOUT(type,1)); assert(stackCopy);
    assert(xl6StackEqualsV1(stack,stackCopy,LAYOUT(type,1)) && xrtValueEqual(stack,stackCopy));
    assert(hash(stack)==hash(stackCopy));
    xrtValueRelease(stackCopy); xrtValueRelease(stack); xrtValueRelease(wider);
    xrtValueRelease(clone); xrtValueRelease(b); xrtValueRelease(a);
}
static void text(void)
{
    const xrttype* type=xl6StringTypeV1();
    xvalue* a=primitive(type,2,XL6_VALUE_ADMIT_STRING,false);
    xvalue* b=primitive(type,2,XL6_VALUE_ADMIT_STRING,false); assert(a && b);
    xstrview full={"a\0\xE4\xBD\xA0",5},prefix={"a",1};
    assert(xl6ArrayPushV1(a,LAYOUT(type,2),&full) && xl6ArrayPushV1(b,LAYOUT(type,2),&prefix));
    equal(a,b,type,2,false); assert(xl6ArraySetV1(b,LAYOUT(type,2),0,&full)); equal(a,b,type,2,true);
    xvalue* clone=xl6ArrayCloneV1(a,LAYOUT(type,2)); assert(clone); equal(a,clone,type,2,true);
    xrtValueRelease(clone); xrtValueRelease(b); xrtValueRelease(a);
    type=xl6NullableI64TypeV1(); a=primitive(type,1,XL6_VALUE_ADMIT_I8,true);
    b=primitive(type,1,XL6_VALUE_ADMIT_I8,true); assert(a && b);
    XL6_NullableI64V1 absent={false,99},otherAbsent={false,-7},present={true,7};
    assert(xl6ArrayPushV1(a,LAYOUT(type,1),&absent) && xl6ArrayPushV1(b,LAYOUT(type,1),&otherAbsent));
    equal(a,b,type,1,true); assert(xl6ArraySetV1(b,LAYOUT(type,1),0,&present)); equal(a,b,type,1,false);
    xrtValueRelease(b); xrtValueRelease(a);
    /* The resident typed float comparator normalizes +/-0 in its bit-key order.
     * This gate preserves that contract; it does not assert IEEE language ==. */
    type=xrtTypeFloat64(); a=xl6ArrayCreateV1(LAYOUT(type,1)); b=xl6ArrayCreateV1(LAYOUT(type,1)); assert(a && b);
    double plus=0.0,minus=-0.0; assert(xl6ArrayPushV1(a,LAYOUT(type,1),&plus) && xl6ArrayPushV1(b,LAYOUT(type,1),&minus));
    equal(a,b,type,1,true); assert(xl6ArraySetV1(b,LAYOUT(type,1),0,&plus)); equal(a,b,type,1,true);
    uint64 bits=UINT64_C(0x7FF8000000000013); double nan; memcpy(&nan,&bits,sizeof(nan));
    assert(xl6ArraySetV1(a,LAYOUT(type,1),0,&nan) && xl6ArraySetV1(b,LAYOUT(type,1),0,&nan)); equal(a,b,type,1,true);
    xrtValueRelease(b); xrtValueRelease(a);
}
static xvalue* references(void)
{ return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,XL6_VALUE_ADMIT_ANY,true,0); }
static xvalue* deep(void)
{
    xvalue* result=xrtValueInt(7); assert(result);
    for (unsigned i=0;i<48;++i) {
        xvalue* parent=xrtValueObject(); assert(parent && xrtValueObjectSetNew(parent,(xstrview){"child",5},result));
        result=parent;
    }
    return result;
}
static void managed(void)
{
    const xrttype* type=xrtTypeValue(); xvalue* a=references(),*b=references(); assert(a && b);
    xvalue* absent=NULL,*canonical=xrtValueNull();
    assert(xl6ArrayPushV1(a,LAYOUT(type,3),&absent) && xl6ArrayPushV1(b,LAYOUT(type,3),&canonical)); equal(a,b,type,3,true);
    xvalue* x=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3});
    xvalue* y=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(x && y && x!=y);
    assert(xl6ArrayPushV1(a,LAYOUT(type,3),&x) && xl6ArrayPushV1(b,LAYOUT(type,3),&y)); equal(a,b,type,3,true);
    xrtValueRelease(y); xrtValueRelease(x);
    x=primitive(xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8,false);
    y=primitive(xrtTypeInt64(),1,XL6_VALUE_ADMIT_I8,false); assert(x && y);
    int64_t item=7; assert(xl6ArrayPushV1(x,LAYOUT(xrtTypeInt64(),1),&item) && xl6ArrayPushV1(y,LAYOUT(xrtTypeInt64(),1),&item));
    assert(xl6ArrayPushV1(a,LAYOUT(type,3),&x) && xl6ArrayPushV1(b,LAYOUT(type,3),&y)); equal(a,b,type,3,true);
    item=8; assert(xl6ArraySetV1(y,LAYOUT(xrtTypeInt64(),1),0,&item)); equal(a,b,type,3,false);
    xrtValueRelease(y); xrtValueRelease(x);
    /* Unsupported child hashes stay unsupported, never reference fallbacks. */
    x=deep(); assert(xl6ArrayPushV1(a,LAYOUT(type,3),&x)); xrtValueRelease(x);
    uint64 output=23; assert(!xrtValueHash(a,&output) && output==23); error(XERR_TYPE);
    xrtValueRelease(b); xrtValueRelease(a);
}
static xfuture* future(void)
{
    xfuture* value=NULL; xpromise* promise=xrtPromiseCreate(&value,NULL); assert(promise && value);
    assert(xrtPromiseResolve(promise,NULL)); xrtPromiseDestroy(promise); return value;
}
static void futures(void)
{
    const xrttype* type=xrtTypeFuture(); unsigned transfer=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1;
    xvalue* a=xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,ELEMENT+1,true);
    xvalue* b=xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,ELEMENT+1,true); assert(a && b);
    xfuture* first=future(),*second=future(),*absent=NULL;
    assert(xl6ArrayPushV1(a,LAYOUT(type,transfer),&first) && xl6ArrayPushV1(b,LAYOUT(type,transfer),&first));
    equal(a,b,type,transfer,true);
    assert(xl6ArraySetV1(b,LAYOUT(type,transfer),0,&second)); equal(a,b,type,transfer,false);
    assert(xl6ArraySetV1(b,LAYOUT(type,transfer),0,&first));
    assert(xl6ArrayPushV1(a,LAYOUT(type,transfer),&absent) && xl6ArrayPushV1(b,LAYOUT(type,transfer),&absent)); equal(a,b,type,transfer,true);
    xrtFutureDestroy(first); xrtFutureDestroy(second); xrtValueRelease(b); xrtValueRelease(a);
}
static void cursors(void)
{
    const xrttype* type=xrtTypeInt64();
    xvalue* a=xl6SequenceWrapAsV1(xrtTypedArrayCreate(type),XL6_SEQUENCE_CURSOR_V1,LAYOUT(type,1));
    xvalue* b=xl6SequenceWrapAsV1(xrtTypedArrayCreate(type),XL6_SEQUENCE_CURSOR_V1,LAYOUT(type,1)); assert(a && b);
    uint64 output=23; assert(!xrtValueHash(a,&output) && output==23); error(XERR_UNSUPPORTED);
    assert(!xrtValueEqual(a,b)); error(XERR_UNSUPPORTED);
    xrtValueRelease(b); xrtValueRelease(a);
}
static void depth(unsigned count,bool expected)
{
    const xrttype* type=xrtTypeValue(); xvalue* a=xrtValueInt(7),*b=xrtValueInt(7); assert(a && b);
    for (unsigned i=0;i<count;++i) {
        xvalue* left=references(),*right=references(); assert(left && right);
        assert(xl6ArrayPushV1(left,LAYOUT(type,3),&a) && xl6ArrayPushV1(right,LAYOUT(type,3),&b));
        xrtValueRelease(a); xrtValueRelease(b); a=left; b=right;
    }
    uint64 output=23;
    if (expected) equal(a,b,type,3,true);
    else {
        assert(!xrtValueHash(a,&output) && output==23); error(XERR_RANGE);
        assert(!xrtValueEqual(a,b)); error(XERR_RANGE);
        assert(!xl6ArrayEqualsV1(a,b,LAYOUT(type,3))); error(XERR_RANGE);
    }
    xrtValueRelease(b); xrtValueRelease(a);
    /* Every refusal unwinds the thread's call path and every storage pin. */
    scalar();
}
typedef struct Worker { uint64 thread; bool done; } Worker;
static xatomic32 workersReady,workersStart;
static void worker(Worker* state)
{
    xrtownershipscope scope={0}; assert(xrtOwnershipMutationBegin(&scope)); state->thread=scope.Thread;
    assert(xrtOwnershipScopeEnd(&scope));
    xrtAtomic32FetchAdd(&workersReady,1,XMEMORY_RELEASE);
    while (!xrtAtomic32Load(&workersStart,XMEMORY_ACQUIRE)) {
#if defined(_WIN32)
        Sleep(0);
#else
        sched_yield();
#endif
    }
    for (unsigned i=0;i<4;++i) depth(16,true);
    state->done=!xrtGetError();
}
#if defined(_WIN32)
static DWORD WINAPI workerEntry(void* state) { worker(state); return 0; }
#else
static void* workerEntry(void* state) { worker(state); return NULL; }
#endif
static void concurrency(void)
{
    enum { count=80 }; Worker states[count]={0};
#if defined(_WIN32)
    HANDLE threads[count];
    for (unsigned i=0;i<count;++i) { threads[i]=CreateThread(NULL,0,workerEntry,&states[i],0,NULL); assert(threads[i]); }
#else
    pthread_t threads[count];
    for (unsigned i=0;i<count;++i) assert(!pthread_create(&threads[i],NULL,workerEntry,&states[i]));
#endif
    while (xrtAtomic32Load(&workersReady,XMEMORY_ACQUIRE)!=count) {
#if defined(_WIN32)
        Sleep(0);
#else
        sched_yield();
#endif
    }
    xrtAtomic32Store(&workersStart,1,XMEMORY_RELEASE);
    for (unsigned i=0;i<count;++i) {
#if defined(_WIN32)
        assert(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0 && CloseHandle(threads[i]));
#else
        assert(!pthread_join(threads[i],NULL));
#endif
        assert(states[i].done);
        for (unsigned j=0;j<i;++j) assert(states[i].thread!=states[j].thread);
    }
    puts("identity concurrency: 80 simultaneous threads, colliding lanes, independent call paths");
}
static unsigned hashCalls,compareCalls,mode;
static xerror* callbackFailure;
static const xerror* arm(unsigned selected)
{
    xrtSetErrorKind(XERR_VALUE); callbackFailure=xrtTakeError(); mode=selected;
    hashCalls=compareCalls=0; return callbackFailure;
}
static void fail(void) { xrtSetErrorTake(callbackFailure); callbackFailure=NULL; }
static uint64 typedHash(const void* slot,const xrttype* type)
{
    (void)type; ++hashCalls;
    if (mode==1 && hashCalls==1) fail();
    else if (mode==1) xrtClearError(); /* Must never hide first-slot failure. */
    return (uint64)*(const int64_t*)slot;
}
static int typedCompare(const void* a,const void* b,const xrttype* type)
{
    (void)type; ++compareCalls;
    if (mode==2 && compareCalls==1) fail();
    else if (mode==2) xrtClearError();
    return (*(const int64_t*)a>*(const int64_t*)b)-(*(const int64_t*)a<*(const int64_t*)b);
}
static const xrttypeops customOps={.Hash=typedHash,.Compare=typedCompare};
static const xrttype customType={.Id=UINT64_C(0x4754653DEB39D662),.Kind=XRT_TYPE_SIGNED_INT,
    .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP|XRT_TYPE_FLAG_RELOCATABLE,
    .Name=XRT_STR_INIT("identity-test"),.AbiName=XRT_STR_INIT("identity-test"),
    .Size=sizeof(int64_t),.Align=_Alignof(int64_t),.InstanceAlign=1,.Ops=&customOps};
static void callbacks(void)
{
    const xrttype* type=&customType; xvalue* a=xl6ArrayCreateV1(LAYOUT(type,1)),*b=xl6ArrayCreateV1(LAYOUT(type,1)); assert(a && b);
    int64_t item=7;
    for (unsigned i=0;i<3;++i) assert(xl6ArrayPushV1(a,LAYOUT(type,1),&item) && xl6ArrayPushV1(b,LAYOUT(type,1),&item));
    uint64 output=23; const xerror* original=arm(1);
    assert(!xrtValueHash(a,&output) && output==23 && hashCalls==1 && xrtGetError()==original); xrtClearError();
    original=arm(2); assert(!xrtValueEqual(a,b) && compareCalls==1 && xrtGetError()==original); xrtClearError();
    original=arm(2); assert(!xl6ArrayEqualsV1(a,b,LAYOUT(type,1)) && compareCalls==1 && xrtGetError()==original); xrtClearError();
    mode=0; equal(a,b,type,1,true);
    xrtValueRelease(b); xrtValueRelease(a);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context; const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static void cycles(void)
{
    const xrttype* type=xrtTypeValue(); xvalue* a=references(),*b=references(); assert(a && b);
    assert(xl6ArrayPushV1(a,LAYOUT(type,3),&a) && xl6ArrayPushV1(b,LAYOUT(type,3),&b));
    uint64 output=23; assert(!xrtValueHash(a,&output) && output==23); error(XERR_STATE);
    assert(!xrtValueEqual(a,b)); error(XERR_STATE);
    /* Identity does not silently turn a cyclic graph into address equality;
     * its refusal releases every pin, leaving the real retirement plan usable. */
    xvalue* owners[]={a,b};
    for (unsigned i=0;i<2;++i) {
        xvalue* weak=xrtValueWeakRef(owners[i]); assert(weak);
        xrtValueRelease(owners[i]); XL6_OwnershipPlan* plan=NULL;
        assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(owners[i]),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    }
}
static void fault(size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    const xrttype* type=xrtTypeValue(); xvalue* a=references(),*b=references(),*x=deep(),*y=deep(); assert(a && b);
    assert(xl6ArrayPushV1(a,LAYOUT(type,3),&x) && xl6ArrayPushV1(b,LAYOUT(type,3),&y));
    xrtValueRelease(x); xrtValueRelease(y);
    assert(xrtMemDebugFailAfter(point)); bool same=xrtValueEqual(a,b);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!same); error(XERR_MEMORY); }
    else assert(same && !xrtGetError());
    assert(xrtValueEqual(a,b) && xl6ArrayEqualsV1(a,b,LAYOUT(type,3)) && !xrtGetError());
    xrtValueRelease(b); xrtValueRelease(a); balanced(&before);
    printf("identity equality point=%zu hit=%d balanced\n",point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2); assert(xrtMemDebugEnable(true));
    if (argc==2) fault((size_t)strtoull(argv[1],NULL,10));
    else {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        depth(XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1,true);
        depth(XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1+1,false);
        concurrency();
        for (unsigned i=0;i<30;++i) { scalar(); text(); managed(); futures(); cursors(); callbacks(); cycles(); }
        balanced(&before); puts("sequence identity: 30 cycles, scalar/text/managed/future/error/graph balanced");
    }
    return 0;
}
