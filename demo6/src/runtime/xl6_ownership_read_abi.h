#ifndef XL6_OWNERSHIP_READ_ABI_H
#define XL6_OWNERSHIP_READ_ABI_H
#include "xl6_code_owner.h"

/* Compiler-private artifact certificate. The host borrows this immutable
 * descriptor only while the image is resident. It names the SAME complete
 * owning-slot trace as the module CodeOwner graph, never a subset/leaf view.
 * Every generated execution touching those slots uses a tracked host entry,
 * resident CodeScope or borrowed CodeActivity; opaque native writers cannot
 * claim that property. Read-only getters/trace do not enter a mutation scope.
 * This says nothing about admission of arbitrary transitive graph families. */
typedef struct XL6_ModuleReadContractV1 {
    size_t size;
    unsigned long long abiTag;
    XL6_ModuleOwnershipTrace trace;
} XL6_ModuleReadContractV1;
typedef const XL6_ModuleReadContractV1* (*XL6_ModuleReadContractGetV1)(void);
#endif
