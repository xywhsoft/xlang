#ifndef XRT_MAIL_MESSAGE_H
#define XRT_MAIL_MESSAGE_H

#include <xrt/mail_codec.h>
#include <xrt/mail_header.h>



#if defined(XMAIL_FEATURE_MAIL_MESSAGE) && \
	(!defined(XMAIL_FEATURE_MAIL_CODEC) || !defined(XMAIL_FEATURE_MAIL_HEADER))
	#error "XMAIL_FEATURE_MAIL_MESSAGE requires mail codec and mail header"
#endif



#if defined(XMAIL_FEATURE_MAIL_MESSAGE)

/* 零限制使用适合互联网邮件的保守默认预算。 */
#define XMAIL_MESSAGE_HEADER_BYTES_DEFAULT (256u * 1024u)
#define XMAIL_MESSAGE_HEADERS_DEFAULT 1024u



/* 正文传输编码保留 UNKNOWN，使上层可以决定兼容或拒绝未知扩展。 */
typedef enum xmailtransfer {
	XMAIL_TRANSFER_UNKNOWN = 0,
	XMAIL_TRANSFER_7BIT,
	XMAIL_TRANSFER_8BIT,
	XMAIL_TRANSFER_BINARY,
	XMAIL_TRANSFER_QUOTED_PRINTABLE,
	XMAIL_TRANSFER_BASE64
} xmailtransfer;



/* 消息视图完全借用输入报文；Headers 包含字段后的最后一个 CRLF。 */
typedef struct xmailmessageview {
	xstrview Source;
	xstrview Headers;
	xstrview Body;
	size_t HeaderCount;
} xmailmessageview;



XRT_EXTERN_C_BEGIN



/* 严格解析 RFC 消息的字段块与正文，并在发布结果前验证全部字段。 */
XRT_API bool xrtMailMessageParse(
	xstrview Source,
	size_t iMaxHeaderBytes,
	size_t iMaxHeaders,
	xmailmessageview* pMessage
);



/* 按 ASCII 大小写不敏感名称查找第 N 个字段，返回三态结果。 */
XRT_API xmailnext xrtMailMessageHeader(
	const xmailmessageview* pMessage,
	xstrview Name,
	size_t iOccurrence,
	xmailheaderview* pHeader
);



/* 解析已展开或合法折叠的 Content-Transfer-Encoding 字段值。 */
XRT_API xmailtransfer xrtMailTransferParse(xstrview Value);



/* 返回消息正文的传输编码；字段缺失时按标准返回 7bit。 */
XRT_API bool xrtMailMessageTransfer(
	const xmailmessageview* pMessage,
	xmailtransfer* pTransfer
);



/* 按指定传输编码解码消息正文；原样编码允许同址写入。 */
XRT_API bool xrtMailMessageBodyWrite(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码正文并返回由 xrtFree 释放的字节；末尾附加零但不计入长度。 */
XRT_API bytes xrtMailMessageBody(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
