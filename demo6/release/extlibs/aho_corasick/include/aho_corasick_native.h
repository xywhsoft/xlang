#ifndef AHO_CORASICK_NATIVE_H
#define AHO_CORASICK_NATIVE_H

#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#include "xrt_aho_corasick.h"

#if defined(_WIN32)
#define XAHO_EXPORT __declspec(dllexport)
#else
#define XAHO_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 拥有型句柄只交给一个析构者；返回 Buffer 由调用方释放。 */
XAHO_EXPORT const char* xac_last_error(void);
XAHO_EXPORT void* xac_create(int ignoreCase, uint64 maxPatterns,
    uint64 maxPatternBytes, uint64 maxTotalPatternBytes, uint64 maxNodes,
    uint64 maxMatches, uint64 maxInputBytes, uint64 maxOutputBytes);
XAHO_EXPORT void xac_destroy(void* handle);
XAHO_EXPORT int xac_add_pattern(void* handle, const char* text, size_t size);
XAHO_EXPORT int xac_build(void* handle);
XAHO_EXPORT int xac_is_built(void* handle);
XAHO_EXPORT int64 xac_pattern_count(void* handle);
XAHO_EXPORT void* xac_pattern(void* handle, int64 index);
XAHO_EXPORT void* xac_search(void* handle, const char* text, size_t size);
XAHO_EXPORT void* xac_search_non_overlapping(void* handle, const char* text, size_t size);
XAHO_EXPORT void* xac_find(void* handle, const char* text, size_t size);
XAHO_EXPORT int64 xac_result_count(void* result);
XAHO_EXPORT void* xac_result_pattern(void* result, int64 index);
XAHO_EXPORT int64 xac_result_start(void* result, int64 index);
XAHO_EXPORT int64 xac_result_end(void* result, int64 index);
XAHO_EXPORT void xac_result_destroy(void* result);
XAHO_EXPORT int xac_contains(void* handle, const char* text, size_t size);
XAHO_EXPORT int64 xac_count(void* handle, const char* text, size_t size);

/* 自定义替换器直接追加字节切片，统一执行输出预算，不创建中间字符串。 */
XAHO_EXPORT int xac_output_append(void* handle, void* output, const char* text,
    size_t size, size_t start, size_t count);
/* 仅在原缓冲可容纳终止字节且内容无 NUL 时移交 Data；否则保持缓冲不变。 */
XAHO_EXPORT void* xac_buffer_take_cstr(void* output);
XAHO_EXPORT void* xac_replace(void* handle, const char* text, size_t size,
    const char* replacement, size_t replacementSize);
XAHO_EXPORT void* xac_mask(void* handle, const char* text, size_t size,
    const char* mask, size_t maskSize);
XAHO_EXPORT void* xac_highlight(void* handle, const char* text, size_t size,
    const char* open, size_t openSize, const char* close, size_t closeSize);

/* scan 借用整个输入；stream 拥有当前分块。next 成功后才有可读匹配。 */
XAHO_EXPORT void* xac_scan_begin(void* handle, const char* text, size_t size);
XAHO_EXPORT int xac_scan_next(void* scan);
XAHO_EXPORT int64 xac_scan_start(void* scan);
XAHO_EXPORT int64 xac_scan_end_pos(void* scan);
XAHO_EXPORT void* xac_scan_pattern(void* scan);
XAHO_EXPORT void xac_scan_end(void* scan);
XAHO_EXPORT void* xac_stream_create(void* handle);
XAHO_EXPORT int xac_stream_feed(void* stream, const char* text, size_t size);
XAHO_EXPORT int xac_stream_next(void* stream);
XAHO_EXPORT int xac_stream_enter(void* stream);
XAHO_EXPORT void xac_stream_leave(void* stream);
XAHO_EXPORT int xac_stream_pending(void* stream);
XAHO_EXPORT int64 xac_stream_offset(void* stream);
XAHO_EXPORT int64 xac_stream_matches(void* stream);
XAHO_EXPORT int64 xac_stream_start(void* stream);
XAHO_EXPORT int64 xac_stream_end_pos(void* stream);
XAHO_EXPORT void* xac_stream_pattern(void* stream);
XAHO_EXPORT void xac_stream_reset(void* stream);
XAHO_EXPORT void xac_stream_destroy(void* stream);

#ifdef __cplusplus
}
#endif
#endif
