#include "xl6_array_native_value_admission.h"
#include "xl6_slot_map_key_value.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct KeyPolicy {
    XL6_SequenceNativeValuePolicyV1 contract; /* mandatory transfer-5 prefix */
    volatile int32_t refs;
} KeyPolicy;
static bool retain(const void* raw)
{
    KeyPolicy* policy=(KeyPolicy*)raw;
    if (policy && xrtRefRetain(&policy->refs)>0) return true;
    xrtSetErrorKind(XERR_STATE); return false;
}
static void drop(const void* raw)
{
    KeyPolicy* policy=(KeyPolicy*)raw;
    int32_t refs=xrtRefRelease(&policy->refs); if (refs<0) abort();
    if (!refs) xrtFree(policy);
}
static bool empty(const void* raw)
{ const XL6_SlotMapKeyV1* key=raw; return key && !key->instance && !key->slot; }
static bool copy(const void* input,void* output)
{
    return xl6NativeLeafValueCopyV1(xl6SlotMapKeyValueProfileV1Get(),input,output);
}
static bool slot(const void* raw,const void* item)
{
    (void)raw; const XL6_SlotMapKeyV1* key=item;
    return key && (!key->instance==!key->slot);
}
static bool value(const void* raw,const xvalue* item)
{ (void)raw; return xl6SlotMapKeyValueAdmitsV1(item); }
static bool equivalent(const void* a,const void* b) { (void)a; (void)b; return true; }
static bool defaultable(const void* raw) { (void)raw; return true; }
static bool trace(const void* raw,const void* item,xrtownershipvisitor visit,void* context)
{ (void)context; return visit && slot(raw,item); } /* no language owning edges */
static const XL6_SequenceAdmissionOpsV2 operations={retain,drop,slot,value,equivalent,defaultable,NULL,trace};
const XL6_SequenceAdmissionOpsV2* xl6ArraySlotMapKeyAdmissionOpsV2Get(void) { return &operations; }
xvalue* xl6SequenceSlotMapKeyCreateV1(unsigned int role,uint64_t identity)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    xvalue* result=NULL; KeyPolicy* policy=NULL;
    if (!identity || (role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1)) xrtSetErrorKind(XERR_ARGUMENT);
    else policy=xrtCalloc(1,sizeof(*policy));
    if (policy) {
        policy->refs=1;
        policy->contract=(XL6_SequenceNativeValuePolicyV1){xl6SlotMapKeyTypeV1(),xl6SlotMapKeyContentPolicyV1(),empty,copy};
        XL6_SequenceAdmissionV3 admission={&operations,policy,UINT64_C(0x5BF65A92DC10A954),
            XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,sizeof(XL6_SlotMapKeyV1),xl6SlotMapKeyTypeV1(),UINT64_C(0x5BF65A92DC10A954)};
        result=xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
    }
    if (!result || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) {
        xrtValueRelease(result); xl6ErrorScopePropagateFailure(outcome); return NULL;
    }
    return result;
}
