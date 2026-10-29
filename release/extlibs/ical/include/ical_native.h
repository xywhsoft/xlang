#ifndef XL_EXTLIB_ICAL_NATIVE_H
#define XL_EXTLIB_ICAL_NATIVE_H

#include <stddef.h>
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* parse 返回独立 xvalue*；build 返回 XRT 分配的 C 字符串。 */
xvalue *xi_parse(const char *text, size_t size);
void *xi_build(xvalue *calendar);

#ifdef __cplusplus
}
#endif

#endif
