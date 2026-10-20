#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_semaphore_admission.h"
#include "src/runtime/xl6_semaphore.h"
#include <stdlib.h>

/* A semaphore has no generic child schema or owning language edges. Its
 * immutable policy contains only the exact identity + private native family
 * check, never a live semaphore, count/bounds, source callback or module data. */
typedef struct SemPolicy {
    volatile int32_t refs;
    uint64_t identity;
    bool nullable;
} SemPolicy;
static bool retain(const void* raw)
{
    SemPolicy* policy = (SemPolicy*)raw;
    if (!policy || xrtRefRetain(&policy->refs) <= 0) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    return true;
}
static void drop(const void* raw)
{
    SemPolicy* policy = (SemPolicy*)raw;
    int32_t refs = xrtRefRelease(&policy->refs);
    if (refs < 0) abort();
    if (!refs) xrtFree(policy);
}
static bool value(const void* raw, const xvalue* item)
{
    const SemPolicy* policy = raw;
    if (!item || xrtValueType(item) == XVALUE_NULL) return policy->nullable;
    return xl6SemAdmitsV1(item, policy->identity);
}
static bool slot(const void* raw, const void* item)
{ return item && value(raw, *(xvalue* const*)item); }
static bool equivalent(const void* left, const void* right)
{
    const SemPolicy* a = left; const SemPolicy* b = right;
    return a->identity == b->identity && a->nullable == b->nullable;
}
static bool defaultable(const void* raw) { return ((const SemPolicy*)raw)->nullable; }
static const XL6_SequenceAdmissionOpsV2 ops = {retain, drop, slot, value, equivalent,
    defaultable, NULL, NULL};
const XL6_SequenceAdmissionOpsV2* xl6ArraySemAdmissionOpsV2Get(void) { return &ops; }
xvalue* xl6SequenceSemCreateV1(unsigned int role, uint64_t identity, uint64_t element,
    uint64_t baseIdentity, bool nullable)
{
    if (!identity || !element || !baseIdentity ||
        (nullable ? element == baseIdentity : element != baseIdentity)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    SemPolicy* policy = xrtMalloc(sizeof(*policy));
    if (!policy) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *policy = (SemPolicy){.refs = 1, .identity = baseIdentity, .nullable = nullable};
    XL6_SequenceAdmissionV3 admission = {&ops, policy, element,
        3,0, sizeof(xvalue*), xrtTypeValue(), 0};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role, identity, &admission);
}
