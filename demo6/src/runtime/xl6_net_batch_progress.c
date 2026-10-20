#include "xl6_net_batch_progress_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
void xl6NetBatchProgressClearV1(void* raw)
{if(raw)*(XL6_NetBatchProgressV1*)raw=(XL6_NetBatchProgressV1){.status=XNET_RESULT_ERROR};}
void* xl6NetBatchProgressCreateV1(void)
{
    void* value=xrtMalloc(sizeof(XL6_NetBatchProgressV1));
    if(value)xl6NetBatchProgressClearV1(value);else xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return value;
}
void xl6NetBatchProgressDestroyV1(void* raw){xrtFree(raw);}
bool xl6NetBatchProgressAttemptedV1(const void* raw)
{return raw&&((const XL6_NetBatchProgressV1*)raw)->attempted;}
uint64_t xl6NetBatchProgressCountV1(const void* raw)
{return raw?((const XL6_NetBatchProgressV1*)raw)->completed:0;}
uint64_t xl6NetBatchProgressStartV1(const void* raw)
{return raw?((const XL6_NetBatchProgressV1*)raw)->start:0;}
int64_t xl6NetBatchProgressStatusV1(const void* raw)
{return raw?((const XL6_NetBatchProgressV1*)raw)->status:XNET_RESULT_ERROR;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetBatchProgressHandlePolicyV1Get,xl6NetBatchProgressDestroyV1)
