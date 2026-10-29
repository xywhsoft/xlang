#ifndef XRT_MAIL_CODEC_H
#define XRT_MAIL_CODEC_H

#include <xrt/mail.h>



#if defined(XMAIL_FEATURE_MAIL_CODEC) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XMAIL_FEATURE_MAIL_CODEC requires XRT_FEATURE_CODEC_BASE64"
#endif



#if defined(XMAIL_FEATURE_MAIL_CODEC)

/* Quoted-Printable 文本模式会把所有输入换行规范为 CRLF。 */
typedef enum xmailqpflag {
	XMAIL_QP_BINARY = 0,
	XMAIL_QP_TEXT = UINT32_C(0x00000001),
	XMAIL_QP_RELAXED_SOFT_BREAK = UINT32_C(0x00000002)
} xmailqpflag;



XRT_EXTERN_C_BEGIN



/*
	按 RFC 2045 写出 Quoted-Printable；零行宽使用 76。
	文本结果要求输出容量包含末尾零字节。
*/
XRT_API bool xrtMailQpWrite(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 Quoted-Printable 文本。 */
XRT_API str xrtMailQp(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	size_t* pOutputSize
);



/* 严格解码 Quoted-Printable，输出允许与输入从同一地址开始。 */
XRT_API bool xrtMailQpDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码并返回由 xrtFree 释放的字节；额外末尾零不计入长度。 */
XRT_API bytes xrtMailQpDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
);



/*
	按 MIME 行宽写出标准 Base64；行宽必须是不超过 76 的四的倍数。
	非空结果以 CRLF 结束，输出容量必须包含末尾零字节。
*/
XRT_API bool xrtMailBase64Write(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 MIME Base64 文本。 */
XRT_API str xrtMailBase64(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	size_t* pOutputSize
);



/* 忽略 MIME 空白并严格解码 Base64，输出允许与输入同址。 */
XRT_API bool xrtMailBase64DecodeWrite(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码并返回由 xrtFree 释放的字节；额外末尾零不计入长度。 */
XRT_API bytes xrtMailBase64Decode(xstrview Text, size_t* pOutputSize);



XRT_EXTERN_C_END

#endif

#endif
