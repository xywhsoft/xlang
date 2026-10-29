#ifndef XRT_SMTP_CLIENT_H
#define XRT_SMTP_CLIENT_H

#include <xrt/mail_net.h>
#include <xrt/smtp.h>



#if defined(XMAIL_FEATURE_SMTP_CLIENT) && \
	(!defined(XMAIL_FEATURE_SMTP) || !defined(XMAIL_FEATURE_MAIL_NET))
	#error "XMAIL_FEATURE_SMTP_CLIENT requires SMTP and mail net"
#endif

#if defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) && \
	(!defined(XMAIL_FEATURE_SMTP_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_TLS))
	#error "XMAIL_FEATURE_SMTP_CLIENT_TLS requires SMTP client and mail net TLS"
#endif



#if defined(XMAIL_FEATURE_SMTP_CLIENT)

#define XSMTP_HELLO_MAX 255u



typedef struct xsmtpclient xsmtpclient;



/* SMTP 会话状态明确区分事务阶段和不可恢复的传输失败。 */
typedef enum xsmtpclientstate {
	XSMTP_CLIENT_READY = 0,
	XSMTP_CLIENT_MAIL,
	XSMTP_CLIENT_RECIPIENT,
	XSMTP_CLIENT_DATA,
	XSMTP_CLIENT_CHUNK,
	XSMTP_CLIENT_CLOSED,
	XSMTP_CLIENT_FAILED
} xsmtpclientstate;



/* 客户端借用网络配置和 EHLO 名称，只在 Open 调用期间读取。 */
typedef struct xsmtpclientconfig {
	xmailnetconfig Net;
	xstrview Hello;
	size_t ReplyLines;
	bool HeloFallback;
} xsmtpclientconfig;



/* 最终响应行摘要借用 Client，下一次 Receive 或销毁 Client 后失效。 */
typedef struct xsmtpreply {
	int Code;
	size_t Lines;
	xstrview Text;
} xsmtpreply;



XRT_EXTERN_C_BEGIN



/* 初始化明文 25 端口、localhost EHLO 和有界响应配置。 */
XRT_API void xrtSmtpClientConfigInit(xsmtpclientconfig* pConfig);



/* 验证网络所有者、EHLO 名称和响应行上限。 */
XRT_API bool xrtSmtpClientConfigValid(const xsmtpclientconfig* pConfig);



/* 建立连接，验证 220 banner 并完成 EHLO/可选 HELO 和 STARTTLS。 */
XRT_API xsmtpclient* xrtSmtpClientOpen(
	const xsmtpclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 返回当前事务或终态。 */
XRT_API xsmtpclientstate xrtSmtpClientState(const xsmtpclient* pClient);



/* 返回最近一次 EHLO 得到的已知能力位集。 */
XRT_API uint64 xrtSmtpClientCapabilities(const xsmtpclient* pClient);



/* 返回服务器声明的 SIZE；零表示未声明或未给出上限。 */
XRT_API uint64 xrtSmtpClientSizeLimit(const xsmtpclient* pClient);



/* 返回当前会话实际使用的明文或 TLS 传输安全级别。 */
XRT_API xmailsecurity xrtSmtpClientSecurity(const xsmtpclient* pClient);



/* 返回当前会话是否已成功完成 SMTP AUTH。 */
XRT_API bool xrtSmtpClientAuthenticated(const xsmtpclient* pClient);



/* 取得最近一条完整响应的稳定借用摘要。 */
XRT_API bool xrtSmtpClientLastReply(
	const xsmtpclient* pClient,
	xsmtpreply* pReply
);



/* 发送一条不含 CRLF 的低层 SMTP 行。 */
XRT_API bool xrtSmtpClientSend(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送预编码的 SASL 响应行；允许认证协议要求的扩展行长。 */
XRT_API bool xrtSmtpClientAuthLine(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取并验证一条完整单行或多行响应。 */
XRT_API bool xrtSmtpClientReceive(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 构建并发送命令，然后读取完整响应。 */
XRT_API bool xrtSmtpClientCommand(
	xsmtpclient* pClient,
	xstrview Verb,
	xstrview Arguments,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始新 envelope；空 ReversePath 表示标准空反向路径。 */
XRT_API bool xrtSmtpClientMail(
	xsmtpclient* pClient,
	xstrview ReversePath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 增加一个收件人；接受 250、251 和 252 响应。 */
XRT_API bool xrtSmtpClientRcpt(
	xsmtpclient* pClient,
	xstrview ForwardPath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 进入 DATA 模式并验证 354 响应。 */
XRT_API bool xrtSmtpClientDataBegin(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送任意分块边界的消息片段，并跨片段执行 dot transparency。 */
XRT_API bool xrtSmtpClientDataWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 补足 DATA 终止行、读取最终响应并回到 READY。 */
XRT_API bool xrtSmtpClientDataEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 直接流式发送完整消息视图，执行 dot transparency 而不复制整份报文。 */
XRT_API bool xrtSmtpClientData(
	xsmtpclient* pClient,
	xstrview Message,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 声明一个 BDAT 块并直接发送命令头；随后必须精确写入 iChunkSize 个字节。 */
XRT_API bool xrtSmtpClientBdatBegin(
	xsmtpclient* pClient,
	size_t iChunkSize,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 写入当前 BDAT 块的任意片段；不扫描、不转义且不追加线路分隔符。 */
XRT_API bool xrtSmtpClientBdatWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 验证块长度、读取 250 响应，并进入下一块或结束当前 envelope。 */
XRT_API bool xrtSmtpClientBdatEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 零副本发送一个连续 BDAT 块；服务器拒绝后必须 RSET 才能继续。 */
XRT_API bool xrtSmtpClientBdat(
	xsmtpclient* pClient,
	xbytesview Data,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 取消当前 envelope 并回到 READY。 */
XRT_API bool xrtSmtpClientReset(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 验证连接仍可交换命令。 */
XRT_API bool xrtSmtpClientNoop(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送 QUIT、验证 221 并认证关闭传输。 */
XRT_API bool xrtSmtpClientQuit(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 不发送 QUIT，直接正常关闭传输。 */
XRT_API bool xrtSmtpClientClose(
	xsmtpclient* pClient,
	xdeadline iDeadline
);



/* 立即异常中止连接；重复调用成功，FAILED 状态保留到销毁。 */
XRT_API bool xrtSmtpClientAbort(xsmtpclient* pClient);



/* 释放客户端；尚未关闭时执行异常中止。 */
XRT_API void xrtSmtpClientDestroy(xsmtpclient* pClient);



XRT_EXTERN_C_END

#endif

#endif
