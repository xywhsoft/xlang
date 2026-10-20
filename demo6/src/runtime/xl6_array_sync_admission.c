#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_sync_admission.h"
#include "src/runtime/xl6_sync.h"
#include <stdlib.h>
/* Owned leaf metadata; the native kind is part of equivalence even if a C
 * host deliberately reuses an identity. No live object, bounds, source code,
 * callbacks or module identity spans are retained. Admission allocates nothing. */
typedef struct SyncPolicy {
    volatile int32_t refs;
    uint64_t identity;
    unsigned int kind;
    bool nullable;
} SyncPolicy;
static bool retain(const void* raw)
{
    SyncPolicy* policy=(SyncPolicy*)raw;
    if (!policy || xrtRefRetain(&policy->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* raw)
{
    SyncPolicy* policy=(SyncPolicy*)raw; int32_t refs=xrtRefRelease(&policy->refs);
    if (refs<0) abort();
    if (!refs) xrtFree(policy);
}
static bool value(const void* raw,const xvalue* item)
{
    const SyncPolicy* policy=raw;
    if (!item || xrtValueType(item)==XVALUE_NULL) return policy->nullable;
    return xl6SyncAdmitsV1(item,policy->identity,policy->kind);
}
static bool slot(const void* raw,const void* item)
{ return item && value(raw,*(xvalue* const*)item); }
static bool equivalent(const void* left,const void* right)
{
    const SyncPolicy* a=left; const SyncPolicy* b=right;
    return a->identity==b->identity && a->kind==b->kind && a->nullable==b->nullable;
}
static bool defaultable(const void* raw) { return ((const SyncPolicy*)raw)->nullable; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};
const XL6_SequenceAdmissionOpsV2* xl6ArraySyncAdmissionOpsV2Get(void) { return &ops; }
xvalue* xl6SequenceSyncCreateV1(unsigned int role,uint64_t identity,uint64_t element,
    uint64_t baseIdentity,bool nullable,unsigned int kind)
{
    if (!identity || !element || !baseIdentity || kind<1 || kind>3 ||
        (nullable ? element==baseIdentity : element!=baseIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    SyncPolicy* policy=xrtMalloc(sizeof(*policy));
    if (!policy) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *policy=(SyncPolicy){.refs=1,.identity=baseIdentity,.kind=kind,.nullable=nullable};
    XL6_SequenceAdmissionV3 admission={&ops,policy,element,3,0,sizeof(xvalue*),xrtTypeValue(),0};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
}
