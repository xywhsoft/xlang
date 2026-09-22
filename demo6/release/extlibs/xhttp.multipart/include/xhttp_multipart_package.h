#ifndef XLANG_EXTLIB_XHTTP_MULTIPART_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_MULTIPART_PACKAGE_H

/* xhttp.multipart 同时启用整包、分段写出、流读取和安全随机边界。 */
#define XHTTP_MODULE_MULTIPART_WRITE
#define XHTTP_MODULE_MULTIPART_STREAM
#define XHTTP_MODULE_MULTIPART_RANDOM
#define XHTTP_MODULE_HTTP_HEADERS

#ifndef XRT_MODULE_HTTP
	#define XRT_MODULE_HTTP
#endif
#ifndef XRT_MODULE_HTTP_PARAM
	#define XRT_MODULE_HTTP_PARAM
#endif
#ifndef XRT_MODULE_CODEC_PERCENT
	#define XRT_MODULE_CODEC_PERCENT
#endif
#ifndef XRT_MODULE_UNICODE
	#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
	#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif

#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/http_headers.h"
#include "xrt/mime.h"
#include "xrt/http_ext_value.h"
#include "xrt/http_content_disposition.h"
#include "xrt/multipart.h"

#endif
