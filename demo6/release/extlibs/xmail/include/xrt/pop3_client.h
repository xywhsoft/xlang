#ifndef XRT_POP3_CLIENT_H
#define XRT_POP3_CLIENT_H

#include <xrt/mail_net.h>
#include <xrt/pop3.h>



#if defined(XMAIL_FEATURE_POP3_CLIENT) && \
	(!defined(XMAIL_FEATURE_POP3) || !defined(XMAIL_FEATURE_MAIL_NET))
	#error "XMAIL_FEATURE_POP3_CLIENT requires POP3 and mail net"
#endif

#if defined(XMAIL_FEATURE_POP3_CLIENT_TLS) && \
	(!defined(XMAIL_FEATURE_POP3_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_TLS))
	#error "XMAIL_FEATURE_POP3_CLIENT_TLS requires POP3 client and mail net TLS"
#endif



#if defined(XMAIL_FEATURE_POP3_CLIENT)

typedef struct xpop3client xpop3client;



/* POP3 状态显式区分认证、事务和未消费完的多行响应。 */
typedef enum xpop3clientstate {
	XPOP3_CLIENT_AUTHORIZATION = 0,
	XPOP3_CLIENT_TRANSACTION,
	XPOP3_CLIENT_MULTILINE,
	XPOP3_CLIENT_UPDATE,
	XPOP3_CLIENT_CLOSED,
	XPOP3_CLIENT_FAILED
} xpop3clientstate;



/* 配置借用共享网络对象，只在 Open 期间读取。 */
typedef struct xpop3clientconfig {
	xmailnetconfig Net;
	bool ReadCapabilities;
} xpop3clientconfig;



/* 最后状态行借用 Client，下一次 Receive 或销毁后失效。 */
typedef struct xpop3reply {
	bool Ok;
	xstrview Source;
	xstrview Text;
} xpop3reply;



XRT_EXTERN_C_BEGIN



/* 初始化明文 110 端口并默认读取 CAPA。 */
XRT_API void xrtPop3ClientConfigInit(xpop3clientconfig* pConfig);



/* 验证网络配置和 STLS 所需的能力读取设置。 */
XRT_API bool xrtPop3ClientConfigValid(const xpop3clientconfig* pConfig);



/* 建立连接，验证 greeting，读取 CAPA 并按需完成 STLS。 */
XRT_API xpop3client* xrtPop3ClientOpen(
	const xpop3clientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 返回当前 POP3 状态。 */
XRT_API xpop3clientstate xrtPop3ClientState(const xpop3client* pClient);



/* 返回最近一次 CAPA 的已知能力位集。 */
XRT_API uint32 xrtPop3ClientCapabilities(const xpop3client* pClient);



/* 返回最近一次 CAPA 中 SASL 参数列出的已知认证机制。 */
XRT_API uint32 xrtPop3ClientSaslMechanisms(const xpop3client* pClient);



/* 返回当前会话实际使用的传输安全级别。 */
XRT_API xmailsecurity xrtPop3ClientSecurity(const xpop3client* pClient);



/* 取得最近状态行的稳定借用视图。 */
XRT_API bool xrtPop3ClientLastReply(
	const xpop3client* pClient,
	xpop3reply* pReply
);



/* 发送一条不含 CRLF 的低层 POP3 行。 */
XRT_API bool xrtPop3ClientSend(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送可超过普通命令上限的 SASL 响应或取消行。 */
XRT_API bool xrtPop3ClientAuthLine(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取一条不含 CRLF 的原始线路，用于 SASL continuation 等扩展。 */
XRT_API bool xrtPop3ClientLine(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取并验证一条 +OK 或 -ERR 状态行。 */
XRT_API bool xrtPop3ClientReceive(
	xpop3client* pClient,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 构建并发送命令，然后读取状态行。 */
XRT_API bool xrtPop3ClientCommand(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送预期多行数据的命令；+OK 后进入 MULTILINE。 */
XRT_API bool xrtPop3ClientBegin(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 逐行去除 dot transparency；END 自动恢复命令前状态。 */
XRT_API xmailnext xrtPop3ClientNext(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 查询邮箱消息数量和总字节数。 */
XRT_API bool xrtPop3ClientStat(
	xpop3client* pClient,
	xpop3stat* pStat,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 查询一条消息的 LIST 大小。 */
XRT_API bool xrtPop3ClientList(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3listview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取全部 LIST 项。 */
XRT_API bool xrtPop3ClientListAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 查询一条消息的 UIDL。 */
XRT_API bool xrtPop3ClientUidl(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3uidlview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取全部 UIDL 项。 */
XRT_API bool xrtPop3ClientUidlAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取一封完整邮件。 */
XRT_API bool xrtPop3ClientRetr(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取邮件字段和指定数量的正文行。 */
XRT_API bool xrtPop3ClientTop(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 标记一条消息在 UPDATE 阶段删除。 */
XRT_API bool xrtPop3ClientDelete(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 清除当前会话的删除标记。 */
XRT_API bool xrtPop3ClientReset(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 验证事务连接仍可交换命令。 */
XRT_API bool xrtPop3ClientNoop(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送 QUIT，验证成功状态并认证关闭传输。 */
XRT_API bool xrtPop3ClientQuit(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 不发送 QUIT，直接正常关闭传输。 */
XRT_API bool xrtPop3ClientClose(
	xpop3client* pClient,
	xdeadline iDeadline
);



/* 立即异常中止连接；重复调用成功，FAILED 状态保留到销毁。 */
XRT_API bool xrtPop3ClientAbort(xpop3client* pClient);



/* 释放客户端；尚未关闭时执行异常中止。 */
XRT_API void xrtPop3ClientDestroy(xpop3client* pClient);



XRT_EXTERN_C_END

#endif

#endif
