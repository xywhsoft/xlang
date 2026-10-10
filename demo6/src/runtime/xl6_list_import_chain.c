#include "src/runtime/xl6_list_import_chain.h"
#include "src/runtime/xl6_list_graph_access.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct ListTemplate {
    XL6_ListGraphViewV1 view;
    XL6_SequenceAdmissionV3 policy;
} ListTemplate;
static bool begin(const xvalue* source,ptr* output)
{
    ListTemplate* value=xrtMalloc(sizeof(*value));
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    *value=(ListTemplate){0}; *output=value;
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!source || xrtValueType(source)!=XVALUE_HANDLE ||
        !xrtValueGetHandle(source,&raw,&ops,&user)) { xrtSetErrorKind(XERR_TYPE); return false; }
    int status=xl6ListGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&value->view);
    if (!status) xrtSetErrorKind(XERR_TYPE);
    if (status!=1) return false;
    XL6_ListLayoutV1 layout={0};
    if (!xl6ListSchemaCopyV1(source,&layout,&value->policy)) return false;
    if (xrtTypedListCount(value->view.storage)) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void end(ptr opaque)
{
    ListTemplate* value=opaque; if (!value) return;
    if (value->view.owner) xl6ListGraphEndV1(&value->view);
    xl6SequenceAdmissionDropV3(&value->policy); xrtFree(value);
}
static xvalue* empty(ptr opaque)
{ return xl6ListGraphEmptyV1(&((ListTemplate*)opaque)->view); }
static bool admits(ptr opaque,const xvalue* value)
{
    const ListTemplate* source=opaque;
    return source->policy.ops->Value(source->policy.data,value);
}
static bool append(xvalue* target,int64_t key,const xvalue* value)
{ return xl6ListErasedSetV1(target,key,value); }
typedef struct ListInput {
    XL6_ArrayInputV1 owned; /* Independent pins; no source slots escape. */
    int64_t* keys;
} ListInput;
static bool inputBegin(const xvalue* source,XL6_CollectionImportInputV1* output)
{
    if (!output || output->context || output->items || output->keys || output->count) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListInput* input=xrtCalloc(1,sizeof(*input)); xvalue* wire=NULL;
    XL6_ListGraphViewV1 view={0}; xtypedlistiter nativeIterator={0};
    xvalueiter iterator={0}; bool started=false,nativeStarted=false,ok=false;
    if (!input) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    output->context=input;
    /* Native ops authenticate before payload access. A foreign handle or wire
     * Array is not a List, even if it carries a matching logical TypeId. */
    if (source && xrtValueType(source)==XVALUE_INT_MAP) {
        wire=xrtValueRetain(source);
        if (!wire) goto done;
    } else if (source && xrtValueType(source)==XVALUE_HANDLE) {
        void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
        if (!xrtValueGetHandle(source,&raw,&ops,&user)) goto done;
        int status=xl6ListGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&view);
        if (!status) xrtSetErrorKind(XERR_TYPE);
        if (status!=1) goto done;
    } else { xrtSetErrorKind(XERR_TYPE); goto done; }
    size_t count=wire ? xrtValueCount(wire) : xrtTypedListCount(view.storage);
    if (count>SIZE_MAX/sizeof(*input->owned.items) || count>SIZE_MAX/sizeof(*input->keys)) {
        xrtSetErrorKind(XERR_RANGE); goto done;
    }
    input->owned.items=count ? xrtMalloc(count*sizeof(*input->owned.items)) : NULL;
    if (count && !input->owned.items) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    input->keys=count ? xrtMalloc(count*sizeof(*input->keys)) : NULL;
    if (count && !input->keys) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    if (wire) { if (!xrtValueIterBegin(wire,&iterator)) goto done; started=true; }
    else { if (!xrtTypedListIterBegin(view.storage,&nativeIterator)) goto done; nativeStarted=true; }
    for (size_t i=0;i<count;++i) {
        int64_t key=0; xvalue* owned=NULL;
        if (wire) {
            xvaluekey actual={0}; xvalue* item=NULL;
            if (xrtValueIterAdvance(&iterator,&actual,&item)!=XVALUE_ITER_ITEM || actual.Type!=XVALUE_KEY_INT) {
                if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
                goto done;
            }
            key=actual.Integer; owned=xrtValueRetain(item);
        } else {
            const void* slot=xrtTypedListIterNext(&nativeIterator,&key);
            if (!slot || xrtGetError() || scope.failed) goto done;
            /* Real native slots project once, without an intermediate int-map
             * or typed clone. Reference edges keep their actual child shell. */
            if (view.layout.value.transfer==3) {
                const xvalue* item=*(xvalue* const*)slot;
                owned=xrtValueRetain(item ? item : xrtValueNull());
            } else owned=xl6ListGraphProjectSlotV1(&view,slot);
        }
        if (owned) input->owned.items[input->owned.count++]=owned;
        if (!owned || xrtGetError() || scope.failed) goto done;
        input->keys[i]=key;
    }
    if (wire) {
        xvaluekey key={0}; xvalue* item=NULL;
        if (xrtValueIterAdvance(&iterator,&key,&item)!=XVALUE_ITER_END) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
            goto done;
        }
    } else if (xrtTypedListIterNext(&nativeIterator,NULL) || xrtGetError() || scope.failed) {
        if (!xrtGetError() && !scope.failed) xrtSetErrorKind(XERR_STATE);
        goto done;
    }
    output->items=(const xvalue* const*)input->owned.items;
    output->keys=input->keys; output->count=count; ok=true;
done:
    if (!ok || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtValueIterEnd(&iterator);
    if (nativeStarted) xrtTypedListIterEnd(&nativeIterator);
    if (view.owner) xl6ListGraphEndV1(&view);
    xrtValueRelease(wire);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    return ok;
}
static void inputEnd(XL6_CollectionImportInputV1* source)
{
    ListInput* input=source->context; *source=(XL6_CollectionImportInputV1){0};
    if (!input) return;
    xl6ArrayInputReleaseV1(&input->owned); xrtFree(input->keys); xrtFree(input);
}
static bool inputConversion(ptr opaque,const xvalue* source,bool* output)
{
    const ListTemplate* target=opaque;
    /* Authenticate the full destination schema without pinning/reading child
     * storage. A matching BUSY child therefore remains a normal reference. */
    if (xl6ListAdmitsSchemaV1(source,&target->view.layout,&target->policy)) {
        *output=false; return true;
    }
    int status=xl6ListErasedProbeV1(source);
    if (status<0) return false;
    if (!status) { *output=false; return true; }
    XL6_ListLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    if (!xl6ListSchemaCopyV1(source,&layout,&policy)) return false;
    bool erased=xl6ReferenceAnyAdmissionV1(&policy,layout.value.elementIdentity);
    xl6SequenceAdmissionDropV3(&policy);
    if (xrtGetError()) return false;
    *output=erased; return true;
}
const XL6_CollectionImportProviderV1* xl6ListImportProviderV1(void)
{ static const XL6_CollectionImportProviderV1 provider={begin,end,empty,admits,append,inputBegin,inputEnd,inputConversion}; return &provider; }
