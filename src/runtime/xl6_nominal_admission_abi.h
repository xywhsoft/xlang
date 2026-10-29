#ifndef XL6_NOMINAL_ADMISSION_ABI_H
#define XL6_NOMINAL_ADMISSION_ABI_H
#include <stddef.h>
#include <stdint.h>

/* Immutable, callback-free data. The identity span is borrowed only for the
 * synchronous call; this descriptor is NOT an Array owner's stored policy.
 * Runtime-tag identities come from the verified class/protocol closure, while
 * exact identities cover nominal values without an object-backed layout. */
typedef enum XL6_NominalAdmissionKindV1 {
    XL6_NOMINAL_ADMIT_CLASS_TAG = 1,
    XL6_NOMINAL_ADMIT_EXACT_ID,
    XL6_NOMINAL_ADMIT_PROTOCOL_TAG
} XL6_NominalAdmissionKindV1;
typedef struct XL6_NominalAdmissionV1 {
    XL6_NominalAdmissionKindV1 kind;
    uint64_t nonObjectIdentity;
    const uint64_t* identities;
    size_t count;
} XL6_NominalAdmissionV1;
#endif
