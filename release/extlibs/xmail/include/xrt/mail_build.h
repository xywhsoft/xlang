#ifndef XRT_MAIL_BUILD_H
#define XRT_MAIL_BUILD_H

#include <xrt/mail_address.h>
#include <xrt/mail_header.h>
#include <xrt/mail_multipart.h>



#if defined(XMAIL_FEATURE_MAIL_BUILD) && \
	(!defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	 !defined(XMAIL_FEATURE_MAIL_HEADER) || \
	 !defined(XMAIL_FEATURE_MAIL_MULTIPART))
	#error "XMAIL_FEATURE_MAIL_BUILD requires address, header and multipart"
#endif



#if defined(XMAIL_FEATURE_MAIL_BUILD)

/* Builder 状态公开，便于栈对象、诊断和无额外查询的快速路径。 */
typedef enum xmailbuilderstate {
	XMAIL_BUILDER_HEADERS = 0,
	XMAIL_BUILDER_BODY,
	XMAIL_BUILDER_CLOSED,
	XMAIL_BUILDER_FAILED
} xmailbuilderstate;



/* Builder 不拥有回调和用户数据，不缓存正文，也不执行隐式网络操作。 */
typedef struct xmailbuilder {
	xmailwriteproc Write;
	ptr UserData;
	size_t Written;
	xmailbuilderstate State;
	unsigned char Tail[2];
	size_t TailSize;
	bool Busy;
} xmailbuilder;



XRT_EXTERN_C_BEGIN



/* 初始化处于字段阶段的同步流式 Builder。 */
XRT_API bool xrtMailBuilderInit(
	xmailbuilder* pBuilder,
	xmailwriteproc pWrite,
	ptr pUserData
);



/* 验证、折叠并写出一个 `Name: Value\r\n` 字段。 */
XRT_API bool xrtMailBuilderHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	size_t iLineSize
);



/* 编码 UTF-8 字段值后写出字段，适用于 Subject 等非结构化字段。 */
XRT_API bool xrtMailBuilderWordHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	xmailwordencoding Encoding,
	size_t iLineSize
);



/* 格式化 mailbox 数组后写出地址字段。 */
XRT_API bool xrtMailBuilderAddressHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t iLineSize
);



/* 零复制写出一个或多个已经完整验证、以 CRLF 结束的字段。 */
XRT_API bool xrtMailBuilderHeaderBlock(
	xmailbuilder* pBuilder,
	xstrview Block
);



/* 写出字段终止空行并进入正文阶段。 */
XRT_API bool xrtMailBuilderHeadersEnd(xmailbuilder* pBuilder);



/* 零复制写出任意正文或已编码 MIME 片段。 */
XRT_API bool xrtMailBuilderBody(
	xmailbuilder* pBuilder,
	const void* pData,
	size_t iSize
);



/* 写出可与 part 字段和正文直接拼接的 multipart 分隔片段。 */
XRT_API bool xrtMailBuilderMultipart(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
);



/* 写出 FIRST/NEXT 分隔片段并进入当前 part 的字段阶段。 */
XRT_API bool xrtMailBuilderPartBegin(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
);



/* 关闭 Builder；不自动补换行、boundary 或传输编码。 */
XRT_API bool xrtMailBuilderFinish(xmailbuilder* pBuilder);



XRT_EXTERN_C_END

#endif

#endif
