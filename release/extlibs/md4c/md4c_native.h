#ifndef XL_MD4C_NATIVE_H
#define XL_MD4C_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* HTML 结果由调用方用 xrtFree 释放；last_error 借用当前线程的 XRT 错误信息。 */
int xmd4c_available(void);
const char* xmd4c_version(void);
const char* xmd4c_last_error(void);
char* xmd4c_html(const char* markdown);
char* xmd4c_html_flags(const char* markdown, int parser_flags, int renderer_flags);
char* xmd4c_file_html(const char* path, int parser_flags, int renderer_flags);
int xmd4c_commonmark_flags(void);
int xmd4c_github_flags(void);
int xmd4c_xhtml_flag(void);
int xmd4c_skip_utf8_bom_flag(void);
int xmd4c_verbatim_entities_flag(void);
int xmd4c_no_html_flag(void);

#ifdef __cplusplus
}
#endif
#endif
