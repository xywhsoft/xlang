#include "src/runtime/xl6_process_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
void xl6ProcessStatusDestroyV1(void* handle){xrtFree(handle);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ProcessStatusHandlePolicyV1Get, xl6ProcessStatusDestroyV1)
static xprocessstatus* statusCheck(void* handle)
{if(!handle)xrtSetErrorKind(XERR_ARGUMENT);return handle;}
#define STATUS(Name,Type,Field) \
Type xl6ProcessStatus##Name##V1(void* handle){xprocessstatus* s=statusCheck(handle);return s ? s->Field : 0;}
STATUS(Kind,int64_t,Kind)
STATUS(Code,int64_t,Code)
STATUS(Signal,int64_t,Signal)
STATUS(Stop,int64_t,Stop)
STATUS(CoreDumped,bool,CoreDumped)
#undef STATUS
