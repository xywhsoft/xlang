#include "../internal/xrt_url.h"



#if defined(XHTTP_FEATURE_URL)

/* 内部写入器同时服务长度查询和已经验证容量的实际写出。 */
typedef struct xrt_url_writer {
	bytes Data;
	size_t Capacity;
	size_t Size;
} xrt_url_writer;



/* 区分完整 URL、authority、host 和 HTTP target 四种写出路径。 */
typedef enum xrt_url_output_mode {
	XRT_URL_OUTPUT_FULL,
	XRT_URL_OUTPUT_AUTHORITY,
	XRT_URL_OUTPUT_HOST,
	XRT_URL_OUTPUT_TARGET
} xrt_url_output_mode;



/* 用至多两段借用视图表示待规范化路径，避免为路径合并分配内存。 */
typedef struct xrt_url_path_source {
	xstrview First;
	xstrview Second;
	size_t Size;
} xrt_url_path_source;



/* 引用解析计划只借用 Base 与 Reference，写出阶段不再重新解析。 */
typedef struct xrt_url_resolve_plan {
	xurl Target;
	xrt_url_path_source Path;
	bool NormalizePath;
} xrt_url_resolve_plan;



/* 验证通用字符串视图的空值表示。 */
static bool __xrtUrlViewValid(xstrview Text)
{
	return __xrtRangeValid(Text.Data, Text.Size);
}



/* 在不对空指针做算术的前提下创建子视图。 */
static xstrview __xrtUrlSlice(
	xstrview Text,
	size_t iStart,
	size_t iSize
)
{
	xstrview Result;

	Result.Data = (Text.Data == NULL) ? NULL : Text.Data + iStart;
	Result.Size = iSize;
	return Result;
}



/* 安全计算两个 size_t 的和。 */
static bool __xrtUrlAddSize(size_t iLeft, size_t iRight, size_t* pResult)
{
	if ( (pResult == NULL) || (iLeft > (SIZE_MAX - iRight)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 把 ASCII 大写字母折叠为小写，其余字节保持不变。 */
static unsigned char __xrtUrlLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte + ('a' - 'A'));
	}
	return iByte;
}



/* 按 ASCII 大小写不敏感规则比较两个视图。 */
static bool __xrtUrlCaseEqual(xstrview Left, xstrview Right)
{
	size_t i;

	if ( !__xrtUrlViewValid(Left) ||
		!__xrtUrlViewValid(Right) ||
		(Left.Size != Right.Size) ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xrtUrlLower((unsigned char)Left.Data[i]) !=
			__xrtUrlLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断字节是否为 ASCII 字母。 */
static bool __xrtUrlAlpha(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z'));
}



/* 判断字节是否为 ASCII 十进制数字。 */
static bool __xrtUrlDigit(unsigned char iByte)
{
	return (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9');
}



/* 返回 ASCII 十六进制数字值，非法字节返回负数。 */
static int __xrtUrlHex(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0');
	}
	if ( (iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'f') ) {
		return 10 + (int)(iByte - (unsigned char)'a');
	}
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'F') ) {
		return 10 + (int)(iByte - (unsigned char)'A');
	}
	return -1;
}



/* 判断字节是否为 RFC 3986 unreserved。 */
static bool __xrtUrlUnreserved(unsigned char iByte)
{
	return __xrtUrlAlpha(iByte) || __xrtUrlDigit(iByte) ||
		(iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'~');
}



/* 判断字节是否为 RFC 3986 sub-delims。 */
static bool __xrtUrlSubDelimiter(unsigned char iByte)
{
	return (iByte == (unsigned char)'!') ||
		(iByte == (unsigned char)'$') ||
		(iByte == (unsigned char)'&') ||
		(iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') ||
		(iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)',') ||
		(iByte == (unsigned char)';') ||
		(iByte == (unsigned char)'=');
}



/* 验证 scheme 指定位置的单个 ASCII 字节。 */
bool __xrtUrlSchemeByte(uint8 iByte, size_t iIndex)
{
	return (iIndex == 0) ? __xrtUrlAlpha(iByte) :
		(__xrtUrlAlpha(iByte) || __xrtUrlDigit(iByte) ||
		 (iByte == (uint8)'+') ||
		 (iByte == (uint8)'-') ||
		 (iByte == (uint8)'.'));
}



/* 验证一个完整 scheme。 */
static bool __xrtUrlSchemeValid(xstrview Scheme)
{
	size_t i;

	if ( !__xrtUrlViewValid(Scheme) || (Scheme.Size == 0) ) {
		return false;
	}
	for ( i = 0; i < Scheme.Size; i++ ) {
		if ( !__xrtUrlSchemeByte(
			(uint8)Scheme.Data[i], i
		) ) {
			return false;
		}
	}
	return true;
}



/* 初始化 URI 组件流式验证状态。 */
void __xrtUrlComponentStateInit(
	xrt_url_component_state* pState
)
{
	pState->Percent = 0;
}



/* 向 URI 组件验证状态追加一个字节。 */
bool __xrtUrlComponentStateByte(
	xrt_url_component_state* pState,
	uint8 iByte,
	uint32 iAllowed
)
{
	if ( pState->Percent != 0 ) {
		if ( __xrtUrlHex(iByte) < 0 ) {
			return false;
		}
		pState->Percent--;
		return true;
	}
	if ( __xrtUrlUnreserved(iByte) ||
		__xrtUrlSubDelimiter(iByte) ||
		(((iAllowed & XRT_URL_COMPONENT_COLON) != 0) &&
		 (iByte == (uint8)':')) ||
		(((iAllowed & XRT_URL_COMPONENT_AT) != 0) &&
		 (iByte == (uint8)'@')) ||
		(((iAllowed & XRT_URL_COMPONENT_SLASH) != 0) &&
		 (iByte == (uint8)'/')) ||
		(((iAllowed & XRT_URL_COMPONENT_QUESTION) != 0) &&
		 (iByte == (uint8)'?')) ) {
		return true;
	}
	if ( iByte == (uint8)'%' ) {
		pState->Percent = 2u;
		return true;
	}
	return false;
}



/* 判断 URI 组件流式验证状态已经完整结束。 */
bool __xrtUrlComponentStateValid(
	const xrt_url_component_state* pState
)
{
	return pState->Percent == 0;
}



/* 验证 RFC 3986 ASCII URI 组件与 percent-encoded 字节。 */
static bool __xrtUrlComponentValid(
	xstrview Text,
	bool bColon,
	bool bAt,
	bool bSlash,
	bool bQuestion
)
{
	xrt_url_component_state State;
	uint32 iAllowed = 0;
	size_t i;

	if ( !__xrtUrlViewValid(Text) ) {
		return false;
	}
	if ( bColon ) {
		iAllowed |= XRT_URL_COMPONENT_COLON;
	}
	if ( bAt ) {
		iAllowed |= XRT_URL_COMPONENT_AT;
	}
	if ( bSlash ) {
		iAllowed |= XRT_URL_COMPONENT_SLASH;
	}
	if ( bQuestion ) {
		iAllowed |= XRT_URL_COMPONENT_QUESTION;
	}
	__xrtUrlComponentStateInit(&State);
	for ( i = 0; i < Text.Size; i++ ) {
		if ( !__xrtUrlComponentStateByte(
			&State, (uint8)Text.Data[i], iAllowed
		) ) {
			return false;
		}
	}
	return __xrtUrlComponentStateValid(&State);
}



/* 解析 authority 的 userinfo、host 和显式端口。 */
static bool __xrtUrlAuthorityInto(xstrview Authority, xurl* pUrl)
{
	size_t iAt = XRT_NPOS;
	size_t iHostStart = 0;
	size_t i;
	xhttpauthority Host;

	if ( !__xrtUrlViewValid(Authority) || (pUrl == NULL) ) {
		return false;
	}
	pUrl->Authority = Authority;
	pUrl->Flags |= XURL_HAS_AUTHORITY | XURL_HAS_HOST;
	for ( i = 0; i < Authority.Size; i++ ) {
		if ( Authority.Data[i] != '@' ) {
			continue;
		}
		if ( iAt != XRT_NPOS ) {
			return false;
		}
		iAt = i;
	}
	if ( iAt != XRT_NPOS ) {
		pUrl->UserInfo = (xstrview){ Authority.Data, iAt };
		if ( !__xrtUrlComponentValid(
			pUrl->UserInfo, true, false, false, false
		) ) {
			return false;
		}
		pUrl->Flags |= XURL_HAS_USERINFO;
		iHostStart = iAt + 1u;
	}
	if ( !xrtHttpHostParse((xstrview){
		Authority.Data == NULL ? NULL : Authority.Data + iHostStart,
		Authority.Size - iHostStart
	}, &Host) ) {
		return false;
	}
	pUrl->Host = Host.Host;
	pUrl->PortText = Host.PortText;
	pUrl->Port = Host.Port;
	if ( (Host.Flags & XHTTP_AUTHORITY_IP_LITERAL) != 0 ) {
		pUrl->Flags |= XURL_HOST_IP_LITERAL;
	}
	if ( (Host.Flags & XHTTP_AUTHORITY_HAS_PORT) != 0 ) {
		pUrl->Flags |= XURL_HAS_PORT;
	}
	if ( (Host.Flags & XHTTP_AUTHORITY_PORT_EMPTY) != 0 ) {
		pUrl->Flags |= XURL_PORT_EMPTY;
	}
	if ( (Host.Flags & XHTTP_AUTHORITY_PORT_VALUE) != 0 ) {
		pUrl->Flags |= XURL_PORT_VALUE;
	}
	return true;
}



/* 严格解析 URI-reference 到局部结果，失败时不发布半成品。 */
bool __xrtUrlParseValue(xstrview Text, xurl* pUrl)
{
	xurl Url;
	size_t iFragment = XRT_NPOS;
	size_t iQuery = XRT_NPOS;
	size_t iMainEnd;
	size_t iPathStart = 0;
	size_t iColon = XRT_NPOS;
	size_t iSlash = XRT_NPOS;
	size_t i;

	if ( (pUrl == NULL) || !__xrtUrlViewValid(Text) ) {
		return false;
	}
	memset(&Url, 0, sizeof(Url));
	for ( i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] == '#' ) {
			iFragment = i;
			break;
		}
	}
	iMainEnd = (iFragment == XRT_NPOS) ? Text.Size : iFragment;
	for ( i = 0; i < iMainEnd; i++ ) {
		if ( Text.Data[i] == '?' ) {
			iQuery = i;
			break;
		}
	}
	if ( iQuery != XRT_NPOS ) {
		iMainEnd = iQuery;
	}
	for ( i = 0; i < iMainEnd; i++ ) {
		if ( (Text.Data[i] == ':') && (iColon == XRT_NPOS) ) {
			iColon = i;
		}
		if ( Text.Data[i] == '/' ) {
			iSlash = i;
			break;
		}
	}
	if ( (iColon != XRT_NPOS) &&
		((iSlash == XRT_NPOS) || (iColon < iSlash)) ) {
		Url.Scheme = (xstrview){ Text.Data, iColon };
		if ( !__xrtUrlSchemeValid(Url.Scheme) ) {
			return false;
		}
		Url.Flags |= XURL_HAS_SCHEME;
		iPathStart = iColon + 1u;
	}
	if ( ((iMainEnd - iPathStart) >= 2u) &&
		(Text.Data[iPathStart] == '/') &&
		(Text.Data[iPathStart + 1u] == '/') ) {
		xurl Authority;
		size_t iAuthorityStart = iPathStart + 2u;
		size_t iAuthorityEnd = iAuthorityStart;

		while ( (iAuthorityEnd < iMainEnd) &&
			(Text.Data[iAuthorityEnd] != '/') ) {
			iAuthorityEnd++;
		}
		memset(&Authority, 0, sizeof(Authority));
		if ( !__xrtUrlAuthorityInto(
			(xstrview){
				Text.Data + iAuthorityStart,
				iAuthorityEnd - iAuthorityStart
			},
			&Authority
		) ) {
			return false;
		}
		Url.Authority = Authority.Authority;
		Url.UserInfo = Authority.UserInfo;
		Url.Host = Authority.Host;
		Url.PortText = Authority.PortText;
		Url.Port = Authority.Port;
		Url.Flags |= Authority.Flags;
		iPathStart = iAuthorityEnd;
	}
	Url.Path = __xrtUrlSlice(
		Text, iPathStart, iMainEnd - iPathStart
	);
	if ( !__xrtUrlComponentValid(
		Url.Path, true, true, true, false
	) ) {
		return false;
	}
	if ( ((Url.Flags & XURL_HAS_AUTHORITY) != 0) &&
		(Url.Path.Size != 0) && (Url.Path.Data[0] != '/') ) {
		return false;
	}
	if ( ((Url.Flags & XURL_HAS_AUTHORITY) == 0) &&
		(Url.Path.Size >= 2) && (Url.Path.Data[0] == '/') &&
		(Url.Path.Data[1] == '/') ) {
		return false;
	}
	if ( (iQuery != XRT_NPOS) ) {
		size_t iQueryEnd = (iFragment == XRT_NPOS) ?
			Text.Size : iFragment;

		Url.Query = (xstrview){
			Text.Data + iQuery + 1u,
			iQueryEnd - iQuery - 1u
		};
		if ( !__xrtUrlComponentValid(
			Url.Query, true, true, true, true
		) ) {
			return false;
		}
		Url.Flags |= XURL_HAS_QUERY;
	}
	if ( iFragment != XRT_NPOS ) {
		Url.Fragment = (xstrview){
			Text.Data + iFragment + 1u,
			Text.Size - iFragment - 1u
		};
		if ( !__xrtUrlComponentValid(
			Url.Fragment, true, true, true, true
		) ) {
			return false;
		}
		Url.Flags |= XURL_HAS_FRAGMENT;
	}
	*pUrl = Url;
	return true;
}



/* 严格解析 RFC 3986 URI-reference。 */
XRT_API bool xrtUrlParse(xstrview Text, xurl* pUrl)
{
	xurl Url = { 0 };

	if ( !__xrtRangeValid(pUrl, sizeof(Url)) ||
		!__xrtUrlViewValid(Text) ||
		__xrtRangesOverlap(
			pUrl, sizeof(Url), Text.Data, Text.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtUrlParseValue(Text, &Url) ) {
		memcpy(pUrl, &Url, sizeof(Url));
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pUrl, &Url, sizeof(Url));
	return true;
}



/* 独立解析 authority 并发布借用视图。 */
XRT_API bool xrtUrlAuthorityParse(xstrview Authority, xurl* pUrl)
{
	xurl Url;

	if ( !__xrtRangeValid(pUrl, sizeof(Url)) ||
		!__xrtUrlViewValid(Authority) ||
		__xrtRangesOverlap(
			pUrl, sizeof(Url), Authority.Data, Authority.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Url, 0, sizeof(Url));
	if ( !__xrtUrlAuthorityInto(Authority, &Url) ) {
		memcpy(pUrl, &Url, sizeof(Url));
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pUrl, &Url, sizeof(Url));
	return true;
}



/* 返回 XRT HTTP 与 WebSocket 协议族的已知默认端口。 */
XRT_API uint16 xrtUrlDefaultPort(xstrview Scheme)
{
	if ( __xrtUrlCaseEqual(Scheme, XRT_STR_LITERAL("http")) ||
		__xrtUrlCaseEqual(Scheme, XRT_STR_LITERAL("ws")) ) {
		return 80;
	}
	if ( __xrtUrlCaseEqual(Scheme, XRT_STR_LITERAL("https")) ||
		__xrtUrlCaseEqual(Scheme, XRT_STR_LITERAL("wss")) ) {
		return 443;
	}
	return 0;
}



/* 无错误副作用地取得可用的显式端口或已知默认端口。 */
bool __xrtUrlPortValue(const xurl* pUrl, uint16* pPort)
{
	uint16 iPort;

	if ( (pUrl == NULL) || (pPort == NULL) ) {
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_PORT) != 0) &&
		((pUrl->Flags & XURL_PORT_EMPTY) == 0) ) {
		if ( (pUrl->Flags & XURL_PORT_VALUE) == 0 ) {
			return false;
		}
		iPort = pUrl->Port;
	} else {
		iPort = xrtUrlDefaultPort(pUrl->Scheme);
		if ( iPort == 0 ) {
			return false;
		}
	}
	*pPort = iPort;
	return true;
}



/* 取得可直接传给网络层的端口，并区分无默认值与数值越界。 */
XRT_API bool xrtUrlPort(const xurl* pUrl, uint16* pPort)
{
	xurl Url;
	uint16 iPort;

	if ( !__xrtRangeValid(pUrl, sizeof(Url)) ||
		!__xrtRangeValid(pPort, sizeof(iPort)) ||
		__xrtRangesOverlap(
			pUrl, sizeof(Url), pPort, sizeof(iPort)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Url, pUrl, sizeof(Url));
	if ( !__xrtUrlValueValid(&Url) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtUrlPortValue(&Url, &iPort) ) {
		if ( ((Url.Flags & XURL_HAS_PORT) != 0) &&
			((Url.Flags & XURL_PORT_EMPTY) == 0) ) {
			__xrtErrorSetRange();
		} else {
			__xrtErrorSetValue();
		}
		return false;
	}
	memcpy(pPort, &iPort, sizeof(iPort));
	return true;
}



/* 判断 URL scheme。 */
XRT_API bool xrtUrlSchemeIs(const xurl* pUrl, xstrview Scheme)
{
	if ( (pUrl == NULL) || !__xrtUrlViewValid(Scheme) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return ((pUrl->Flags & XURL_HAS_SCHEME) != 0) &&
		__xrtUrlCaseEqual(pUrl->Scheme, Scheme);
}



/* 判断 HTTPS 或 WSS。 */
XRT_API bool xrtUrlSecure(const xurl* pUrl)
{
	if ( pUrl == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtUrlSchemeIs(pUrl, XRT_STR_LITERAL("https")) ||
		xrtUrlSchemeIs(pUrl, XRT_STR_LITERAL("wss"));
}



/* 判断显式端口是否为空或等于已知默认端口。 */
XRT_API bool xrtUrlPortIsDefault(const xurl* pUrl)
{
	uint16 iDefault;

	if ( pUrl == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_PORT) == 0) ||
		((pUrl->Flags & XURL_HAS_SCHEME) == 0) ) {
		return false;
	}
	iDefault = xrtUrlDefaultPort(pUrl->Scheme);
	return (iDefault != 0) &&
		(((pUrl->Flags & XURL_PORT_EMPTY) != 0) ||
		 (((pUrl->Flags & XURL_PORT_VALUE) != 0) &&
		  (pUrl->Port == iDefault)));
}



/* 验证 URL 结构的组件、存在位和层次路径约束保持一致。 */
bool __xrtUrlValueValid(const xurl* pUrl)
{
	const uint32 iKnownFlags = XURL_HAS_SCHEME |
		XURL_HAS_AUTHORITY | XURL_HAS_USERINFO |
		XURL_HAS_HOST | XURL_HAS_PORT |
		XURL_HAS_QUERY | XURL_HAS_FRAGMENT |
		XURL_HOST_IP_LITERAL | XURL_PORT_EMPTY |
		XURL_PORT_VALUE;
	size_t i;

	if ( (pUrl == NULL) || ((pUrl->Flags & ~iKnownFlags) != 0) ||
		!__xrtUrlViewValid(pUrl->Scheme) ||
		!__xrtUrlViewValid(pUrl->Authority) ||
		!__xrtUrlViewValid(pUrl->UserInfo) ||
		!__xrtUrlViewValid(pUrl->Host) ||
		!__xrtUrlViewValid(pUrl->PortText) ||
		!__xrtUrlViewValid(pUrl->Path) ||
		!__xrtUrlViewValid(pUrl->Query) ||
		!__xrtUrlViewValid(pUrl->Fragment) ) {
		return false;
	}
	if ( (pUrl->Flags & XURL_HAS_SCHEME) != 0 ) {
		if ( !__xrtUrlSchemeValid(pUrl->Scheme) ) {
			return false;
		}
	} else if ( pUrl->Scheme.Size != 0 ) {
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_PORT) == 0) &&
		(((pUrl->Flags & (XURL_PORT_EMPTY |
		 XURL_PORT_VALUE)) != 0) || (pUrl->Port != 0) ||
		 (pUrl->PortText.Size != 0)) ) {
		return false;
	}
	if ( (pUrl->Flags & XURL_HAS_AUTHORITY) != 0 ) {
		xhttpauthority Host = { 0 };

		if ( (pUrl->Flags & XURL_HAS_HOST) == 0 ) {
			return false;
		}
		if ( (pUrl->Flags & XURL_HAS_USERINFO) != 0 ) {
			if ( !__xrtUrlComponentValid(
				pUrl->UserInfo, true, false, false, false
			) ) {
				return false;
			}
		} else if ( pUrl->UserInfo.Size != 0 ) {
			return false;
		}
		Host.Port = pUrl->Port;
		Host.Host = pUrl->Host;
		Host.PortText = pUrl->PortText;
		if ( (pUrl->Flags & XURL_HAS_PORT) != 0 ) {
			Host.Flags |= XHTTP_AUTHORITY_HAS_PORT;
		}
		if ( (pUrl->Flags & XURL_HOST_IP_LITERAL) != 0 ) {
			Host.Flags |= XHTTP_AUTHORITY_IP_LITERAL;
		}
		if ( (pUrl->Flags & XURL_PORT_EMPTY) != 0 ) {
			Host.Flags |= XHTTP_AUTHORITY_PORT_EMPTY;
		}
		if ( (pUrl->Flags & XURL_PORT_VALUE) != 0 ) {
			Host.Flags |= XHTTP_AUTHORITY_PORT_VALUE;
		}
		if ( !xrtHttpAuthorityValid(&Host) ) {
			return false;
		}
	} else if ( (pUrl->Flags & (XURL_HAS_USERINFO |
		XURL_HAS_HOST | XURL_HAS_PORT |
		XURL_HOST_IP_LITERAL | XURL_PORT_EMPTY |
		XURL_PORT_VALUE)) != 0 ) {
		return false;
	}
	if ( !__xrtUrlComponentValid(
		pUrl->Path, true, true, true, false
	) ) {
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_AUTHORITY) != 0) &&
		(pUrl->Path.Size != 0) && (pUrl->Path.Data[0] != '/') ) {
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_AUTHORITY) == 0) &&
		(pUrl->Path.Size >= 2) && (pUrl->Path.Data[0] == '/') &&
		(pUrl->Path.Data[1] == '/') ) {
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_SCHEME) == 0) &&
		((pUrl->Flags & XURL_HAS_AUTHORITY) == 0) ) {
		for ( i = 0; i < pUrl->Path.Size; i++ ) {
			if ( pUrl->Path.Data[i] == '/' ) {
				break;
			}
			if ( pUrl->Path.Data[i] == ':' ) {
				return false;
			}
		}
	}
	if ( (pUrl->Flags & XURL_HAS_QUERY) != 0 ) {
		if ( !__xrtUrlComponentValid(
			pUrl->Query, true, true, true, true
		) ) {
			return false;
		}
	} else if ( pUrl->Query.Size != 0 ) {
		return false;
	}
	if ( (pUrl->Flags & XURL_HAS_FRAGMENT) != 0 ) {
		if ( !__xrtUrlComponentValid(
			pUrl->Fragment, true, true, true, true
		) ) {
			return false;
		}
	} else if ( pUrl->Fragment.Size != 0 ) {
		return false;
	}
	return true;
}



/* 向长度查询或实际输出写入一段字节。 */
static bool __xrtUrlWriterAppend(
	xrt_url_writer* pWriter,
	const void* pData,
	size_t iSize
)
{
	size_t iEnd;

	if ( (pWriter == NULL) ||
		((pData == NULL) && (iSize != 0)) ||
		!__xrtUrlAddSize(pWriter->Size, iSize, &iEnd) ) {
		return false;
	}
	if ( pWriter->Data != NULL ) {
		if ( iEnd > pWriter->Capacity ) {
			__xrtErrorSetRange();
			return false;
		}
		if ( iSize != 0 ) {
			memcpy(pWriter->Data + pWriter->Size, pData, iSize);
		}
	}
	pWriter->Size = iEnd;
	return true;
}



/* 以十进制写出一个 uint16 端口。 */
static bool __xrtUrlWriterPort(
	xrt_url_writer* pWriter,
	uint16 iPort
)
{
	char Digits[5];
	char Output[5];
	size_t iDigits = 0;
	size_t i;
	uint32 iValue = iPort;

	do {
		Digits[iDigits++] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0 );
	for ( i = 0; i < iDigits; i++ ) {
		Output[i] = Digits[iDigits - i - 1u];
	}
	return __xrtUrlWriterAppend(pWriter, Output, iDigits);
}



/* 写出 host 与可选显式端口。 */
static bool __xrtUrlWriteHost(
	const xurl* pUrl,
	xrt_url_writer* pWriter
)
{
	if ( (pUrl->Flags & XURL_HOST_IP_LITERAL) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "[", 1) ||
			!__xrtUrlWriterAppend(
				pWriter, pUrl->Host.Data, pUrl->Host.Size
			) || !__xrtUrlWriterAppend(pWriter, "]", 1) ) {
			return false;
		}
	} else if ( !__xrtUrlWriterAppend(
		pWriter, pUrl->Host.Data, pUrl->Host.Size
	) ) {
		return false;
	}
	if ( (pUrl->Flags & XURL_HAS_PORT) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, ":", 1) ) {
			return false;
		}
		if ( (pUrl->Flags & XURL_PORT_EMPTY) == 0 ) {
			if ( pUrl->PortText.Size != 0 ) {
				if ( !__xrtUrlWriterAppend(
					pWriter,
					pUrl->PortText.Data,
					pUrl->PortText.Size
				) ) {
					return false;
				}
			} else if ( !__xrtUrlWriterPort(pWriter, pUrl->Port) ) {
				return false;
			}
		}
	}
	return true;
}



/* 写出 authority 的结构化组件。 */
static bool __xrtUrlWriteAuthority(
	const xurl* pUrl,
	xrt_url_writer* pWriter
)
{
	if ( (pUrl->Flags & XURL_HAS_USERINFO) != 0 ) {
		if ( !__xrtUrlWriterAppend(
			pWriter, pUrl->UserInfo.Data, pUrl->UserInfo.Size
		) || !__xrtUrlWriterAppend(pWriter, "@", 1) ) {
			return false;
		}
	}
	return __xrtUrlWriteHost(pUrl, pWriter);
}



/* 写出完整 URI-reference。 */
static bool __xrtUrlWriteFull(
	const xurl* pUrl,
	xrt_url_writer* pWriter
)
{
	if ( (pUrl->Flags & XURL_HAS_SCHEME) != 0 ) {
		if ( !__xrtUrlWriterAppend(
			pWriter, pUrl->Scheme.Data, pUrl->Scheme.Size
		) || !__xrtUrlWriterAppend(pWriter, ":", 1) ) {
			return false;
		}
	}
	if ( (pUrl->Flags & XURL_HAS_AUTHORITY) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "//", 2) ||
			!__xrtUrlWriteAuthority(pUrl, pWriter) ) {
			return false;
		}
	}
	if ( !__xrtUrlWriterAppend(
		pWriter, pUrl->Path.Data, pUrl->Path.Size
	) ) {
		return false;
	}
	if ( (pUrl->Flags & XURL_HAS_QUERY) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "?", 1) ||
			!__xrtUrlWriterAppend(
				pWriter, pUrl->Query.Data, pUrl->Query.Size
			) ) {
			return false;
		}
	}
	if ( (pUrl->Flags & XURL_HAS_FRAGMENT) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "#", 1) ||
			!__xrtUrlWriterAppend(
				pWriter, pUrl->Fragment.Data, pUrl->Fragment.Size
			) ) {
			return false;
		}
	}
	return true;
}



/* 写出 HTTP origin-form target，明确排除 fragment。 */
static bool __xrtUrlWriteTarget(
	const xurl* pUrl,
	xrt_url_writer* pWriter
)
{
	if ( pUrl->Path.Size == 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "/", 1) ) {
			return false;
		}
	} else {
		if ( (pUrl->Path.Data[0] != '/') ||
			!__xrtUrlWriterAppend(
				pWriter, pUrl->Path.Data, pUrl->Path.Size
			) ) {
			return false;
		}
	}
	if ( (pUrl->Flags & XURL_HAS_QUERY) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "?", 1) ||
			!__xrtUrlWriterAppend(
				pWriter, pUrl->Query.Data, pUrl->Query.Size
			) ) {
			return false;
		}
	}
	return true;
}



/* 判断输出范围是否与一个实际读取视图重叠。 */
static bool __xrtUrlViewOverlap(
	xstrview Input,
	const void* pOutput,
	size_t iSize
)
{
	return __xrtRangesOverlap(
		Input.Data, Input.Size, pOutput, iSize
	);
}



/* 判断输出范围是否与 URL 的任一实际读取组件重叠。 */
static bool __xrtUrlOverlap(
	const xurl* pUrl,
	const void* pOutput,
	size_t iSize
)
{
	return __xrtUrlViewOverlap(pUrl->Scheme, pOutput, iSize) ||
		__xrtUrlViewOverlap(pUrl->UserInfo, pOutput, iSize) ||
		__xrtUrlViewOverlap(pUrl->Host, pOutput, iSize) ||
		__xrtUrlViewOverlap(pUrl->PortText, pOutput, iSize) ||
		__xrtUrlViewOverlap(pUrl->Path, pOutput, iSize) ||
		__xrtUrlViewOverlap(pUrl->Query, pOutput, iSize) ||
		__xrtUrlViewOverlap(pUrl->Fragment, pOutput, iSize);
}



/* 统一完整 URL、authority、host 和 target 的原子写出契约。 */
static bool __xrtUrlOutput(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xrt_url_output_mode Mode
)
{
	xurl Url;
	xrt_url_writer Writer;
	size_t iSize = 0;
	bool bValid;

	if ( !__xrtRangeValid(pUrl, sizeof(Url)) ||
		!__xrtRangeValid(pSize, sizeof(iSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ||
		__xrtRangesOverlap(
			pUrl, sizeof(Url), pSize, sizeof(iSize)
		) || ((pOutput != NULL) &&
		 __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iSize)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Url, pUrl, sizeof(Url));
	if ( __xrtUrlOverlap(&Url, pSize, sizeof(iSize)) ||
		((pOutput != NULL) &&
		 (__xrtRangesOverlap(
			pUrl, sizeof(Url), pOutput, iCapacity
		  ) || __xrtUrlOverlap(
			&Url, pOutput, iCapacity
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iSize, sizeof(iSize));
	bValid = __xrtUrlValueValid(&Url);
	if ( bValid && ((Mode == XRT_URL_OUTPUT_AUTHORITY) ||
		(Mode == XRT_URL_OUTPUT_HOST)) ) {
		bValid = (Url.Flags & XURL_HAS_AUTHORITY) != 0;
	}
	if ( bValid && (Mode == XRT_URL_OUTPUT_TARGET) &&
		(Url.Path.Size != 0) ) {
		bValid = Url.Path.Data[0] == '/';
	}
	if ( !bValid ) {
		__xrtErrorSetValue();
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	if ( ((Mode == XRT_URL_OUTPUT_FULL) &&
		!__xrtUrlWriteFull(&Url, &Writer)) ||
		((Mode == XRT_URL_OUTPUT_AUTHORITY) &&
		 !__xrtUrlWriteAuthority(&Url, &Writer)) ||
		((Mode == XRT_URL_OUTPUT_HOST) &&
		 !__xrtUrlWriteHost(&Url, &Writer)) ||
		((Mode == XRT_URL_OUTPUT_TARGET) &&
		 !__xrtUrlWriteTarget(&Url, &Writer)) ) {
		return false;
	}
	iSize = Writer.Size;
	memcpy(pSize, &iSize, sizeof(iSize));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < Writer.Size ) {
		__xrtErrorSetRange();
		return false;
	}
	Writer.Data = (bytes)pOutput;
	Writer.Capacity = iCapacity;
	Writer.Size = 0;
	if ( ((Mode == XRT_URL_OUTPUT_FULL) &&
		!__xrtUrlWriteFull(&Url, &Writer)) ||
		((Mode == XRT_URL_OUTPUT_AUTHORITY) &&
		 !__xrtUrlWriteAuthority(&Url, &Writer)) ||
		((Mode == XRT_URL_OUTPUT_HOST) &&
		 !__xrtUrlWriteHost(&Url, &Writer)) ||
		((Mode == XRT_URL_OUTPUT_TARGET) &&
		 !__xrtUrlWriteTarget(&Url, &Writer)) ) {
		return false;
	}
	return true;
}



/* 原样写出完整 URL。 */
XRT_API bool xrtUrlWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtUrlOutput(
		pUrl, pOutput, iCapacity, pSize, XRT_URL_OUTPUT_FULL
	);
}



/* 构建零结尾完整 URL。 */
XRT_API str xrtUrlBuild(const xurl* pUrl, size_t* pSize)
{
	xurl Url;
	size_t iSize;
	str sOutput;

	if ( pSize != NULL ) {
		if ( !__xrtRangeValid(pSize, sizeof(iSize)) ||
			!__xrtRangeValid(pUrl, sizeof(Url)) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		memcpy(&Url, pUrl, sizeof(Url));
		if ( __xrtRangesOverlap(
			pUrl, sizeof(Url), pSize, sizeof(iSize)
		) || __xrtUrlOverlap(
			&Url, pSize, sizeof(iSize)
		) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		iSize = 0;
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	if ( !xrtUrlWrite(pUrl, NULL, 0, &iSize) ) {
		return NULL;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtUrlWrite(pUrl, sOutput, iSize, &iSize) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iSize] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	return sOutput;
}



/* 写出 authority。 */
XRT_API bool xrtUrlAuthorityWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtUrlOutput(
		pUrl, pOutput, iCapacity, pSize, XRT_URL_OUTPUT_AUTHORITY
	);
}



/* 写出 host 与显式端口。 */
XRT_API bool xrtUrlHostWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtUrlOutput(
		pUrl, pOutput, iCapacity, pSize, XRT_URL_OUTPUT_HOST
	);
}



/* 写出 HTTP origin-form target。 */
XRT_API bool xrtUrlTargetWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtUrlOutput(
		pUrl, pOutput, iCapacity, pSize, XRT_URL_OUTPUT_TARGET
	);
}



/* 把单段路径包装为统一路径源。 */
static void __xrtUrlPathSourceOne(
	xstrview Path,
	xrt_url_path_source* pSource
)
{
	pSource->First = Path;
	pSource->Second = (xstrview){ NULL, 0 };
	pSource->Size = Path.Size;
}



/* 把两段路径包装为统一路径源，并检查总长度。 */
static bool __xrtUrlPathSourceTwo(
	xstrview First,
	xstrview Second,
	xrt_url_path_source* pSource
)
{
	size_t iSize;

	if ( !__xrtUrlAddSize(First.Size, Second.Size, &iSize) ) {
		return false;
	}
	pSource->First = First;
	pSource->Second = Second;
	pSource->Size = iSize;
	return true;
}



/* 从复合路径源读取一个字节。 */
static uint8 __xrtUrlPathByte(
	const xrt_url_path_source* pSource,
	size_t iOffset
)
{
	if ( iOffset < pSource->First.Size ) {
		return (uint8)pSource->First.Data[iOffset];
	}
	return (uint8)pSource->Second.Data[
		iOffset - pSource->First.Size
	];
}



/* 从复合路径源复制一段连续逻辑区间。 */
static void __xrtUrlPathCopy(
	const xrt_url_path_source* pSource,
	size_t iOffset,
	bytes pOutput,
	size_t iSize
)
{
	size_t iFirst = 0;

	if ( (iOffset < pSource->First.Size) && (iSize != 0) ) {
		iFirst = pSource->First.Size - iOffset;
		if ( iFirst > iSize ) {
			iFirst = iSize;
		}
		memmove(pOutput, pSource->First.Data + iOffset, iFirst);
	}
	if ( iFirst < iSize ) {
		size_t iSecond = iOffset + iFirst -
			pSource->First.Size;

		memmove(
			pOutput + iFirst,
			pSource->Second.Data + iSecond,
			iSize - iFirst
		);
	}
}



/* 判断剩余输入是否以指定 ASCII 字面量开头。 */
static bool __xrtUrlPathStarts(
	const xrt_url_path_source* pSource,
	size_t iOffset,
	cstr sPrefix,
	size_t iPrefix
)
{
	size_t i;

	if ( (iOffset > pSource->Size) ||
		(iPrefix > (pSource->Size - iOffset)) ) {
		return false;
	}
	for ( i = 0; i < iPrefix; i++ ) {
		if ( __xrtUrlPathByte(pSource, iOffset + i) !=
			(uint8)sPrefix[i] ) {
			return false;
		}
	}
	return true;
}



/* 从 remove_dot_segments 输出中删除最后一个路径段。 */
static void __xrtUrlPathPop(bytes pOutput, size_t* pSize)
{
	size_t i = *pSize;

	while ( i != 0 ) {
		i--;
		if ( pOutput[i] == (uint8)'/' ) {
			*pSize = i;
			return;
		}
	}
	*pSize = 0;
}



/* 在足够大的工作区中执行 RFC 3986 remove_dot_segments。 */
static bool __xrtUrlPathNormalizeRaw(
	const xrt_url_path_source* pSource,
	bytes pOutput,
	size_t* pSize
)
{
	size_t iInput = 0;
	size_t iOutput = 0;

	while ( iInput < pSource->Size ) {
		if ( __xrtUrlPathStarts(pSource, iInput, "../", 3) ) {
			iInput += 3u;
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, "./", 2) ) {
			iInput += 2u;
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, "/./", 3) ) {
			iInput += 2u;
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, "/.", 2) &&
			((iInput + 2u) == pSource->Size) ) {
			iInput += 2u;
			pOutput[iOutput++] = (uint8)'/';
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, "/../", 4) ) {
			iInput += 3u;
			__xrtUrlPathPop(pOutput, &iOutput);
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, "/..", 3) &&
			((iInput + 3u) == pSource->Size) ) {
			iInput += 3u;
			__xrtUrlPathPop(pOutput, &iOutput);
			pOutput[iOutput++] = (uint8)'/';
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, ".", 1) &&
			((iInput + 1u) == pSource->Size) ) {
			iInput++;
			continue;
		}
		if ( __xrtUrlPathStarts(pSource, iInput, "..", 2) &&
			((iInput + 2u) == pSource->Size) ) {
			iInput += 2u;
			continue;
		}
		{
			size_t iStart = iInput;

			if ( __xrtUrlPathByte(pSource, iInput) == (uint8)'/' ) {
				iInput++;
			}
			while ( (iInput < pSource->Size) &&
				(__xrtUrlPathByte(
					pSource, iInput
				) != (uint8)'/') ) {
				iInput++;
			}
			__xrtUrlPathCopy(
				pSource, iStart,
				pOutput + iOutput, iInput - iStart
			);
			iOutput += iInput - iStart;
		}
	}
	*pSize = iOutput;
	return true;
}



/* 跳过输入开头会被规则 A、B 与 D 删除的点段。 */
static size_t __xrtUrlPathPrefix(
	const xrt_url_path_source* pSource
)
{
	size_t iOffset = 0;

	for ( ;; ) {
		if ( __xrtUrlPathStarts(
			pSource, iOffset, "../", 3
		) ) {
			iOffset += 3u;
			continue;
		}
		if ( __xrtUrlPathStarts(
			pSource, iOffset, "./", 2
		) ) {
			iOffset += 2u;
			continue;
		}
		if ( __xrtUrlPathStarts(
			pSource, iOffset, "..", 2
		) && ((iOffset + 2u) == pSource->Size) ) {
			return pSource->Size;
		}
		if ( __xrtUrlPathStarts(
			pSource, iOffset, ".", 1
		) && ((iOffset + 1u) == pSource->Size) ) {
			return pSource->Size;
		}
		return iOffset;
	}
}



/* 线性反向扫描点段栈，精确计算规范化路径长度。 */
static bool __xrtUrlPathNormalizeMeasure(
	const xrt_url_path_source* pSource,
	size_t* pSize
)
{
	size_t iBegin = __xrtUrlPathPrefix(pSource);
	size_t iEnd = pSource->Size;
	size_t iDebt = 0;
	size_t iSize = 0;
	bool bTerminal = true;

	while ( iEnd > iBegin ) {
		size_t iSegment = iEnd;
		size_t iChunk;
		size_t iSegmentSize;
		bool bSlash;
		bool bDot;
		bool bDotDot;

		while ( (iSegment > iBegin) &&
			(__xrtUrlPathByte(
				pSource, iSegment - 1u
			) != (uint8)'/') ) {
			iSegment--;
		}
		bSlash = iSegment > iBegin;
		iChunk = bSlash ? (iSegment - 1u) : iBegin;
		iSegmentSize = iEnd - iSegment;
		bDot = (iSegmentSize == 1u) &&
			(__xrtUrlPathByte(
				pSource, iSegment
			) == (uint8)'.');
		bDotDot = (iSegmentSize == 2u) &&
			(__xrtUrlPathByte(
				pSource, iSegment
			) == (uint8)'.') &&
			(__xrtUrlPathByte(
				pSource, iSegment + 1u
			) == (uint8)'.');

		if ( bDotDot && bSlash ) {
			iDebt++;
			if ( bTerminal &&
				!__xrtUrlAddSize(iSize, 1u, &iSize) ) {
				return false;
			}
		} else if ( bDot ) {
			if ( bTerminal && bSlash &&
				!__xrtUrlAddSize(iSize, 1u, &iSize) ) {
				return false;
			}
		} else if ( iDebt != 0 ) {
			iDebt--;
		} else if ( !__xrtUrlAddSize(
			iSize, iEnd - iChunk, &iSize
		) ) {
			return false;
		}
		iEnd = iChunk;
		bTerminal = false;
	}
	*pSize = iSize;
	return true;
}



/* 分配并规范化 URL path。 */
XRT_API str xrtUrlPathNormalizeBuild(
	xstrview Path,
	size_t* pSize
)
{
	xrt_url_path_source Source;
	str sOutput;
	size_t iSize;

	if ( !__xrtUrlViewValid(Path) ||
		((pSize != NULL) &&
		 (!__xrtRangeValid(pSize, sizeof(iSize)) ||
		  __xrtRangesOverlap(
			Path.Data, Path.Size, pSize, sizeof(iSize)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pSize != NULL ) {
		memset(pSize, 0, sizeof(iSize));
	}
	if ( !__xrtUrlComponentValid(
		Path, true, true, true, false
	) ) {
		__xrtErrorSetValue();
		return NULL;
	}
	__xrtUrlPathSourceOne(Path, &Source);
	if ( !__xrtUrlPathNormalizeMeasure(&Source, &iSize) ) {
		return NULL;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	(void)__xrtUrlPathNormalizeRaw(
		&Source, (bytes)sOutput, &iSize
	);
	sOutput[iSize] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	return sOutput;
}



/* 原子写出规范化 URL path。 */
XRT_API bool xrtUrlPathNormalize(
	xstrview Path,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xrt_url_path_source Source;
	xrt_url_path_source WriteSource;
	size_t iSize;
	bool bOverlap;

	if ( !__xrtRangeValid(pSize, sizeof(iSize)) ||
		!__xrtUrlViewValid(Path) ||
		__xrtRangesOverlap(
			Path.Data, Path.Size, pSize, sizeof(iSize)
		) || ((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pSize, 0, sizeof(iSize));
	if ( !__xrtUrlComponentValid(
		Path, true, true, true, false
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	__xrtUrlPathSourceOne(Path, &Source);
	if ( !__xrtUrlPathNormalizeMeasure(&Source, &iSize) ) {
		return false;
	}
	memcpy(pSize, &iSize, sizeof(iSize));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iCapacity) ||
		__xrtRangesOverlap(
			pOutput, iSize, pSize, sizeof(iSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iSize ) {
		__xrtErrorSetRange();
		return false;
	}
	bOverlap = __xrtRangesOverlap(
		Path.Data, Path.Size, pOutput, iSize
	);
	WriteSource = Source;
	if ( bOverlap &&
		((uintptr_t)pOutput >
		 (uintptr_t)(const void*)Path.Data) ) {
		if ( (iCapacity < Path.Size) ||
			__xrtRangesOverlap(
				pOutput, Path.Size,
				pSize, sizeof(iSize)
			) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memmove(pOutput, Path.Data, Path.Size);
		__xrtUrlPathSourceOne(
			(xstrview){ (cstr)pOutput, Path.Size },
			&WriteSource
		);
	}
	(void)__xrtUrlPathNormalizeRaw(
		&WriteSource, (bytes)pOutput, &iSize
	);
	return true;
}



/* 取得 RFC 3986 merge 使用的基础目录视图。 */
static xstrview __xrtUrlMergePrefix(const xurl* pBase)
{
	size_t i;

	if ( ((pBase->Flags & XURL_HAS_AUTHORITY) != 0) &&
		(pBase->Path.Size == 0) ) {
		return XRT_STR_LITERAL("/");
	}
	for ( i = pBase->Path.Size; i != 0; i-- ) {
		if ( pBase->Path.Data[i - 1u] == '/' ) {
			return (xstrview){ pBase->Path.Data, i };
		}
	}
	return (xstrview){ NULL, 0 };
}



/* 复制 authority 组件及其存在位。 */
static void __xrtUrlCopyAuthority(xurl* pTarget, const xurl* pSource)
{
	const uint32 iAuthorityFlags = XURL_HAS_AUTHORITY |
		XURL_HAS_USERINFO | XURL_HAS_HOST |
		XURL_HAS_PORT | XURL_HOST_IP_LITERAL |
		XURL_PORT_EMPTY | XURL_PORT_VALUE;

	pTarget->Flags &= ~iAuthorityFlags;
	pTarget->Flags |= pSource->Flags & iAuthorityFlags;
	pTarget->Authority = pSource->Authority;
	pTarget->UserInfo = pSource->UserInfo;
	pTarget->Host = pSource->Host;
	pTarget->PortText = pSource->PortText;
	pTarget->Port = pSource->Port;
}



/* 根据 Base 与引用生成只借用输入的 RFC 3986 解析计划。 */
static bool __xrtUrlResolvePlanInit(
	const xurl* pBase,
	xstrview Reference,
	xrt_url_resolve_plan* pPlan
)
{
	xstrview Prefix;
	xurl Ref;

	if ( !__xrtUrlValueValid(pBase) ||
		((pBase->Flags & XURL_HAS_SCHEME) == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtUrlParseValue(Reference, &Ref) ) {
		__xrtErrorSetValue();
		return false;
	}
	memset(pPlan, 0, sizeof(*pPlan));
	if ( (Ref.Flags & XURL_HAS_SCHEME) != 0 ) {
		pPlan->Target = Ref;
		__xrtUrlPathSourceOne(Ref.Path, &pPlan->Path);
		pPlan->NormalizePath = true;
		return true;
	}

	pPlan->Target.Scheme = pBase->Scheme;
	pPlan->Target.Flags = XURL_HAS_SCHEME;
	if ( (Ref.Flags & XURL_HAS_AUTHORITY) != 0 ) {
		__xrtUrlCopyAuthority(&pPlan->Target, &Ref);
		__xrtUrlPathSourceOne(Ref.Path, &pPlan->Path);
		pPlan->NormalizePath = true;
		pPlan->Target.Query = Ref.Query;
		pPlan->Target.Flags |= Ref.Flags & XURL_HAS_QUERY;
	} else {
		__xrtUrlCopyAuthority(&pPlan->Target, pBase);
		if ( Ref.Path.Size == 0 ) {
			__xrtUrlPathSourceOne(
				pBase->Path, &pPlan->Path
			);
			if ( (Ref.Flags & XURL_HAS_QUERY) != 0 ) {
				pPlan->Target.Query = Ref.Query;
				pPlan->Target.Flags |= XURL_HAS_QUERY;
			} else if ( (pBase->Flags & XURL_HAS_QUERY) != 0 ) {
				pPlan->Target.Query = pBase->Query;
				pPlan->Target.Flags |= XURL_HAS_QUERY;
			}
		} else {
			pPlan->NormalizePath = true;
			if ( Ref.Path.Data[0] == '/' ) {
				__xrtUrlPathSourceOne(
					Ref.Path, &pPlan->Path
				);
			} else {
				Prefix = __xrtUrlMergePrefix(pBase);
				if ( !__xrtUrlPathSourceTwo(
					Prefix, Ref.Path, &pPlan->Path
				) ) {
					return false;
				}
			}
			pPlan->Target.Query = Ref.Query;
			pPlan->Target.Flags |= Ref.Flags & XURL_HAS_QUERY;
		}
	}
	pPlan->Target.Fragment = Ref.Fragment;
	pPlan->Target.Flags |= Ref.Flags & XURL_HAS_FRAGMENT;
	return true;
}



/* 写出解析计划中的原样路径或点段规范化路径。 */
static bool __xrtUrlResolvePathWrite(
	const xrt_url_resolve_plan* pPlan,
	xrt_url_writer* pWriter
)
{
	size_t iEnd;
	size_t iPath;

	if ( pPlan->NormalizePath ) {
		if ( !__xrtUrlPathNormalizeMeasure(
			&pPlan->Path, &iPath
		) ) {
			return false;
		}
	} else {
		iPath = pPlan->Path.Size;
	}
	if ( !__xrtUrlAddSize(pWriter->Size, iPath, &iEnd) ) {
		return false;
	}
	if ( pWriter->Data != NULL ) {
		if ( iEnd > pWriter->Capacity ) {
			__xrtErrorSetRange();
			return false;
		}
		if ( pPlan->NormalizePath ) {
			(void)__xrtUrlPathNormalizeRaw(
				&pPlan->Path,
				pWriter->Data + pWriter->Size,
				&iPath
			);
		} else {
			__xrtUrlPathCopy(
				&pPlan->Path, 0,
				pWriter->Data + pWriter->Size,
				iPath
			);
		}
	}
	pWriter->Size = iEnd;
	return true;
}



/* 统一测量或写出一个已经验证的引用解析计划。 */
static bool __xrtUrlResolvePlanWrite(
	const xrt_url_resolve_plan* pPlan,
	xrt_url_writer* pWriter
)
{
	const xurl* pTarget = &pPlan->Target;

	if ( !__xrtUrlWriterAppend(
		pWriter, pTarget->Scheme.Data, pTarget->Scheme.Size
	) || !__xrtUrlWriterAppend(pWriter, ":", 1) ) {
		return false;
	}
	if ( (pTarget->Flags & XURL_HAS_AUTHORITY) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "//", 2) ||
			!__xrtUrlWriteAuthority(pTarget, pWriter) ) {
			return false;
		}
	}
	if ( !__xrtUrlResolvePathWrite(pPlan, pWriter) ) {
		return false;
	}
	if ( (pTarget->Flags & XURL_HAS_QUERY) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "?", 1) ||
			!__xrtUrlWriterAppend(
				pWriter,
				pTarget->Query.Data,
				pTarget->Query.Size
			) ) {
			return false;
		}
	}
	if ( (pTarget->Flags & XURL_HAS_FRAGMENT) != 0 ) {
		if ( !__xrtUrlWriterAppend(pWriter, "#", 1) ||
			!__xrtUrlWriterAppend(
				pWriter,
				pTarget->Fragment.Data,
				pTarget->Fragment.Size
			) ) {
			return false;
		}
	}
	return true;
}



/* 判断引用解析输出是否覆盖仍需读取的输入。 */
static bool __xrtUrlResolveOverlap(
	const xurl* pBase,
	xstrview Reference,
	const void* pOutput,
	size_t iSize
)
{
	return __xrtRangesOverlap(
		pBase, sizeof(*pBase), pOutput, iSize
	) || __xrtUrlOverlap(pBase, pOutput, iSize) ||
		__xrtUrlViewOverlap(Reference, pOutput, iSize);
}



/* 原子写出 RFC 3986 引用解析结果，全程不申请动态内存。 */
XRT_API bool xrtUrlResolve(
	const xurl* pBase,
	xstrview Reference,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xrt_url_resolve_plan Plan;
	xrt_url_writer Writer;
	size_t iSize;

	if ( !__xrtRangeValid(pBase, sizeof(*pBase)) ||
		!__xrtUrlViewValid(Reference) ||
		!__xrtRangeValid(pSize, sizeof(iSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pBase, sizeof(*pBase), pSize, sizeof(iSize)
		) || __xrtUrlViewOverlap(
			Reference, pSize, sizeof(iSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pSize, 0, sizeof(iSize));
	if ( !__xrtUrlResolvePlanInit(
		pBase, Reference, &Plan
	) ) {
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	if ( !__xrtUrlResolvePlanWrite(&Plan, &Writer) ) {
		return false;
	}
	iSize = Writer.Size;
	memcpy(pSize, &iSize, sizeof(iSize));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iCapacity) ||
		__xrtRangesOverlap(
			pOutput, iSize, pSize, sizeof(iSize)
		) || __xrtUrlResolveOverlap(
			pBase, Reference, pOutput, iSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iSize ) {
		__xrtErrorSetRange();
		return false;
	}
	Writer.Data = (bytes)pOutput;
	Writer.Capacity = iCapacity;
	Writer.Size = 0;
	return __xrtUrlResolvePlanWrite(&Plan, &Writer);
}



/* 单次分配并构建 RFC 3986 引用解析结果。 */
XRT_API str xrtUrlResolveBuild(
	const xurl* pBase,
	xstrview Reference,
	size_t* pSize
)
{
	xrt_url_resolve_plan Plan;
	xrt_url_writer Writer;
	str sOutput;
	size_t iSize;

	if ( !__xrtRangeValid(pBase, sizeof(*pBase)) ||
		!__xrtUrlViewValid(Reference) ||
		((pSize != NULL) &&
		 (!__xrtRangeValid(pSize, sizeof(iSize)) ||
		  __xrtRangesOverlap(
			pBase, sizeof(*pBase), pSize, sizeof(iSize)
		  ) || __xrtUrlViewOverlap(
			Reference, pSize, sizeof(iSize)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pSize != NULL ) {
		memset(pSize, 0, sizeof(iSize));
	}
	if ( !__xrtUrlResolvePlanInit(
		pBase, Reference, &Plan
	) ) {
		return NULL;
	}
	memset(&Writer, 0, sizeof(Writer));
	if ( !__xrtUrlResolvePlanWrite(&Plan, &Writer) ) {
		return NULL;
	}
	iSize = Writer.Size;
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	Writer.Data = (bytes)sOutput;
	Writer.Capacity = iSize;
	Writer.Size = 0;
	if ( !__xrtUrlResolvePlanWrite(&Plan, &Writer) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[Writer.Size] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &Writer.Size, sizeof(Writer.Size));
	}
	return sOutput;
}

#endif
