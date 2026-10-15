/* Exact resident capability; selected independently of its producer. */
#include "release/include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_sync_admission.h"

XL6_OWNERSHIP_DOMAIN_POLICY(SequenceSync,Sequence,xl6ArraySyncAdmissionOpsV2Get)
