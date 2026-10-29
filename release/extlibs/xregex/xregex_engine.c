#include "include/xregex.h"

/* 完整宿主已提供同一正则体系；裁剪运行库时由包提供唯一的执行内核。 */
#if !defined(XRT_FEATURE_REGEX_CORE) && !defined(XLANG_XREGEX_DECLARED_BY_SDK)
#include "src/third_party/bbre/bbre.c"
#endif
