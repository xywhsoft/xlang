#include "xl6_native_leaf_value.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct NativeLeaf {
    volatile int32_t refs;
    const XL6_NativeLeafValueProfileV1* profile;
    _Alignas(max_align_t) unsigned char value[];
} NativeLeaf;
bool xl6NativeLeafValueProfileValidV1(const XL6_NativeLeafValueProfileV1* profile)
{
    const xrttype* type=profile && profile->Type ? profile->Type() : NULL;
    return type && type->Id && profile->identity && profile->content && profile->content->Hash &&
        profile->content->Equal && profile->Empty && type->Kind==XRT_TYPE_RECORD && type->Size && type->Align &&
        type->Align<=_Alignof(max_align_t) && (_Alignof(max_align_t)%type->Align)==0 && type->Size%type->Align==0 &&
        type->Size<=SIZE_MAX - offsetof(NativeLeaf,value) && type->Ops && type->Ops->Copy && type->Ops->Drop &&
        (type->Flags&XRT_TYPE_FLAG_COPYABLE) && !(type->Flags&XRT_TYPE_FLAG_REFERENCE);
}
static bool enter(xl6_error_scope* scope)
{ return xl6ErrorScopeBegin(scope); }
static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    /* Copy/unbox return checked status; boxing returns an owned Value or
     * NULL. These outcomes must not escape an enclosing language catch. */
    if (!result.ok) xrtSetErrorTake(result.error);
    return result.ok;
}
static bool aligned(const void* value,const xrttype* type)
{ return value && (uintptr_t)value%type->Align==0; }
static bool copy_value(const XL6_NativeLeafValueProfileV1* profile,const void* input,void* output,xl6_error_scope* scope)
{
    bool copied=false;
    if (!xl6NativeLeafValueProfileValidV1(profile)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        const xrttype* type=profile->Type();
        /* Do not inspect either native record before alignment/alias
         * admission. In particular Empty itself may perform typed loads. */
        if (!aligned(input,type) || !aligned(output,type) ||
            xrtMemRangesOverlap(input,type->Size,output,type->Size) || !profile->Empty(output))
            xrtSetErrorKind(XERR_ARGUMENT);
        else {
            copied=xrtTypeCopyValue(type,output,input);
            if (!copied || xrtGetError() || scope->failed) {
                xl6ErrorScopeCaptureFailure(scope);
                xrtTypeDropValue(type,output); copied=false;
            }
        }
    }
    return copied;
}
bool xl6NativeLeafValueCopyV1(const XL6_NativeLeafValueProfileV1* profile,const void* input,void* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    /* Box/unbox already own a transaction and use this same body, avoiding
     * a second TLS scope at each otherwise allocation-free native copy. */
    bool copied=copy_value(profile,input,output,&scope);
    return finish(&scope,copied);
}
static void drop(ptr raw,ptr context)
{
    (void)context; NativeLeaf* box=raw; if (!box) return;
    int32_t refs=xrtRefRelease(&box->refs); if (refs<0) abort();
    if (!refs) { xrtTypeDropValue(box->profile->Type(),box->value); xrtFree(box); }
}
static bool retain(ptr raw,ptr* output,ptr context)
{
    NativeLeaf* box=raw;
    if (context || !box || !output || xrtRefRetain(&box->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    *output=box; return true;
}
static uint64 hash(ptr raw,ptr context)
{
    (void)context; const NativeLeaf* box=raw;
    return box->profile->content->Hash(box->value,box->profile->content->UserData);
}
static bool equal(ptr left,ptr right,ptr context)
{
    (void)context; const NativeLeaf* a=left,*b=right;
    return a->profile==b->profile && a->profile->content->Equal(a->value,b->value,a->profile->content->UserData);
}
static const xvaluehandleops operations={retain,drop,hash,equal};
static bool leaf(const xvalue* value,xrtownershipvisitor visit,ptr context)
{ (void)value; (void)context; return visit!=NULL; }
static const NativeLeaf* probe(const xvalue* value)
{
    if (!value || xrtValueType(value)!=XVALUE_HANDLE) return NULL;
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
    /* Authenticate the producer before reading ANY foreign raw payload. */
    if (!xrtValueGetHandle(value,&raw,&actual,&user) || actual!=&operations || !raw || user) return NULL;
    const NativeLeaf* box=raw;
    return box->profile->identity==xrtValueTypeId(value) ? box : NULL;
}
bool xl6NativeLeafValueIsV1(const xvalue* value) { return probe(value)!=NULL; }
bool xl6NativeLeafValueAdmitsV1(const xvalue* value,const XL6_NativeLeafValueProfileV1* profile)
{ const NativeLeaf* box=probe(value); return box && box->profile==profile; }
const xrtownershipadapterv1* xl6NativeLeafValueOwnershipAdapterV1(
    xrtownershipref ref,const XL6_NativeLeafValueProfileV1* profile)
{
    const xrtownershipadapterv1* adapter=xrtValueHandleOwnershipAdapterV1(ref,&operations,leaf);
    return adapter && xl6NativeLeafValueAdmitsV1((const xvalue*)ref.Data,profile) ? adapter : NULL;
}
xvalue* xl6NativeLeafValueBoxV1(const XL6_NativeLeafValueProfileV1* profile,const void* input)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    NativeLeaf* box=NULL; xvalue* result=NULL;
    if (!xl6NativeLeafValueProfileValidV1(profile) || !aligned(input,profile->Type())) xrtSetErrorKind(XERR_ARGUMENT);
    else box=xrtCalloc(1,offsetof(NativeLeaf,value)+profile->Type()->Size);
    if (box) {
        box->refs=1; box->profile=profile;
        bool copied=copy_value(profile,input,box->value,&scope);
        if (!copied || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
        else {
            result=xrtValueHandleTake((ptr*)&box,&operations,NULL);
            if (result && (!xrtValueHandleOwnershipBind(result,leaf) || !xrtValueTypeIdBind(result,profile->identity))) {
                xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
            }
        }
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    drop(box,NULL);
    if (xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    if (!finish(&scope,result!=NULL)) return NULL;
    return result;
}
bool xl6NativeLeafValueUnboxV1(const xvalue* value,const XL6_NativeLeafValueProfileV1* profile,void* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    const NativeLeaf* box=probe(value); bool copied=false; xvalue* pin=NULL;
    if (!xl6NativeLeafValueProfileValidV1(profile) || !aligned(output,profile->Type()) || !profile->Empty(output)) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!box || box->profile!=profile) xrtSetErrorKind(XERR_TYPE);
    else if ((pin=xrtValueRetain((xvalue*)value))!=NULL) {
        copied=copy_value(profile,box->value,output,&scope);
    }
    xrtValueRelease(pin);
    if (copied && (xrtGetError() || scope.failed)) {
        /* The pin's release is part of this transaction too. A late failure
         * must not publish an owned output while reporting checked failure. */
        xl6ErrorScopeCaptureFailure(&scope); xrtTypeDropValue(profile->Type(),output); copied=false;
    }
    return finish(&scope,copied);
}
