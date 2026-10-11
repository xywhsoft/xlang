#include "xl6_slot_map_key_value.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

/* Canonical language `type:SlotMapKey`, distinct from the resident C ABI ID.
 * Immutable leaf: owns only an instance record, never the map or source code. */
#define KEY_VALUE_ID UINT64_C(0x5BF65A92DC10A954)
typedef struct KeyBox { volatile int32_t refs; XL6_SlotMapKeyV1 key; } KeyBox;
static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
}
static bool enter(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
}
static void drop(ptr raw,ptr context)
{
    (void)context; KeyBox* box=raw; if (!box) return;
    int32_t refs=xrtRefRelease(&box->refs); if (refs<0) abort();
    if (!refs) { xrtTypeDropValue(xl6SlotMapKeyTypeV1(),&box->key); xrtFree(box); }
}
static bool retain(ptr raw,ptr* output,ptr context)
{
    KeyBox* box=raw;
    if (context || !box || !output || xrtRefRetain(&box->refs)<=0) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    *output=box; return true;
}
uint64 xl6SlotMapKeyHashV1(XL6_SlotMapKeyV1 key)
{
    /* No struct padding, slot address or text sentinel. Real XRT hash;
     * process-local identity, not a persistent identifier. */
    const uint64 fields[2]={(uint64)(uintptr_t)key.instance,(uint64)key.slot};
    return xrtHash64(fields,sizeof(fields));
}
static uint64 hash(ptr raw,ptr context)
{ (void)context; return xl6SlotMapKeyHashV1(((const KeyBox*)raw)->key); }
static bool equal(ptr left,ptr right,ptr context)
{ (void)context; return xl6SlotMapKeyEqualV1(((const KeyBox*)left)->key,((const KeyBox*)right)->key); }
static const xvaluehandleops operations={retain,drop,hash,equal};
static bool leaf(const xvalue* value,xrtownershipvisitor visit,ptr context)
{ (void)value; (void)context; return visit!=NULL; }
const xrtownershipadapterv1* xl6SlotMapKeyValueOwnershipAdapterV1(xrtownershipref ref)
{ return xrtValueHandleOwnershipAdapterV1(ref,&operations,leaf); }
static const KeyBox* probe(const xvalue* value)
{
    if (!value || xrtValueType(value)!=XVALUE_HANDLE || xrtValueTypeId(value)!=KEY_VALUE_ID) return NULL;
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
    /* Authenticate producer BEFORE touching a foreign raw pointer. */
    if (!xrtValueGetHandle(value,&raw,&actual,&user) || actual!=&operations || !raw || user) return NULL;
    return raw;
}
bool xl6SlotMapKeyValueAdmitsV1(const xvalue* value) { return probe(value)!=NULL; }
xvalue* xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1 key)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return NULL;
    KeyBox* box=xrtCalloc(1,sizeof(*box)); xvalue* result=NULL;
    if (box) {
        box->refs=1;
        bool copied=xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&box->key,&key);
        if (!copied || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
        else {
            result=xrtValueHandleTake((ptr*)&box,&operations,NULL);
            if (result && (!xrtValueHandleOwnershipBind(result,leaf) || !xrtValueTypeIdBind(result,KEY_VALUE_ID))) {
                xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
            }
        }
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    drop(box,NULL);
    if (!finish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
    return result;
}
bool xl6SlotMapKeyUnboxV1(const xvalue* value,XL6_SlotMapKeyV1* output)
{
    xl6_error_scope scope={0}; if (!enter(&scope)) return false;
    XL6_SlotMapKeyV1 result={0}; const KeyBox* box=probe(value); bool success=false;
    if (!output || output->instance || output->slot) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!box) xrtSetErrorKind(XERR_TYPE);
    else success=xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&result,&box->key);
    success=finish(&scope,success);
    if (success) *output=result;
    else xrtTypeDropValue(xl6SlotMapKeyTypeV1(),&result);
    return success;
}
static bool toTyped(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    if (context || type!=xl6SlotMapKeyTypeV1()) { xrtSetErrorKind(XERR_TYPE); return false; }
    return xl6SlotMapKeyUnboxV1(value,output);
}
static xvalue* fromTyped(const xrttype* type,const void* input,ptr context)
{
    if (context || type!=xl6SlotMapKeyTypeV1()) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    if (!input) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    return xl6SlotMapKeyBoxV1(*(const XL6_SlotMapKeyV1*)input);
}
const xvalueconverter* xl6SlotMapKeyConverterV1(void)
{
    static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
static uint64 contentHash(const void* key,ptr context)
{ (void)context; return xl6SlotMapKeyHashV1(*(const XL6_SlotMapKeyV1*)key); }
static bool contentEqual(const void* left,const void* right,ptr context)
{ (void)context; return xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)left,*(const XL6_SlotMapKeyV1*)right); }
const xtypedsetkeypolicy* xl6SlotMapKeyContentPolicyV1(void)
{
    static const xtypedsetkeypolicy policy={contentHash,contentEqual,NULL}; return &policy;
}
