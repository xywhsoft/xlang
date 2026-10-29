#include "src/runtime/xl6_set_schema.h"
#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_resident_reference_schema.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TUPLE(t) (t).identity,(t).elementIdentity,(t).transfer,(t).size,(t).type
static unsigned entryFailure,cleanupFailure;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (entryFailure) { unsigned kind=entryFailure; entryFailure=0;
        if (kind==2) xrtSetErrorKind(XERR_TIMEOUT);
        return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failed)
{
    if (cleanupFailure) { cleanupFailure=0; return false; }
    return __real_xl6ErrorScopeBeginCleanup(scope,failed);
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static XL6_SetLayoutV1 leafLayout(unsigned profile)
{
    const xrttype* type=profile==0 ? xrtTypeInt64() : profile==1 ? xl6StringTypeV1() :
        profile==2 ? xrtTypeValue() : xrtTypeFuture();
    return (XL6_SetLayoutV1){400+profile,{500+profile,600+profile,
        profile==0 ? 1 : profile==1 ? 2 : profile==2 ? 3 : 4,type->Size,type}};
}
static xvalue* leaf(unsigned profile,bool wide)
{
    XL6_SetLayoutV1 t=leafLayout(profile); xvalue* domain;
    if (profile==0) domain=xl6SequencePrimitiveCreateV1(1,TUPLE(t.value),
        wide ? XL6_VALUE_ADMIT_I64 : XL6_VALUE_ADMIT_I8,false,t.value.elementIdentity);
    else if (profile==1) domain=xl6SequencePrimitiveCreateV1(1,TUPLE(t.value),
        XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity+(wide ? 100 : 0));
    else if (profile==2) domain=xl6SequenceReferenceCreateV1(1,t.value.identity,t.value.elementIdentity,
        wide ? XL6_VALUE_ADMIT_ANY : XL6_VALUE_ADMIT_BYTES,false,wide ? 0 : t.value.elementIdentity);
    else domain=xl6SequenceFutureCreateV1(1,t.value.identity,t.value.elementIdentity,
        t.value.elementIdentity+(wide ? 100 : 0),wide);
    return xl6SetCreateTakeV1(&t,domain);
}
static void fill(xvalue* value,unsigned profile)
{
    XL6_SetLayoutV1 t=leafLayout(profile); bool added=false;
    if (profile==0) { int64_t n=7; assert(xl6SetAddV1(value,&t,&n,&added) && added); }
    else if (profile==1) { xstrview s={"a\0\xE4\xBD\xA0",5}; assert(xl6SetAddV1(value,&t,&s,&added) && added); }
    else if (profile==2) {
        xvalue* b=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3});
        assert(b && xl6SetAddV1(value,&t,&b,&added) && added); xrtValueRelease(b);
    } else {
        xfuture* f=NULL; xpromise* p=xrtPromiseCreate(&f,NULL); assert(p && f);
        assert(xl6SetAddV1(value,&t,&f,&added) && added); xrtFutureDestroy(f); xrtPromiseDestroy(p);
    }
}
typedef struct Parent { unsigned family; XL6_SetLayoutV1 layout; bool nullable; } Parent;
static Parent parent(unsigned family,unsigned profile,bool nullable)
{
    uint64_t id=1000+family*100+profile*10+nullable;
    return (Parent){family,{id,{family<3 ? id : id+10000,nullable ? 2000+profile : 400+profile,
        3,sizeof(xvalue*),xrtTypeValue()}},nullable};
}
static xvalue* createParent(const Parent* p,xvalue* child)
{
    XL6_SequenceLayoutV1 t=p->layout.value;
    xvalue* domain=xl6SequenceSetCreateTakeV1(p->family==2 ? 2 : 1,t.identity,t.elementIdentity,child,p->nullable);
    if (p->family==4) return xl6SetCreateTakeV1(&p->layout,domain);
    if (p->family==3) { XL6_ListLayoutV1 l={p->layout.identity,t}; return xl6ListCreateTakeV1(&l,domain); }
    return domain;
}
static bool put(const Parent* p,xvalue* owner,xvalue* child,bool* added)
{
    if (p->family==4) return xl6SetAddV1(owner,&p->layout,&child,added);
    if (p->family==3) { XL6_ListLayoutV1 l={p->layout.identity,p->layout.value};
        return xl6ListSetV1(owner,&l,xl6ListCountV1(owner,&l),&child); }
    XL6_SequenceLayoutV1 t=p->layout.value;
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,p->family,TUPLE(t)); if (!pin) return false;
    bool ok=xl6SequenceAdmitSlotV1(pin,&child) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&child);
    xl6SequenceEndV1(pin); return ok;
}
static void profiles(void)
{
    for (unsigned profile=0;profile<4;++profile) for (unsigned family=1;family<=4;++family)
    for (unsigned nullable=0;nullable<2;++nullable) {
        Parent p=parent(family,profile,nullable); XL6_SetLayoutV1 t=leafLayout(profile),captured={0};
        XL6_SequenceAdmissionV3 policy={0}; xvalue* good=leaf(profile,false); assert(good); fill(good,profile);
        assert(xl6SetSchemaCopyV1(good,&captured,&policy));
        xvalue* weak=xrtValueWeakRef(good); assert(weak);
        xvalue* owner=createParent(&p,xrtValueRetain(good)); assert(owner);
        bool added=false; assert(put(&p,owner,good,&added));
        if (family==4) { assert(added && put(&p,owner,good,&added) && !added); }
        xvalue* same=leaf(profile,false); assert(same && xl6SetAdmitsSchemaV1(same,&captured,&policy));
        assert(put(&p,owner,same,&added)); if (family==4) assert(added); xrtValueRelease(same);
        xvalue* wrong=leaf(profile,true); assert(wrong && !xl6SetAdmitsSchemaV1(wrong,&captured,&policy));
        assert(!put(&p,owner,wrong,&added)); error(XERR_TYPE); xrtValueRelease(wrong);
        wrong=xrtValueSet(); assert(wrong && xrtValueTypeIdBind(wrong,t.identity));
        assert(!put(&p,owner,wrong,&added)); error(XERR_TYPE); xrtValueRelease(wrong);
        XL6_SequenceAdmissionV3 copy={0}; XL6_SetLayoutV1 layout={0};
        assert(xl6SetSchemaCopyV1(good,&layout,&copy));
        xvalue* domain=xl6SequenceCreateAdmittedV1(1,layout.value.identity,&copy); assert(domain);
        XL6_ListLayoutV1 list={layout.identity,layout.value}; wrong=xl6ListCreateTakeV1(&list,domain); assert(wrong);
        assert(!put(&p,owner,wrong,&added)); error(XERR_TYPE); xrtValueRelease(wrong);
        if (nullable) assert(put(&p,owner,NULL,&added));
        else { assert(!put(&p,owner,NULL,&added)); error(XERR_TYPE); }
        /* Metadata comparison remains allocation-free and preserves incoming error. */
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError(); assert(xrtMemDebugFailAfter(0));
        assert(xl6SetAdmitsSchemaV1(good,&captured,&policy) && xl6SetSchemaCopyV1(good,&layout,&copy));
        assert(xrtGetError()==pending && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
        xl6SequenceAdmissionDropV3(&copy);
        if (family==4) {
            if (!profile) { int64_t n=8; assert(xl6SetAddV1(good,&t,&n,&added) && added); }
            assert(xl6SetKeyV1(owner,&p.layout,&good,false,&added) && added);
            xvalue* clone=xl6SetCloneV1(owner,&p.layout); assert(clone);
            assert(xl6SetKeyV1(clone,&p.layout,&good,false,&added) && added); xrtValueRelease(clone);
        }
        xrtValueRelease(good); assert(!xrtValueWeakRefExpired(weak));
        xrtValueRelease(owner); assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
        /* Only metadata remains; no original child wrapper is needed. */
        same=leaf(profile,false); wrong=leaf(profile,true); assert(same && wrong);
        assert(xl6SetAdmitsSchemaV1(same,&captured,&policy) && !xl6SetAdmitsSchemaV1(wrong,&captured,&policy));
        xrtValueRelease(same); xrtValueRelease(wrong); xl6SequenceAdmissionDropV3(&policy);
    }
}
/* Shared metadata depth across Array, Stack, List and Set; every ancestor has
 * the same IDs and physical tuple even when the leaf's logical domain differs. */
static xvalue* chain(unsigned depth,bool wide,unsigned* family,XL6_SetLayoutV1* layout)
{
    xvalue* value=leaf(2,wide); *family=4; *layout=leafLayout(2);
    for (unsigned at=1;value && at<=depth;++at) {
        uint64_t child=family[0]<3 ? layout->value.identity : layout->identity;
        unsigned next=1+at%4; uint64_t id=40000+at;
        XL6_SequenceLayoutV1 t={next<3 ? id : id+10000,child,3,sizeof(xvalue*),xrtTypeValue()};
        xvalue* domain;
        if (*family==4) domain=xl6SequenceSetCreateTakeV1(next==2 ? 2 : 1,t.identity,child,value,false);
        else if (*family==3) domain=xl6SequenceListCreateTakeV1(next==2 ? 2 : 1,t.identity,child,value,false);
        else domain=xl6SequenceNestedCreateTakeV2(next==2 ? 2 : 1,t.identity,child,value,false);
        *family=next; *layout=(XL6_SetLayoutV1){id,t};
        if (next==4) value=xl6SetCreateTakeV1(layout,domain);
        else if (next==3) { XL6_ListLayoutV1 l={id,t}; value=xl6ListCreateTakeV1(&l,domain); }
        else value=domain;
    }
    return value;
}
static void depths(void)
{
    unsigned family=0; XL6_SetLayoutV1 l={0}; xvalue* good=chain(64,false,&family,&l); assert(good && family==1);
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 t={0}; XL6_SequenceAdmissionV3 p={0};
    assert(xl6SequenceSchemaCopyV1(good,&role,&t,&p) && xl6ResidentAdmissionDepthV1(&p)==64);
    xvalue* same=chain(64,false,&family,&l),*wrong=chain(64,true,&family,&l); assert(same && wrong);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceAdmitsSchemaV1(same,role,&t,&p) && !xl6SequenceAdmitsSchemaV1(wrong,role,&t,&p));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(!chain(65,false,&family,&l)); error(XERR_RANGE);
    xrtValueRelease(good); xrtValueRelease(same); xrtValueRelease(wrong); xl6SequenceAdmissionDropV3(&p);
}
static void boundaries(void)
{
    for (unsigned mode=1;mode<=2;++mode) for (unsigned cleanup=0;cleanup<2;++cleanup) {
        xvalue* source=leaf(0,false),*weak=xrtValueWeakRef(source); assert(source && weak);
        xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer)); entryFailure=mode; cleanupFailure=cleanup;
        assert(!xl6SequenceSetCreateTakeV1(1,900,400,source,false));
        xl6_error_result result=xl6ErrorScopeEnd(&outer); assert(!result.ok && xrtValueWeakRefExpired(weak));
        assert(mode==1 ? !result.error : result.error && xrtErrorKind(result.error)==XERR_TIMEOUT);
        xrtErrorFree(result.error); xrtValueRelease(weak); assert(!xrtGetError());
    }
    for (unsigned bad=0;bad<3;++bad) {
        xvalue* source=leaf(0,false); assert(source);
        assert(!xl6SequenceSetCreateTakeV1(bad==0 ? 3 : 1,900,bad==1 ? 401 : 400,source,bad==2)); error(XERR_ARGUMENT);
    }
    xvalue* source=leaf(0,false),*weak=xrtValueWeakRef(source); assert(source && weak);
    xvalue* owner=xl6SequenceSetCreateTakeV1(1,900,400,source,false); assert(owner && xrtValueWeakRefExpired(weak));
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,1,900,400,3,sizeof(xvalue*),xrtTypeValue()); assert(pin);
    assert(!xl6SequenceResizeStorageV1(pin,1)); error(XERR_TYPE); xl6SequenceEndV1(pin); xrtValueRelease(owner); xrtValueRelease(weak);
    source=leaf(0,false); assert(source); owner=xl6SequenceSetCreateTakeV1(1,900,901,source,true); assert(owner);
    pin=xl6SequenceBeginRoleV1(owner,1,900,901,3,sizeof(xvalue*),xrtTypeValue()); assert(pin);
    assert(xl6SequenceResizeStorageV1(pin,2) && !*(xvalue* const*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0));
    xl6SequenceEndV1(pin); xrtValueRelease(owner);
}
static void prefixes(void)
{
    size_t attempts=0;
    for (unsigned lane=0;lane<5;++lane) {
        bool complete=false;
        for (size_t point=0;point<4097;++point) {
            xl6ProductMemoryBegin(SIZE_MAX); xvalue* source=lane<4 ? leaf(1,false) : NULL;
            if (source) fill(source,1);
            xvalue* weak=source ? xrtValueWeakRef(source) : NULL;
            xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope) && xrtMemDebugFailAfter(point));
            unsigned family=0; XL6_SetLayoutV1 tuple={0}; Parent p=parent(lane+1,1,false);
            xvalue* result=lane<4 ? createParent(&p,source) : chain(64,false,&family,&tuple);
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
            assert(hit ? !result && !outcome.ok : result && outcome.ok);
            if (hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
            if (weak) { assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); }
            xrtValueRelease(result); xrtErrorFree(outcome.error); assert(!xrtGetError());
            assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); ++attempts;
            printf("Set schema lane=%u point=%zu hit=%d balanced\n",lane,point,hit);
            if (!hit) { complete=true; printf("Set schema lane=%u first-no-hit=%zu\n",lane,point); break; }
        }
        assert(complete);
    }
    printf("Set schema full-prefix-attempts=%zu\n",attempts);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xl6_error_scope warm={0}; assert(xl6ErrorScopeBegin(&warm) && xl6ErrorScopeEnd(&warm).ok);
    for (unsigned i=0;i<3;++i) { xrtSetErrorKind(i==0 ? XERR_TYPE : i==1 ? XERR_ARGUMENT : XERR_TIMEOUT); xrtClearError(); }
    xl6ProductMemoryBegin(SIZE_MAX); profiles(); depths(); boundaries(); assert(!xrtGetError()); xl6ProductMemoryVerify();
    prefixes(); puts("[PASS] complete resident Set reference domains"); return 0;
}
