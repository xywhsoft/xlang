#ifndef XLANG_EXTLIB_XHTTP_BODY_ASYNC_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_BODY_ASYNC_PACKAGE_H

/* xhttp.body.async 只增加 Reader 可读性 Future。 */
#define XHTTP_MODULE_HTTP_BODY_ASYNC

/* Native sources may share one compiler preprocessor. Recompute the
 * monotonic feature closure when another split HTTP package is activated. */
#undef XHTTP_FEATURES_H
#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/http_body.h"

#endif
