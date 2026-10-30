#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"

/* Owns only terminal modes, OS handles and native byte buffers. Final-owner destruction restores modes even on another thread; operational calls remain thread-affine. */
static void destroy(void* payload) { xrtConsoleSessionDestroy(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ConsoleSessionHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(ConsoleSession,Handle,xl6ConsoleSessionHandlePolicyV1Get)
