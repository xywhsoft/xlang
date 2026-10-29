#ifndef XRT_INTERNAL_MAIL_H
#define XRT_INTERNAL_MAIL_H

#include <xrt/mail.h>
#include <xrt/memory.h>

#if defined(XMAIL_FEATURE_MAIL_CODEC)
	#include <xrt/mail_codec.h>
	#include <xrt/codec.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_HEADER)
	#include <xrt/mail_header.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_WORD)
	#include <xrt/mail_word.h>
	#include <xrt/charset.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_ADDRESS)
	#include <xrt/mail_address.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_DATE)
	#include <xrt/mail_date.h>
	#include <xrt/time.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_ID)
	#include <xrt/mail_id.h>
	#include <xrt/random.h>
	#include <xrt/charset.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_PARAM)
	#include <xrt/mail_param.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_MULTIPART)
	#include <xrt/mail_multipart.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_MESSAGE)
	#include <xrt/mail_message.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_TREE)
	#include <xrt/mail_tree.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_BUILD)
	#include <xrt/mail_build.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_COMPOSE)
	#include <xrt/mail_compose.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_WIRE)
	#include <xrt/mail_wire.h>
#endif

#if defined(XMAIL_FEATURE_SMTP)
	#include <xrt/smtp.h>
#endif

#if defined(XMAIL_FEATURE_POP3)
	#include <xrt/pop3.h>
#endif

#if defined(XMAIL_FEATURE_POP3_CLIENT)
	#include <xrt/pop3_client.h>
#endif

#if defined(XMAIL_FEATURE_POP3_AUTH)
	#include <xrt/pop3_auth.h>
#endif

#if defined(XMAIL_FEATURE_POP3_MESSAGE)
	#include <xrt/pop3_message.h>
#endif

#if defined(XMAIL_FEATURE_IMAP)
	#include <xrt/imap.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_DATA)
	#include <xrt/imap_data.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_BODY)
	#include <xrt/imap_body.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_CLIENT)
	#include <xrt/imap_client.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_AUTH)
	#include <xrt/imap_auth.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_COMMAND)
	#include <xrt/imap_command.h>
#endif

#if defined(XMAIL_FEATURE_IMAP_MESSAGE)
	#include <xrt/imap_message.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_NET)
	#include <xrt/mail_net.h>
#endif

#if defined(XMAIL_FEATURE_SMTP_CLIENT)
	#include <xrt/smtp_client.h>
#endif

#if defined(XMAIL_FEATURE_SMTP_AUTH)
	#include <xrt/smtp_auth.h>
#endif

#if defined(XMAIL_FEATURE_SMTP_SUBMIT)
	#include <xrt/smtp_submit.h>
#endif

#include <string.h>



#if defined(XMAIL_FEATURE_MAIL_CORE)

/* 邮件扩展只通过 XRT 的公开错误入口发布错误。 */
#define __xrtMailSetInvalidArgument() \
	xrtSetErrorInfo(XERR_ARGUMENT, "xrt.mail", 0, "invalid argument")
#define __xrtMailSetRange() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.mail", 0, "value out of range")
#define __xrtMailSetSizeOverflow() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.mail", 0, "size overflow")



/* 检查字符串视图的指针和长度组合。 */
static inline bool __xrtMailViewValid(xstrview Text)
{
	return xrtMemRangeValid(Text.Data, Text.Size);
}



/* 从明确地址与长度创建借用字符串视图。 */
static inline xstrview __xrtMailView(const char* sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 从有效视图中创建子视图，并避免对空指针执行零偏移运算。 */
static inline xstrview __xrtMailSlice(
	xstrview Text,
	size_t iStart,
	size_t iSize
)
{
	return __xrtMailView(
		Text.Data != NULL ? Text.Data + iStart : NULL,
		iSize
	);
}



/* 返回邮件编码统一使用的大写十六进制字符。 */
static inline char __xrtMailHex(unsigned char iValue)
{
	static const char sDigits[] = "0123456789ABCDEF";

	return sDigits[iValue & 0x0Fu];
}



/* 把一个十六进制字符转换为数值。 */
static inline int __xrtMailHexValue(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0');
	}
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'F') ) {
		return (int)(iByte - (unsigned char)'A') + 10;
	}
	if ( (iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'f') ) {
		return (int)(iByte - (unsigned char)'a') + 10;
	}
	return -1;
}



/* 把 ASCII 字节转换为小写，非 ASCII 字节保持不变。 */
static inline unsigned char __xrtMailAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte + ((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



/* 判断两个显式长度文本是否按 ASCII 大小写不敏感规则相等。 */
static inline bool __xrtMailAsciiEqualI(xstrview Left, xstrview Right)
{
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		if ( __xrtMailAsciiLower((unsigned char)Left.Data[i]) !=
			 __xrtMailAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断字节是否属于 RFC 5322 atext。 */
static inline bool __xrtMailAtext(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'!') || (iByte == (unsigned char)'#') ||
		(iByte == (unsigned char)'$') || (iByte == (unsigned char)'%') ||
		(iByte == (unsigned char)'&') || (iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'*') || (iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)'-') || (iByte == (unsigned char)'/') ||
		(iByte == (unsigned char)'=') || (iByte == (unsigned char)'?') ||
		(iByte == (unsigned char)'^') || (iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'`') || (iByte == (unsigned char)'{') ||
		(iByte == (unsigned char)'|') || (iByte == (unsigned char)'}') ||
		(iByte == (unsigned char)'~');
}



/* 执行带溢出检查的 size_t 加法。 */
static inline bool __xrtMailSizeAdd(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( iRight > (SIZE_MAX - iLeft) ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 把 uint64 写成不带末尾零字节的十进制文本。 */
size_t __xrtMailUint64Write(char* sOutput, uint64 iValue);



/* 设置带稳定邮件错误代码的协议错误。 */
static inline void __xrtMailError(
	xerrkind Kind,
	xmailerror Code,
	cstr sMessage
)
{
	xrtSetErrorInfo(Kind, "xrt.mail", (int32)Code, sMessage);
}

#endif

#endif
