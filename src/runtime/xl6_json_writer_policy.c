#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"

/* Only the audited default/pretty language factories are admitted. They use the internal native buffer sink; value() serializes immediately and retains no xvalue. */
static void destroy(void* payload) { xrtJsonWriterFree(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6JsonWriterHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(JsonWriter,Handle,xl6JsonWriterHandlePolicyV1Get)
