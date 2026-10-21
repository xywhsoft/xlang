#ifndef XRT_SHELLWORDS_H
#define XRT_SHELLWORDS_H

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 返回值由调用方释放；失败时检查 XRT 错误。 */
xvalue* xrtShellwordsSplit(cstr sLine);
str xrtShellwordsQuote(cstr sWord);
str xrtShellwordsQuoteWindows(cstr sWord);
str xrtShellwordsJoin(const xvalue* pWords);

#ifdef __cplusplus
}
#endif

#endif
