#ifndef XLANG_EXTLIB_XHTTP_BODY_STREAM_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_BODY_STREAM_PACKAGE_H

/* xhttp.body_stream 启用有界生产流及其异步正文依赖。 */
#define XHTTP_MODULE_HTTP_BODY_STREAM
#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif

#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/http_body.h"
#include "xrt/http_body_stream.h"

#endif
