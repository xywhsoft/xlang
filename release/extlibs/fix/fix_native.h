#ifndef XL_FIX_NATIVE_H
#define XL_FIX_NATIVE_H

#include <stdbool.h>
#include <stdint.h>
typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* build 结果由 C 调用方以 xrtFree 释放；xvalue 结果以 xrtValueRelease 释放。 */
xvalue *xfix_field(int64_t tag, const char *value);
void *xfix_build(const char *begin_string, xvalue *fields);
xvalue *xfix_parse(const char *message);
bool xfix_validate(const char *message);
/* validate 的格式错误不留错误；分配失败保留 XRT 内存错误。 */
bool xfix_has_error(void);
bool xfix_memory_error(void);
bool xfix_range_error(void);
const char *xfix_error_message(void);

#ifdef __cplusplus
}
#endif
#endif
