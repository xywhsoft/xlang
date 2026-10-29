#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "lib/tcc/libtcc.h"
#include "xl6_tcc_context_policy.h"
#include "src/api/xl_foreign_error.h"

/* TCC states own compiler/native allocations, not owning xvalue edges.
 * Registered native symbols and callback userdata remain borrowed. Both the
 * policy and the same statically linked TCC core stay process-resident. */
static void destroy_context(void* payload)
{ if (payload) tcc_delete((TCCState*)payload); }

static void destroy_handler(void* payload)
{
    XL6_TccHandler* handler=(XL6_TccHandler*)payload;
    if (!handler) return;
    destroy_context(handler->state);
    xrtFree(handler);
}

XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TccContextHandlePolicyV1Get,destroy_context)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TccHandlerHandlePolicyV1Get,destroy_handler)
