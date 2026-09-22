#ifndef XRT_MAIL_H
#define XRT_MAIL_H

#include <xrt/error.h>



#if (defined(XMAIL_FEATURE_MAIL_CODEC) || defined(XMAIL_FEATURE_MAIL_HEADER) || \
	 defined(XMAIL_FEATURE_MAIL_WORD) || defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	 defined(XMAIL_FEATURE_MAIL_DATE) || defined(XMAIL_FEATURE_MAIL_ID) || \
	 defined(XMAIL_FEATURE_MAIL_PARAM) || defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	 defined(XMAIL_FEATURE_MAIL_MESSAGE) || defined(XMAIL_FEATURE_MAIL_TREE) || \
	 defined(XMAIL_FEATURE_MAIL_BUILD) || \
	 defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	 defined(XMAIL_FEATURE_MAIL_WIRE) || \
	 defined(XMAIL_FEATURE_SMTP) || defined(XMAIL_FEATURE_POP3) || \
	 defined(XMAIL_FEATURE_IMAP) || defined(XMAIL_FEATURE_IMAP_DATA) || \
	 defined(XMAIL_FEATURE_IMAP_BODY) || \
	 defined(XMAIL_FEATURE_MAIL_NET) || \
	 defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	 defined(XMAIL_FEATURE_SMTP_CLIENT) || \
	 defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
	 defined(XMAIL_FEATURE_SMTP_AUTH) || \
	 defined(XMAIL_FEATURE_SMTP_SUBMIT) || \
	 defined(XMAIL_FEATURE_POP3_CLIENT) || \
	 defined(XMAIL_FEATURE_POP3_CLIENT_TLS) || \
	 defined(XMAIL_FEATURE_POP3_AUTH) || \
	 defined(XMAIL_FEATURE_POP3_MESSAGE) || \
	 defined(XMAIL_FEATURE_IMAP_CLIENT) || \
	 defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
	 defined(XMAIL_FEATURE_IMAP_AUTH) || \
	 defined(XMAIL_FEATURE_IMAP_COMMAND) || \
	 defined(XMAIL_FEATURE_IMAP_MESSAGE) || \
	 defined(XMAIL_FEATURE_IMAP_APPEND) || \
	 defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || \
	 defined(XMAIL_FEATURE_IMAP_COMPRESS)) && \
	!defined(XMAIL_FEATURE_MAIL_CORE)
	#error "xmail content features require XMAIL_FEATURE_MAIL_CORE"
#endif



#if defined(XMAIL_FEATURE_MAIL_CORE)

/* 邮件正文编码和字段折叠使用的标准行宽。 */
#define XMAIL_QP_LINE_DEFAULT 76u
#define XMAIL_BASE64_LINE_DEFAULT 76u
#define XMAIL_HEADER_LINE_DEFAULT 78u
#define XMAIL_HEADER_LINE_HARD 998u
#define XMAIL_BOUNDARY_MAX 70u



/* 流式邮件解析统一使用三态结果。 */
typedef enum xmailnext {
	XMAIL_NEXT_ERROR = -1,
	XMAIL_NEXT_END = 0,
	XMAIL_NEXT_ITEM = 1
} xmailnext;



/* 邮件内容流统一使用只借用当前片段的同步 sink。 */
typedef bool (*xmailwriteproc)(xbytesview Data, ptr pUserData);



/* 邮件扩展在 xrt.mail 错误域内使用稳定代码。 */
typedef enum xmailerror {
	XMAIL_ERROR_CONFIG = 1601,
	XMAIL_ERROR_LINE,
	XMAIL_ERROR_HEADER,
	XMAIL_ERROR_ENCODING,
	XMAIL_ERROR_CHARSET,
	XMAIL_ERROR_ADDRESS,
	XMAIL_ERROR_MIME,
	XMAIL_ERROR_PROTOCOL,
	XMAIL_ERROR_LIMIT,
	XMAIL_ERROR_AUTH,
	XMAIL_ERROR_CALLBACK
} xmailerror;



XRT_EXTERN_C_BEGIN



/*
	把 LF、CRLF 和独立 CR 统一写成 CRLF。
	查询模式使用空输出和零容量；实际写入要求容量包含末尾零字节。
*/
XRT_API bool xrtMailCrlfWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 CRLF 规范文本。 */
XRT_API str xrtMailCrlf(xstrview Text, size_t* pOutputSize);



/* 判断文本是否可以直接作为 MIME boundary 参数值使用。 */
XRT_API bool xrtMailBoundaryValid(xstrview Boundary);



XRT_EXTERN_C_END

#endif

#endif
