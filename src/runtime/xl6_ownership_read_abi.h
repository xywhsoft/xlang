#ifndef XL6_OWNERSHIP_READ_ABI_H
#define XL6_OWNERSHIP_READ_ABI_H
#include "xl6_code_owner.h"

/* Independent resident host capability, copied once before artifact entry.
 * Keep this declaration usable with an incomplete ownership reference: no
 * XRT implementation, graph operations or runtime lookup is embedded here.
 * Begin/End exclude host entry/retirement around a nonblocking Freeze; Admits
 * is called ONLY inside that Freeze. The scope is zero-initialized, same-
 * thread and uncopyable. The callbacks must stay resident with the host.
 * This grants neither semantic clone rights nor admission of unknown graphs.
 */
typedef struct XL6_OwnershipReadScopeV1 {
    const void* self;
    unsigned long long thread;
} XL6_OwnershipReadScopeV1;
enum { XL6_OWNERSHIP_READ_ERROR_V1=-1, XL6_OWNERSHIP_READ_BUSY_V1=0,
    XL6_OWNERSHIP_READ_READY_V1=1 };
typedef struct XL6_OwnershipReadBoundaryV1 {
    size_t size;
    int (*Begin)(XL6_OwnershipReadScopeV1*);
    bool (*Admits)(const XL6_OwnershipReadScopeV1*,struct xrtownershipref);
    void (*End)(XL6_OwnershipReadScopeV1*);
} XL6_OwnershipReadBoundaryV1;
typedef int (*XL6_BindOwnershipReadV1)(const XL6_OwnershipReadBoundaryV1*);

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
