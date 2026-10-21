#ifndef XL_CBOR_NATIVE_H
#define XL_CBOR_NATIVE_H

#include <stddef.h>
#include <stdbool.h>
typedef struct xvalue xvalue;
typedef struct xbuffer xbuffer;

#ifdef __cplusplus
extern "C" {
#endif

xbuffer* xcbor_encode(xvalue* value);
/* C 解码失败返回 NULL，合法 CBOR null 返回拥有型 XVALUE_NULL。 */
xvalue* xcbor_decode(const void* data, size_t size);
xbuffer* xcbor_encode_bytes(const void* data, size_t size);
xbuffer* xcbor_decode_bytes(const void* data, size_t size);
/* to_hex 返回调用方拥有的 XRT 字符串；其余 xbuffer* 用 xrtBufferDestroy。 */
char* xcbor_to_hex(const void* data, size_t size);
xbuffer* xcbor_from_hex(const char* text);
const char* xcbor_error_message(void);
/* XLang ABI 将 C 的 NULL 失败值变成空值哨兵，用线程错误槽区分。 */
bool xcbor_failed(void);

/* XLang point ABI 的薄入口。 */
void* xcbor_encode_abi(xvalue* value);
xvalue* xcbor_decode_abi(void* data, size_t size);
void* xcbor_encode_bytes_abi(void* data, size_t size);
void* xcbor_decode_bytes_abi(void* data, size_t size);
void* xcbor_to_hex_abi(void* data, size_t size);
void* xcbor_from_hex_abi(const char* text);

#ifdef __cplusplus
}
#endif
#endif
