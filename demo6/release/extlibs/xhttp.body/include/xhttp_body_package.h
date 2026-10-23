#ifndef XLANG_EXTLIB_XHTTP_BODY_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_BODY_PACKAGE_H

/* xhttp.body 只启用正文源、Reader 与正文组合能力。 */
#define XHTTP_MODULE_HTTP_BODY_COMPOSE

#ifndef XRT_MODULE_ATOMIC
	#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif

/* Native sources may share one compiler preprocessor. Recompute the
 * monotonic feature closure when another split HTTP package is activated. */
#undef XHTTP_FEATURES_H
#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/http_body.h"
#include "xrt/http_body_compose.h"

#endif
