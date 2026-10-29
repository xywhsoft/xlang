#include "xl_clock.h"
/* SMTP 客户端桥保留底层命令、DATA/BDAT 流式路径和常用认证/提交路径。 */



typedef struct xmail_native_smtp {
	xmail_native_protocol Protocol;
	xsmtpclient* Client;
} xmail_native_smtp;



/* 检查 SMTP 拥有型句柄。 */
static xmail_native_smtp* xmail_native_smtp_require(void* pHandle)
{
	xmail_native_smtp* pSmtp = (xmail_native_smtp*)pHandle;

	if ( (pSmtp == NULL) || (pSmtp->Client == NULL) ) {
		xmail_native_fail(XERR_ARGUMENT, "SMTP client handle is null");
		return NULL;
	}
	return pSmtp;
}



/* 配置并建立 SMTP 会话；resolver 只在本次同步拨号期间存活。 */
void* xmail_smtp_open(
	xnetengine* pEngine,
	cstr sHost,
	size_t iHostSize,
	uint64 iPort,
	uint64 iSecurity,
	cstr sHello,
	size_t iHelloSize,
	uint64 iLineLimit,
	int64 iTimeoutMilliseconds,
	void* pTlsConfig
)
{
	xsmtpclientconfig Config;
	xmail_native_smtp* pSmtp;
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
	pSmtp = (xmail_native_smtp*)xrtCalloc(1u, sizeof(*pSmtp));
	if ( pSmtp == NULL ) {
		return NULL;
	}
	pResolver = xmail_native_protocol_begin(pEngine);
	if ( pResolver == NULL ) {
		xrtFree(pSmtp);
		return NULL;
	}
	pSmtp->Protocol.Engine = pEngine;
	xrtSmtpClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = sHost;
	Config.Net.Port = (uint16)iPort;
	Config.Net.Security = (xmailsecurity)iSecurity;
	Config.Net.LineLimit = (size_t)iLineLimit;
	Config.Hello = xmail_native_view(sHello, iHelloSize);
	if ( !xmail_native_tls_open(pTlsConfig, &Config.Net, &Tls) ) {
		goto failed;
	}
	pSmtp->Client = xrtSmtpClientOpen(&Config, iDeadline, NULL);
	pError = pSmtp->Client == NULL ? xrtTakeError() : NULL;
	xmail_native_tls_close(&Tls);
	bResolverDestroyed = xrtNetResolverDestroy(pResolver);
	if ( !bResolverDestroyed && (pError == NULL) ) {
		pError = xrtTakeError();
	}
	if ( (pSmtp->Client != NULL) && !bResolverDestroyed ) {
		xrtSmtpClientDestroy(pSmtp->Client);
		pSmtp->Client = NULL;
	}
	if ( pSmtp->Client == NULL ) {
		xmail_native_protocol_release(&pSmtp->Protocol);
		xrtFree(pSmtp);
		xmail_native_restore_error(pError);
		return NULL;
	}
	xmail_native_restore_error(pError);
	return pSmtp;

failed:
	pError = xrtTakeError();
	xmail_native_tls_close(&Tls);
	(void)xrtNetResolverDestroy(pResolver);
	xmail_native_protocol_release(&pSmtp->Protocol);
	xrtFree(pSmtp);
	xmail_native_restore_error(pError);
	return NULL;
}



/* point 构造只接管一份 SMTP 包装对象所有权。 */
void* xmail_smtp_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 SMTP 会话；未关闭会话由 xmail 执行异常中止。 */
void xmail_smtp_destroy(void* pHandle)
{
	xmail_native_smtp* pSmtp = (xmail_native_smtp*)pHandle;

	if ( pSmtp == NULL ) {
		return;
	}
	xrtSmtpClientDestroy(pSmtp->Client);
	pSmtp->Client = NULL;
	xmail_native_protocol_release(&pSmtp->Protocol);
	memset(pSmtp, 0, sizeof(*pSmtp));
	xrtFree(pSmtp);
}



/* 返回 SMTP 会话状态。 */
int64 xmail_smtp_state(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);

	return pSmtp != NULL ? (int64)xrtSmtpClientState(pSmtp->Client) : -1;
}



/* 返回最近 EHLO 的能力位。 */
uint64 xmail_smtp_capabilities(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);

	return pSmtp != NULL ? xrtSmtpClientCapabilities(pSmtp->Client) : 0;
}



/* 返回服务器声明的 SIZE 上限。 */
uint64 xmail_smtp_size_limit(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);

	return pSmtp != NULL ? xrtSmtpClientSizeLimit(pSmtp->Client) : 0;
}



/* 返回实际传输安全模式。 */
int64 xmail_smtp_security(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);

	return pSmtp != NULL ? (int64)xrtSmtpClientSecurity(pSmtp->Client) : -1;
}



/* 返回会话是否已认证。 */
bool xmail_smtp_authenticated(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);

	return (pSmtp != NULL) && xrtSmtpClientAuthenticated(pSmtp->Client);
}



/* 返回最近响应码。 */
int64 xmail_smtp_reply_code(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xsmtpreply Reply;

	return (pSmtp != NULL) && xrtSmtpClientLastReply(pSmtp->Client, &Reply) ?
		(int64)Reply.Code : 0;
}



/* 复制最近响应文本。 */
void* xmail_smtp_reply_text(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xsmtpreply Reply;

	if ( (pSmtp == NULL) || !xrtSmtpClientLastReply(pSmtp->Client, &Reply) ) {
		return NULL;
	}
	return xmail_native_copy_text(Reply.Text);
}



/* 发送低层 SMTP 行。 */
bool xmail_smtp_send(
	void* pHandle,
	cstr sLine,
	size_t iLineSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientSend(pSmtp->Client,
			xmail_native_view(sLine, iLineSize), iDeadline, NULL);
}



/* 接收一条完整 SMTP 响应。 */
bool xmail_smtp_receive(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xsmtpreply Reply;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientReceive(pSmtp->Client, &Reply, iDeadline, NULL);
}



/* 发送命令并读取完整响应。 */
bool xmail_smtp_command(
	void* pHandle,
	cstr sVerb,
	size_t iVerbSize,
	cstr sArguments,
	size_t iArgumentsSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	xsmtpreply Reply;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientCommand(pSmtp->Client,
			xmail_native_view(sVerb, iVerbSize),
			xmail_native_view(sArguments, iArgumentsSize),
			&Reply, iDeadline, NULL);
}



/* 开始 SMTP envelope。 */
bool xmail_smtp_mail(
	void* pHandle,
	cstr sPath,
	size_t iPathSize,
	cstr sParameters,
	size_t iParametersSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientMail(pSmtp->Client,
			xmail_native_view(sPath, iPathSize),
			xmail_native_view(sParameters, iParametersSize),
			iDeadline, NULL);
}



/* 增加一个 SMTP envelope 收件人。 */
bool xmail_smtp_rcpt(
	void* pHandle,
	cstr sPath,
	size_t iPathSize,
	cstr sParameters,
	size_t iParametersSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientRcpt(pSmtp->Client,
			xmail_native_view(sPath, iPathSize),
			xmail_native_view(sParameters, iParametersSize),
			iDeadline, NULL);
}



/* 进入 DATA 模式。 */
bool xmail_smtp_data_begin(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientDataBegin(pSmtp->Client, iDeadline, NULL);
}



/* 写入任意 DATA 片段并跨片段保持 dot transparency 状态。 */
bool xmail_smtp_data_write(
	void* pHandle,
	const void* pData,
	size_t iSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xbytesview Data;
	XL6_DeadlineV1 iDeadline;

	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientDataWrite(pSmtp->Client, Data, iDeadline, NULL);
}



/* 文本重载只转发到同一 DATA 分片实现。 */
bool xmail_smtp_data_write_text(
	void* pHandle,
	cstr sData,
	size_t iSize,
	int64 iTimeoutMilliseconds
)
{
	return xmail_smtp_data_write(
		pHandle,
		sData,
		iSize,
		iTimeoutMilliseconds
	);
}



/* 完成 DATA 并读取最终响应。 */
bool xmail_smtp_data_end(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientDataEnd(pSmtp->Client, iDeadline, NULL);
}



/* 直接发送一份连续 DATA 消息视图。 */
bool xmail_smtp_data(
	void* pHandle,
	cstr sMessage,
	size_t iMessageSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientData(pSmtp->Client,
			xmail_native_view(sMessage, iMessageSize), iDeadline, NULL);
}



/* 声明 BDAT 块。 */
bool xmail_smtp_bdat_begin(
	void* pHandle,
	uint64 iSize,
	bool Last,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) && (iSize <= SIZE_MAX) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientBdatBegin(pSmtp->Client,
			(size_t)iSize, Last, iDeadline, NULL);
}



/* 写入当前 BDAT 块。 */
bool xmail_smtp_bdat_write(
	void* pHandle,
	const void* pData,
	size_t iSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xbytesview Data;
	XL6_DeadlineV1 iDeadline;

	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientBdatWrite(pSmtp->Client, Data, iDeadline, NULL);
}



/* 完成当前 BDAT 块。 */
bool xmail_smtp_bdat_end(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientBdatEnd(pSmtp->Client, iDeadline, NULL);
}



/* 零拼接发送一个连续 BDAT 块。 */
bool xmail_smtp_bdat(
	void* pHandle,
	const void* pData,
	size_t iSize,
	bool Last,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xbytesview Data;
	XL6_DeadlineV1 iDeadline;

	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientBdat(pSmtp->Client, Data, Last, iDeadline, NULL);
}



/* 使用内置 SASL 机制认证。 */
bool xmail_smtp_auth(
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
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xsmtpauthconfig Config;
	XL6_DeadlineV1 iDeadline;

	if ( (pSmtp == NULL) || (iMethod > XSMTP_AUTH_OAUTHBEARER) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	xrtSmtpAuthConfigInit(&Config);
	Config.Method = (xsmtpauthmethod)iMethod;
	Config.Username = xmail_native_view(sUsername, iUsernameSize);
	Config.Secret = xmail_native_view(sSecret, iSecretSize);
	Config.AuthorizationId = xmail_native_view(
		sAuthorizationId,
		iAuthorizationIdSize
	);
	Config.InitialResponse = InitialResponse;
	Config.AllowPlaintext = AllowPlaintext;
	return xrtSmtpClientAuth(pSmtp->Client, &Config, iDeadline, NULL);
}



/* 从 xlang 消息对象直接流式提交，不生成整封临时字符串。 */
bool xmail_smtp_submit(
	void* pHandle,
	const xvalue* pValue,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	xmail_native_message Native;
	XL6_DeadlineV1 iDeadline;
	bool bResult;

	if ( (pSmtp == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ||
		!xmail_native_message_map(pValue, &Native) ) {
		return false;
	}
	bResult = xrtSmtpSubmit(pSmtp->Client, &Native.Message, iDeadline, NULL);
	xmail_native_message_free(&Native);
	return bResult;
}



/* 取消当前 SMTP envelope。 */
bool xmail_smtp_reset(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientReset(pSmtp->Client, iDeadline, NULL);
}



/* 验证 SMTP 连接仍可交换命令。 */
bool xmail_smtp_noop(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;

	return (pSmtp != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtSmtpClientNoop(pSmtp->Client, iDeadline, NULL);
}



/* 发送 QUIT 并认证关闭。 */
bool xmail_smtp_quit(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	bool bResult;

	if ( (pSmtp == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	bResult = xrtSmtpClientQuit(pSmtp->Client, iDeadline, NULL);
	if ( bResult ) {
		xmail_native_protocol_release(&pSmtp->Protocol);
	}
	return bResult;
}



/* 不发送 QUIT，正常关闭传输。 */
bool xmail_smtp_close(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	XL6_DeadlineV1 iDeadline;
	bool bResult;

	if ( (pSmtp == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	bResult = xrtSmtpClientClose(pSmtp->Client, iDeadline);
	if ( bResult ) {
		xmail_native_protocol_release(&pSmtp->Protocol);
	}
	return bResult;
}



/* 立即异常中止 SMTP 会话。 */
bool xmail_smtp_abort(void* pHandle)
{
	xmail_native_smtp* pSmtp = xmail_native_smtp_require(pHandle);
	bool bResult;

	if ( pSmtp == NULL ) {
		return false;
	}
	bResult = xrtSmtpClientAbort(pSmtp->Client);
	if ( bResult ) {
		xmail_native_protocol_release(&pSmtp->Protocol);
	}
	return bResult;
}
