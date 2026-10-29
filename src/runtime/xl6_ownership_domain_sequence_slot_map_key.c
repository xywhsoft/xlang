/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_array_native_value_admission.h"

XL6_OWNERSHIP_DOMAIN_POLICY(SequenceSlotMapKey,Sequence,xl6ArraySlotMapKeyAdmissionOpsV2Get)
