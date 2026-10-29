#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "include/xl.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Audited XL_Context, NOT XLC_Context or XL6_XlangHandler. It owns native
 * text/configuration and OS library handles, never an xvalue or XL_Module.
 * Compile units, module entries and borrowed arguments live on each call's
 * stack; activeCalls defers context destruction until those calls return.
 * The policy and its destructor stay resident, unlike a generated Drop thunk.
 * Adding owning language state to XL_Context requires a new graph contract. */
static void destroy(void* payload)
{ xlDestroyContext((XL_Context*)payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6XlangContextHandlePolicyV1Get,destroy)
