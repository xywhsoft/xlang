#include "../internal/xrt_http_client.h"


#include <stdio.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)

/* 准备过程中的临时状态只借用请求，成功计划会复制全部线路元数据。 */
typedef struct xrt_http1_request_prepare {
	const xhttprequest* Request;
	const xurl* Url;
	xstrview Method;
	xhttpbody* Body;
	uint64 BodyLength;
	xhttprequestbodymode BodyMode;
	str Target;
	size_t TargetSize;
	str HostHeader;
	size_t HostHeaderSize;
	const xhttpfield* Trailers;
	size_t TrailerCount;
	str TrailerNames;
	size_t TrailerNamesSize;
	const xhttpfield* ExtraField;
	xhttpfield* Fields;
	size_t FieldCount;
	char LengthText[21];
	uint16 Port;
	bool Close;
	bool ExpectContinue;
	bool TrailerDeclared;
	bool ConnectionTe;
} xrt_http1_request_prepare;



/* HTTP/1 请求计划使用一块尾随存储持有 Head、target 与端点 host。 */
struct xhttp1requestplan {
	bytes Head;
	size_t HeadSize;
	bytes End;
	size_t EndSize;
	str Method;
	size_t MethodSize;
	str Url;
	size_t UrlSize;
	str Target;
	size_t TargetSize;
	str Host;
	size_t HostSize;
	xhttpbody* Body;
	uint64 BodyLength;
	uint16 Port;
	xhttprequestbodymode BodyMode;
	bool Secure;
	bool Close;
	bool ExpectContinue;
};



/* 发布请求准备阶段的值错误。 */
static bool __xrtHttp1RequestPrepareError(
	xhttprequesterror Code,
	cstr sMessage
)
{
	__xrtHttpRequestError(
		Code, "prepare-http1-request", sMessage
	);
	return false;
}



/* 把协议端口转换为客户端可执行的网络端口。 */
static bool __xrtHttp1RequestPort(
	const xurl* pUrl,
	uint16* pPort
)
{
	if ( !xrtUrlPort(pUrl, pPort) ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_TARGET,
			"prepare-http1-request",
			"HTTP client URL has no usable network port"
		);
		return false;
	}
	if ( *pPort == 0 ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TARGET,
			"HTTP client URL port must not be zero"
		);
	}
	return true;
}



/* 规范生成时把显式空端口折叠为省略端口。 */
static void __xrtHttp1RequestNormalizeEmptyPort(
	const xurl* pSource,
	xurl* pTarget
)
{
	*pTarget = *pSource;
	if ( (pTarget->Flags & XURL_PORT_EMPTY) != 0 ) {
		pTarget->Flags &= ~(
			(uint32)XURL_HAS_PORT |
			(uint32)XURL_PORT_EMPTY |
			(uint32)XURL_PORT_VALUE
		);
		pTarget->Port = 0;
		pTarget->PortText = (xstrview){ NULL, 0 };
	}
}



/* 复制一段非空 target 文本并附加调试用零字符。 */
static str __xrtHttp1RequestTargetCopy(
	xstrview Target,
	size_t* pSize
)
{
	str sTarget;

	if ( (pSize == NULL) ||
		!xrtHttp1TargetValid(Target) ||
		(Target.Size == SIZE_MAX) ) {
		(void)__xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TARGET,
			"HTTP/1 request-target is invalid"
		);
		return NULL;
	}
	sTarget = (str)xrtMalloc(Target.Size + 1u);
	if ( sTarget == NULL ) {
		return NULL;
	}
	memcpy(sTarget, Target.Data, Target.Size);
	sTarget[Target.Size] = '\0';
	*pSize = Target.Size;
	return sTarget;
}



/* 调用 URL 写出器生成拥有型 target。 */
static str __xrtHttp1RequestTargetWrite(
	const xurl* pUrl,
	bool (*pWrite)(
		const xurl*,
		void*,
		size_t,
		size_t*
	),
	size_t* pSize
)
{
	str sTarget;
	size_t iSize;

	if ( !pWrite(pUrl, NULL, 0, &iSize) ||
		(iSize == 0) || (iSize == SIZE_MAX) ) {
		(void)__xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TARGET,
			"HTTP/1 request-target could not be generated"
		);
		return NULL;
	}
	sTarget = (str)xrtMalloc(iSize + 1u);
	if ( sTarget == NULL ) {
		return NULL;
	}
	if ( !pWrite(pUrl, sTarget, iSize, &iSize) ) {
		xrtFree(sTarget);
		return NULL;
	}
	sTarget[iSize] = '\0';
	*pSize = iSize;
	return sTarget;
}



/* 生成 CONNECT authority-form；URL 没有显式端口时补入协议默认端口。 */
static str __xrtHttp1RequestAuthorityTarget(
	const xurl* pUrl,
	uint16 iPort,
	size_t* pSize
)
{
	xurl Url;
	str sTarget;
	size_t iHostSize;
	size_t iPortSize = 0;
	char sPort[6];
	int iWritten;

	__xrtHttp1RequestNormalizeEmptyPort(pUrl, &Url);
	if ( !xrtUrlHostWrite(&Url, NULL, 0, &iHostSize) ||
		(iHostSize == 0) ) {
		(void)__xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TARGET,
			"HTTP/1 authority target has no host"
		);
		return NULL;
	}
	if ( (Url.Flags & XURL_HAS_PORT) == 0 ) {
		iWritten = snprintf(
			sPort, sizeof(sPort), "%u",
			(unsigned int)iPort
		);
		if ( (iWritten <= 0) ||
			((size_t)iWritten >= sizeof(sPort)) ) {
			(void)__xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TARGET,
				"HTTP/1 authority target has no valid port"
			);
			return NULL;
		}
		iPortSize = (size_t)iWritten + 1u;
	}
	if ( (iHostSize > (SIZE_MAX - iPortSize)) ||
		((iHostSize + iPortSize) == SIZE_MAX) ) {
		__xrtHttpRequestSizeOverflow();
		return NULL;
	}
	sTarget = (str)xrtMalloc(iHostSize + iPortSize + 1u);
	if ( sTarget == NULL ) {
		return NULL;
	}
	if ( !xrtUrlHostWrite(
		&Url, sTarget, iHostSize, &iHostSize
	) ) {
		xrtFree(sTarget);
		return NULL;
	}
	if ( iPortSize != 0 ) {
		sTarget[iHostSize++] = ':';
		memcpy(
			sTarget + iHostSize,
			sPort,
			iPortSize - 1u
		);
		iHostSize += iPortSize - 1u;
	}
	sTarget[iHostSize] = '\0';
	*pSize = iHostSize;
	return sTarget;
}



/* 使用公共协议解析器校验方法与生成目标形式的组合。 */
static bool __xrtHttp1RequestTargetValid(
	const xrt_http1_request_prepare* pPrepare,
	xhttp1targetform Form
)
{
	xhttptarget Target;
	xhttptargetform Expected;

	if ( !xrtHttpTargetParse(
		pPrepare->Method,
		(xstrview){
			pPrepare->Target,
			pPrepare->TargetSize
		},
		&Target
	) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TARGET,
			"HTTP/1 request method and target form do not match"
		);
	}
	if ( Form == XHTTP1_TARGET_CUSTOM ) {
		return true;
	}
	switch ( Form ) {
		case XHTTP1_TARGET_ORIGIN:
			Expected = XHTTP_TARGET_ORIGIN;
			break;

		case XHTTP1_TARGET_ABSOLUTE:
			Expected = XHTTP_TARGET_ABSOLUTE;
			break;

		case XHTTP1_TARGET_AUTHORITY:
			Expected = XHTTP_TARGET_AUTHORITY;
			break;

		case XHTTP1_TARGET_ASTERISK:
			Expected = XHTTP_TARGET_ASTERISK;
			break;

		default:
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TARGET,
				"HTTP/1 resolved request-target form is invalid"
			);
	}
	if ( Target.Form == Expected ) {
		return true;
	}
	return __xrtHttp1RequestPrepareError(
		XHTTP_REQUEST_ERROR_TARGET,
		"HTTP/1 generated request-target has the wrong form"
	);
}



/* 按选项生成唯一 request-target，fragment 永远不进入线路。 */
static bool __xrtHttp1RequestTarget(
	xrt_http1_request_prepare* pPrepare,
	const xhttp1requestoptions* pOptions
)
{
	xurl Url;
	xhttp1targetform Form = pOptions->TargetForm;

	if ( (pOptions->TargetForm != XHTTP1_TARGET_CUSTOM) &&
		((pOptions->CustomTarget.Data != NULL) ||
		 (pOptions->CustomTarget.Size != 0)) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TARGET,
			"custom request-target requires custom target form"
		);
	}
	if ( Form == XHTTP1_TARGET_AUTO ) {
		Form = xrtHttpMethodEqual(
			pPrepare->Method,
			XRT_STR_LITERAL("CONNECT")
		) ? XHTTP1_TARGET_AUTHORITY : XHTTP1_TARGET_ORIGIN;
	}
	switch ( Form ) {
		case XHTTP1_TARGET_ORIGIN:
			pPrepare->Target = __xrtHttp1RequestTargetWrite(
				pPrepare->Url,
				xrtUrlTargetWrite,
				&pPrepare->TargetSize
			);
			break;

		case XHTTP1_TARGET_ABSOLUTE:
			__xrtHttp1RequestNormalizeEmptyPort(
				pPrepare->Url, &Url
			);
			Url.Flags &= ~(uint32)XURL_HAS_FRAGMENT;
			Url.Fragment = (xstrview){ NULL, 0 };
			pPrepare->Target = __xrtHttp1RequestTargetWrite(
				&Url, xrtUrlWrite, &pPrepare->TargetSize
			);
			break;

		case XHTTP1_TARGET_AUTHORITY:
			pPrepare->Target = __xrtHttp1RequestAuthorityTarget(
				pPrepare->Url,
				pPrepare->Port,
				&pPrepare->TargetSize
			);
			break;

		case XHTTP1_TARGET_ASTERISK:
			pPrepare->Target = __xrtHttp1RequestTargetCopy(
				XRT_STR_LITERAL("*"), &pPrepare->TargetSize
			);
			break;

		case XHTTP1_TARGET_CUSTOM:
			pPrepare->Target = __xrtHttp1RequestTargetCopy(
				pOptions->CustomTarget,
				&pPrepare->TargetSize
			);
			break;

		default:
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TARGET,
				"HTTP/1 request-target form is invalid"
			);
	}
	return (pPrepare->Target != NULL) &&
		__xrtHttp1RequestTargetValid(pPrepare, Form);
}



/* 扫描特殊字段并发布连接、分帧和 Expect 事实。 */
static bool __xrtHttp1RequestHeaders(
	xrt_http1_request_prepare* pPrepare,
	const xhttpfield** ppHost,
	const xhttpfield** ppLength,
	const xhttpfield** ppTransfer
)
{
	const xhttpfield* pHost = NULL;
	const xhttpfield* pLength = NULL;
	const xhttpfield* pTransfer = NULL;
	const xhttpfield* pFields =
		xrtHttpRequestHeaderData(pPrepare->Request);
	xhttpfieldtokencursor ConnectionCursor;
	xhttpexpectresult Expect;
	xhttpteinfo Te;
	xhttpnext Next;
	xstrview ConnectionOption;
	size_t iCount = xrtHttpRequestHeaderCount(pPrepare->Request);
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpRequestHeaderAt(pPrepare->Request, i);

		if ( pField == NULL ) {
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TARGET,
				"HTTP request Header storage is invalid"
			);
		}
		if ( xrtHttpFieldNameEqual(
			pField->Name, XRT_STR_LITERAL("Host")
		) ) {
			if ( pHost != NULL ) {
				return __xrtHttp1RequestPrepareError(
					XHTTP_REQUEST_ERROR_HOST_HEADER,
					"HTTP/1 request contains multiple Host fields"
				);
			}
			pHost = pField;
		} else if ( xrtHttpFieldNameEqual(
			pField->Name, XRT_STR_LITERAL("Content-Length")
		) ) {
			if ( pLength != NULL ) {
				return __xrtHttp1RequestPrepareError(
					XHTTP_REQUEST_ERROR_CONTENT_LENGTH,
					"HTTP/1 request contains multiple Content-Length fields"
				);
			}
			pLength = pField;
		} else if ( xrtHttpFieldNameEqual(
			pField->Name, XRT_STR_LITERAL("Transfer-Encoding")
		) ) {
			if ( pTransfer != NULL ) {
				return __xrtHttp1RequestPrepareError(
					XHTTP_REQUEST_ERROR_TRANSFER_ENCODING,
					"HTTP/1 request contains multiple Transfer-Encoding fields"
				);
			}
			pTransfer = pField;
		} else if ( xrtHttpFieldNameEqual(
			pField->Name, XRT_STR_LITERAL("Trailer")
		) ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
				pPrepare->TrailerDeclared = true;
			#else
				return __xrtHttp1RequestPrepareError(
					XHTTP_REQUEST_ERROR_TRAILER,
					"HTTP request Trailer support is not enabled"
				);
			#endif
		}
	}
	xrtHttpConnectionCursorInit(&ConnectionCursor);
	for ( ;; ) {
		Next = xrtHttpConnectionNext(
			pFields,
			iCount,
			&ConnectionCursor,
			&ConnectionOption
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtHttpRequestWrapError(
				XERR_PROTOCOL,
				XHTTP_REQUEST_ERROR_CONNECTION,
				"prepare-http1-request",
				"HTTP/1 Connection field is invalid"
			);
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( xrtHttpTokenEqual(
			ConnectionOption, XRT_STR_LITERAL("close")
		) ) {
			pPrepare->Close = true;
		} else if ( xrtHttpTokenEqual(
			ConnectionOption, XRT_STR_LITERAL("TE")
		) ) {
			pPrepare->ConnectionTe = true;
		}
	}
	if ( (pHost != NULL) &&
		!xrtHttpHostValid(pHost->Value) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_HOST_HEADER,
			"HTTP/1 Host field must contain one valid authority"
		);
	}
	if ( !xrtHttpTeParse(pFields, iCount, &Te) ) {
		__xrtHttpRequestWrapError(
			XERR_PROTOCOL,
			XHTTP_REQUEST_ERROR_TE,
			"prepare-http1-request",
			"HTTP/1 request contains an invalid TE field"
		);
		return false;
	}
	if ( ((Te.Flags & XHTTP_TE_PRESENT) != 0) &&
		!pPrepare->ConnectionTe ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TE,
			"HTTP/1 TE field requires a TE connection option"
		);
	}
	Expect = xrtHttpExpectFields(pFields, iCount);
	if ( Expect == XHTTP_EXPECT_ERROR ) {
		__xrtHttpRequestWrapError(
			XERR_PROTOCOL,
			XHTTP_REQUEST_ERROR_EXPECT,
			"prepare-http1-request",
			"HTTP/1 request contains an invalid Expect field"
		);
		return false;
	}
	if ( Expect == XHTTP_EXPECT_UNSUPPORTED ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_EXPECT,
			"HTTP/1 client cannot execute an extension expectation"
		);
	}
	pPrepare->ExpectContinue =
		Expect == XHTTP_EXPECT_CONTINUE;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( pPrepare->TrailerDeclared &&
			(pPrepare->TrailerCount == 0) ) {
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TRAILER,
				"HTTP/1 request declares Trailer without actual Trailer fields"
			);
		}
	#endif
	*ppHost = pHost;
	*ppLength = pLength;
	*ppTransfer = pTransfer;
	return true;
}



/* 验证 absolute-form 的显式 Host 与目标 URL 有相同有效 authority。 */
static bool __xrtHttp1RequestAbsoluteHost(
	const xrt_http1_request_prepare* pPrepare,
	const xhttp1requestoptions* pOptions,
	const xhttpfield* pHost
)
{
	xhttpauthority Host;
	uint16 iHostPort;

	if ( (pOptions->TargetForm != XHTTP1_TARGET_ABSOLUTE) ||
		(pHost == NULL) ) {
		return true;
	}
	if ( !xrtHttpHostParse(pHost->Value, &Host) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_HOST_HEADER,
			"HTTP/1 absolute-form Host is invalid"
		);
	}
	if ( ((Host.Flags & XHTTP_AUTHORITY_HAS_PORT) != 0) &&
		((Host.Flags & XHTTP_AUTHORITY_PORT_EMPTY) == 0) ) {
		if ( (Host.Flags & XHTTP_AUTHORITY_PORT_VALUE) == 0 ) {
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_HOST_HEADER,
				"HTTP/1 absolute-form Host port is outside network range"
			);
		}
		iHostPort = Host.Port;
	} else {
		iHostPort = xrtUrlDefaultPort(pPrepare->Url->Scheme);
	}
	if ( xrtHttpHostEqual(
		Host.Host, pPrepare->Url->Host
	) && (iHostPort == pPrepare->Port) ) {
		return true;
	}
	return __xrtHttp1RequestPrepareError(
		XHTTP_REQUEST_ERROR_HOST_HEADER,
		"HTTP/1 absolute-form Host must match the target authority"
	);
}



/* 将正文工厂和用户分帧字段收敛为唯一发送模式。 */
static bool __xrtHttp1RequestFraming(
	xrt_http1_request_prepare* pPrepare,
	const xhttpfield* pLength,
	const xhttpfield* pTransfer
)
{
	uint64 iDeclared = 0;

	pPrepare->BodyLength = pPrepare->Body != NULL ?
		xrtHttpBodyLength(pPrepare->Body) : 0;
	if ( (pLength != NULL) && (pTransfer != NULL) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TRANSFER_ENCODING,
			"HTTP/1 request cannot combine Transfer-Encoding and Content-Length"
		);
	}
	if ( pPrepare->TrailerCount != 0 ) {
		if ( pLength != NULL ) {
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TRAILER,
				"HTTP/1 request Trailer fields require chunked framing"
			);
		}
		if ( pTransfer != NULL ) {
			xstrview Transfer = xrtHttpOwsTrim(
				pTransfer->Value
			);

			if ( !xrtHttpTokenEqual(
				Transfer, XRT_STR_LITERAL("chunked")
			) ) {
				return __xrtHttp1RequestPrepareError(
					XHTTP_REQUEST_ERROR_TRANSFER_ENCODING,
					"HTTP/1 request Trailer fields require final chunked transfer coding"
				);
			}
		}
		pPrepare->BodyMode = XHTTP_REQUEST_BODY_CHUNKED;
	} else if ( pLength != NULL ) {
		if ( !xrtHttpContentLengthParse(
			pLength->Value, &iDeclared
		) || (pPrepare->BodyLength == XHTTP_BODY_UNKNOWN) ||
			(iDeclared != pPrepare->BodyLength) ) {
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_CONTENT_LENGTH,
				"HTTP/1 Content-Length does not match the request body"
			);
		}
		pPrepare->BodyMode = XHTTP_REQUEST_BODY_FIXED;
	} else if ( pTransfer != NULL ) {
		xstrview Transfer = xrtHttpOwsTrim(pTransfer->Value);

		if ( !xrtHttpTokenEqual(
			Transfer, XRT_STR_LITERAL("chunked")
		) ) {
			return __xrtHttp1RequestPrepareError(
				XHTTP_REQUEST_ERROR_TRANSFER_ENCODING,
				"HTTP/1 client only sends Transfer-Encoding: chunked"
			);
		}
		pPrepare->BodyMode = XHTTP_REQUEST_BODY_CHUNKED;
	} else if ( pPrepare->Body != NULL ) {
		pPrepare->BodyMode =
			(pPrepare->BodyLength == XHTTP_BODY_UNKNOWN) ?
			XHTTP_REQUEST_BODY_CHUNKED :
			XHTTP_REQUEST_BODY_FIXED;
	} else {
		pPrepare->BodyMode = XHTTP_REQUEST_BODY_NONE;
	}
	if ( (pPrepare->Method.Size == 5) &&
		(memcmp(pPrepare->Method.Data, "TRACE", 5) == 0) &&
		((pPrepare->Body != NULL) ||
		 (pPrepare->BodyMode == XHTTP_REQUEST_BODY_CHUNKED) ||
		 ((pPrepare->BodyMode == XHTTP_REQUEST_BODY_FIXED) &&
		  (pPrepare->BodyLength != 0))) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_TRACE_BODY,
			"TRACE request cannot contain a body"
		);
	}
	if ( pPrepare->ExpectContinue &&
		((pPrepare->BodyMode == XHTTP_REQUEST_BODY_NONE) ||
		 ((pPrepare->BodyMode == XHTTP_REQUEST_BODY_FIXED) &&
		  (pPrepare->BodyLength == 0))) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_EXPECT,
			"Expect: 100-continue requires a non-empty or streaming body"
		);
	}
	return true;
}



/* 构建与实际 Trailer 字段完全一致的规范声明值。 */
static bool __xrtHttp1RequestTrailerNames(
	xrt_http1_request_prepare* pPrepare
)
{
	const xerror* pCause;
	xerrkind Kind;

	if ( pPrepare->TrailerCount == 0 ) {
		return true;
	}
	pPrepare->TrailerNames = xrtHttpTrailerNamesBuild(
		pPrepare->Trailers,
		pPrepare->TrailerCount,
		&pPrepare->TrailerNamesSize
	);
	if ( pPrepare->TrailerNames == NULL ) {
		pCause = xrtGetError();
		Kind = pCause != NULL ?
			xrtErrorKind(pCause) : XERR_INTERNAL;
		__xrtHttpRequestWrapError(
			Kind,
			XHTTP_REQUEST_ERROR_TRAILER,
			"prepare-http1-request",
			"HTTP request Trailer declaration could not be built"
		);
		return false;
	}
	return true;
}



/* 生成 URL 派生的 Host 字段值。 */
static bool __xrtHttp1RequestHostBuild(
	xrt_http1_request_prepare* pPrepare
)
{
	xurl Url;

	__xrtHttp1RequestNormalizeEmptyPort(
		pPrepare->Url, &Url
	);
	if ( !xrtUrlHostWrite(
		&Url,
		NULL,
		0,
		&pPrepare->HostHeaderSize
	) || (pPrepare->HostHeaderSize == 0) ||
		(pPrepare->HostHeaderSize == SIZE_MAX) ) {
		return __xrtHttp1RequestPrepareError(
			XHTTP_REQUEST_ERROR_HOST_HEADER,
			"HTTP/1 Host field could not be generated"
		);
	}
	pPrepare->HostHeader = (str)xrtMalloc(
		pPrepare->HostHeaderSize + 1u
	);
	if ( pPrepare->HostHeader == NULL ) {
		return false;
	}
	if ( !xrtUrlHostWrite(
		&Url,
		pPrepare->HostHeader,
		pPrepare->HostHeaderSize,
		&pPrepare->HostHeaderSize
	) ) {
		return false;
	}
	pPrepare->HostHeader[pPrepare->HostHeaderSize] = '\0';
	return true;
}



/* 创建包含生成字段和原始用户字段的借用描述符数组。 */
static bool __xrtHttp1RequestFields(
	xrt_http1_request_prepare* pPrepare,
	const xhttpfield* pHost,
	const xhttpfield* pLength,
	const xhttpfield* pTransfer
)
{
	static const xstrview HostName = XRT_STR_INIT("Host");
	static const xstrview LengthName = XRT_STR_INIT("Content-Length");
	static const xstrview TransferName = XRT_STR_INIT("Transfer-Encoding");
	static const xstrview Chunked = XRT_STR_INIT("chunked");
	static const xstrview TrailerName = XRT_STR_INIT("Trailer");
	size_t iUserCount = xrtHttpRequestHeaderCount(pPrepare->Request);
	size_t iGenerated = pHost == NULL ? 1u : 0u;
	size_t iSkipped = 0;
	size_t i = 0;
	size_t iIndex = 0;
	int iWritten;

	if ( (pLength == NULL) && (pTransfer == NULL) &&
		(pPrepare->BodyMode != XHTTP_REQUEST_BODY_NONE) ) {
		iGenerated++;
	}
	if ( pPrepare->TrailerCount != 0 ) {
		iGenerated++;
	}
	if ( pPrepare->ExtraField != NULL ) {
		iGenerated++;
	}
	for ( i = 0; i < iUserCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpRequestHeaderAt(pPrepare->Request, i);

		if ( (pPrepare->TrailerDeclared &&
			xrtHttpFieldNameEqual(
				pField->Name, TrailerName
			)) || ((pPrepare->ExtraField != NULL) &&
			xrtHttpFieldNameEqual(
				pField->Name,
				pPrepare->ExtraField->Name
			)) ) {
			iSkipped++;
		}
	}
	if ( (iSkipped > iUserCount) ||
		((iUserCount - iSkipped) >
		 (SIZE_MAX - iGenerated)) ) {
		__xrtHttpRequestSizeOverflow();
		return false;
	}
	pPrepare->FieldCount =
		iUserCount - iSkipped + iGenerated;
	if ( pPrepare->FieldCount > (SIZE_MAX / sizeof(xhttpfield)) ) {
		__xrtHttpRequestSizeOverflow();
		return false;
	}
	pPrepare->Fields = (xhttpfield*)xrtMalloc(
		pPrepare->FieldCount * sizeof(xhttpfield)
	);
	if ( pPrepare->Fields == NULL ) {
		return false;
	}
	if ( pHost == NULL ) {
		if ( !__xrtHttp1RequestHostBuild(pPrepare) ) {
			return false;
		}
		pPrepare->Fields[iIndex++] = (xhttpfield){
			HostName,
			{
				pPrepare->HostHeader,
				pPrepare->HostHeaderSize
			}
		};
	}
	if ( (pLength == NULL) && (pTransfer == NULL) &&
		(pPrepare->BodyMode == XHTTP_REQUEST_BODY_FIXED) ) {
		iWritten = snprintf(
			pPrepare->LengthText,
			sizeof(pPrepare->LengthText),
			"%llu",
			(unsigned long long)pPrepare->BodyLength
		);
		if ( (iWritten <= 0) ||
			((size_t)iWritten >= sizeof(pPrepare->LengthText)) ) {
			__xrtHttpRequestSizeOverflow();
			return false;
		}
		pPrepare->Fields[iIndex++] = (xhttpfield){
			LengthName,
			{ pPrepare->LengthText, (size_t)iWritten }
		};
	} else if ( (pLength == NULL) && (pTransfer == NULL) &&
		(pPrepare->BodyMode == XHTTP_REQUEST_BODY_CHUNKED) ) {
		pPrepare->Fields[iIndex++] = (xhttpfield){
			TransferName, Chunked
		};
	}
	if ( pPrepare->TrailerCount != 0 ) {
		pPrepare->Fields[iIndex++] = (xhttpfield){
			TrailerName,
			{
				pPrepare->TrailerNames,
				pPrepare->TrailerNamesSize
			}
		};
	}
	if ( pPrepare->ExtraField != NULL ) {
		pPrepare->Fields[iIndex++] = *pPrepare->ExtraField;
	}
	for ( i = 0; i < iUserCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpRequestHeaderAt(pPrepare->Request, i);

		if ( (pPrepare->TrailerDeclared &&
			xrtHttpFieldNameEqual(
				pField->Name, TrailerName
			)) || ((pPrepare->ExtraField != NULL) &&
			xrtHttpFieldNameEqual(
				pField->Name,
				pPrepare->ExtraField->Name
			)) ) {
			continue;
		}
		pPrepare->Fields[iIndex++] = *pField;
	}
	if ( iIndex != pPrepare->FieldCount ) {
		__xrtHttpRequestInternal();
		return false;
	}
	return true;
}



/* 安全累计计划尾随存储长度。 */
static bool __xrtHttp1RequestStorageAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xrtHttpRequestSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 分配最终计划并直接把 HTTP/1 Header 写入其尾随存储。 */
static xhttp1requestplan* __xrtHttp1RequestPlanCreate(
	xrt_http1_request_prepare* pPrepare
)
{
	xhttp1requestplan* pPlan;
	xhttpbody* pBody = NULL;
	size_t iHeadSize;
	size_t iEndSize = 0;
	size_t iStorage = 0;
	bytes pStorage;
	xstrview Url = xrtHttpRequestUrlText(pPrepare->Request);

	if ( !xrtHttp1RequestWrite(
		pPrepare->Method,
		(xstrview){
			pPrepare->Target,
			pPrepare->TargetSize
		},
		XHTTP_VERSION_1_1,
		pPrepare->Fields,
		pPrepare->FieldCount,
		NULL,
		0,
		&iHeadSize
	) || ((pPrepare->BodyMode ==
		XHTTP_REQUEST_BODY_CHUNKED) &&
		!xrtHttp1ChunkEndWrite(
			pPrepare->Trailers,
			pPrepare->TrailerCount,
			NULL,
			0,
			&iEndSize
		)) || !__xrtHttp1RequestStorageAdd(
		&iStorage, iHeadSize
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, iEndSize
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, pPrepare->Method.Size
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, 1u
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, Url.Size
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, 1u
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, pPrepare->TargetSize
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, 1u
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, pPrepare->Url->Host.Size
	) || !__xrtHttp1RequestStorageAdd(
		&iStorage, 1u
	) || (sizeof(*pPlan) > (SIZE_MAX - iStorage)) ) {
		return NULL;
	}
	if ( pPrepare->Body != NULL ) {
		pBody = xrtHttpBodyRef(pPrepare->Body);
		if ( pBody == NULL ) {
			return NULL;
		}
	}
	pPlan = (xhttp1requestplan*)xrtMalloc(
		sizeof(*pPlan) + iStorage
	);
	if ( pPlan == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	memset(pPlan, 0, sizeof(*pPlan));
	pStorage = (bytes)(pPlan + 1);
	pPlan->Head = pStorage;
	pPlan->HeadSize = iHeadSize;
	pPlan->End = pStorage + iHeadSize;
	pPlan->EndSize = iEndSize;
	pPlan->Method = (str)(pPlan->End + iEndSize);
	pPlan->MethodSize = pPrepare->Method.Size;
	pPlan->Url = pPlan->Method + pPlan->MethodSize + 1u;
	pPlan->UrlSize = Url.Size;
	pPlan->Target = pPlan->Url + pPlan->UrlSize + 1u;
	pPlan->TargetSize = pPrepare->TargetSize;
	pPlan->Host = pPlan->Target + pPlan->TargetSize + 1u;
	pPlan->HostSize = pPrepare->Url->Host.Size;
	pPlan->Body = pBody;
	pPlan->BodyLength = pPrepare->BodyLength;
	pPlan->Port = pPrepare->Port;
	pPlan->BodyMode = pPrepare->BodyMode;
	pPlan->Secure = xrtUrlSecure(pPrepare->Url);
	pPlan->Close = pPrepare->Close;
	pPlan->ExpectContinue = pPrepare->ExpectContinue;
	if ( !xrtHttp1RequestWrite(
		pPrepare->Method,
		(xstrview){
			pPrepare->Target,
			pPrepare->TargetSize
		},
		XHTTP_VERSION_1_1,
		pPrepare->Fields,
		pPrepare->FieldCount,
		pPlan->Head,
		pPlan->HeadSize,
		&pPlan->HeadSize
	) || ((pPrepare->BodyMode ==
		XHTTP_REQUEST_BODY_CHUNKED) &&
		!xrtHttp1ChunkEndWrite(
			pPrepare->Trailers,
			pPrepare->TrailerCount,
			pPlan->End,
			pPlan->EndSize,
			&pPlan->EndSize
		)) ) {
		xrtHttp1RequestPlanDestroy(pPlan);
		return NULL;
	}
	memcpy(
		pPlan->Method,
		pPrepare->Method.Data,
		pPrepare->Method.Size
	);
	pPlan->Method[pPlan->MethodSize] = '\0';
	memcpy(pPlan->Url, Url.Data, Url.Size);
	pPlan->Url[pPlan->UrlSize] = '\0';
	memcpy(
		pPlan->Target,
		pPrepare->Target,
		pPrepare->TargetSize
	);
	pPlan->Target[pPlan->TargetSize] = '\0';
	memcpy(
		pPlan->Host,
		pPrepare->Url->Host.Data,
		pPrepare->Url->Host.Size
	);
	pPlan->Host[pPlan->HostSize] = '\0';
	return pPlan;
}



/* 释放准备阶段的全部临时存储。 */
static void __xrtHttp1RequestPrepareUnit(
	xrt_http1_request_prepare* pPrepare
)
{
	xrtFree(pPrepare->Fields);
	xrtFree(pPrepare->TrailerNames);
	xrtFree(pPrepare->HostHeader);
	xrtFree(pPrepare->Target);
	memset(pPrepare, 0, sizeof(*pPrepare));
}



/* 初始化默认准备选项。 */
XRT_API void xrtHttp1RequestOptionsInit(
	xhttp1requestoptions* pOptions
)
{
	const xhttp1requestoptions Options = {
		XHTTP1_TARGET_AUTO,
		{ NULL, 0 }
	};

	if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
		__xrtHttpRequestInvalidArgument();
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 冻结请求，并在最终 target 上按需生成一个替换型补充字段。 */
xhttp1requestplan* __xrtHttp1RequestPrepareField(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pInputOptions,
	__xrtHttp1RequestFieldFunction pField,
	ptr pContext
)
{
	xhttp1requestoptions Options;
	xrt_http1_request_prepare Prepare;
	xhttpfield ExtraField;
	const xhttpfield* pHost;
	const xhttpfield* pLength;
	const xhttpfield* pTransfer;
	xhttp1requestplan* pPlan = NULL;

	if ( !xrtMemRangeValid(pRequest, sizeof(*pRequest)) ) {
		__xrtHttpRequestInvalidArgument();
		return NULL;
	}
	if ( pInputOptions == NULL ) {
		xrtHttp1RequestOptionsInit(&Options);
	} else {
		if ( !xrtMemRangeValid(
			pInputOptions, sizeof(*pInputOptions)
		) ) {
			__xrtHttpRequestInvalidArgument();
			return NULL;
		}
		memcpy(&Options, pInputOptions, sizeof(Options));
	}
	memset(&Prepare, 0, sizeof(Prepare));
	memset(&ExtraField, 0, sizeof(ExtraField));
	Prepare.Request = pRequest;
	Prepare.Url = xrtHttpRequestUrl(pRequest);
	Prepare.Method = xrtHttpRequestMethod(pRequest);
	Prepare.Body = xrtHttpRequestBody(pRequest);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		Prepare.TrailerCount =
			xrtHttpRequestTrailerCount(pRequest);
		Prepare.Trailers =
			xrtHttpRequestTrailerData(pRequest);
	#endif
	if ( (Prepare.Url == NULL) ||
		!__xrtHttp1RequestPort(Prepare.Url, &Prepare.Port) ||
		!xrtHttpTokenValid(Prepare.Method) ||
		!__xrtHttp1RequestTarget(&Prepare, &Options) ||
		!__xrtHttp1RequestHeaders(
			&Prepare, &pHost, &pLength, &pTransfer
		) || !__xrtHttp1RequestAbsoluteHost(
			&Prepare, &Options, pHost
		) || !__xrtHttp1RequestFraming(
			&Prepare, pLength, pTransfer
		) || !__xrtHttp1RequestTrailerNames(
			&Prepare
		) ) {
		__xrtHttp1RequestPrepareUnit(&Prepare);
		return NULL;
	}
	if ( pField != NULL ) {
		if ( !pField(
			Prepare.Method,
			(xstrview){ Prepare.Target, Prepare.TargetSize },
			pContext,
			&ExtraField
		) || !xrtHttpTokenValid(ExtraField.Name) ||
			!xrtHttpFieldValueValid(ExtraField.Value) ) {
			__xrtHttp1RequestPrepareUnit(&Prepare);
			return NULL;
		}
		Prepare.ExtraField = &ExtraField;
	}
	if ( !__xrtHttp1RequestFields(
		&Prepare, pHost, pLength, pTransfer
	) ) {
		__xrtHttp1RequestPrepareUnit(&Prepare);
		return NULL;
	}
	pPlan = __xrtHttp1RequestPlanCreate(&Prepare);
	__xrtHttp1RequestPrepareUnit(&Prepare);
	return pPlan;
}



/* 冻结请求并生成完整 HTTP/1.1 Header。 */
XRT_API xhttp1requestplan* xrtHttp1RequestPrepare(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions
)
{
	return __xrtHttp1RequestPrepareField(
		pRequest, pOptions, NULL, NULL
	);
}



/* 销毁请求计划。 */
XRT_API void xrtHttp1RequestPlanDestroy(
	xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return;
	}
	xrtHttpBodyDestroy(pPlan->Body);
	memset(pPlan, 0, sizeof(*pPlan));
	xrtFree(pPlan);
}



/* 返回完整 Header 字节。 */
XRT_API xbytesview xrtHttp1RequestPlanHead(
	const xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){ pPlan->Head, pPlan->HeadSize };
}



/* 返回计划冻结的完整 last-chunk。 */
XRT_API xbytesview xrtHttp1RequestPlanEnd(
	const xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){ pPlan->End, pPlan->EndSize };
}



/* 返回计划拥有的请求方法。 */
XRT_API xstrview xrtHttp1RequestPlanMethod(
	const xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){ pPlan->Method, pPlan->MethodSize };
}



/* 返回计划拥有的完整原始 URL。 */
XRT_API xstrview xrtHttp1RequestPlanUrl(
	const xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){ pPlan->Url, pPlan->UrlSize };
}



/* 返回 request-target。 */
XRT_API xstrview xrtHttp1RequestPlanTarget(
	const xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){ pPlan->Target, pPlan->TargetSize };
}



/* 返回连接端点 host。 */
XRT_API xstrview xrtHttp1RequestPlanHost(
	const xhttp1requestplan* pPlan
)
{
	if ( pPlan == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){ pPlan->Host, pPlan->HostSize };
}



/* 返回连接端点端口。 */
XRT_API uint16 xrtHttp1RequestPlanPort(
	const xhttp1requestplan* pPlan
)
{
	return pPlan != NULL ? pPlan->Port : 0;
}



/* 判断连接端点是否使用 TLS。 */
XRT_API bool xrtHttp1RequestPlanSecure(
	const xhttp1requestplan* pPlan
)
{
	return (pPlan != NULL) && pPlan->Secure;
}



/* 返回借用正文对象。 */
XRT_API xhttpbody* xrtHttp1RequestPlanBody(
	const xhttp1requestplan* pPlan
)
{
	return pPlan != NULL ? pPlan->Body : NULL;
}



/* 返回正文发送模式。 */
XRT_API xhttprequestbodymode xrtHttp1RequestPlanBodyMode(
	const xhttp1requestplan* pPlan
)
{
	return pPlan != NULL ?
		pPlan->BodyMode : XHTTP_REQUEST_BODY_NONE;
}



/* 返回正文源长度。 */
XRT_API uint64 xrtHttp1RequestPlanBodyLength(
	const xhttp1requestplan* pPlan
)
{
	return pPlan != NULL ? pPlan->BodyLength : 0;
}



/* 判断请求是否要求关闭连接。 */
XRT_API bool xrtHttp1RequestPlanClose(
	const xhttp1requestplan* pPlan
)
{
	return (pPlan != NULL) && pPlan->Close;
}



/* 判断请求是否使用 100 Continue 握手。 */
XRT_API bool xrtHttp1RequestPlanExpectContinue(
	const xhttp1requestplan* pPlan
)
{
	return (pPlan != NULL) && pPlan->ExpectContinue;
}

#endif
