#ifndef XLANG_EXTLIB_XHTTP_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_PACKAGE_H

/* xhttp 基础包只启用 URL、Query 与 form-urlencoded 参数体系。 */
#define XHTTP_MODULE_URL
#define XHTTP_MODULE_QUERY_CODEC
#define XHTTP_MODULE_QUERY_PARAMS

#ifndef XRT_MODULE_CODEC_PERCENT
	#define XRT_MODULE_CODEC_PERCENT
#endif
#ifndef XRT_MODULE_HTTP_HOST
	#define XRT_MODULE_HTTP_HOST
#endif
#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif

#include "xhttp/features.h"
#include <xrt.h>

#include "xrt/form.h"
#include "xrt/query.h"
#include "xrt/query_params.h"
#include "xrt/url.h"

#endif
