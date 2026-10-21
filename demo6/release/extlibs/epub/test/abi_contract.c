#include <stdint.h>
#include "../include/epub_native.h"

/* 构建字节数与 XLang int 的 C ABI 保持一致。 */
extern int64_t xep_build(const char *path, xvalue *meta, xvalue *chapters);
