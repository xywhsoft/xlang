#ifndef XRT_MAIL_PARAM_H
#define XRT_MAIL_PARAM_H

#include <xrt/charset.h>
#include <xrt/mail.h>



#if defined(XMAIL_FEATURE_MAIL_PARAM) && !defined(XRT_FEATURE_UNICODE)
	#error "XMAIL_FEATURE_MAIL_PARAM requires XRT_FEATURE_UNICODE"
#endif



#if defined(XMAIL_FEATURE_MAIL_PARAM)

/* 非连续参数使用该值表示没有 section 编号。 */
#define XMAIL_PARAM_SECTION_NONE XRT_NPOS
#define XMAIL_PARAM_SECTIONS_MAX 64u
#define XMAIL_PARAM_SECTION_SIZE 60u



/* MIME type/subtype 和后续参数均借用原字段值。 */
typedef struct xmailmediatypeview {
	xstrview Source;
	xstrview Type;
	xstrview Subtype;
	xstrview Parameters;
} xmailmediatypeview;



/* Content-Disposition 主 token 和后续参数均借用原字段值。 */
typedef struct xmaildispositionview {
	xstrview Source;
	xstrview Type;
	xstrview Parameters;
} xmaildispositionview;



/* 参数视图同时暴露原始形式、基础名称、值和 RFC 2231 section 信息。 */
typedef struct xmailparamview {
	xstrview Source;
	xstrview RawName;
	xstrview Name;
	xstrview RawValue;
	xstrview Value;
	size_t Section;
	bool Extended;
	bool Continued;
	bool Quoted;
} xmailparamview;



/* 参数游标只持有借用文本和下一参数位置。 */
typedef struct xmailparamcursor {
	xstrview Text;
	size_t Position;
	bool Done;
} xmailparamcursor;



/* 合并参数元数据中的字符集和语言视图借用原字段值。 */
typedef struct xmailparaminfo {
	xstrview Charset;
	xstrview Language;
	size_t Sections;
	bool Extended;
	bool Continued;
} xmailparaminfo;



/* AUTO 选择 token、quoted-string 或 UTF-8 扩展参数的最短合法形式。 */
typedef enum xmailparamencoding {
	XMAIL_PARAM_ENCODING_AUTO = 0,
	XMAIL_PARAM_ENCODING_TOKEN,
	XMAIL_PARAM_ENCODING_QUOTED,
	XMAIL_PARAM_ENCODING_UTF8
} xmailparamencoding;



XRT_EXTERN_C_BEGIN



/* 解析并完整验证 Content-Type 字段值。 */
XRT_API bool xrtMailMediaTypeParse(
	xstrview Text,
	xmailmediatypeview* pMediaType
);



/* 解析并完整验证 Content-Disposition 字段值。 */
XRT_API bool xrtMailDispositionParse(
	xstrview Text,
	xmaildispositionview* pDisposition
);



/* 初始化以分号开头或为空的 MIME 参数游标。 */
XRT_API bool xrtMailParamCursorInit(
	xmailparamcursor* pCursor,
	xstrview Parameters
);



/* 返回下一个 MIME 参数及其 RFC 2231 section 描述。 */
XRT_API xmailnext xrtMailParamNext(
	xmailparamcursor* pCursor,
	xmailparamview* pParameter
);



/* 解码单个参数 section 的 quoted-pair 和扩展百分号编码。 */
XRT_API bool xrtMailParamDecodeWrite(
	const xmailparamview* pParameter,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xstrview* pCharset,
	xstrview* pLanguage
);



/* 查找、合并并解码参数；返回 ITEM、END 或 ERROR。 */
XRT_API xmailnext xrtMailParamFindWrite(
	xstrview Parameters,
	xstrview Name,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
);



/* 创建由 xrtFree 释放的合并参数；未找到和错误均返回 NULL。 */
XRT_API str xrtMailParamFind(
	xstrview Parameters,
	xstrview Name,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
);



/* 写出包含分号前缀的 MIME 参数，长值自动拆成 RFC 2231 连续段。 */
XRT_API bool xrtMailParamWrite(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的单个 MIME 参数。 */
XRT_API str xrtMailParam(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
