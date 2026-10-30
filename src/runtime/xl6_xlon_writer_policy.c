#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"

/* The language factories use native buffer output, with copied format configuration and no user sink or retained input xvalue. */
static void destroy(void* payload) { xrtXlonWriterFree(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6XlonWriterHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(XlonWriter,Handle,xl6XlonWriterHandlePolicyV1Get)
