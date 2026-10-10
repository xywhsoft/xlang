#include "xl6_restore_open.h"
#include "xl6_restore_class.h"
#include "xl6_dynamic_fields.h"
#include "src/api/xl_foreign_error.h"

static bool native(const xvalue* value)
{ return xl6RestoreClassClosedProviderV1()->IsNative(value); }
static bool admits(const xvalue* value,uint64_t identity,int64_t tag)
{ return xl6RestoreClassClosedProviderV1()->Admits(value,identity,tag); }
static bool ownership(xrtownershipref ref)
{
    const XL6_NativeHandlePolicyV1* policy=xl6DynamicFieldsHandlePolicyV1Get();
    return xl6RestoreClassClosedProviderV1()->OwnershipAdmits(ref) ||
        xrtDynamicFieldsOwnershipAdapterV1(ref) ||
        xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace);
}
static bool copyDefault(xstrview key,const xvalue* value,ptr raw)
{ return xrtDynamicFieldsSetRef(raw,key,value); }
static void* storageHandle(const xvalue* value,const XL6_RestoreOpenV1* recipe)
{
    if (!admits(value,recipe->identity,recipe->runtimeTag)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
        return NULL;
    }
    const xvalue* boxed=xrtValueObjectGet(value,recipe->handle);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    if (!boxed || !xrtValueGetHandle(boxed,&raw,&ops,&user) || !raw || user ||
        ops!=xl6DynamicFieldsHandlePolicyV1Get()->Ops) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
        return NULL;
    }
    return raw;
}
static void release(xvalue* value,xl6_error_scope* operation)
{
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(operation);
    xrtValueRelease(value);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(operation);
}
static bool prepare(const XL6_RestoreOpenV1* recipe,const xvalue* receiver,
    const xvalue* source,xvalue** storage,xvalue** handle)
{
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) return false;
    bool ok=false; xvalue* snapshot=NULL,*cloned=NULL,*boxed=NULL; void* raw=NULL;
    /* The importer validates the data recipe before any construction or
     * wire rejection. Check output ownership independently at this boundary. */
    if (!xl6RestoreOpenRecipeValidV1(recipe) || !storage || !handle || storage==handle || *storage || *handle ||
        !native(receiver) || !source || xrtValueType(source)!=XVALUE_OBJECT || native(source)) {
        xrtSetErrorKind(XERR_ARGUMENT); goto finish;
    }
    const xvalue* original=xrtValueObjectGet(receiver,recipe->storage);
    if (!original || !storageHandle(original,recipe)) goto finish;
    /* Hold the first wire store across arbitrary declared clone code. */
    snapshot=xrtValueClone(source); if (!snapshot) goto finish;
    size_t extras=0,count=xrtValueCount(snapshot);
    for (size_t i=0;i<count;++i) {
        xstrview key; if (!xrtValueObjectAt(snapshot,i,&key)) goto finish;
        if (!xl6RestoreOpenClaimsKeyV1(recipe,key)) ++extras;
    }
    if (!extras) { ok=true; goto finish; }
    if (!recipe->clone) { xrtSetErrorKind(XERR_STATE); goto finish; }
    /* Clone outside every ownership fence. Returned-self or bad physical
     * class/handle identity is checked failure BEFORE touching any payload. */
    cloned=recipe->clone(original);
    if (!cloned || operation.failed || xrtGetError()) goto finish;
    if (cloned==original || cloned==receiver || cloned==source) {
        xrtSetErrorKind(XERR_STATE); goto finish;
    }
    void* defaults=storageHandle(cloned,recipe); if (!defaults) goto finish;
    size_t defaultsCount=xrtDynamicFieldsCount(defaults);
    if (defaultsCount>SIZE_MAX-extras) { xrtSetErrorKind(XERR_RANGE); goto finish; }
    raw=xl6_xrt_dynamic_fields_create();
    if (!raw || !xrtDynamicFieldsReserve(raw,defaultsCount+extras) ||
        !xrtDynamicFieldsVisitV1(defaults,copyDefault,raw)) goto finish;
    for (size_t i=0;i<count;++i) {
        xstrview key; const xvalue* borrowed=xrtValueObjectAt(snapshot,i,&key);
        if (!borrowed) goto finish;
        if (xl6RestoreOpenClaimsKeyV1(recipe,key)) continue;
        /* Existing erased wire-value semantics, not a structural class clone
         * or a second typed restoration graph. Exact byte keys are preserved. */
        xvalue* item=xrtValueClone(borrowed);
        bool stored=item && xrtDynamicFieldsSetTake(raw,key,&item);
        if (!stored) xl6ErrorScopeCaptureFailure(&operation);
        release(item,&operation);
        if (!stored || operation.failed || xrtGetError()) goto finish;
    }
    boxed=xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get());
    ok=boxed!=NULL && !operation.failed && !xrtGetError();
finish:
    if (!ok) xl6ErrorScopeCaptureFailure(&operation);
    release(snapshot,&operation);
    if (operation.failed || xrtGetError()) { release(boxed,&operation); boxed=NULL; release(cloned,&operation); cloned=NULL; }
    xl6_xrt_dynamic_fields_destroy(raw);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    *storage=cloned; *handle=boxed; return true;
}
const XL6_RestoreObjectProviderV1* xl6RestoreOpenProviderV1(void)
{
    static const XL6_RestoreObjectProviderV1 provider={native,admits,ownership,prepare};
    return &provider;
}
