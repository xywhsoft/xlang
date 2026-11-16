#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

int main(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));
    xl6_error_result ok=xl6ErrorScopeEnd(&warm);assert(ok.ok && !ok.error);
    xl6ForeignIntegerFailureV1(-17,"warm");xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xl6ForeignIntegerFailureV1(-17,"count");xerror* error=xrtTakeError();
    assert(error && xrtErrorCode(error)==-17 && strstr(xrtErrorMessage(error),"-17"));xrtErrorFree(error);
    xl6ForeignIntegerFailureV1(INT64_MIN,"wide");error=xrtTakeError();
    assert(error && xrtErrorCode(error)==INT32_MIN && strstr(xrtErrorMessage(error),"-9223372036854775808"));xrtErrorFree(error);
    xrtSetErrorInfo(XERR_TYPE,"producer",321,"first diagnostic");const xerror* primary=xrtGetError();assert(primary);
    xl6ForeignIntegerFailureV1(-17,"must not replace");assert(xrtGetError()==primary && xrtErrorCode(primary)==321);xrtClearError();
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(0));
    xl6ForeignIntegerFailureV1(-17,"diagnostic allocation failure");
    xl6ErrorScopeCaptureFailure(&scope);xl6_error_result failed=xl6ErrorScopeEnd(&scope);
    assert(xrtMemDebugFailAfter(-1));assert(!failed.ok);xrtErrorFree(failed.error);
    xrtMemDebugSnapshot(&after);assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    puts("integer failure: exact/wide/primary diagnostic/OOM/heap balance passed");return 0;
}
