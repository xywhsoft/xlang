#include "xl6_native_leaf_value.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct NativeLeaf {
    volatile int32_t refs;
    const XL6_NativeLeafValueProfileV1* profile;
    _Alignas(max_align_t) unsigned char value[];
} NativeLeaf;
static bool valid(const XL6_NativeLeafValueProfileV1* profile)
{
    const xrttype* type=profile && profile->Type ? profile->Type() : NULL;
    return type && profile->identity && profile->content && profile->content->Hash &&
        profile->content->Equal && profile->Empty && type->Kind==XRT_TYPE_RECORD && type->Size && type->Align &&
        type->Align<=_Alignof(max_align_t) && (_Alignof(max_align_t)%type->Align)==0 &&
        type->Size<=SIZE_MAX - offsetof(NativeLeaf,value) && type->Ops && type->Ops->Copy && type->Ops->Drop &&
        (type->Flags&XRT_TYPE_FLAG_COPYABLE) && !(type->Flags&XRT_TYPE_FLAG_REFERENCE);
}
static bool enter(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
}
static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
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
    if (!valid(profile) || !input) xrtSetErrorKind(XERR_ARGUMENT);
    else box=xrtCalloc(1,offsetof(NativeLeaf,value)+profile->Type()->Size);
    if (box) {
        box->refs=1; box->profile=profile;
        bool copied=xrtTypeCopyValue(profile->Type(),box->value,input);
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
    if (!finish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
    return result;
}
bool xl6NativeLeafValueUnboxV1(const xvalue* value,const XL6_NativeLeafValueProfileV1* profile,void* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    const NativeLeaf* box=probe(value); bool copied=false; xvalue* pin=NULL;
    if (!valid(profile) || !output || !profile->Empty(output)) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!box || box->profile!=profile) xrtSetErrorKind(XERR_TYPE);
    else if ((pin=xrtValueRetain((xvalue*)value))!=NULL) {
        copied=xrtTypeCopyValue(profile->Type(),output,box->value);
        if (!copied || xrtGetError() || scope.failed) {
            xl6ErrorScopeCaptureFailure(&scope); xrtTypeDropValue(profile->Type(),output);
        }
    }
    xrtValueRelease(pin);
    return finish(&scope,copied);
}
