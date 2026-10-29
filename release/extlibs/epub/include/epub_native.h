#ifndef XEP_NATIVE_H
#define XEP_NATIVE_H

#include <stdint.h>
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 成功返回文件字节数；失败返回 0，错误保存在 XRT。 */
int64_t xep_build(const char *path, xvalue *meta, xvalue *chapters);

/* 读取本库生成的 stored 文本条目；不存在返回拥有型空串，失败返回 NULL。 */
void *xep_read_entry(const char *path, const char *name);

#ifdef __cplusplus
}
#endif

#endif
