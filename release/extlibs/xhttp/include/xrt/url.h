#ifndef XRT_URL_H
#define XRT_URL_H

#include <xrt.h>





#if defined(XHTTP_FEATURE_URL) && !defined(XRT_FEATURE_HTTP_HOST)
	#error "xhttp URL support requires HTTP Host support"
#endif

#if defined(XHTTP_FEATURE_URL_PARAM) && \
	(!defined(XHTTP_FEATURE_URL) || \
	 !defined(XRT_FEATURE_HTTP_PARAM_HOST))
	#error "xhttp URL parameter support requires URL and HTTP parameter Host support"
#endif



#if defined(XHTTP_FEATURE_URL)

/* URL 包含 scheme，Scheme 可以直接用于协议判断。 */
#define XURL_HAS_SCHEME		UINT32_C(0x00000001)

/* URL 包含双斜杠引入的 authority，包括显式空 authority。 */
#define XURL_HAS_AUTHORITY	UINT32_C(0x00000002)

/* authority 包含 @ 引入的 userinfo，包括显式空 userinfo。 */
#define XURL_HAS_USERINFO	UINT32_C(0x00000004)

/* authority 已解析出 host；RFC 3986 允许 host 为空。 */
#define XURL_HAS_HOST		UINT32_C(0x00000008)

/* authority 包含冒号引入的端口，包括显式空端口。 */
#define XURL_HAS_PORT		UINT32_C(0x00000010)

/* URL 包含问号引入的 query，包括显式空 query。 */
#define XURL_HAS_QUERY		UINT32_C(0x00000020)

/* URL 包含井号引入的 fragment，包括显式空 fragment。 */
#define XURL_HAS_FRAGMENT	UINT32_C(0x00000040)

/* Host 是 IPv6 或 IPvFuture 字面地址，Host 视图不包含方括号。 */
#define XURL_HOST_IP_LITERAL	UINT32_C(0x00000080)

/* 显式端口只有冒号而没有数字；该标志必须与 XURL_HAS_PORT 同时存在。 */
#define XURL_PORT_EMPTY		UINT32_C(0x00000100)

/* 非空端口已经无损转换为 uint16；超出网络端口范围的协议文本不设置该位。 */
#define XURL_PORT_VALUE		UINT32_C(0x00000200)



/* URL 视图借用原始文本；修改或释放输入会使全部非空视图失效。 */
typedef struct xurl {
	/* 组件存在位；空组件是否存在只能由这些标志判断。 */
	uint32 Flags;

	/* 仅在 XURL_PORT_VALUE 存在时保存可直接用于网络层的数值端口。 */
	uint16 Port;

	/* 以下视图全部借用输入，Authority 保留原始 authority 文本。 */
	xstrview Scheme;
	xstrview Authority;
	xstrview UserInfo;
	xstrview Host;

	/* PortText 保留端口数字的词法形式，因此 :00080 可以精确往返。 */
	xstrview PortText;
	xstrview Path;
	xstrview Query;
	xstrview Fragment;
} xurl;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_URL)

/*
	严格解析 ASCII RFC 3986 URI-reference；结果全部借用 Text。
	pUrl 可使用未对齐存储，但完整可写区间不得回绕或覆盖 Text。
*/
XRT_API bool xrtUrlParse(xstrview Text, xurl* pUrl);



/*
	独立解析 authority；允许空 host 与空端口，结果全部借用 Authority。
	pUrl 可使用未对齐存储，但完整可写区间不得回绕或覆盖 Authority。
*/
XRT_API bool xrtUrlAuthorityParse(xstrview Authority, xurl* pUrl);



/* 返回 HTTP、HTTPS、WS 或 WSS 的默认端口，其他 scheme 返回零。 */
XRT_API uint16 xrtUrlDefaultPort(xstrview Scheme);



/* 取得可用网络端口；省略或空端口使用已知默认值，超出 uint16 范围失败。 */
XRT_API bool xrtUrlPort(const xurl* pUrl, uint16* pPort);



/* 按 ASCII 大小写不敏感规则判断 scheme。 */
XRT_API bool xrtUrlSchemeIs(const xurl* pUrl, xstrview Scheme);



/* 判断 URL 是否使用 HTTPS 或 WSS。 */
XRT_API bool xrtUrlSecure(const xurl* pUrl);



/* 判断显式端口对已知 scheme 为空或等于默认端口。 */
XRT_API bool xrtUrlPortIsDefault(const xurl* pUrl);



/*
	精确写出 URI-reference；不写零结尾，空输出可查询长度。
	pSize 可使用未对齐存储；输出区间回绕或覆盖输入、借用视图或 pSize 时失败。
*/
XRT_API bool xrtUrlWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	分配并构建零结尾 URI-reference；返回值由 xrtFree 释放。
	pSize 可使用未对齐存储，但不得覆盖输入对象或其借用视图。
*/
XRT_API str xrtUrlBuild(const xurl* pUrl, size_t* pSize);



/* 写出 authority，不含前导双斜杠或零结尾；空输出可查询长度。 */
XRT_API bool xrtUrlAuthorityWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出 host 与显式端口，不含 userinfo 或零结尾；保留显式默认端口。 */
XRT_API bool xrtUrlHostWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出 HTTP origin-form target：空路径变为斜杠，query 保留，fragment 丢弃。 */
XRT_API bool xrtUrlTargetWrite(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 按 RFC 3986 零分配规范化 path；原地或前向重叠可用，失败不修改输出。 */
XRT_API bool xrtUrlPathNormalize(
	xstrview Path,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配零结尾的规范化 URL path；返回值由 xrtFree 释放。 */
XRT_API str xrtUrlPathNormalizeBuild(
	xstrview Path,
	size_t* pSize
);



/* 零分配解析 RFC 3986 引用；Base 必须含 scheme，输出不得覆盖输入。 */
XRT_API bool xrtUrlResolve(
	const xurl* pBase,
	xstrview Reference,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 单次分配零结尾的解析结果；返回值由 xrtFree 释放。 */
XRT_API str xrtUrlResolveBuild(
	const xurl* pBase,
	xstrview Reference,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_URL_PARAM)

/* 无分配验证 HTTP 参数解码后的语义值是完整 RFC 3986 URI-reference。 */
XRT_API bool xrtUrlParamValid(const xhttpparam* pParam);

#endif



XRT_EXTERN_C_END

#endif
