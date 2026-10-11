#include "xl6_set_import_chain.h"
#include "xl6_set_graph_access.h"
#include "xl6_array_input.h"
#include "xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"

typedef struct SetTemplate { XL6_SetGraphViewV1 view; XL6_SequenceAdmissionV3 policy; } SetTemplate;
static bool begin(const xvalue* source,ptr* output)
{
    SetTemplate* value=xrtCalloc(1,sizeof(*value));
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    *output=value;
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!source || xrtValueType(source)!=XVALUE_HANDLE || !xrtValueGetHandle(source,&raw,&ops,&user)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    int status=xl6SetGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&value->view);
    if (!status) xrtSetErrorKind(XERR_TYPE);
    if (status!=1) return false;
    XL6_SetLayoutV1 layout={0};
    if (!xl6SetSchemaCopyV1(source,&layout,&value->policy)) return false;
    if (xrtTypedSetCount(value->view.storage)) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void end(ptr opaque)
{
    SetTemplate* value=opaque; if (!value) return;
    if (value->view.owner) xl6SetGraphEndV1(&value->view);
    xl6SequenceAdmissionDropV3(&value->policy); xrtFree(value);
}
static xvalue* empty(ptr opaque) { return xl6SetGraphEmptyV1(&((SetTemplate*)opaque)->view); }
static bool admits(ptr opaque,const xvalue* value)
{ const SetTemplate* source=opaque; return source->policy.ops->Value(source->policy.data,value); }
static bool append(xvalue* target,int64_t key,const xvalue* value)
{ (void)key; bool added=false; return xl6SetErasedKeyV1(target,value,XL6_SET_ADD_V1,&added); }
static bool inputBegin(const xvalue* source,XL6_CollectionImportInputV1* output)
{
    if (!output || output->context || output->items || output->keys || output->count || output->textKeys) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    XL6_ArrayInputV1* input=xrtCalloc(1,sizeof(*input));
    XL6_SetGraphViewV1 view={0}; xtypedsetiter iterator={0}; bool started=false,ok=false;
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!input) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    output->context=input;
    if (!source || xrtValueType(source)!=XVALUE_HANDLE || !xrtValueGetHandle(source,&raw,&ops,&user)) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    int status=xl6SetGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&view);
    if (!status) xrtSetErrorKind(XERR_TYPE);
    if (status!=1) goto done;
    size_t count=xrtTypedSetCount(view.storage);
    if (count>SIZE_MAX/sizeof(*input->items)) { xrtSetErrorKind(XERR_RANGE); goto done; }
    input->items=count ? xrtMalloc(count*sizeof(*input->items)) : NULL;
    if (count && !input->items) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    if (!xrtTypedSetIterBegin(view.storage,&iterator)) goto done;
    started=true;
    for (size_t i=0;i<count;++i) {
        const void* slot=xrtTypedSetIterNext(&iterator);
        if (!slot || xrtGetError() || scope.failed) goto done;
        xvalue* owned;
        if (view.layout.value.transfer==3) {
            const xvalue* item=*(xvalue* const*)slot;
            owned=xrtValueRetain(item ? item : xrtValueNull());
        } else owned=xl6SetGraphProjectSlotV1(&view,slot);
        if (owned) input->items[input->count++]=owned;
        if (!owned || xrtGetError() || scope.failed) goto done;
    }
    if (xrtTypedSetIterNext(&iterator) || xrtGetError() || scope.failed) goto done;
    output->items=(const xvalue* const*)input->items; output->count=count; ok=true;
done:
    if (!ok || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtTypedSetIterEnd(&iterator);
    if (view.owner) xl6SetGraphEndV1(&view);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    return ok;
}
static void inputEnd(XL6_CollectionImportInputV1* source)
{
    XL6_ArrayInputV1* input=source->context; *source=(XL6_CollectionImportInputV1){0};
    if (input) { xl6ArrayInputReleaseV1(input); xrtFree(input); }
}
static bool inputConversion(ptr opaque,const xvalue* source,bool* output)
{
    const SetTemplate* target=opaque;
    if (xl6SetAdmitsSchemaV1(source,&target->view.layout,&target->policy)) { *output=false; return true; }
    int status=xl6SetErasedProbeV1(source);
    if (status<0) return false;
    if (!status) { *output=false; return true; }
    XL6_SetLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    if (!xl6SetSchemaCopyV1(source,&layout,&policy)) return false;
    bool erased=xl6ReferenceAnyAdmissionV1(&policy,layout.value.elementIdentity);
    xl6SequenceAdmissionDropV3(&policy);
    if (xrtGetError()) return false;
    *output=erased; return true;
}
const XL6_CollectionImportProviderV1* xl6SetImportProviderV1(void)
{ static const XL6_CollectionImportProviderV1 provider={begin,end,empty,admits,append,inputBegin,inputEnd,inputConversion,NULL}; return &provider; }
