#ifndef XL_EXTLIB_LANGDETECT_NATIVE_H
#define XL_EXTLIB_LANGDETECT_NATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 返回借用的静态结果文字，不由调用方释放；输入使用真实字节长度。 */
const char *xlangdetect_detect(const char *text, size_t size);
const char *xlangdetect_code(const char *text, size_t size);
const char *xlangdetect_script(const char *text, size_t size);
int64_t xlangdetect_confidence(const char *text, size_t size);
bool xlangdetect_valid_utf8(const char *text, size_t size);
const char *xlangdetect_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
