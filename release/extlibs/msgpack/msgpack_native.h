#ifndef XL_MSGPACK_NATIVE_H
#define XL_MSGPACK_NATIVE_H

#include "xrt.h"

/* C 调用方获得强类型接口，返回的 xbuffer/xvalue 由调用方释放。 */
xbuffer* xmsgpack_encode(xvalue* value);
/* C 解码失败返回 NULL；合法 nil 返回拥有型 XVALUE_NULL。 */
xvalue* xmsgpack_decode(const void* data, size_t size);
xbuffer* xmsgpack_encode_bytes(const void* data, size_t size);
xbuffer* xmsgpack_decode_bytes(const void* data, size_t size);
str xmsgpack_to_hex(const void* data, size_t size);
xbuffer* xmsgpack_from_hex(cstr text);
cstr xmsgpack_error_message(void);
bool xmsgpack_has_error(void);

/* XLang point ABI 适配指针签名，并将 C 的 NULL 失败值变为空值哨兵。 */
void* xmsgpack_encode_abi(xvalue* value);
xvalue* xmsgpack_decode_abi(void* data, size_t size);
void* xmsgpack_encode_bytes_abi(void* data, size_t size);
void* xmsgpack_decode_bytes_abi(void* data, size_t size);
void* xmsgpack_to_hex_abi(void* data, size_t size);
void* xmsgpack_from_hex_abi(cstr text);

#endif
