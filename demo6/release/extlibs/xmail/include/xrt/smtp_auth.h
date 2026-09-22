#ifndef XRT_SMTP_AUTH_H
#define XRT_SMTP_AUTH_H

#include <xrt/codec.h>
#include <xrt/smtp_client.h>



#if defined(XMAIL_FEATURE_SMTP_AUTH) && \
	(!defined(XMAIL_FEATURE_SMTP_CLIENT) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XMAIL_FEATURE_SMTP_AUTH requires SMTP client and Base64"
#endif



#if defined(XMAIL_FEATURE_SMTP_AUTH)

/* 内置认证覆盖 SMTP 常用的口令和 OAuth2 bearer 机制。 */
typedef enum xsmtpauthmethod {
	XSMTP_AUTH_PLAIN = 0,
	XSMTP_AUTH_LOGIN,
	XSMTP_AUTH_XOAUTH2,
	XSMTP_AUTH_OAUTHBEARER
} xsmtpauthmethod;



/* 凭据视图只在 Auth 调用期间借用，调用结束后不会保存在 Client。 */
typedef struct xsmtpauthconfig {
	xsmtpauthmethod Method;
	xstrview Username;
	xstrview Secret;
	xstrview AuthorizationId;
	bool InitialResponse;
	bool AllowPlaintext;
} xsmtpauthconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 PLAIN、初始响应开启、明文凭据发送关闭的安全默认值。 */
XRT_API void xrtSmtpAuthConfigInit(xsmtpauthconfig* pConfig);



/* 验证认证机制、凭据视图和机制专属分隔符。 */
XRT_API bool xrtSmtpAuthConfigValid(const xsmtpauthconfig* pConfig);



/* 在 READY 会话上完成一次认证；服务器拒绝后仍允许调用方选择其他机制。 */
XRT_API bool xrtSmtpClientAuth(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
