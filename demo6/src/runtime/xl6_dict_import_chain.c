#include "xl6_dict_import_chain.h"
#include "xl6_dict_graph_access.h"
#include "xl6_array_input.h"
#include "xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

typedef struct DictTemplate { XL6_DictGraphViewV1 view; XL6_SequenceAdmissionV3 policy; } DictTemplate;
static bool begin(const xvalue* source,ptr* output)
{
    DictTemplate* value=xrtCalloc(1,sizeof(*value));
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    *output=value;
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!source || xrtValueType(source)!=XVALUE_HANDLE || !xrtValueGetHandle(source,&raw,&ops,&user)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    int status=xl6DictGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&value->view);
    if (!status) xrtSetErrorKind(XERR_TYPE);
    if (status!=1) return false;
    XL6_DictLayoutV1 layout={0};
    if (!xl6DictSchemaCopyV1(source,&layout,&value->policy)) return false;
    if (xrtTypedDictCount(value->view.storage)) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void end(ptr opaque)
{
    DictTemplate* value=opaque; if (!value) return;
    if (value->view.owner) xl6DictGraphEndV1(&value->view);
    xl6SequenceAdmissionDropV3(&value->policy); xrtFree(value);
}
static xvalue* empty(ptr opaque) { return xl6DictGraphEmptyV1(&((DictTemplate*)opaque)->view); }
static bool admits(ptr opaque,const xvalue* value)
{ const DictTemplate* source=opaque; return source->policy.ops->Value(source->policy.data,value); }
static bool appendText(xvalue* target,xstrview key,const xvalue* value)
{ return xl6DictErasedSetV1(target,key,value); }

typedef struct DictInput { XL6_ArrayInputV1 owned; xstrview* keys; size_t keyCount; } DictInput;
static bool inputBegin(const xvalue* source,XL6_CollectionImportInputV1* output)
{
    if (!output || output->context || output->items || output->keys || output->textKeys || output->count) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictInput* input=xrtCalloc(1,sizeof(*input)); xvalue* wire=NULL;
    XL6_DictGraphViewV1 view={0}; xtypeddictiter nativeIterator={0}; xvalueiter iterator={0};
    bool started=false,nativeStarted=false,ok=false;
    if (!input) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    output->context=input;
    if (source && xrtValueType(source)==XVALUE_OBJECT) {
        wire=xrtValueRetain(source); if (!wire) goto done;
    } else if (source && xrtValueType(source)==XVALUE_HANDLE) {
        void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
        if (!xrtValueGetHandle(source,&raw,&ops,&user)) goto done;
        int status=xl6DictGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&view);
        if (!status) xrtSetErrorKind(XERR_TYPE);
        if (status!=1) goto done;
    } else { xrtSetErrorKind(XERR_TYPE); goto done; }
    size_t count=wire ? xrtValueCount(wire) : xrtTypedDictCount(view.storage);
    if (count>SIZE_MAX/sizeof(*input->owned.items) || count>SIZE_MAX/sizeof(*input->keys)) {
        xrtSetErrorKind(XERR_RANGE); goto done;
    }
    input->owned.items=count ? xrtMalloc(count*sizeof(*input->owned.items)) : NULL;
    input->keys=count ? xrtMalloc(count*sizeof(*input->keys)) : NULL;
    if (count && (!input->owned.items || !input->keys)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        goto done;
    }
    if (wire) { if (!xrtValueIterBegin(wire,&iterator)) goto done; started=true; }
    else { if (!xrtTypedDictIterBegin(view.storage,&nativeIterator)) goto done; nativeStarted=true; }
    for (size_t i=0;i<count;++i) {
        xstrview key={0}; xvalue* owned=NULL;
        if (wire) {
            xvaluekey actual={0}; xvalue* item=NULL;
            if (xrtValueIterAdvance(&iterator,&actual,&item)!=XVALUE_ITER_ITEM || actual.Type!=XVALUE_KEY_STRING) {
                if (!xrtGetError() && !scope.failed) xrtSetErrorKind(XERR_STATE);
                goto done;
            }
            key=actual.String; owned=xrtValueRetain(item);
        } else {
            const void* slot=xrtTypedDictIterNext(&nativeIterator,&key);
            if (!slot || xrtGetError() || scope.failed) goto done;
            if (view.layout.value.transfer==3) {
                const xvalue* item=*(xvalue* const*)slot;
                owned=xrtValueRetain(item ? item : xrtValueNull());
            } else owned=xl6DictGraphProjectSlotV1(&view,slot);
        }
        if (owned) input->owned.items[input->owned.count++]=owned;
        if (!owned || xrtGetError() || scope.failed) goto done;
        if (key.Size && !key.Data) { xrtSetErrorKind(XERR_STATE); goto done; }
        char* copy=key.Size ? xrtMalloc(key.Size) : NULL;
        if (key.Size && !copy) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
        if (key.Size) memcpy(copy,key.Data,key.Size);
        input->keys[input->keyCount++]=(xstrview){copy,key.Size};
    }
    if (wire) {
        xvaluekey key={0}; xvalue* item=NULL;
        if (xrtValueIterAdvance(&iterator,&key,&item)!=XVALUE_ITER_END) {
            if (!xrtGetError() && !scope.failed) xrtSetErrorKind(XERR_STATE);
            goto done;
        }
    } else if (xrtTypedDictIterNext(&nativeIterator,NULL) || xrtGetError() || scope.failed) goto done;
    output->items=(const xvalue* const*)input->owned.items;
    output->textKeys=input->keys; output->count=count; ok=true;
done:
    if (!ok || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtValueIterEnd(&iterator);
    if (nativeStarted) xrtTypedDictIterEnd(&nativeIterator);
    if (view.owner) xl6DictGraphEndV1(&view);
    xrtValueRelease(wire);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    return ok;
}
static void inputEnd(XL6_CollectionImportInputV1* source)
{
    DictInput* input=source->context; *source=(XL6_CollectionImportInputV1){0};
    if (!input) return;
    for (size_t i=0;i<input->keyCount;++i) xrtFree((void*)input->keys[i].Data);
    xrtFree(input->keys); xl6ArrayInputReleaseV1(&input->owned); xrtFree(input);
}
static bool inputConversion(ptr opaque,const xvalue* source,bool* output)
{
    const DictTemplate* target=opaque;
    if (xl6DictAdmitsSchemaV1(source,&target->view.layout,&target->policy)) { *output=false; return true; }
    int status=xl6DictErasedProbeV1(source);
    if (status<0) return false;
    if (!status) { *output=false; return true; }
    XL6_DictLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    if (!xl6DictSchemaCopyV1(source,&layout,&policy)) return false;
    bool erased=xl6ReferenceAnyAdmissionV1(&policy,layout.value.elementIdentity);
    xl6SequenceAdmissionDropV3(&policy);
    if (xrtGetError()) return false;
    *output=erased; return true;
}
const XL6_CollectionImportProviderV1* xl6DictImportProviderV1(void)
{
    static const XL6_CollectionImportProviderV1 provider={
        .Begin=begin,.End=end,.Empty=empty,.Admits=admits,.AppendText=appendText,
        .InputBegin=inputBegin,.InputEnd=inputEnd,.InputConversion=inputConversion,
        .OwnershipAdapter=xl6DictOwnershipAdapterV1};
    return &provider;
}
