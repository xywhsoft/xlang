#ifndef XL_EXTLIB_COM_H
#define XL_EXTLIB_COM_H

#include <stdint.h>
#include "xrt.h"

typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* C 接口使用 XRT 的 xvalue；返回的 xvalue 由调用者释放。 */
void* x_com_create(const char* progid);
void* x_com_attach(void* dispatch);
int64_t x_com_available(void);
const char* x_com_version(void);
int64_t x_com_release(void* handle);
const char* x_com_last_error(void);
int64_t x_com_is_null(void* handle);

/* 文本借用自 handle，下一次该 handle 的文本读取或释放后失效。 */
const char* x_com_get_text(void* handle, const char* name);
/* 返回值由调用方 xrtFree；失败返回 NULL，空文本返回独立空串。 */
void* x_com_get_text_owned(void* handle, const char* name);
int64_t x_com_get_int(void* handle, const char* name);
int64_t x_com_put_var(void* handle, const char* name, xvalue* value);
int64_t x_com_put_ref(void* handle, const char* name, xvalue* value);
void* x_com_missing(void);
xvalue* x_com_invoke_array(void* handle, const char* name, xvalue* args);
xvalue* x_com_invoke_out(void* handle, const char* name, xvalue* args, xvalue* byrefs);
xvalue* x_com_invoke_named(void* handle, const char* name, xvalue* args, xvalue* names);

#ifdef __cplusplus
}
#endif

#endif
