#ifndef XLANG_EXTLIB_CLI_NATIVE_H
#define XLANG_EXTLIB_CLI_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

XRT_EXTERN_C_BEGIN

/* 返回调用方拥有的结果；失败返回 NULL，详情见 XRT 线程错误。 */
xvalue* xcli_parse(xvalue* args, xvalue* spec);

XRT_EXTERN_C_END

#endif
