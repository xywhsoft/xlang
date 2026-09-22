#ifndef XRT_POP3_AUTH_H
#define XRT_POP3_AUTH_H

#include <xrt/codec.h>
#include <xrt/pop3_client.h>



#if defined(XMAIL_FEATURE_POP3_AUTH) && \
	(!defined(XMAIL_FEATURE_POP3_CLIENT) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XMAIL_FEATURE_POP3_AUTH requires POP3 client and Base64"
#endif



#if defined(XMAIL_FEATURE_POP3_AUTH)

/* USER/PASS 保留传统命令，其余机制使用 RFC 5034 SASL 交换。 */
typedef enum xpop3authmethod {
	XPOP3_AUTH_USER_PASS = 0,
	XPOP3_AUTH_PLAIN,
	XPOP3_AUTH_XOAUTH2,
	XPOP3_AUTH_OAUTHBEARER
} xpop3authmethod;



/* 凭据只在 Auth 调用期间借用，结束前所有临时副本都会被清零。 */
typedef struct xpop3authconfig {
	xpop3authmethod Method;
	xstrview Username;
	xstrview Secret;
	xstrview AuthorizationId;
	bool InitialResponse;
	bool AllowPlaintext;
} xpop3authconfig;

XRT_EXTERN_C_BEGIN



/* 初始化 PLAIN、初始响应开启和明文凭据关闭的安全默认值。 */
XRT_API void xrtPop3AuthConfigInit(xpop3authconfig* pConfig);



/* 验证认证机制、凭据视图和机制专属分隔符。 */
XRT_API bool xrtPop3AuthConfigValid(const xpop3authconfig* pConfig);



/* 在 AUTHORIZATION 会话上执行 USER/PASS 或配置的 SASL 机制。 */
XRT_API bool xrtPop3ClientAuth(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 使用 USER/PASS 认证；默认应拒绝明文传输，显式参数用于受控兼容场景。 */
XRT_API bool xrtPop3ClientLogin(
	xpop3client* pClient,
	xstrview Username,
	xstrview Password,
	bool AllowPlaintext,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
