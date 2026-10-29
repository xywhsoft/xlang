#ifndef XLANG_EXTLIB_XHTTP_BODY_CODEC_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_BODY_CODEC_PACKAGE_H

/* 编解码正文始终保留异步来源的 AGAIN/Wait 转发能力。 */
#define XHTTP_MODULE_HTTP_BODY_ASYNC
#define XHTTP_MODULE_HTTP_BODY_DEFLATE
#define XHTTP_MODULE_HTTP_BODY_INFLATE

/* Native sources may share one compiler preprocessor. Recompute the
 * monotonic feature closure when another split HTTP package is activated. */
#undef XHTTP_FEATURES_H
#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/http_body_deflate.h"
#include "xrt/http_body_inflate.h"

#endif
