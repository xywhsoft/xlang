#ifndef XRT_INTERNAL_FORM_H
#define XRT_INTERNAL_FORM_H

#include "xrt_codec.h"
#include "xrt_query.h"

#include "../../include/xrt/form.h"



#if defined(XHTTP_FEATURE_FORM_URLENCODED)

/* 初始化供 form-urlencoded 编码器共享的 ASCII 安全字符位图。 */
bool __xrtFormSafeMap(xpercentmap* pSafe);

#endif

#endif
