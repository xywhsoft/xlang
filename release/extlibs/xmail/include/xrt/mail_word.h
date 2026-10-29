#ifndef XRT_MAIL_WORD_H
#define XRT_MAIL_WORD_H

#include <xrt/mail.h>



#if defined(XMAIL_FEATURE_MAIL_WORD) && \
	(!defined(XMAIL_FEATURE_MAIL_CODEC) || !defined(XRT_FEATURE_UNICODE))
	#error "XMAIL_FEATURE_MAIL_WORD requires mail codec and XRT Unicode features"
#endif



#if defined(XMAIL_FEATURE_MAIL_WORD)

/* RFC 2047 编码词支持 Base64 与适用于短文本的 Q 编码。 */
typedef enum xmailwordencoding {
	XMAIL_WORD_BASE64 = 0,
	XMAIL_WORD_Q
} xmailwordencoding;



/* 容错解码保留无法识别或无法解码的原始编码词。 */
typedef enum xmailwordflag {
	XMAIL_WORD_STRICT = 0,
	XMAIL_WORD_RELAXED = UINT32_C(0x00000001)
} xmailwordflag;



/* 编码词视图全部借用输入，Source 包含完整的 =?...?=。 */
typedef struct xmailwordview {
	xstrview Source;
	xstrview Charset;
	xstrview Encoded;
	xmailwordencoding Encoding;
} xmailwordview;



XRT_EXTERN_C_BEGIN



/*
	从输入开头读取一个 RFC 2047 编码词。
	非编码词返回 XMAIL_NEXT_END，具有编码词前缀但格式错误时返回错误。
*/
XRT_API xmailnext xrtMailWordParse(xstrview Text, xmailwordview* pWord);



/*
	把 UTF-8 字段文本写成 RFC 2047 编码词序列；纯安全 ASCII 原样返回。
	查询模式使用空输出和零容量，实际输出容量必须包含末尾零字节。
*/
XRT_API bool xrtMailWordEncodeWrite(
	xstrview Text,
	xmailwordencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 RFC 2047 字段文本。 */
XRT_API str xrtMailWordEncode(
	xstrview Text,
	xmailwordencoding Encoding,
	size_t* pOutputSize
);



/*
	把混合普通文本与编码词的字段值解码为 UTF-8。
	相邻编码词之间的线性空白会被忽略，输出允许与输入从同一地址开始。
*/
XRT_API bool xrtMailWordDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 UTF-8 解码文本。 */
XRT_API str xrtMailWordDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
