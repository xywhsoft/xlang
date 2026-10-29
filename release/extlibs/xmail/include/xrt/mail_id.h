#ifndef XRT_MAIL_ID_H
#define XRT_MAIL_ID_H

#include <xrt/mail.h>
#include <xrt/charset.h>



#if defined(XMAIL_FEATURE_MAIL_ID) && !defined(XRT_FEATURE_RANDOM_SECURE)
	#error "XMAIL_FEATURE_MAIL_ID requires XRT_FEATURE_RANDOM_SECURE"
#endif

#if defined(XMAIL_FEATURE_MAIL_ID) && !defined(XRT_FEATURE_UNICODE)
	#error "XMAIL_FEATURE_MAIL_ID requires XRT_FEATURE_UNICODE"
#endif



#if defined(XMAIL_FEATURE_MAIL_ID)

/* 默认只接受 ASCII；UTF-8 标识必须由调用方显式开启。 */
typedef enum xmailidflag {
	XMAIL_ID_DEFAULT = 0,
	XMAIL_ID_UTF8 = UINT32_C(0x00000001)
} xmailidflag;



/* Message-ID 视图借用输入，并保留尖括号内左右两部分。 */
typedef struct xmailmessageidview {
	xstrview Source;
	xstrview Left;
	xstrview Right;
} xmailmessageidview;



XRT_EXTERN_C_BEGIN



/* 解析一个完整 Message-ID，不接受过时的 quoted id-left 语法。 */
XRT_API bool xrtMailMessageIdParse(
	xstrview Text,
	uint32 iFlags,
	xmailmessageidview* pMessageId
);



/* 使用安全随机源和给定 id-right 写入一个全新 Message-ID。 */
XRT_API bool xrtMailMessageIdWrite(
	xstrview Right,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的全新 Message-ID。 */
XRT_API str xrtMailMessageId(xstrview Right, size_t* pOutputSize);



/* 使用安全随机源写入不带引号的 MIME boundary。 */
XRT_API bool xrtMailBoundaryWrite(
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的全新 MIME boundary。 */
XRT_API str xrtMailBoundary(size_t* pOutputSize);



XRT_EXTERN_C_END

#endif

#endif
