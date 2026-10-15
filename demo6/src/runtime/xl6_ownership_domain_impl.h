#ifndef XL6_OWNERSHIP_DOMAIN_IMPL_H
#define XL6_OWNERSHIP_DOMAIN_IMPL_H
#include "xl6_ownership_catalog.h"
/* Small ABI adapters only. Semantic graph algorithms remain in OwnershipPlan
 * and the actual provider; these macros never inspect a node's data/layout. */
#define XL6_OWNERSHIP_DOMAIN(Name, Loader, Query) \
    XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomain##Name##V1Get(void) \
    { static const XL6_OwnershipDomainV1 domain={sizeof(domain),Loader,Query};return &domain; }
#define XL6_OWNERSHIP_QUERY(Name) \
    static bool ownershipDomain##Name(struct xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies, \
        const struct xrtownershipadapterv1** adapter,const struct xrtownershippreparationv1** preparation, \
        const XL6_OwnershipFinishV1** completion)
#define XL6_OWNERSHIP_DOMAIN_SIMPLE(Name, Function) \
    XL6_OWNERSHIP_QUERY(Name) \
    { (void)policies;(void)preparation;(void)completion;*adapter=Function(ref);return *adapter!=NULL; } \
    XL6_OWNERSHIP_DOMAIN(Name,NULL,ownershipDomain##Name)
#define XL6_OWNERSHIP_DOMAIN_SEQUENCE(Name, Function) \
    XL6_OWNERSHIP_QUERY(Name) \
    { (void)preparation;(void)completion;*adapter=Function(ref,policies->Sequence,policies->SequenceCount);return *adapter!=NULL; } \
    XL6_OWNERSHIP_DOMAIN(Name,NULL,ownershipDomain##Name)
#define XL6_OWNERSHIP_DOMAIN_PREPARED(Name, Function) \
    XL6_OWNERSHIP_QUERY(Name) \
    { (void)policies;(void)completion;*adapter=Function(ref,preparation);return *adapter!=NULL; } \
    XL6_OWNERSHIP_DOMAIN(Name,NULL,ownershipDomain##Name)
#define XL6_OWNERSHIP_DOMAIN_POLICY(Name, Group, Function) \
    static bool ownershipLoad##Name(XL6_OwnershipCatalog* catalog) \
    { return xl6OwnershipCatalogAdd##Group##V1(catalog,Function()); } \
    XL6_OWNERSHIP_DOMAIN(Name,ownershipLoad##Name,NULL)
#endif
