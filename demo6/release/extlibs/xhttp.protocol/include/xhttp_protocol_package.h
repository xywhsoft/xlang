#ifndef XLANG_EXTLIB_XHTTP_PROTOCOL_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_PROTOCOL_PACKAGE_H

/* xhttp.protocol 只启用报文字段、Cookie 和媒体类型协议对象。 */
#define XHTTP_MODULE_HTTP_HEADERS
#define XHTTP_MODULE_SET_COOKIE
#define XHTTP_MODULE_MIME
#define XHTTP_MODULE_MIME_TYPES

#ifndef XRT_MODULE_HTTP
	#define XRT_MODULE_HTTP
#endif
#ifndef XRT_MODULE_HTTP_PARAM
	#define XRT_MODULE_HTTP_PARAM
#endif
#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_TIME_TEXT
	#define XRT_MODULE_TIME_TEXT
#endif

#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/cookie.h"
#include "xrt/http_headers.h"
#include "xrt/mime.h"

#endif
