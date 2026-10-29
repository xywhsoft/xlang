#include "src/runtime/xl6_array_extract_native_value_abi.h"
#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_native_leaf_value.h"
#include "src/runtime/xl6_native_collection_value.h"
#include "src/api/xl_foreign_error.h"

typedef bool (*Unbox)(const xvalue*,const void*,void*);
static bool extract(const xvalue* source,uint64_t identity,uint64_t element,int64_t index,bool pop,
    bool valid,const xrttype* type,uint64_t profileIdentity,const xtypedsetkeypolicy* content,
    bool (*empty)(const void*),Unbox unbox,const void* profile,void* output)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    XL6_ArrayExtractView view={0};
    bool admitted=false,ok=false;
    if (!valid || element!=profileIdentity || !output)
        xrtSetErrorKind(XERR_ARGUMENT);
    else {
        if ((uintptr_t)output%type->Align || !empty(output)) xrtSetErrorKind(XERR_ARGUMENT);
        else admitted=true;
    }
    if (admitted && xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) {
        if (view.owner) {
            xtypedarray* storage=xl6SequenceStorageV1(view.owner);
            XL6_SequenceAdmissionV3 policy={0};
            if (xl6ArrayExtractSlotMatches(&view,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,type->Size,type) &&
                xl6SequenceAdmissionCopyV3(view.owner,&policy)) {
                const XL6_SequenceNativeValuePolicyV1* value=xl6SequenceNativeValuePolicyV1(&policy);
                if (!value || value->content!=content) xrtSetErrorKind(XERR_TYPE);
                else if (xrtTypedArrayCapacity(storage)>SIZE_MAX/type->Size) xrtSetErrorKind(XERR_STATE);
                else if (xrtMemRangesOverlap(xrtTypedArrayConstGet(storage,0),
                    xrtTypedArrayCapacity(storage)*type->Size,output,type->Size)) {
                    /* Refusal must never Drop a slot belonging to the receiver,
                     * including an empty non-selected slot in its backing. */
                    admitted=false; xrtSetErrorKind(XERR_ARGUMENT);
                }
                else ok=xrtTypedArrayTake(storage,view.at,output);
            }
            xl6SequenceAdmissionDropV3(&policy);
        } else {
            ok=unbox(view.item,profile,output);
            if (ok) ok=xl6ArrayExtractWireCommit(&view);
        }
    }
    ok=xl6ArrayExtractFinish(&view,ok);
    if (!ok || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope);
        if (admitted && !empty(output)) xrtTypeDropValue(type,output);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static bool unboxLeaf(const xvalue* item,const void* p,void* output)
{return xl6NativeLeafValueUnboxV1(item,p,output);}
static bool unboxCollection(const xvalue* item,const void* raw,void* output)
{
    const XL6_NativeCollectionValueProfileV1* p=raw;
    return p->Converter()->ToTyped(item,p->Type(),output,NULL);
}
bool xl6ArrayExtractNativeValueV1(const xvalue* source,uint64_t identity,uint64_t element,
    int64_t index,bool pop,const XL6_NativeLeafValueProfileV1* p,void* output)
{
    bool valid=xl6NativeLeafValueProfileValidV1(p);
    return extract(source,identity,element,index,pop,valid,valid?p->Type():NULL,
        valid?p->identity:0,valid?p->content:NULL,valid?p->Empty:NULL,unboxLeaf,p,output);
}
bool xl6ArrayExtractNativeValueV2(const xvalue* source,uint64_t identity,uint64_t element,
    int64_t index,bool pop,const XL6_NativeCollectionValueProfileV1* p,void* output)
{
    bool valid=xl6NativeCollectionValueProfileValidV1(p);
    return extract(source,identity,element,index,pop,valid,valid?p->Type():NULL,
        valid?p->identity:0,valid?p->Content():NULL,valid?p->Empty:NULL,unboxCollection,p,output);
}
