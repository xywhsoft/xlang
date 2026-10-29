#include "src/runtime/xl6_restore_open.h"
#include "src/runtime/xl6_dynamic_fields.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ROOT_ID UINT64_C(0xED170)
#define FIELDS_ID UINT64_C(0xED171)
enum { ROOT_TAG=170,FIELDS_TAG=171 };
static xvalue* sharedFields,*leaked,*sharedRoot,*mutableWire;
static unsigned mode,clones,rootClones,factories;
static bool retire(xvalue* value,ptr user) { (void)value; (void)user; return true; }
static void release(xvalue* value) { xrtValueRelease(value); assert(!xrtGetError()); }
static void* rawOf(const xvalue* fields)
{
    void* raw=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(xrtValueObjectGet(fields,XRT_STR_LITERAL("handle")),&raw,&ops,NULL));
    assert(raw && ops==xl6DynamicFieldsHandlePolicyV1Get()->Ops); return raw;
}
static xvalue* makeClass(uint64_t identity,int64_t tag)
{
    xvalue* value=xl6ClassRuntimeV2Get()->Create(NULL,retire);
    if (value && (!xrtValueTypeIdBind(value,identity) ||
        !xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(tag)))) {
        xrtValueRelease(value); return NULL;
    }
    return value;
}
static xvalue* fieldsMake(void* raw)
{
    xvalue* boxed=raw ? xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get()) : NULL;
    xvalue* value=boxed ? makeClass(FIELDS_ID,FIELDS_TAG) : NULL;
    if (!value || !xrtValueObjectSetTake(value,XRT_STR_LITERAL("handle"),&boxed)) {
        xrtValueRelease(boxed); xrtValueRelease(value); return NULL;
    }
    return xl6ClassRuntimeV2Get()->CommitTake(value);
}
static xvalue* rootMake(void)
{
    xvalue* value=makeClass(ROOT_ID,ROOT_TAG);
    xvalue* fields=value ? xrtValueRetain(sharedFields) : NULL;
    if (!value || !fields || !xrtValueObjectSetTake(value,XRT_STR_LITERAL("storage"),&fields) ||
        !xrtValueObjectSetNew(value,XRT_STR_LITERAL("id"),xrtValueInt(-1))) {
        xrtValueRelease(fields); xrtValueRelease(value); return NULL;
    }
    return xl6ClassRuntimeV2Get()->CommitTake(value);
}
static xvalue* rootDefault(void)
{ ++factories; return mode==5 ? xrtValueRetain(sharedRoot) : rootMake(); }
static xvalue* rootClone(const xvalue* original)
{ assert(original==sharedRoot); ++rootClones; return rootMake(); }
static xvalue* fieldsClone(const xvalue* original)
{
    ++clones; assert(original==sharedFields);
    xrtownershipscope mutation={0}; assert(xrtOwnershipMutationBegin(&mutation));
    assert(xrtOwnershipScopeEnd(&mutation)); /* Actual declared code outside the fence. */
    if (mode==1) return xrtValueRetain(original);
    if (mode==2) { xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); return NULL; }
    if (mode==3) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    xvalue* value=fieldsMake(xl6_xrt_dynamic_fields_clone(rawOf(original)));
    if (value && mode==4) { assert(!leaked); leaked=xrtValueRetain(value); }
    if (value && mode==6) assert(xrtValueObjectSetNew(mutableWire,XRT_STR_LITERAL("extra\0你"),xrtValueInt(99)));
    return value;
}
static XL6_RestorePlanV1 plan(XL6_RestoreNodeV1* nodes,XL6_RestoreFieldV1* field,XL6_RestoreOpenV1* open)
{
    static const xstrview claimed[]={XRT_STR_INIT("id"),XRT_STR_INIT("wire\0id"),XRT_STR_INIT("ignored")};
    *open=(XL6_RestoreOpenV1){FIELDS_ID,FIELDS_TAG,XRT_STR_INIT("storage"),XRT_STR_INIT("handle"),3,claimed,fieldsClone};
    nodes[0]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_OBJECT_V1,.identity=ROOT_ID,
        .as.object={rootDefault,ROOT_TAG,0,1,xl6RestoreOpenProviderV1()},.clone=rootClone,.open=open};
    nodes[1]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SCALAR_V1,.identity=UINT64_C(0xED172),.as.scalar=XL6_VALUE_ADMIT_I64};
    *field=(XL6_RestoreFieldV1){XRT_STR_INIT("wire\0id"),XRT_STR_INIT("id"),1,0};
    return (XL6_RestorePlanV1){sizeof(XL6_RestorePlanV1),2,1,0,nodes,field,false,NULL,0,NULL};
}
static xvalue* wire(bool extras,bool invalid)
{
    xvalue* value=xrtValueObject(); assert(value);
    assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("wire\0id"),invalid ? xrtValueString(XRT_STR_LITERAL("bad")) : xrtValueInt(9)));
    assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("ignored"),xrtValueInt(1)));
    assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("storage"),xrtValueInt(1)));
    assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(1)));
    if (extras) assert(xrtValueObjectSetNew(value,XRT_STR_LITERAL("extra\0你"),xrtValueInt(7)));
    return value;
}
static int64_t number(const xvalue* value)
{ int64_t number=0; assert(value && xrtValueGetInt(value,&number)); return number; }
static void unchanged(void)
{
    assert(xrtDynamicFieldsCount(rawOf(sharedFields))==1);
    assert(!xrtDynamicFieldsHas(rawOf(sharedFields),XRT_STR_LITERAL("extra\0你")));
    if (sharedRoot) assert(number(xrtValueObjectGet(sharedRoot,XRT_STR_LITERAL("id")))==-1);
}
static void vectors(void)
{
    for (mode=0;mode<=6;++mode) {
        XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field; XL6_RestoreOpenV1 open;
        XL6_RestorePlanV1 p=plan(nodes,&field,&open);
        xvalue* source=wire(true,false); mutableWire=source;
        clones=rootClones=factories=0;
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        xvalue* result=NULL; bool ok=xl6PersistenceRestorePlanReadTakeV1(NULL,0,NULL,source,&p,&result);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        bool success=mode==0 || mode==5 || mode==6;
        if (ok!=success || outcome.ok!=success || factories!=1)
            fprintf(stderr,"Open mode=%u ok=%d scope=%d factories=%u clones=%u rootClones=%u error=%u\n",
                mode,ok,outcome.ok,factories,clones,rootClones,outcome.error ? (unsigned)xrtErrorKind(outcome.error) : 0);
        assert(ok==success && outcome.ok==success && factories==1);
        if (success) {
            assert(result && result!=sharedRoot && number(xrtValueObjectGet(result,XRT_STR_LITERAL("id")))==9);
            const xvalue* fields=xrtValueObjectGet(result,XRT_STR_LITERAL("storage")); assert(fields && fields!=sharedFields);
            void* raw=rawOf(fields); assert(xrtDynamicFieldsCount(raw)==2);
            assert(number(xrtDynamicFieldsGet(raw,XRT_STR_LITERAL("extra\0你")))==7);
            assert(!xrtDynamicFieldsHas(raw,XRT_STR_LITERAL("wire\0id")) && !xrtDynamicFieldsHas(raw,XRT_STR_LITERAL("ignored")));
            if (mode==5) assert(rootClones==1);
            release(result);
        } else {
            assert(!result && !outcome.ok);
            if (mode==2) assert(!outcome.error);
            if (leaked) assert(!xrtDynamicFieldsHas(rawOf(leaked),XRT_STR_LITERAL("extra\0你")));
            xrtErrorFree(outcome.error); xrtClearError();
        }
        unchanged(); release(leaked); leaked=NULL; release(source); mutableWire=NULL;
    }
    mode=0;
    for (unsigned variant=0;variant<9;++variant) {
        XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field; XL6_RestoreOpenV1 open;
        XL6_RestorePlanV1 p=plan(nodes,&field,&open);
        if (variant==0 || variant==1) open.clone=NULL;
        if (variant==3) open.identity=0;
        if (variant==4) open.claimedCount=0;
        if (variant==5) open.storage=XRT_STR_LITERAL("$xlir.class.runtime-type");
        if (variant==6) field.target=open.storage;
        if (variant==7) open.claimedCount=SIZE_MAX;
        if (variant==8) nodes[1].open=&open;
        xvalue* source=wire(variant!=0,variant==2 || variant==3);
        clones=factories=0; xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        xvalue* result=NULL; bool ok=xl6PersistenceRestorePlanReadTakeV1(NULL,0,NULL,source,&p,&result);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        if (variant==0) { assert(ok && outcome.ok && result && !clones); release(result); }
        else if (variant==2) { assert(ok && outcome.ok && xrtValueType(result)==XVALUE_NULL && !clones); release(result); }
        else { assert(!ok && !outcome.ok && !result && !clones); if (variant>=3) assert(!factories); }
        xrtErrorFree(outcome.error); xrtClearError(); unchanged(); release(source);
    }
    puts("Open: exact/NUL keys, ignored/aliased fields, private publication, shared/self clone refusal and diagnosticless outcomes verified");
}
static void foreignDrop(ptr raw,ptr user) { assert(raw==(ptr)1 && !user); }
static void physicalRefusal(void)
{
    static const xvaluehandleops foreign={NULL,foreignDrop,NULL,NULL};
    for (unsigned variant=0;variant<4;++variant) {
        XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field; XL6_RestoreOpenV1 open;
        (void)plan(nodes,&field,&open);
        xvalue* storage=variant==0 ? xrtValueObject() : makeClass(FIELDS_ID,variant==1 ? FIELDS_TAG+1 : FIELDS_TAG);
        assert(storage);
        if (variant==0) {
            assert(xrtValueTypeIdBind(storage,FIELDS_ID));
            assert(xrtValueObjectSetNew(storage,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(FIELDS_TAG)));
        }
        if (variant==3) {
            ptr raw=(ptr)1; xvalue* boxed=xrtValueHandleTake(&raw,&foreign,NULL); assert(boxed && !raw);
            assert(xrtValueObjectSetTake(storage,XRT_STR_LITERAL("handle"),&boxed));
        }
        if (variant) storage=xl6ClassRuntimeV2Get()->CommitTake(storage);
        assert(storage); xvalue* parent=makeClass(ROOT_ID,ROOT_TAG); assert(parent);
        assert(xrtValueObjectSetTake(parent,XRT_STR_LITERAL("storage"),&storage));
        parent=xl6ClassRuntimeV2Get()->CommitTake(parent); assert(parent);
        xvalue* source=wire(false,false),*copy=NULL,*handle=NULL;
        clones=0; xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        bool ok=xl6RestoreOpenProviderV1()->PrepareOpen(&open,parent,source,&copy,&handle);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!ok && !outcome.ok && outcome.error && xrtErrorKind(outcome.error)==XERR_TYPE);
        assert(!copy && !handle && !clones); xrtErrorFree(outcome.error); xrtClearError();
        release(source); release(parent); unchanged();
    }
    puts("Open: forged class/tag, absent handle and foreign invalid payload rejected before payload access");
}
static void prefixes(void)
{
    mode=0; xvalue* source=wire(true,false); size_t prefix;
    for (prefix=0;prefix<1000;++prefix) {
        XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field; XL6_RestoreOpenV1 open;
        XL6_RestorePlanV1 p=plan(nodes,&field,&open);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        xl6ProductMemoryBegin(prefix);
        xvalue* result=xl6PersistenceRestorePlanTakeV1(NULL,0,NULL,source,&p);
        bool hit=xl6ProductMemoryEnd(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(hit ? !result && !outcome.ok && outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY : result && outcome.ok);
        xrtValueRelease(result); xrtErrorFree(outcome.error); xrtClearError();
        unchanged(); xl6ProductMemoryVerify(); if (!hit) break;
    }
    assert(prefix<1000); release(source);
    printf("Open: %zu complete allocation-failure prefixes balanced\n",prefix+1);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xl6_error_scope warm={0}; assert(xl6ErrorScopeBegin(&warm));
    xl6_error_result outcome=xl6ErrorScopeEnd(&warm); assert(outcome.ok);
    void* raw=xl6_xrt_dynamic_fields_create(); assert(raw);
    xvalue* keep=xrtValueString(XRT_STR_LITERAL("base")); assert(keep);
    assert(xrtDynamicFieldsSetTake(raw,XRT_STR_LITERAL("keep"),&keep));
    sharedFields=fieldsMake(raw); assert(sharedFields); sharedRoot=rootMake(); assert(sharedRoot);
    xl6ProductMemoryBegin(SIZE_MAX); vectors(); physicalRefusal(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    prefixes(); release(sharedRoot); release(sharedFields); return 0;
}
