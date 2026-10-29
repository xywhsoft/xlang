#ifndef XRT_MAIL_COMPOSE_H
#define XRT_MAIL_COMPOSE_H

#include <xrt/mail_build.h>
#include <xrt/mail_codec.h>
#include <xrt/mail_date.h>
#include <xrt/mail_id.h>
#include <xrt/mail_param.h>



#if defined(XMAIL_FEATURE_MAIL_COMPOSE) && \
	(!defined(XMAIL_FEATURE_MAIL_BUILD) || \
	 !defined(XMAIL_FEATURE_MAIL_CODEC) || \
	 !defined(XMAIL_FEATURE_MAIL_DATE) || \
	 !defined(XMAIL_FEATURE_MAIL_ID) || \
	 !defined(XMAIL_FEATURE_MAIL_PARAM))
	#error "XMAIL_FEATURE_MAIL_COMPOSE requires build, codec, date, id and param"
#endif



#if defined(XMAIL_FEATURE_MAIL_COMPOSE)

/* 附件描述只借用文件名、媒体类型、Content-ID 和原始数据。 */
typedef struct xmailattachment {
	xstrview FileName;
	xstrview MediaType;
	xstrview ContentId;
	xbytesview Data;
	bool Inline;
} xmailattachment;



/*
	高层消息描述全部借用调用方数据；Bcc 只供提交层取得收件人，不写入报文。
	空 Date、MessageId 和 boundary 由库生成，空 MessageIdDomain 从 From 推导。
*/
typedef struct xmailmessage {
	xmailaddress From;
	xmailaddress ReplyTo;
	const xmailaddress* To;
	size_t ToCount;
	const xmailaddress* Cc;
	size_t CcCount;
	const xmailaddress* Bcc;
	size_t BccCount;
	xstrview Subject;
	xstrview Text;
	xstrview Html;
	const xmailattachment* Attachments;
	size_t AttachmentCount;
	const xmailheaderview* Headers;
	size_t HeaderCount;
	xstrview Date;
	xstrview MessageId;
	xstrview MessageIdDomain;
	xstrview MixedBoundary;
	xstrview AlternativeBoundary;
	xstrview RelatedBoundary;
	xmailwordencoding WordEncoding;
	uint32 AddressFlags;
	size_t HeaderLineSize;
} xmailmessage;



XRT_EXTERN_C_BEGIN



/* 初始化 Base64 编码词、默认字段行宽和其余空视图。 */
XRT_API void xrtMailMessageInit(xmailmessage* pMessage);



/* 完整验证消息描述，且不生成随机值、不分配结果或调用 sink。 */
XRT_API bool xrtMailMessageValid(const xmailmessage* pMessage);



/*
	验证后流式构建完整 RFC 消息；回调失败时可能已经提交前缀。
	文本使用 UTF-8 Quoted-Printable，附件按块写出 MIME Base64。
*/
XRT_API bool xrtMailComposeWrite(
	const xmailmessage* pMessage,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
);



/* 构建由 xrtFree 释放的完整 RFC 消息。 */
XRT_API str xrtMailCompose(
	const xmailmessage* pMessage,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
