#ifndef XRT_MAIL_NET_H
#define XRT_MAIL_NET_H

#include <xrt/mail_wire.h>
#include <xrt/tcp.h>

#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
	#include <xrt/tls_stream.h>
#endif



#if defined(XMAIL_FEATURE_MAIL_NET) && \
	(!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL_SYNC))
	#error "XMAIL_FEATURE_MAIL_NET requires mail wire and TCP sync dial"
#endif

#if defined(XMAIL_FEATURE_MAIL_NET_TLS) && \
	(!defined(XMAIL_FEATURE_MAIL_NET) || \
	 !defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
	 !defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
	 !defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	 !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
	 !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || \
	 !defined(XRT_FEATURE_TLS_RECORD_AES) || \
	 !defined(XRT_FEATURE_TLS_RECORD_CHACHA))
	#error "XMAIL_FEATURE_MAIL_NET_TLS requires mail net, TLS Future" \
		" dial/stream, verified client and the standard TLS profile"
#endif



#if defined(XMAIL_FEATURE_MAIL_NET)

#define XMAIL_NET_HOST_MAX 253u
#define XMAIL_NET_READ_CHUNK_DEFAULT 4096u
#define XMAIL_NET_WRITE_CHUNK_DEFAULT 16384u



/* 安全模式明确区分明文、隐式 TLS 和由协议命令触发的 STARTTLS。 */
typedef enum xmailsecurity {
	XMAIL_SECURITY_PLAIN = 0,
	XMAIL_SECURITY_TLS,
	XMAIL_SECURITY_STARTTLS
} xmailsecurity;



/*
	邮件客户端借用 Engine、Resolver、Host 和可选 TLS 共享对象。
	Dial 控制底层 TCP 拨号和流限制。
*/
typedef struct xmailnetconfig {
	xnetengine* Engine;
	xnetresolver* Resolver;
	cstr Host;
	uint16 Port;
	xmailsecurity Security;
	size_t LineLimit;
	size_t ReadChunk;
	size_t WriteChunk;
	xnetdialconfig Dial;
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		xtlsclientconfig Tls;
		xtlsstreamconfig TlsStream;
		uint64 TlsTimeout;
	#endif
} xmailnetconfig;



XRT_EXTERN_C_BEGIN



/* 初始化有界线路、16 KiB 发送分片和 XRT 默认拨号/TLS 策略。 */
XRT_API void xrtMailNetConfigInit(xmailnetconfig* pConfig);



/* 完整验证主机、端口、安全策略、所有者和缓冲硬边界。 */
XRT_API bool xrtMailNetConfigValid(const xmailnetconfig* pConfig);



XRT_EXTERN_C_END

#endif

#endif
