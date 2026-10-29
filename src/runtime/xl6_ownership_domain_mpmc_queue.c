/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_concurrent_queue.h"

XL6_OWNERSHIP_DOMAIN_SEQUENCE(MpmcQueue,xl6MpmcQueueOwnershipAdapterV1)
