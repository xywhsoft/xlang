#include "xrt.h"
#include <string.h>

/* 不依赖 ZIP 的真实 XRT 错误；公开 getter 只借用消息。 */
bool xl_test_error_pending(void) {
    xrtSetErrorInfo(XERR_IO, "extlib.error-probe", 7, "pending native diagnostic");
    return xrtGetError() != NULL;
}
const char *xl_test_error_text(void) {
    return xrtErrorMessage(xrtGetError());
}
int xl_test_error_direct(void) {
    bool ok = xl_test_error_pending() &&
        strcmp(xl_test_error_text(), "pending native diagnostic") == 0;
    xrtClearError();
    return ok ? 0 : 1;
}
