#ifndef XLANG_EXTLIB_DNS_NATIVE_H
#define XLANG_EXTLIB_DNS_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

/* 名称映射返回静态借用文本，不得释放。 */
const char* xdns_type_name(int64_t type);
const char* xdns_class_name(int64_t cls);
const char* xdns_rcode_name(int64_t rcode);
const char* xdns_opcode_name(int64_t opcode);

/* 编码结果由 XRT 分配，调用方使用 xrtFree 释放。 */
char* xdns_encode_name(const char* domain);
int64_t xdns_valid_name(const char* name);
int64_t xdns_is_ipv4(const char* address);

/* 解析结果由调用方使用 xrtValueRelease 释放。 */
xvalue* xdns_resolve(const char* host, int64_t family);

#endif
