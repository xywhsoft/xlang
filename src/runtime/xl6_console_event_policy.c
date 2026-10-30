#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"

/* Owns its UTF-8 xbuffer and scalar event data, with no language reference or retained callback. */
static void destroy(void* payload) { xrtConsoleEventDestroy(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ConsoleEventHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(ConsoleEvent,Handle,xl6ConsoleEventHandlePolicyV1Get)
