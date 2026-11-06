#ifndef XLANG_SERIAL_H
#define XLANG_SERIAL_H

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"

/* 回环有界队列保留短写语义；单次设备 I/O 不进行无界分配。 */
#define XSERIAL_LOOPBACK_CAPACITY 4096u
#define XSERIAL_IO_CHUNK 65536u

#ifdef __cplusplus
extern "C" {
#endif

/* name 是 UTF-8；失败返回 NULL，并通过 XRT 当前线程错误槽报告原因。 */
void* xserial_open(const char* name, int64_t baudrate,
    int64_t databits, int64_t stopbits, int64_t parity);
void xserial_close(void* handle);
bool xserial_is_open(void* handle);

/* 设备读写等待上限（毫秒）；0 请求不等待，Windows 写入取最短 1ms。 */
bool xserial_set_timeouts(void* handle, int64_t readMs, int64_t writeMs);

/* write 返回实际写入的字节数，允许短写；失败为 -1。 */
int64_t xserial_write(void* handle, const void* data, size_t size);
int64_t xserial_write_text(void* handle, const char* text);
int64_t xserial_write_text_len(void* handle, const char* text, size_t size);

/* xlang point 的精确 C 原型；只读借用 data，不接管或修改缓冲。 */
int64_t xserial_write_bytes(void* handle, void* data, size_t size);

/* 返回拥有型 xbuffer*，调用方使用 xrtBufferDestroy 释放。NULL 只表示失败。 */
void* xserial_read(void* handle, int64_t maxBytes);
int64_t xserial_available(void* handle);
bool xserial_flush(void* handle);
int64_t xserial_get_baudrate(void* handle);

/* port 借用到 close 为止；error 借用到当前线程的错误槽下次变更为止。 */
const char* xserial_get_port(void* handle);
const char* xserial_error(void);

#ifdef __cplusplus
}
#endif
#endif
