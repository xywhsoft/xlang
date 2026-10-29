#ifndef XRT_IMAP_CLIENT_H
#define XRT_IMAP_CLIENT_H

#include <xrt/imap.h>
#include <xrt/mail_net.h>



#if defined(XMAIL_FEATURE_IMAP_CLIENT) && \
	(!defined(XMAIL_FEATURE_IMAP) || !defined(XMAIL_FEATURE_MAIL_NET))
	#error "XMAIL_FEATURE_IMAP_CLIENT requires IMAP and mail net"
#endif

#if defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) && \
	(!defined(XMAIL_FEATURE_IMAP_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_TLS))
	#error "XMAIL_FEATURE_IMAP_CLIENT_TLS requires IMAP client and mail net TLS"
#endif



#if defined(XMAIL_FEATURE_IMAP_CLIENT)

#define XIMAP_CLIENT_TAG_MAX 15u
#define XIMAP_APPEND_LIMIT_UNKNOWN UINT64_MAX



typedef struct ximapclient ximapclient;



/* 会话状态只表达 IMAP 协议层级，不隐藏正在交换的原始命令。 */
typedef enum ximapclientstate {
	XIMAP_CLIENT_NOT_AUTHENTICATED = 0,
	XIMAP_CLIENT_AUTHENTICATED,
	XIMAP_CLIENT_SELECTED,
	XIMAP_CLIENT_CLOSED,
	XIMAP_CLIENT_FAILED
} ximapclientstate;



/* RESPONSE 是新响应首行，FRAGMENT 是服务器 literal 后的同一响应续行。 */
typedef enum ximapeventkind {
	XIMAP_EVENT_RESPONSE = 1,
	XIMAP_EVENT_FRAGMENT
} ximapeventkind;



/* 客户端只在 Open 期间借用配置；默认连接明文 143 端口。 */
typedef struct ximapclientconfig {
	xmailnetconfig Net;
	size_t CommandLineLimit;
} ximapclientconfig;



/*
	事件中的所有视图借用客户端线路缓冲；下一次 Receive、Next 或 ReadLiteral
	调用后失效。Literal 只在 HasLiteral 为真时有效。
*/
typedef struct ximapevent {
	xstrview Source;
	ximapresponseview Response;
	ximapliteralview Literal;
	ximapeventkind Kind;
	bool HasLiteral;
} ximapevent;



XRT_EXTERN_C_BEGIN



/* 初始化 IMAP 143 端口、64 KiB 命令/响应行和 XRT 网络默认值。 */
XRT_API void xrtImapClientConfigInit(ximapclientconfig* pConfig);



/* 验证网络所有者、线路上限和命令上限。 */
XRT_API bool xrtImapClientConfigValid(const ximapclientconfig* pConfig);



/* 建立连接，读取 greeting，完成 CAPABILITY 和可选 STARTTLS。 */
XRT_API ximapclient* xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 返回当前 IMAP 协议层级或终态。 */
XRT_API ximapclientstate xrtImapClientState(const ximapclient* pClient);



/* 返回最近一次 CAPABILITY 快照中的已知能力位。 */
XRT_API uint64 xrtImapClientCapabilities(const ximapclient* pClient);



/* 返回当前会话实际使用的明文或 TLS 传输安全级别。 */
XRT_API xmailsecurity xrtImapClientSecurity(const ximapclient* pClient);



/* 返回顺序命令当前使用的自动 tag；没有活动命令时返回空视图。 */
XRT_API xstrview xrtImapClientTag(const ximapclient* pClient);



/* 返回当前服务器 literal 尚未读取的字节数。 */
XRT_API size_t xrtImapClientLiteralRemaining(const ximapclient* pClient);



/* 返回当前命令行总长度上限。 */
XRT_API size_t xrtImapClientCommandLimit(const ximapclient* pClient);



/* 返回 CAPABILITY 声明的全局 APPEND 上限，未知或按邮箱决定时返回 UNKNOWN。 */
XRT_API uint64 xrtImapClientAppendLimit(const ximapclient* pClient);



/* 返回最近 greeting 或 tagged completion 的稳定借用视图。 */
XRT_API bool xrtImapClientLastResponse(
	const ximapclient* pClient,
	ximapresponseview* pResponse
);



/*
	直接发送显式 tag 的完整命令行，不创建临时拼接缓冲。
	低层调用方可连续发送多个命令，并用 Receive 按 tag 关联完成响应。
*/
XRT_API bool xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
);



/*
	直接发送由独立参数片组成的显式 tag 命令；各片之间自动插入一个空格，
	不创建参数拼接缓冲。空参数数组表示无参数命令。
*/
XRT_API bool xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送原始命令续传字节，用于 APPEND literal 等协议扩展。 */
XRT_API bool xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送一条不带 tag 的 continuation 数据并自动追加 CRLF。 */
XRT_API bool xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	xdeadline iDeadline,
	xcancel* pCancel
);



/*
	读取下一个响应首行或 literal 后续片段；遇到 literal 后必须先读取全部
	literal，才能继续接收下一事件。
*/
XRT_API bool xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 把当前 literal 的下一段读入调用方缓冲区，Read 返回实际读取量。 */
XRT_API bool xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 生成唯一 tag 并开始一个顺序命令。 */
XRT_API bool xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 生成唯一 tag 并以零拼接方式开始多参数顺序命令。 */
XRT_API bool xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
);



/*
	返回顺序命令的下一事件；匹配 tag 的 completion 返回 END，其他响应返回
	ITEM，传输、协议或状态错误返回 ERROR。
*/
XRT_API xmailnext xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 在没有活动顺序命令时重新获取并替换 CAPABILITY 快照。 */
XRT_API bool xrtImapClientRefresh(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送 LOGOUT，消费 BYE 和 tagged completion，并正常关闭传输。 */
XRT_API bool xrtImapClientLogout(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 不发送 LOGOUT，直接正常关闭传输。 */
XRT_API bool xrtImapClientClose(
	ximapclient* pClient,
	xdeadline iDeadline
);



/* 立即异常中止连接；重复调用成功，FAILED 状态保留到销毁。 */
XRT_API bool xrtImapClientAbort(ximapclient* pClient);



/* 释放客户端；尚未关闭时执行异常中止。 */
XRT_API void xrtImapClientDestroy(ximapclient* pClient);



XRT_EXTERN_C_END

#endif

#endif
