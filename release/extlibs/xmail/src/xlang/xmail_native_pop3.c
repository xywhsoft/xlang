#include "xl_clock.h"
/* POP3 客户端桥保留低层线路、多行流和常用邮箱命令。 */



typedef struct xmail_native_pop3 {
	xmail_native_protocol Protocol;
	xpop3client* Client;
	__xmailtext LastLine;
	uint64 First;
	uint64 Second;
} xmail_native_pop3;



/* 检查 POP3 拥有型句柄。 */
static xmail_native_pop3* xmail_native_pop3_require(void* pHandle)
{
	xmail_native_pop3* pPop3 = (xmail_native_pop3*)pHandle;

	if ( (pPop3 == NULL) || (pPop3->Client == NULL) ) {
		xmail_native_fail(XERR_ARGUMENT, "POP3 client handle is null");
		return NULL;
	}
	return pPop3;
}



/* 保存最近一次多行项目，避免把客户端借用缓冲泄漏给语言层。 */
static bool xmail_native_pop3_line_set(
	xmail_native_pop3* pPop3,
	xstrview Line
)
{
	if ( pPop3 == NULL ) {
		return false;
	}
	return __xrtMailTextSet(&pPop3->LastLine, Line);
}



/* 配置并建立 POP3 会话。 */
void* xmail_pop3_open(
	xnetengine* pEngine,
	cstr sHost,
	size_t iHostSize,
	uint64 iPort,
	uint64 iSecurity,
	bool ReadCapabilities,
	uint64 iLineLimit,
	int64 iTimeoutMilliseconds,
	void* pTlsConfig
)
{
	xpop3clientconfig Config;
	xmail_native_pop3* pPop3;
	xnetresolver* pResolver;
	xmail_native_tls Tls;
	XL6_DeadlineV1 iDeadline;
	xerror* pError;
	bool bResolverDestroyed;

	xrtClearError();
	if ( !xmail_native_host(sHost, iHostSize) || (iPort == 0) ||
		(iPort > UINT16_MAX) || (iSecurity > XMAIL_SECURITY_STARTTLS) ||
		(iLineLimit > SIZE_MAX) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return NULL;
	}
	pPop3 = (xmail_native_pop3*)xrtCalloc(1u, sizeof(*pPop3));
	if ( pPop3 == NULL ) {
		return NULL;
	}
	pResolver = xmail_native_protocol_begin(pEngine);
	if ( pResolver == NULL ) {
		xrtFree(pPop3);
		return NULL;
	}
	pPop3->Protocol.Engine = pEngine;
	xrtPop3ClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = sHost;
	Config.Net.Port = (uint16)iPort;
	Config.Net.Security = (xmailsecurity)iSecurity;
	Config.Net.LineLimit = (size_t)iLineLimit;
	Config.ReadCapabilities = ReadCapabilities;
	if ( !xmail_native_tls_open(pTlsConfig, &Config.Net, &Tls) ) {
		goto failed;
	}
	pPop3->Client = xrtPop3ClientOpen(&Config, iDeadline, NULL);
	pError = pPop3->Client == NULL ? xrtTakeError() : NULL;
	xmail_native_tls_close(&Tls);
	bResolverDestroyed = xrtNetResolverDestroy(pResolver);
	if ( !bResolverDestroyed && (pError == NULL) ) {
		pError = xrtTakeError();
	}
	if ( (pPop3->Client != NULL) && !bResolverDestroyed ) {
		xrtPop3ClientDestroy(pPop3->Client);
		pPop3->Client = NULL;
	}
	if ( pPop3->Client == NULL ) {
		xmail_native_protocol_release(&pPop3->Protocol);
		xrtFree(pPop3);
		xmail_native_restore_error(pError);
		return NULL;
	}
	xmail_native_restore_error(pError);
	return pPop3;

failed:
	pError = xrtTakeError();
	xmail_native_tls_close(&Tls);
	(void)xrtNetResolverDestroy(pResolver);
	xmail_native_protocol_release(&pPop3->Protocol);
	xrtFree(pPop3);
	xmail_native_restore_error(pError);
	return NULL;
}



/* point 构造只接管一份 POP3 包装对象所有权。 */
void* xmail_pop3_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 POP3 会话和最近项目副本。 */
void xmail_pop3_destroy(void* pHandle)
{
	xmail_native_pop3* pPop3 = (xmail_native_pop3*)pHandle;

	if ( pPop3 == NULL ) {
		return;
	}
	xrtPop3ClientDestroy(pPop3->Client);
	pPop3->Client = NULL;
	__xrtMailTextDestroy(&pPop3->LastLine);
	xmail_native_protocol_release(&pPop3->Protocol);
	memset(pPop3, 0, sizeof(*pPop3));
	xrtFree(pPop3);
}



/* 返回 POP3 会话状态。 */
int64 xmail_pop3_state(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);

	return pPop3 != NULL ? (int64)xrtPop3ClientState(pPop3->Client) : -1;
}



/* 返回最近 CAPA 的能力位。 */
uint64 xmail_pop3_capabilities(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);

	return pPop3 != NULL ? xrtPop3ClientCapabilities(pPop3->Client) : 0;
}



/* 返回最近 CAPA 的 SASL 机制位。 */
uint64 xmail_pop3_sasl(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);

	return pPop3 != NULL ? xrtPop3ClientSaslMechanisms(pPop3->Client) : 0;
}



/* 返回实际传输安全模式。 */
int64 xmail_pop3_security(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);

	return pPop3 != NULL ? (int64)xrtPop3ClientSecurity(pPop3->Client) : -1;
}



/* 返回最近状态行是否成功。 */
bool xmail_pop3_reply_ok(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	xpop3reply Reply;

	return (pPop3 != NULL) &&
		xrtPop3ClientLastReply(pPop3->Client, &Reply) && Reply.Ok;
}



/* 复制最近状态行正文。 */
void* xmail_pop3_reply_text(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	xpop3reply Reply;

	if ( (pPop3 == NULL) || !xrtPop3ClientLastReply(pPop3->Client, &Reply) ) {
		return NULL;
	}
	return xmail_native_copy_text(Reply.Text);
}



/* 复制最近完整状态行。 */
void* xmail_pop3_reply_source(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	xpop3reply Reply;

	if ( (pPop3 == NULL) || !xrtPop3ClientLastReply(pPop3->Client, &Reply) ) {
		return NULL;
	}
	return xmail_native_copy_text(Reply.Source);
}



/* 发送一条低层 POP3 行。 */
bool xmail_pop3_send(
	void* pHandle,
	cstr sLine,
	size_t iLineSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientSend(pPop3->Client,
			xmail_native_view(sLine, iLineSize), iDeadline, NULL);
}



/* 读取一条原始线路并保存稳定副本。 */
bool xmail_pop3_line(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xstrview Line;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientLine(pPop3->Client, &Line, iDeadline, NULL) &&
		xmail_native_pop3_line_set(pPop3, Line);
}



/* 接收一条 POP3 状态行。 */
bool xmail_pop3_receive(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xpop3reply Reply;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientReceive(pPop3->Client, &Reply, iDeadline, NULL);
}



/* 发送 POP3 命令并读取状态行。 */
bool xmail_pop3_command(
	void* pHandle,
	cstr sVerb,
	size_t iVerbSize,
	cstr sArguments,
	size_t iArgumentsSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xpop3reply Reply;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientCommand(pPop3->Client,
			xmail_native_view(sVerb, iVerbSize),
			xmail_native_view(sArguments, iArgumentsSize),
			&Reply, iDeadline, NULL);
}



/* 开始一条多行 POP3 命令。 */
bool xmail_pop3_begin(
	void* pHandle,
	cstr sVerb,
	size_t iVerbSize,
	cstr sArguments,
	size_t iArgumentsSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientBegin(pPop3->Client,
			xmail_native_view(sVerb, iVerbSize),
			xmail_native_view(sArguments, iArgumentsSize),
			iDeadline, NULL);
}



/* 读取多行响应的下一项并保存稳定副本。 */
int64 xmail_pop3_next(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xstrview Line;
	xmailnext Next;

	if ( (pPop3 == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtPop3ClientNext(pPop3->Client, &Line, iDeadline, NULL);
	if ( (Next == XMAIL_NEXT_ITEM) &&
		!xmail_native_pop3_line_set(pPop3, Line) ) {
		return XMAIL_NEXT_ERROR;
	}
	return (int64)Next;
}



/* 复制最近一条原始或多行项目。 */
void* xmail_pop3_last_line(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	xstrview Line;

	if ( pPop3 == NULL ) {
		return NULL;
	}
	Line.Data = pPop3->LastLine.Data;
	Line.Size = pPop3->LastLine.Size;
	return xmail_native_copy_text(Line);
}



/* 查询邮箱统计并保存两个 64 位结果。 */
bool xmail_pop3_stat(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xpop3stat Stat;

	if ( (pPop3 == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ||
		!xrtPop3ClientStat(pPop3->Client, &Stat, iDeadline, NULL) ) {
		return false;
	}
	pPop3->First = Stat.Messages;
	pPop3->Second = Stat.Bytes;
	return true;
}



/* 查询单条 LIST 项并保存序号和字节数。 */
bool xmail_pop3_list(
	void* pHandle,
	uint64 iMessage,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xpop3listview Item;

	if ( (pPop3 == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ||
		!xrtPop3ClientList(pPop3->Client,
			iMessage, &Item, iDeadline, NULL) ) {
		return false;
	}
	pPop3->First = Item.Message;
	pPop3->Second = Item.Bytes;
	return true;
}



/* 查询单条 UIDL 项并保存序号和 ID。 */
bool xmail_pop3_uidl(
	void* pHandle,
	uint64 iMessage,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xpop3uidlview Item;

	if ( (pPop3 == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ||
		!xrtPop3ClientUidl(pPop3->Client,
			iMessage, &Item, iDeadline, NULL) ||
		!xmail_native_pop3_line_set(pPop3, Item.Id) ) {
		return false;
	}
	pPop3->First = Item.Message;
	return true;
}



/* 返回最近复合命令的第一个整数结果。 */
uint64 xmail_pop3_first(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);

	return pPop3 != NULL ? pPop3->First : 0;
}



/* 返回最近复合命令的第二个整数结果。 */
uint64 xmail_pop3_second(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);

	return pPop3 != NULL ? pPop3->Second : 0;
}



/* 开始读取全部 LIST 项。 */
bool xmail_pop3_list_all(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientListAll(pPop3->Client, iDeadline, NULL);
}



/* 开始读取全部 UIDL 项。 */
bool xmail_pop3_uidl_all(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientUidlAll(pPop3->Client, iDeadline, NULL);
}



/* 开始流式读取完整邮件。 */
bool xmail_pop3_retr(
	void* pHandle,
	uint64 iMessage,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientRetr(pPop3->Client, iMessage, iDeadline, NULL);
}



/* 开始流式读取邮件字段和部分正文。 */
bool xmail_pop3_top(
	void* pHandle,
	uint64 iMessage,
	uint64 iLines,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientTop(pPop3->Client,
			iMessage, iLines, iDeadline, NULL);
}



/* 使用内置 USER/PASS 或 SASL 机制认证。 */
bool xmail_pop3_auth(
	void* pHandle,
	uint64 iMethod,
	cstr sUsername,
	size_t iUsernameSize,
	cstr sSecret,
	size_t iSecretSize,
	cstr sAuthorizationId,
	size_t iAuthorizationIdSize,
	bool InitialResponse,
	bool AllowPlaintext,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	xpop3authconfig Config;
	XL6_DeadlineV1 iDeadline;

	if ( (pPop3 == NULL) || (iMethod > XPOP3_AUTH_OAUTHBEARER) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	xrtPop3AuthConfigInit(&Config);
	Config.Method = (xpop3authmethod)iMethod;
	Config.Username = xmail_native_view(sUsername, iUsernameSize);
	Config.Secret = xmail_native_view(sSecret, iSecretSize);
	Config.AuthorizationId = xmail_native_view(
		sAuthorizationId,
		iAuthorizationIdSize
	);
	Config.InitialResponse = InitialResponse;
	Config.AllowPlaintext = AllowPlaintext;
	return xrtPop3ClientAuth(pPop3->Client, &Config, iDeadline, NULL);
}



/* 标记一条消息待删除。 */
bool xmail_pop3_delete(
	void* pHandle,
	uint64 iMessage,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientDelete(pPop3->Client, iMessage, iDeadline, NULL);
}



/* 清除会话删除标记。 */
bool xmail_pop3_reset(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientReset(pPop3->Client, iDeadline, NULL);
}



/* 验证 POP3 连接仍可交换命令。 */
bool xmail_pop3_noop(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pPop3 != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtPop3ClientNoop(pPop3->Client, iDeadline, NULL);
}



/* 发送 QUIT 并在成功后释放 Engine 占用。 */
bool xmail_pop3_quit(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	bool bResult;

	if ( (pPop3 == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	bResult = xrtPop3ClientQuit(pPop3->Client, iDeadline, NULL);
	if ( bResult ) {
		xmail_native_protocol_release(&pPop3->Protocol);
	}
	return bResult;
}



/* 正常关闭 POP3 传输。 */
bool xmail_pop3_close(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	bool bResult;

	if ( (pPop3 == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	bResult = xrtPop3ClientClose(pPop3->Client, iDeadline);
	if ( bResult ) {
		xmail_native_protocol_release(&pPop3->Protocol);
	}
	return bResult;
}



/* 立即异常中止 POP3 会话。 */
bool xmail_pop3_abort(void* pHandle)
{
	xmail_native_pop3* pPop3 = xmail_native_pop3_require(pHandle);
	bool bResult;

	if ( pPop3 == NULL ) {
		return false;
	}
	bResult = xrtPop3ClientAbort(pPop3->Client);
	if ( bResult ) {
		xmail_native_protocol_release(&pPop3->Protocol);
	}
	return bResult;
}
