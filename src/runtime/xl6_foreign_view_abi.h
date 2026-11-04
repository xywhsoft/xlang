#ifndef XL6_FOREIGN_VIEW_ABI_H
#define XL6_FOREIGN_VIEW_ABI_H
#include "xl6_foreign_view.h"
#include "xl6_foreign_view_value.h"
#include "xl6_foreign_view_sequence.h"

/* Minimum alignment guaranteed by the pinned resident XRT heap on every
 * supported target. In-place C records are checked against this ABI limit. */
#define XL6_FOREIGN_RECORD_ALIGNMENT_V1 16u

/* A language owner is one control pointer, never the native payload pointer.
 * Copy publishes one independent owner into an empty output. Drop consumes
 * its by-value owner after the caller has detached its owning storage. */
typedef struct xlir_result_foreign_view_v1 {
    bool ok;
    XL6_ForeignView* value;
} xlir_result_foreign_view_v1;
XL6_CODE_LEASE_API bool xl6ForeignViewCopyValueV1(XL6_ForeignView*,XL6_ForeignView**);
XL6_CODE_LEASE_API void xl6ForeignViewDropValueV1(XL6_ForeignView*);
XL6_CODE_LEASE_API bool xl6ForeignViewEqualValueV1(XL6_ForeignView*,XL6_ForeignView*);

/* A stack observation protects the rollback decision of a native factory.
 * Prepare validates the exact typed watch storage BEFORE calling the factory.
 * Observe runs before checking its outcome. The surrounding native owner
 * scope must keep raw memory addressable; Watch refusal means no live output.
 * Watch notification and all token operations run on the creator thread.
 * The observer never owns raw and never calls its destructor. */
typedef struct XL6_ForeignResultProbeV1 {
    struct XL6_ForeignResultProbeV1* self;
    uint64_t thread;
    XL6_ForeignViewDescriptorV1 descriptor;
    void* storage;
    void* identity;
    bool root;
    bool live;
    bool observed;
} XL6_ForeignResultProbeV1;
XL6_CODE_LEASE_API bool xl6ForeignResultProbePrepareV1(XL6_ForeignResultProbeV1*,
    const XL6_ForeignViewDescriptorV1*,bool root,void*,size_t);
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewCreateRootProbedV1(void*,XL6_ForeignResultProbeV1*,const XL6_CodeOwner*);
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewCreateRootWithDomainProbedV1(
    void*,XL6_ForeignResultProbeV1*,const XL6_OwnershipDomainV1*,const XL6_CodeOwner*);
XL6_CODE_LEASE_API bool xl6ForeignResultProbeObserveV1(XL6_ForeignResultProbeV1*,void*);
XL6_CODE_LEASE_API bool xl6ForeignResultProbeAliveV1(const XL6_ForeignResultProbeV1*);
XL6_CODE_LEASE_API bool xl6ForeignResultProbeEndV1(XL6_ForeignResultProbeV1*);
#endif
