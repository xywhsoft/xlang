#ifndef XRT_SMTP_SUBMIT_H
#define XRT_SMTP_SUBMIT_H

#include <xrt/mail_compose.h>
#include <xrt/smtp_client.h>



#if defined(XMAIL_FEATURE_SMTP_SUBMIT) && \
	(!defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	 !defined(XMAIL_FEATURE_SMTP_CLIENT))
	#error "XMAIL_FEATURE_SMTP_SUBMIT requires mail compose and SMTP client"
#endif



#if defined(XMAIL_FEATURE_SMTP_SUBMIT)

/* 高级 envelope 允许每个收件人携带独立 ESMTP 参数。 */
typedef struct xsmtprecipient {
	xstrview Address;
	xstrview Parameters;
} xsmtprecipient;



/* Envelope 与消息字段彼此独立，全部只在提交调用期间借用。 */
typedef struct xsmtpenvelope {
	xstrview ReversePath;
	xstrview MailParameters;
	const xsmtprecipient* Recipients;
	size_t RecipientCount;
} xsmtpenvelope;



XRT_EXTERN_C_BEGIN



/* 使用独立 envelope 流式构建并提交消息，不创建整封临时报文。 */
XRT_API bool xrtSmtpSubmitEnvelope(
	xsmtpclient* pClient,
	const xsmtpenvelope* pEnvelope,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 从消息 From、To、Cc、Bcc 自动建立 envelope 并流式提交。 */
XRT_API bool xrtSmtpSubmit(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
