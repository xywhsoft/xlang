#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_resident_reference_schema.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_stack.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define TUPLE(t) (t).identity,(t).elementIdentity,(t).transfer,(t).size,(t).type
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static XL6_ListLayoutV1 leafLayout(unsigned profile)
{
    const xrttype* type=profile==0 ? xrtTypeInt64() : profile==1 ? xl6StringTypeV1() :
        profile==2 ? xrtTypeValue() : xrtTypeFuture();
    return (XL6_ListLayoutV1){400+profile,{500+profile,600+profile,
        profile==0 ? 1 : profile==1 ? 2 : profile==2 ? 3 : 4,type->Size,type}};
}
static xvalue* leaf(unsigned profile,bool wide)
{
    XL6_ListLayoutV1 t=leafLayout(profile); xvalue* domain;
    if (profile==0) domain=xl6SequencePrimitiveCreateV1(1,TUPLE(t.value),
        wide ? XL6_VALUE_ADMIT_I64 : XL6_VALUE_ADMIT_I8,false,t.value.elementIdentity);
    else if (profile==1) domain=xl6SequencePrimitiveCreateV1(1,TUPLE(t.value),
        XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity+(wide ? 100 : 0));
    else if (profile==2) domain=xl6SequenceReferenceCreateV1(1,t.value.identity,t.value.elementIdentity,
        wide ? XL6_VALUE_ADMIT_ANY : XL6_VALUE_ADMIT_BYTES,false,wide ? 0 : t.value.elementIdentity);
    else domain=xl6SequenceFutureCreateV1(1,t.value.identity,t.value.elementIdentity,
        t.value.elementIdentity+(wide ? 100 : 0),wide);
    return xl6ListCreateTakeV1(&t,domain);
}
static void fill(xvalue* value,unsigned profile)
{
    XL6_ListLayoutV1 t=leafLayout(profile);
    if (profile==0) { int64_t n=7; assert(xl6ListSetV1(value,&t,INT64_MIN,&n)); }
    else if (profile==1) { xstrview text={"a\0\xE4\xBD\xA0",5}; assert(xl6ListSetV1(value,&t,INT64_MIN,&text)); }
    else if (profile==2) {
        xvalue* bytes=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3});
        assert(bytes && xl6ListSetV1(value,&t,INT64_MIN,&bytes)); xrtValueRelease(bytes);
    } else {
        xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
        assert(xl6ListSetV1(value,&t,INT64_MIN,&future)); xrtFutureDestroy(future); xrtPromiseDestroy(promise);
    }
}
typedef struct Parent {
    unsigned family; /* ARRAY, STACK, LIST; List's metadata domain is an ARRAY. */
    XL6_ListLayoutV1 layout;
    bool nullable;
} Parent;
static Parent parent(unsigned family,unsigned profile,bool nullable)
{
    uint64_t identity=1000+family*100+profile*10+(nullable ? 1 : 0);
    return (Parent){family,{identity,{identity+10000,nullable ? 2000+profile : 400+profile,
        3,sizeof(xvalue*),xrtTypeValue()}},nullable};
}
static XL6_SequenceLayoutV1 parentTuple(const Parent* p)
{
    XL6_SequenceLayoutV1 result=p->layout.value;
    if (p->family!=3) result.identity=p->layout.identity;
    return result;
}
static xvalue* createParent(const Parent* p,xvalue* child)
{
    XL6_SequenceLayoutV1 t=parentTuple(p);
    xvalue* domain=xl6SequenceListCreateTakeV1(p->family==2 ? 2 : 1,t.identity,t.elementIdentity,child,p->nullable);
    return p->family==3 ? xl6ListCreateTakeV1(&p->layout,domain) : domain;
}
static bool put(const Parent* p,xvalue* owner,xvalue* child,bool dynamic)
{
    XL6_SequenceLayoutV1 t=parentTuple(p);
    if (p->family==3) return dynamic ? xl6ListErasedSetV1(owner,17,child ? child : xrtValueNull()) :
        xl6ListSetV1(owner,&p->layout,17,&child);
    if (!dynamic) return p->family==1 ? xl6ArrayPushV1(owner,TUPLE(t),&child) : xl6StackPushV1(owner,TUPLE(t),&child);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,p->family,TUPLE(t)); assert(pin);
    bool ok=xl6SequenceAdmitValueV1(pin,child ? child : xrtValueNull()) &&
        xrtTypedArrayPushValue(xl6SequenceStorageV1(pin),child ? child : xrtValueNull(),xl6ArrayReferenceConverterV1());
    xl6SequenceEndV1(pin); return ok;
}
static void bind(const Parent* p,xvalue* owner)
{
    /* Stack has no erased-Array projection binding. Its dynamic boundary is
     * the independently checked resident Value policy and canonical codec. */
    if (p->family==2) return;
    if (p->family==3) { assert(xl6ListProjectionTakeV1(xrtValueRetain(owner),&p->layout,NULL)==owner); xrtValueRelease(owner); }
    else {
        XL6_SequenceLayoutV1 t=parentTuple(p);
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,p->family,TUPLE(t)); assert(pin);
        assert(xl6SequenceBindProjectionV1(pin,xl6ArrayReferenceConverterV1())); xl6SequenceEndV1(pin);
    }
}
static xvalue* clone(const Parent* p,xvalue* value)
{
    XL6_SequenceLayoutV1 t=parentTuple(p);
    return p->family==3 ? xl6ListCloneV1(value,&p->layout) : p->family==1 ?
        xl6ArrayCloneV1(value,TUPLE(t)) : xl6StackCloneV1(value,TUPLE(t));
}
static xvalue* read(const Parent* p,xvalue* value)
{
    XL6_SequenceLayoutV1 t=parentTuple(p); xvalue* result=NULL; bool found=false;
    bool ok=p->family==3 ? xl6ListErasedReadV1(value,17,false,&result,&found) : p->family==1 ?
        xl6ArrayReadV1(value,TUPLE(t),0,&result) : xl6StackTakeV1(value,TUPLE(t),p->nullable ? 4 : 2,&result,false);
    assert(ok && result && (p->family!=3 || found)); return result;
}
static void refused(const Parent* p,xvalue* owner,xvalue* wrong)
{
    assert(xrtMemDebugFailAfter(0));
    assert(!put(p,owner,wrong,false)); error(XERR_TYPE);
    assert(!put(p,owner,wrong,true)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
}
static void foreignDrop(void* raw,void* context) { (void)raw; (void)context; }
static void profiles(void)
{
    for (unsigned family=1;family<=3;++family) for (unsigned profile=0;profile<4;++profile)
    for (unsigned nullable=0;nullable<2;++nullable) {
        Parent p=parent(family,profile,nullable); XL6_ListLayoutV1 t=leafLayout(profile),captured={0};
        XL6_SequenceAdmissionV3 policy={0}; xvalue* good=leaf(profile,false); assert(good); fill(good,profile);
        assert(xl6ListSchemaCopyV1(good,&captured,&policy));
        assert(captured.identity==t.identity && captured.value.identity==t.value.identity);
        xvalue* weak=xrtValueWeakRef(good); assert(weak);
        /* A populated exemplar is legal; only metadata survives this factory. */
        xvalue* owner=createParent(&p,xrtValueRetain(good)); assert(owner); bind(&p,owner);
        assert(put(&p,owner,good,false) && put(&p,owner,good,true));
        xvalue* same=leaf(profile,false); assert(same && xl6ListAdmitsSchemaV1(same,&captured,&policy));
        assert(put(&p,owner,same,false)); xrtValueRelease(same);
        /* Same complete physical tuple, but different logical policy. */
        xvalue* wrong=leaf(profile,true); assert(wrong && !xl6ListAdmitsSchemaV1(wrong,&captured,&policy));
        refused(&p,owner,wrong); xrtValueRelease(wrong);
        XL6_ListLayoutV1 changed=t; ++changed.value.identity;
        XL6_ListLayoutV1 ignored={0}; XL6_SequenceAdmissionV3 samePolicy={0};
        assert(xl6ListSchemaCopyV1(good,&ignored,&samePolicy));
        xvalue* domain=xl6SequenceCreateAdmittedV1(1,changed.value.identity,&samePolicy); assert(domain && !samePolicy.ops);
        wrong=xl6ListCreateTakeV1(&changed,domain); assert(wrong);
        refused(&p,owner,wrong); xrtValueRelease(wrong); /* Exact same policy, wrong value-domain layout. */
        wrong=xrtValueIntMap(); assert(wrong && xrtValueTypeIdBind(wrong,t.identity));
        refused(&p,owner,wrong); xrtValueRelease(wrong);
        wrong=xl6ListCursorCreateV1(good,&t,t.identity+30000,false,false,0); assert(wrong);
        refused(&p,owner,wrong); xrtValueRelease(wrong);
        static const xvaluehandleops foreign={.Drop=foreignDrop}; void* raw=(void*)(uintptr_t)1;
        wrong=xrtValueHandleTake(&raw,&foreign,NULL); assert(wrong && xrtValueTypeIdBind(wrong,t.identity));
        refused(&p,owner,wrong); xrtValueRelease(wrong);
        if (!nullable) { refused(&p,owner,NULL); refused(&p,owner,xrtValueNull()); }
        else { assert(put(&p,owner,NULL,false) && put(&p,owner,xrtValueNull(),true)); }
        /* Metadata equality is independent of current elements/codec binding.
         * An operation-pinned child is still an admissible reference. */
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError();
        assert(xrtMemDebugFailAfter(0)); XL6_ListLayoutV1 saved={0}; XL6_SequenceAdmissionV3 copy={0};
        assert(xl6ListSchemaCopyV1(good,&saved,&copy) && xl6ListAdmitsSchemaV1(good,&saved,&copy));
        assert(xrtGetError()==pending && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
        xl6SequenceAdmissionDropV3(&copy);
        xvalue* snapshot=clone(&p,owner); assert(snapshot); xrtValueRelease(owner);
        /* Read the stable first slot (List writes replace one key). */
        xvalue* observed=read(&p,snapshot);
        if (family==3 && nullable) assert(xrtValueType(observed)==XVALUE_NULL);
        else if (family==3) assert(observed!=good && xl6ListAdmitsSchemaV1(observed,&captured,&policy));
        else assert(observed==good);
        xrtValueRelease(observed); xrtValueRelease(good); xrtValueRelease(snapshot);
        assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
        /* Only owned metadata remains. It can authorize a fresh equivalent
         * child but not a wider one after all original wrappers disappear. */
        same=leaf(profile,false); wrong=leaf(profile,true); assert(same && wrong);
        assert(xl6ListAdmitsSchemaV1(same,&captured,&policy) && !xl6ListAdmitsSchemaV1(wrong,&captured,&policy));
        xrtValueRelease(wrong); xrtValueRelease(same); xl6SequenceAdmissionDropV3(&policy);
    }
}
static void captureFailures(void)
{
    XL6_ListLayoutV1 captured={.identity=777}; XL6_SequenceAdmissionV3 policy={0};
    assert(!xl6ListSchemaCopyV1(NULL,&captured,&policy)); error(XERR_ARGUMENT);
    assert(captured.identity==777 && !policy.ops);
    xvalue* good=leaf(0,false); assert(good);
    XL6_ListLayoutV1 t=leafLayout(0);
    xvalue* cursor=xl6ListCursorCreateV1(good,&t,888,false,false,0); assert(cursor);
    assert(!xl6ListSchemaCopyV1(cursor,&captured,&policy)); error(XERR_STATE);
    assert(captured.identity==777 && !policy.ops); xrtValueRelease(cursor);
    assert(!xl6SequenceListCreateTakeV1(3,900,400,xrtValueRetain(good),false)); error(XERR_ARGUMENT);
    assert(!xl6SequenceListCreateTakeV1(1,900,401,xrtValueRetain(good),false)); error(XERR_ARGUMENT);
    assert(!xl6SequenceListCreateTakeV1(1,900,400,xrtValueRetain(good),true)); error(XERR_ARGUMENT);
    assert(xl6ListSchemaCopyV1(good,&captured,&policy));
    const void* owned=policy.data;
    assert(!xl6ListSchemaCopyV1(good,&captured,&policy)); error(XERR_ARGUMENT);
    assert(policy.data==owned && captured.identity==400); xl6SequenceAdmissionDropV3(&policy);
    xvalue* weak=xrtValueWeakRef(good); assert(weak);
    xvalue* parent=xl6SequenceListCreateTakeV1(1,900,400,good,false); assert(parent && xrtValueWeakRefExpired(weak));
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(parent,1,900,400,3,sizeof(xvalue*),xrtTypeValue()); assert(pin);
    assert(!xl6SequenceResizeStorageV1(pin,1)); error(XERR_TYPE); xl6SequenceEndV1(pin);
    xrtValueRelease(parent); xrtValueRelease(weak);
    good=leaf(0,false); assert(good); parent=xl6SequenceListCreateTakeV1(1,900,901,good,true); assert(parent);
    pin=xl6SequenceBeginRoleV1(parent,1,900,901,3,sizeof(xvalue*),xrtTypeValue()); assert(pin);
    assert(xl6SequenceResizeStorageV1(pin,2));
    assert(!*(xvalue* const*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0)); xl6SequenceEndV1(pin);
    xrtValueRelease(parent);
}
/* The full recursive schema budget is shared across List/Array/Stack. The
 * leaf policy differs but every ancestor has the exact same IDs and tuple. */
static xvalue* mixedChain(unsigned depth,bool wide,unsigned* family,XL6_SequenceLayoutV1* tuple,XL6_ListLayoutV1* list)
{
    xvalue* result=leaf(2,wide); *family=3; *list=leafLayout(2); *tuple=list->value;
    for (unsigned at=1;result && at<=depth;++at) {
        uint64_t child=*family==3 ? list->identity : tuple->identity;
        unsigned next=1+at%3; uint64_t identity=40000+at;
        XL6_SequenceLayoutV1 t={next==3 ? identity+10000 : identity,child,3,sizeof(xvalue*),xrtTypeValue()};
        xvalue* domain=*family==3 ? xl6SequenceListCreateTakeV1(next==2 ? 2 : 1,t.identity,child,result,false) :
            xl6SequenceNestedCreateTakeV2(next==2 ? 2 : 1,t.identity,child,result,false);
        *list=(XL6_ListLayoutV1){identity,t}; *tuple=t; *family=next;
        result=next==3 ? xl6ListCreateTakeV1(list,domain) : domain;
    }
    return result;
}
static void depth(void)
{
    unsigned family=0; XL6_SequenceLayoutV1 t={0}; XL6_ListLayoutV1 l={0};
    xvalue* good=mixedChain(64,false,&family,&t,&l); assert(good && family==2);
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(good,&role,&layout,&policy) && xl6ResidentAdmissionDepthV1(&policy)==64);
    xvalue* same=mixedChain(64,false,&family,&t,&l),*wrong=mixedChain(64,true,&family,&t,&l); assert(same && wrong);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError(); assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceAdmitsSchemaV1(same,role,&layout,&policy) && !xl6SequenceAdmitsSchemaV1(wrong,role,&layout,&policy));
    assert(xrtGetError()==pending && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
    assert(!mixedChain(65,false,&family,&t,&l)); error(XERR_RANGE);
    xrtValueRelease(wrong); xrtValueRelease(same); xrtValueRelease(good);
    good=xl6SequenceCreateAdmittedV1(role,layout.identity,&policy); assert(good && !policy.ops); xrtValueRelease(good);
}
static const xvalue* busySource;
static unsigned busyCalls;
static xvalue* busyEncode(const xrttype* type,const void* slot,ptr context)
{
    XL6_ListLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0}; ++busyCalls;
    assert(busySource && xrtMemDebugFailAfter(0));
    assert(xl6ListSchemaCopyV1(busySource,&layout,&policy) && xl6ListAdmitsSchemaV1(busySource,&layout,&policy));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xl6SequenceAdmissionDropV3(&policy);
    return xl6StringConverterV1()->FromTyped(type,slot,context);
}
static bool busyDecode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{ return xl6StringConverterV1()->ToTyped(value,type,output,context); }
static void busy(void)
{
    static const xvalueconverter codec={.ToTyped=busyDecode,.FromTyped=busyEncode};
    XL6_ListLayoutV1 layout=leafLayout(1); xvalue* source=leaf(1,false); assert(source); fill(source,1);
    assert(xl6ListProjectionTakeV1(xrtValueRetain(source),&layout,&codec)==source); xrtValueRelease(source);
    busySource=source; busyCalls=0; xvalue* item=NULL; bool found=false;
    assert(xl6ListErasedReadV1(source,INT64_MIN,false,&item,&found) && found && busyCalls==1);
    xstrview text={0}; assert(xrtValueGetString(item,&text) && text.Size==5 && !memcmp(text.Data,"a\0\xE4\xBD\xA0",5));
    xrtValueRelease(item); busySource=NULL; xrtValueRelease(source);
}
void xl6TestListSchemaVectorsV1(const char* mode)
{
    if (!strcmp(mode,"schema-profiles")) profiles();
    else if (!strcmp(mode,"schema-failures")) captureFailures();
    else if (!strcmp(mode,"schema-busy")) busy();
    else { assert(!strcmp(mode,"schema-depth")); depth(); }
}
bool xl6TestListSchemaFaultV1(const char* operation,size_t point)
{
    if (strncmp(operation,"schema-",7)) return false;
    unsigned family=0; XL6_SequenceLayoutV1 t={0}; XL6_ListLayoutV1 l={0};
    xvalue* source=NULL,*result=NULL; bool deep=!strcmp(operation,"schema-deep-create");
    if (!deep) { source=leaf(1,false); assert(source); fill(source,1); }
    xvalue* weak=source ? xrtValueWeakRef(source) : NULL;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    if (deep) result=mixedChain(64,false,&family,&t,&l);
    else if (!strcmp(operation,"schema-create")) result=xl6SequenceListCreateTakeV1(1,900,401,source,false);
    else if (!strcmp(operation,"schema-list-create")) {
        Parent p=parent(3,1,false); result=createParent(&p,source);
    } else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    if (hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
    if (weak) { assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); }
    xrtValueRelease(result); xrtErrorFree(outcome.error);
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit); return true;
}
