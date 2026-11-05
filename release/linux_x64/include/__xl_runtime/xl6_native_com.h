#ifndef XL6_NATIVE_COM_H
#define XL6_NATIVE_COM_H
#include "xl6_code_lease.h"
#include "xl6_ownership_catalog.h"
#include "xnative_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Cold, borrowed service descriptions. FromLease requires a genuine retained
 * capability; ProcessResident is an explicit C-host residency promise. Neither
 * constructor retains the borrowed lease/domain. Each successful wrap does.
 * Portable construction succeeds on all platforms; only Windows implements
 * COM wrapping. Non-Windows wrap calls fail without consuming the native duty. */
XL6_CODE_LEASE_API bool xl6TransferServicesFromLeaseV1(XL6_CodeLease*,
    const XL6_OwnershipDomainV1*,xn_transfer_services_v1*);
XL6_CODE_LEASE_API bool xl6TransferServicesProcessResidentV1(
    const XL6_OwnershipDomainV1*,xn_transfer_services_v1*);
#ifdef __cplusplus
}
#endif
#endif
