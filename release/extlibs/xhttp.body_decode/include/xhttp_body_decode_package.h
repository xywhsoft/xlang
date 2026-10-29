#ifndef XLANG_EXTLIB_XHTTP_BODY_DECODE_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_BODY_DECODE_PACKAGE_H

/* 自动解码复用流式 Inflate，并直接接受拥有型 Headers。 */
#define XHTTP_MODULE_HTTP_BODY_ASYNC
#define XHTTP_MODULE_HTTP_BODY_DECODE
#define XHTTP_MODULE_HTTP_HEADERS

/* Native sources may share one compiler preprocessor. Recompute the
 * monotonic feature closure when another split HTTP package is activated. */
#undef XHTTP_FEATURES_H
#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/http_body_decode.h"
#include "xrt/http_headers.h"

#endif
