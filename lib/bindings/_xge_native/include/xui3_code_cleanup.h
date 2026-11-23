#ifndef XL6_XUI3_CODE_CLEANUP_H
#define XL6_XUI3_CODE_CLEANUP_H

#include <xge3_runtime.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>

static inline bool xl6Xui3CodeCleanupResult(int status, const char* operation)
{
    if (status == XUI_OK) return true;
    if (!xrtGetError()) xl6ForeignStatusFailureV1(status, operation);
    xl6_error_result failure = {false, xrtTakeError()};
    if (!xl6ErrorScopeTryPropagateFailureV1(&failure)) xl6CallbackFailureRecordV1(failure);
    return false;
}

#endif
