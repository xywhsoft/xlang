#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_thread_admission.h"
#include "src/runtime/xl6_thread.h"
#include <stdlib.h>
/* Immutable, owned type metadata. Actual running closures/leases are owning
 * graph children, not opaque schema state and not implied by a TypeId match. */
typedef struct ThreadPolicy {
    volatile int32_t refs;
    uint64_t identity;
    bool nullable;
} ThreadPolicy;
static bool retain(const void* raw)
{
    ThreadPolicy* policy=(ThreadPolicy*)raw;
    if (!policy || xrtRefRetain(&policy->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* raw)
{
    ThreadPolicy* policy=(ThreadPolicy*)raw; int32_t refs=xrtRefRelease(&policy->refs);
    if (refs<0) abort();
    if (!refs) xrtFree(policy);
}
static bool value(const void* raw,const xvalue* item)
{
    const ThreadPolicy* policy=raw;
    if (!item || xrtValueType(item)==XVALUE_NULL) return policy->nullable;
    return xl6ThreadAdmitsV1(item,policy->identity);
}
static bool slot(const void* raw,const void* item)
{ return item && value(raw,*(xvalue* const*)item); }
static bool equivalent(const void* left,const void* right)
{
    const ThreadPolicy* a=left; const ThreadPolicy* b=right;
    return a->identity==b->identity && a->nullable==b->nullable;
}
static bool defaultable(const void* raw) { return ((const ThreadPolicy*)raw)->nullable; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};
const XL6_SequenceAdmissionOpsV2* xl6ArrayThreadAdmissionOpsV2Get(void) { return &ops; }
xvalue* xl6SequenceThreadCreateV1(unsigned int role,uint64_t identity,uint64_t element,
    uint64_t baseIdentity,bool nullable)
{
    if (!identity || !element || !baseIdentity ||
        (nullable ? element==baseIdentity : element!=baseIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    ThreadPolicy* policy=xrtMalloc(sizeof(*policy));
    if (!policy) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *policy=(ThreadPolicy){.refs=1,.identity=baseIdentity,.nullable=nullable};
    XL6_SequenceAdmissionV3 admission={&ops,policy,element,3,sizeof(xvalue*),xrtTypeValue(),0};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
}
