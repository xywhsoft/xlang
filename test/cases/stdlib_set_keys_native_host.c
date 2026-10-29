#include "src/runtime/xl6_set_key_policy.h"
#include "src/runtime/xl6_sequence.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Real callback-boundary refusal injection, not a replacement key algorithm. */
static bool refuseEntry;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (refuseEntry) { refuseEntry=false; xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
static const xrttype* references(void)
{ return xl6SequenceStorageTypeV1(3,xrtTypeValue()); }
static void clearError(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void keys(void)
{
    const xrttype* type=references(); const xtypedsetkeypolicy* policy=xl6SetReferenceKeyPolicyV1();
    assert(type && !type->Ops->Compare && !type->Ops->Hash && policy && !policy->UserData);
    assert(policy==xl6SetReferenceKeyPolicyV1() && policy!=xl6SetFutureKeyPolicyV1());
    xtypedset set; assert(!xrtTypedSetInit(&set,type)); clearError(XERR_UNSUPPORTED);
    assert(xrtTypedSetInitWithKeyPolicy(&set,type,policy));
    const char data[]={'a',0,'b',(char)0xe4,(char)0xbd,(char)0xa0};
    xvalue* one=xrtValueString((xstrview){data,sizeof(data)});
    xvalue* same=xrtValueString((xstrview){data,sizeof(data)});
    xvalue* shortText=xrtValueString((xstrview){data,1});
    xvalue* bytes=xrtValueBytes((xbytesview){(const uint8*)data,sizeof(data)});
    xvalue* integer=xrtValueInt(INT64_MIN),*character=xrtValueChar(0x4f60);
    xvalue* absent=NULL,*nullValue=xrtValueNull();
    assert(one && same && shortText && bytes && integer && character);
    assert(xrtTypedSetAdd(&set,&one) && xrtTypedSetAdd(&set,&same) && xrtTypedSetCount(&set)==1);
    assert(*(xvalue* const*)xrtTypedSetGet(&set,&same)==one);
    assert(xrtTypedSetAdd(&set,&shortText) && xrtTypedSetAdd(&set,&bytes) &&
        xrtTypedSetAdd(&set,&integer) && xrtTypedSetAdd(&set,&character));
    assert(xrtTypedSetAdd(&set,&absent) && xrtTypedSetAdd(&set,&nullValue) && xrtTypedSetCount(&set)==6);
    uint64 a=policy->Hash(&absent,NULL),b=policy->Hash(&nullValue,NULL);
    assert(a==b && policy->Equal(&absent,&nullValue,NULL) && !xrtGetError());
    assert(!policy->Equal(&one,&shortText,NULL) && !xrtGetError());
    xerror* incoming=xrtErrorCreate(XERR_ARGUMENT,"test.incoming",7,"prior"); assert(incoming);
    xrtSetError(incoming); assert(xrtTypedSetHas(&set,&one) && xrtGetError()==incoming);
    xrtClearError(); xrtErrorFree(incoming);
    xtypedset* copy=xrtTypedSetClone(&set); assert(copy && copy->KeyPolicy==policy);
    assert(xrtTypedSetEquals(copy,&set) && *(xvalue* const*)xrtTypedSetGet(copy,&same)==one);
    xrtTypedSetDestroy(copy); xrtTypedSetUnit(&set);
    xrtValueRelease(character); xrtValueRelease(integer); xrtValueRelease(bytes);
    xrtValueRelease(shortText); xrtValueRelease(same); xrtValueRelease(one);
    assert(!type->Ops->Compare && !type->Ops->Hash && !xrtGetError());
}
typedef struct CallbackState { unsigned calls; } CallbackState;
static bool cloneHandle(ptr item,ptr* output,ptr context)
{ (void)context; *output=item; return true; }
static void drop(ptr item,ptr context) { (void)item; (void)context; }
static uint64 handleHash(ptr item,ptr context)
{
    (void)context; CallbackState* state=item;
    ++state->calls; xrtSetErrorKind(XERR_VALUE); return 0;
}
static bool handleEqual(ptr left,ptr right,ptr context)
{
    (void)right; (void)context; CallbackState* state=left;
    ++state->calls; xrtSetErrorKind(XERR_VALUE); return true;
}
static const xvaluehandleops handleOps={cloneHandle,drop,handleHash,handleEqual};
static void errors(void)
{
    const xtypedsetkeypolicy* policy=xl6SetReferenceKeyPolicyV1();
    {
        CallbackState state={0},other={0}; ptr raw=&state;
        xvalue* a=xrtValueHandleTake(&raw,&handleOps,NULL); raw=&state;
        xvalue* b=xrtValueHandleTake(&raw,&handleOps,NULL); assert(a && b && a!=b);
        raw=&other; xvalue* c=xrtValueHandleTake(&raw,&handleOps,NULL); assert(c);
        xtypedset set; assert(xrtTypedSetInitWithKeyPolicy(&set,references(),policy));
        assert(xrtTypedSetAdd(&set,&a) && xrtTypedSetHas(&set,&b));
        assert(xrtTypedSetAdd(&set,&c) && xrtTypedSetCount(&set)==2);
        assert(!state.calls && !other.calls && !xrtGetError());
        for (unsigned lane=0;lane<2;++lane) {
            xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
            refuseEntry=true;
            assert(!xrtTypedSetAdd(&set,lane ? &c : &b) && xrtTypedSetCount(&set)==2);
            xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
            assert(!result.error);
            xrtErrorFree(result.error); xrtClearError();
        }
        assert(xrtTypedSetHas(&set,&b) && !xrtGetError());
        xrtTypedSetUnit(&set); xrtValueRelease(c); xrtValueRelease(b); xrtValueRelease(a);
    }
    (void)policy->Hash(NULL,NULL); clearError(XERR_ARGUMENT);
    assert(!policy->Equal(NULL,NULL,NULL)); clearError(XERR_ARGUMENT);
    xvalue* item=NULL; (void)policy->Hash(&item,(ptr)policy); clearError(XERR_ARGUMENT);
}
static void depth(void)
{
    XL6_CollectionIdentityFrameV1 frames[XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1]={0};
    const xtypedsetkeypolicy* policy=xl6SetReferenceKeyPolicyV1(); xvalue* item=xrtValueInt(7); assert(item);
    for (unsigned i=0;i<XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1;++i) assert(xl6CollectionIdentityEnterV1(&frames[i]));
    /* Stable keys never walk container contents or consume shared depth. */
    (void)policy->Hash(&item,NULL); assert(!xrtGetError());
    assert(policy->Equal(&item,&item,NULL) && !xrtGetError());
    for (unsigned i=XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1;i-->0;) xl6CollectionIdentityLeaveV1(&frames[i]);
    assert(policy->Equal(&item,&item,NULL) && !xrtGetError()); xrtValueRelease(item);
}
static void mutableKeys(void)
{
    const xtypedsetkeypolicy* policy=xl6SetReferenceKeyPolicyV1(); xtypedset set;
    xvalue* a=xrtValueArray(),*b=xrtValueArray(),*one=xrtValueInt(1),*two=xrtValueInt(2);
    assert(a && b && one && two);
    assert(xrtValueArrayAppend(a,one) && xrtValueArrayAppend(b,one));
    assert(xrtTypedSetInitWithKeyPolicy(&set,references(),policy));
    assert(xrtTypedSetAdd(&set,&a) && xrtTypedSetAdd(&set,&b) && xrtTypedSetCount(&set)==2);
    uint64 before=policy->Hash(&a,NULL);
    xvalue* copy=xrtValueClone(a); assert(copy && copy!=a);
    assert(xrtTypedSetAdd(&set,&copy) && xrtTypedSetCount(&set)==3);
    assert(xrtValueArraySet(a,0,two)); /* Detaches shared COW backing. */
    assert(before==policy->Hash(&a,NULL) && xrtTypedSetHas(&set,&a));
    assert(!policy->Equal(&a,&b,NULL) && !policy->Equal(&a,&copy,NULL));
    assert(xrtTypedSetRemove(&set,&a) && xrtTypedSetCount(&set)==2);
    xrtTypedSetUnit(&set); xrtValueRelease(copy); xrtValueRelease(two); xrtValueRelease(one);
    xrtValueRelease(b); xrtValueRelease(a); assert(!xrtGetError());
}
static void futures(void)
{
    const xtypedsetkeypolicy* policy=xl6SetFutureKeyPolicyV1(); xtypedset set;
    xfuture* a=NULL,*b=NULL,*absent=NULL;
    xpromise* first=xrtPromiseCreate(&a,NULL),*second=xrtPromiseCreate(&b,NULL); assert(first && second && a && b);
    assert(xrtTypedSetInitWithKeyPolicy(&set,xrtTypeFuture(),policy));
    assert(xrtTypedSetAdd(&set,&a) && xrtTypedSetAdd(&set,&a) && xrtTypedSetAdd(&set,&b) &&
        xrtTypedSetAdd(&set,&absent) && xrtTypedSetCount(&set)==3);
    uint64 before=policy->Hash(&a,NULL);
    assert(xrtPromiseResolve(first,NULL) && xrtPromiseResolve(second,NULL));
    assert(policy->Hash(&a,NULL)==before && !policy->Equal(&a,&b,NULL) && xrtTypedSetHas(&set,&a));
    xtypedset* clone=xrtTypedSetClone(&set); assert(clone && clone->KeyPolicy==policy && xrtTypedSetEquals(clone,&set));
    xrtTypedSetDestroy(clone); xrtTypedSetUnit(&set);
    xrtPromiseDestroy(second); xrtPromiseDestroy(first); xrtFutureDestroy(b); xrtFutureDestroy(a); assert(!xrtGetError());
}
static void prefixes(void)
{
    size_t attempts=0;
    for (unsigned lane=0;lane<4;++lane) {
        bool complete=false;
        for (size_t point=0;point<1024;++point) {
            xvalue* a=xrtValueString(XRT_STR_LITERAL("a\0b"));
            xvalue* b=xrtValueString(XRT_STR_LITERAL("a\0c")); assert(a && b);
            xtypedset left,right; const xtypedsetkeypolicy* policy=xl6SetReferenceKeyPolicyV1();
            assert(xrtTypedSetInitWithKeyPolicy(&left,references(),policy) &&
                xrtTypedSetInitWithKeyPolicy(&right,references(),policy) &&
                xrtTypedSetAdd(&left,&a) && xrtTypedSetAdd(&right,&b));
            xl6ProductMemoryBegin(point); xtypedset* result=NULL; bool ok=false;
            switch (lane) {
            case 0: result=xrtTypedSetClone(&left); break;
            case 1: result=xrtTypedSetUnion(&left,&right); break;
            case 2: ok=xrtTypedSetMerge(&left,&right); break;
            default: ok=xrtTypedSetAdd(&left,&b); break;
            }
            bool hit=xl6ProductMemoryEnd(); ++attempts; if (lane<2) ok=result!=NULL;
            xrtClearError(); assert(hit ? !ok && xrtTypedSetCount(&left)==1 : ok);
            if (result) { assert(result->KeyPolicy==policy); xrtTypedSetDestroy(result); }
            if (ok && lane>=2) assert(xrtTypedSetRemove(&left,&b));
            xl6ProductMemoryVerify(); xrtTypedSetUnit(&right); xrtTypedSetUnit(&left);
            xrtValueRelease(b); xrtValueRelease(a); xrtClearError();
            if (!hit) { complete=true; printf("resident-key lane=%u attempts=%zu first-no-hit=%zu\n",lane,point+1,point); break; }
        }
        assert(complete);
    }
    printf("resident-key full-prefix-attempts=%zu\n",attempts);
}
int main(void)
{
    /* Establish real resident TLS/identity before the allocation baseline. */
    assert(xrtMemDebugEnable(true)); xl6_error_scope warm={0}; assert(xl6ErrorScopeBegin(&warm));
    assert(xl6ErrorScopeEnd(&warm).ok); XL6_CollectionIdentityFrameV1 frame={0};
    assert(xl6CollectionIdentityEnterV1(&frame)); xl6CollectionIdentityLeaveV1(&frame);
    xl6ProductMemoryBegin(SIZE_MAX); keys(); errors(); depth(); futures(); mutableKeys(); xl6ProductMemoryVerify();
    prefixes(); puts("[PASS] resident typed Set key policies"); return 0;
}
