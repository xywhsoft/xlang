#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_log.h"

/* The resident record owns copied text and typed scalar/error fields. FieldValue constructs a new xvalue on demand; none is stored in the record. */
static void destroy(void* payload) { xl6LogRecordDestroyV1(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6LogRecordHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(LogRecord,Handle,xl6LogRecordHandlePolicyV1Get)
