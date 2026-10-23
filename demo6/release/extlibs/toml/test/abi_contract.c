/* 和 XLang cgen 的 extern 原型逐项核对，尤其防止 LP64 下 uint64/ULL 混用。 */
#define XRT_MODULE_ALL
#include "xrt.h"

extern int64_t xtoml_error_code(void);
extern int64_t xtoml_error_line(void);
extern int64_t xtoml_error_column(void);
extern int64_t xtoml_error_offset(void);
extern xvalue* xtoml_parse(const char*, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long,
    unsigned long long, unsigned int);
extern xvalue* xtoml_load(const char*, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long, unsigned int);
extern void* xtoml_stringify(xvalue*, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned int);
extern void* xtoml_stringify_text(xvalue*, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned int);
extern bool xtoml_save(const char*, xvalue*, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long, unsigned int);
extern int64_t xtoml_datetime_kind(xvalue*);
extern int64_t xtoml_datetime_part(xvalue*, int);

#include "../toml_native.c"
