#include "xl6_slot_map_key.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

struct XL6_SlotMapInstanceV1 { volatile int32_t refs; };
XL6_SlotMapInstanceV1* xl6SlotMapInstanceCreateV1(void)
{
    XL6_SlotMapInstanceV1* instance=xrtMalloc(sizeof(*instance));
    if (instance) instance->refs=1;
    return instance;
}
bool xl6SlotMapInstanceRetainV1(XL6_SlotMapInstanceV1* instance)
{
    if (instance && xrtRefRetain(&instance->refs)>0) return true;
    xrtSetErrorKind(XERR_STATE); return false;
}
void xl6SlotMapInstanceDropV1(XL6_SlotMapInstanceV1* instance)
{
    if (!instance) return;
    int32_t refs=xrtRefRelease(&instance->refs); if (refs<0) abort();
    if (!refs) xrtFree(instance);
}
static bool keyInit(ptr target,const xrttype* type)
{ (void)type; *(XL6_SlotMapKeyV1*)target=(XL6_SlotMapKeyV1){0}; return true; }
static bool keyCopy(ptr target,const void* source,const xrttype* type)
{
    (void)type; if (target==source) return true;
    const XL6_SlotMapKeyV1* input=source; XL6_SlotMapKeyV1* output=target;
    if ((!output->instance!=!output->slot) || (!input->instance!=!input->slot)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if (input->instance && !xl6SlotMapInstanceRetainV1(input->instance)) return false;
    /* xrttype Copy is assignment into an initialized slot, not the public
     * empty-output acquire operation. Acquire first; failure preserves the
     * prior value. Publication and its no-raise leaf retirement are atomic. */
    XL6_SlotMapInstanceV1* prior=output->instance;
    *output=*input; xl6SlotMapInstanceDropV1(prior); return true;
}
static bool keyMove(ptr target,ptr source,const xrttype* type)
{
    (void)type; if (target==source) return true;
    XL6_SlotMapKeyV1* input=source; XL6_SlotMapKeyV1* output=target;
    if ((!output->instance!=!output->slot) || (!input->instance!=!input->slot)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    XL6_SlotMapInstanceV1* prior=output->instance;
    *output=*input; *input=(XL6_SlotMapKeyV1){0}; xl6SlotMapInstanceDropV1(prior); return true;
}
static void keyDrop(ptr target,const xrttype* type)
{
    (void)type; XL6_SlotMapKeyV1* value=target;
    XL6_SlotMapInstanceV1* instance=value->instance; *value=(XL6_SlotMapKeyV1){0}; xl6SlotMapInstanceDropV1(instance);
}
const xrttype* xl6SlotMapKeyTypeV1(void)
{
    static const xrttypeops operations={.Init=keyInit,.Copy=keyCopy,.Move=keyMove,.Drop=keyDrop,.Clone=keyCopy};
    /* xrtTypeId("xlang.runtime.slot-map-key.v1"). No ordering is granted. */
    static const xrttype type={.Id=UINT64_C(0x672CFF1BC1957517),.Kind=XRT_TYPE_RECORD,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("SlotMapKey"),.AbiName=XRT_STR_INIT("xlang.runtime.slot-map-key.v1"),
        .Size=sizeof(XL6_SlotMapKeyV1),.Align=_Alignof(XL6_SlotMapKeyV1),.InstanceAlign=1,.Ops=&operations};
    return &type;
}
bool xl6SlotMapKeyEqualV1(XL6_SlotMapKeyV1 left,XL6_SlotMapKeyV1 right)
{ return left.instance==right.instance && left.slot==right.slot; }
void xl6SlotMapKeyDropValueV1(XL6_SlotMapKeyV1 value)
{ keyDrop(&value,xl6SlotMapKeyTypeV1()); }
/* Direct native ownership stays independent of erasure/hash/conversion. The
 * input carrier is a by-value observation; output must be aligned and empty.
 * The canonical xrttype operation acquires/publishes under its mutation. */
bool xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 input,XL6_SlotMapKeyV1* output)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false;
    }
    bool copied=false;
    if(!output || (uintptr_t)output%_Alignof(XL6_SlotMapKeyV1) || output->instance || output->slot)
        xrtSetErrorKind(XERR_ARGUMENT);
    else{
        copied=xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),output,&input);
        if(!copied || xrtGetError() || scope.failed){
            xl6ErrorScopeCaptureFailure(&scope);xrtTypeDropValue(xl6SlotMapKeyTypeV1(),output);copied=false;
        }
    }
    if(!copied || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result r=xl6ErrorScopeEnd(&scope);if(!r.ok)xl6ErrorScopePropagateFailure(r);return r.ok;
}
XL6_SlotMapKeyV1 xl6SlotMapKeyCloneV1(XL6_SlotMapKeyV1 input)
{
    XL6_SlotMapKeyV1 result={0};
    if(!xl6SlotMapKeyCopyValueV1(input,&result)){
        xl6SlotMapKeyDropValueV1(result);if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
        return (XL6_SlotMapKeyV1){0};
    }return result;
}
