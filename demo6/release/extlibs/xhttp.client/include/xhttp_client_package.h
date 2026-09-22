#ifndef XLANG_EXTLIB_XHTTP_CLIENT_PACKAGE_H
#define XLANG_EXTLIB_XHTTP_CLIENT_PACKAGE_H

/* 客户端请求层启用拥有型 Request 及其非认证数据构建能力。 */
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_TRAILERS
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_TE
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_QUERY
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_FORM
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_FORM_DATA
#define XHTTP_MODULE_HTTP_EXCHANGE
#define XHTTP_MODULE_HTTP_CLIENT_FUTURE
#define XHTTP_MODULE_HTTP_CLIENT_HTTPS

#ifndef XRT_MODULE_HTTP_CONNECTION
	#define XRT_MODULE_HTTP_CONNECTION
#endif
#ifndef XRT_MODULE_HTTP_TE
	#define XRT_MODULE_HTTP_TE
#endif
#ifndef XRT_MODULE_HTTP_EXPECT
	#define XRT_MODULE_HTTP_EXPECT
#endif
#ifndef XRT_MODULE_HTTP_HOST
	#define XRT_MODULE_HTTP_HOST
#endif
#ifndef XRT_MODULE_HTTP_TARGET
	#define XRT_MODULE_HTTP_TARGET
#endif
#ifndef XRT_MODULE_HTTP_TRAILER
	#define XRT_MODULE_HTTP_TRAILER
#endif
#ifndef XRT_MODULE_HTTP1_BODY
	#define XRT_MODULE_HTTP1_BODY
#endif
#ifndef XRT_MODULE_CANCEL
	#define XRT_MODULE_CANCEL
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
	#define XRT_MODULE_FUTURE_BRIDGE
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
	#define XRT_MODULE_NET_TCP_DIAL
#endif
#ifndef XRT_MODULE_SPIN
	#define XRT_MODULE_SPIN
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL
	#define XRT_MODULE_TLS_STREAM_DIAL
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
	#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_X509_STORE_SYSTEM
	#define XRT_MODULE_X509_STORE_SYSTEM
#endif
#ifndef XRT_MODULE_IO_BUFFER
	#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_VALUE
	#define XRT_MODULE_VALUE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
	#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif

#include "xhttp/features.h"
#include <xrt.h>
#include <xruntime.h>
#include <xl_extlib_abi.h>

#include "../../xhttp/include/xhttp_package.h"
#include "../../xhttp.protocol/include/xhttp_protocol_package.h"
#include "../../xhttp.body/include/xhttp_body_package.h"
#include "../../xhttp.multipart/include/xhttp_multipart_package.h"
#include "../../xhttp.form_data/include/xhttp_form_data_package.h"

#include "xrt/http_client.h"
#include "xrt/http_exchange.h"
#include "xrt/http_client_stream.h"
#include "xrt/http_client_runtime.h"
#include "xrt/http_client_future.h"

#endif
