#ifndef XSHM_NATIVE_H
#define XSHM_NATIVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* xshm_create(const char* Name, int64_t Size);
void* xshm_open(const char* Name);
void* xshm_adopt(void* Shared);
void xshm_close(void* Shared);
/* POSIX 可显式删除名称；Windows 名称随最后一个句柄关闭而消失。 */
int xshm_unlink(const char* Name);
/* 空句柄返回 0/空串并设置 XERR_ARGUMENT；正常结果无遗留错误。 */
int64_t xshm_size(void* Shared);
const char* xshm_name_of(void* Shared);
int xshm_write(void* Shared, int64_t Offset, const void* Data, size_t Length);
/* XLang point extern 不区分 const；公开 C API 仍保持只读参数。 */
int xshm_write_abi(void* Shared, int64_t Offset, void* Data, size_t Length);
/* 返回 xbuffer* 的独立副本，调用方用 xrtBufferDestroy 释放。 */
void* xshm_read(void* Shared, int64_t Offset, size_t Length);
/* 返回独立 UTF-8 副本；调用方用 xrtFree 释放，长度由调用方保留。 */
void* xshm_read_text(void* Shared, int64_t Offset, size_t Length);
int xshm_write_int64(void* Shared, int64_t Offset, int64_t Value);
int xshm_read_int64(void* Shared, int64_t Offset, int64_t* Value);
int64_t xshm_read_int64_value(void* Shared, int64_t Offset);
int xshm_zero(void* Shared);

#ifdef __cplusplus
}
#endif
#endif
