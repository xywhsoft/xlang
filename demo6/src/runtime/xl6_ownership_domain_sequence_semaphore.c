/* Exact resident capability; selected independently of its producer. */
#include "release/include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_semaphore_admission.h"

XL6_OWNERSHIP_DOMAIN_POLICY(SequenceSemaphore,Sequence,xl6ArraySemAdmissionOpsV2Get)
