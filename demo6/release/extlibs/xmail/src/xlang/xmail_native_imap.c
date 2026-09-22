/* IMAP 客户端桥以事件和 literal 流为核心，不聚合整条命令结果。 */



typedef struct xmail_native_imap {
	xmail_native_protocol Protocol;
	ximapclient* Client;
	__xmailtext Source;
	__xmailtext Tag;
	__xmailtext Text;
	ximapmailboxinfo Mailbox;
	uint64 LiteralSize;
	int64 EventKind;
	int64 ResponseKind;
	int64 Status;
	bool HasLiteral;
	bool LiteralNonSynchronizing;
	bool LiteralBinary;
} xmail_native_imap;



/* 检查 IMAP 拥有型句柄。 */
static xmail_native_imap* xmail_native_imap_require(void* pHandle)
{
	xmail_native_imap* pImap = (xmail_native_imap*)pHandle;

	if ( (pImap == NULL) || (pImap->Client == NULL) ) {
		xmail_native_fail(XERR_ARGUMENT, "IMAP client handle is null");
		return NULL;
	}
	return pImap;
}



/* 保存借用响应为可跨语言调用稳定读取的事件摘要。 */
static bool xmail_native_imap_event_set(
	xmail_native_imap* pImap,
	const ximapevent* pEvent
)
{
	if ( (pImap == NULL) || (pEvent == NULL) ||
		!__xrtMailTextSet(&pImap->Source, pEvent->Source) ||
		!__xrtMailTextSet(&pImap->Tag, pEvent->Response.Tag) ||
		!__xrtMailTextSet(&pImap->Text, pEvent->Response.Text) ) {
		return false;
	}
	pImap->EventKind = (int64)pEvent->Kind;
	pImap->ResponseKind = (int64)pEvent->Response.Kind;
	pImap->Status = (int64)pEvent->Response.Status;
	pImap->HasLiteral = pEvent->HasLiteral;
	pImap->LiteralSize = pEvent->HasLiteral ? (uint64)pEvent->Literal.Size : 0;
	pImap->LiteralNonSynchronizing = pEvent->HasLiteral &&
		pEvent->Literal.NonSynchronizing;
	pImap->LiteralBinary = pEvent->HasLiteral && pEvent->Literal.Binary;
	return true;
}



/* 配置并建立 IMAP 会话。 */
void* xmail_imap_open(
	xnetengine* pEngine,
	cstr sHost,
	size_t iHostSize,
	uint64 iPort,
	uint64 iSecurity,
	uint64 iLineLimit,
	uint64 iCommandLineLimit,
	int64 iTimeoutMilliseconds,
	void* pTlsConfig
)
{
	ximapclientconfig Config;
	xmail_native_imap* pImap;
	xnetresolver* pResolver;
	xmail_native_tls Tls;
	xdeadline iDeadline;
	xerror* pError;
	bool bResolverDestroyed;

	xrtClearError();
	if ( !xmail_native_host(sHost, iHostSize) || (iPort == 0) ||
		(iPort > UINT16_MAX) || (iSecurity > XMAIL_SECURITY_STARTTLS) ||
		(iLineLimit > SIZE_MAX) || (iCommandLineLimit > SIZE_MAX) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return NULL;
	}
	pImap = (xmail_native_imap*)xrtCalloc(1u, sizeof(*pImap));
	if ( pImap == NULL ) {
		return NULL;
	}
	pResolver = xmail_native_protocol_begin(pEngine);
	if ( pResolver == NULL ) {
		xrtFree(pImap);
		return NULL;
	}
	pImap->Protocol.Engine = pEngine;
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = sHost;
	Config.Net.Port = (uint16)iPort;
	Config.Net.Security = (xmailsecurity)iSecurity;
	Config.Net.LineLimit = (size_t)iLineLimit;
	Config.CommandLineLimit = (size_t)iCommandLineLimit;
	if ( !xmail_native_tls_open(pTlsConfig, &Config.Net, &Tls) ) {
		goto failed;
	}
	pImap->Client = xrtImapClientOpen(&Config, iDeadline, NULL);
	pError = pImap->Client == NULL ? xrtTakeError() : NULL;
	xmail_native_tls_close(&Tls);
	bResolverDestroyed = xrtNetResolverDestroy(pResolver);
	if ( !bResolverDestroyed && (pError == NULL) ) {
		pError = xrtTakeError();
	}
	if ( (pImap->Client != NULL) && !bResolverDestroyed ) {
		xrtImapClientDestroy(pImap->Client);
		pImap->Client = NULL;
	}
	if ( pImap->Client == NULL ) {
		xmail_native_protocol_release(&pImap->Protocol);
		xrtFree(pImap);
		xmail_native_restore_error(pError);
		return NULL;
	}
	xrtImapMailboxInfoInit(&pImap->Mailbox);
	xmail_native_restore_error(pError);
	return pImap;

failed:
	pError = xrtTakeError();
	xmail_native_tls_close(&Tls);
	(void)xrtNetResolverDestroy(pResolver);
	xmail_native_protocol_release(&pImap->Protocol);
	xrtFree(pImap);
	xmail_native_restore_error(pError);
	return NULL;
}



/* point 构造只接管一份 IMAP 包装对象所有权。 */
void* xmail_imap_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 IMAP 会话和事件副本。 */
void xmail_imap_destroy(void* pHandle)
{
	xmail_native_imap* pImap = (xmail_native_imap*)pHandle;

	if ( pImap == NULL ) {
		return;
	}
	xrtImapClientDestroy(pImap->Client);
	pImap->Client = NULL;
	__xrtMailTextDestroy(&pImap->Source);
	__xrtMailTextDestroy(&pImap->Tag);
	__xrtMailTextDestroy(&pImap->Text);
	xmail_native_protocol_release(&pImap->Protocol);
	memset(pImap, 0, sizeof(*pImap));
	xrtFree(pImap);
}



/* 返回 IMAP 会话状态。 */
int64 xmail_imap_state(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ? (int64)xrtImapClientState(pImap->Client) : -1;
}



/* 返回最近 CAPABILITY 的能力位。 */
uint64 xmail_imap_capabilities(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ? xrtImapClientCapabilities(pImap->Client) : 0;
}



/* 返回实际传输安全模式。 */
int64 xmail_imap_security(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ? (int64)xrtImapClientSecurity(pImap->Client) : -1;
}



/* 返回当前 literal 剩余字节数。 */
uint64 xmail_imap_literal_remaining(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ?
		(uint64)xrtImapClientLiteralRemaining(pImap->Client) : 0;
}



/* 返回命令行上限。 */
uint64 xmail_imap_command_limit(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ?
		(uint64)xrtImapClientCommandLimit(pImap->Client) : 0;
}



/* 返回服务器声明的 APPEND 上限。 */
uint64 xmail_imap_append_limit(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ? xrtImapClientAppendLimit(pImap->Client) : 0;
}



/* 复制当前自动 tag。 */
void* xmail_imap_tag(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ?
		xmail_native_copy_text(xrtImapClientTag(pImap->Client)) : NULL;
}



/* 复制最近 greeting 或 tagged completion 的指定字段。 */
void* xmail_imap_last_response_text(void* pHandle, int64 iField)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	ximapresponseview Response;
	xstrview Text;

	if ( (pImap == NULL) ||
		!xrtImapClientLastResponse(pImap->Client, &Response) ) {
		return NULL;
	}
	Text = iField == 0 ? Response.Source :
		(iField == 1 ? Response.Tag : Response.Text);
	return xmail_native_copy_text(Text);
}



/* 返回最近 greeting 或 tagged completion 的枚举字段。 */
int64 xmail_imap_last_response_value(void* pHandle, int64 iField)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	ximapresponseview Response;

	if ( (pImap == NULL) ||
		!xrtImapClientLastResponse(pImap->Client, &Response) ) {
		return 0;
	}
	return iField == 0 ? (int64)Response.Kind : (int64)Response.Status;
}



/* 直接发送显式 tag 的完整命令行。 */
bool xmail_imap_send(
	void* pHandle,
	cstr sTag,
	size_t iTagSize,
	cstr sCommand,
	size_t iCommandSize,
	cstr sArguments,
	size_t iArgumentsSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;

	return (pImap != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtImapClientSend(pImap->Client,
			xmail_native_view(sTag, iTagSize),
			xmail_native_view(sCommand, iCommandSize),
			xmail_native_view(sArguments, iArgumentsSize),
			iDeadline, NULL);
}



/* 写入原始命令续传字节。 */
bool xmail_imap_write(
	void* pHandle,
	const void* pData,
	size_t iSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;

	return (pImap != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtImapClientWrite(pImap->Client,
			pData, iSize, iDeadline, NULL);
}



/* 发送一条 continuation 数据行。 */
bool xmail_imap_continue(
	void* pHandle,
	cstr sData,
	size_t iDataSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;

	return (pImap != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtImapClientContinue(pImap->Client,
			xmail_native_view(sData, iDataSize), iDeadline, NULL);
}



/* 接收一个原始 IMAP 事件并保存稳定摘要。 */
bool xmail_imap_receive(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;
	ximapevent Event;

	return (pImap != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtImapClientReceive(pImap->Client, &Event, iDeadline, NULL) &&
		xmail_native_imap_event_set(pImap, &Event);
}



/* 开始一条自动 tag 的顺序命令。 */
bool xmail_imap_begin(
	void* pHandle,
	cstr sCommand,
	size_t iCommandSize,
	cstr sArguments,
	size_t iArgumentsSize,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;

	return (pImap != NULL) &&
		xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) &&
		xrtImapClientBegin(pImap->Client,
			xmail_native_view(sCommand, iCommandSize),
			xmail_native_view(sArguments, iArgumentsSize),
			iDeadline, NULL);
}



/* 读取顺序命令的下一事件。 */
int64 xmail_imap_next(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;
	ximapevent Event;
	xmailnext Next;

	if ( (pImap == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapClientNext(pImap->Client, &Event, iDeadline, NULL);
	if ( (Next == XMAIL_NEXT_ITEM) &&
		!xmail_native_imap_event_set(pImap, &Event) ) {
		return XMAIL_NEXT_ERROR;
	}
	return (int64)Next;
}



/* 返回最近事件的整数或布尔字段。 */
int64 xmail_imap_event_value(void* pHandle, int64 iField)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	if ( pImap == NULL ) {
		return 0;
	}
	switch ( iField ) {
		case 0: return pImap->EventKind;
		case 1: return pImap->ResponseKind;
		case 2: return pImap->Status;
		case 3: return pImap->HasLiteral ? 1 : 0;
		case 4: return pImap->LiteralNonSynchronizing ? 1 : 0;
		case 5: return pImap->LiteralBinary ? 1 : 0;
		default: return 0;
	}
}



/* 返回最近事件的 literal 总长度。 */
uint64 xmail_imap_event_literal_size(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	return pImap != NULL ? pImap->LiteralSize : 0;
}



/* 复制最近事件的 Source、Tag 或 Text。 */
void* xmail_imap_event_text(void* pHandle, int64 iField)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xstrview Text;

	if ( pImap == NULL ) {
		return NULL;
	}
	if ( iField == 0 ) {
		Text.Data = pImap->Source.Data;
		Text.Size = pImap->Source.Size;
	} else if ( iField == 1 ) {
		Text.Data = pImap->Tag.Data;
		Text.Size = pImap->Tag.Size;
	} else {
		Text.Data = pImap->Text.Data;
		Text.Size = pImap->Text.Size;
	}
	return xmail_native_copy_text(Text);
}



/* 分块读取当前 literal 并把实际字节数交给 Buffer。 */
void* xmail_imap_read_literal(
	void* pHandle,
	uint64 iMaximum,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;
	size_t iRemaining;
	size_t iCapacity;
	size_t iRead;
	bytes pData;
	xbuffer* pBuffer;

	if ( (pImap == NULL) || (iMaximum > SIZE_MAX) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return NULL;
	}
	iRemaining = xrtImapClientLiteralRemaining(pImap->Client);
	iCapacity = iMaximum != 0 ? (size_t)iMaximum : 65536u;
	if ( iCapacity > iRemaining ) {
		iCapacity = iRemaining;
	}
	if ( iCapacity == 0 ) {
		return xrtBufferCreate();
	}
	pData = (bytes)xrtCalloc(iCapacity, 1u);
	if ( pData == NULL ) {
		return NULL;
	}
	if ( !xrtImapClientReadLiteral(pImap->Client,
			pData, iCapacity, &iRead, iDeadline, NULL) ) {
		xrtFree(pData);
		return NULL;
	}
	pBuffer = xrtBufferCreateTake(&pData, iRead, iCapacity);
	if ( pBuffer == NULL ) {
		xrtFree(pData);
	}
	return pBuffer;
}



/* 使用内置 LOGIN 或 SASL 机制认证。 */
bool xmail_imap_auth(
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
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	ximapauthconfig Config;
	xdeadline iDeadline;

	if ( (pImap == NULL) || (iMethod > XIMAP_AUTH_OAUTHBEARER) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	xrtImapAuthConfigInit(&Config);
	Config.Method = (ximapauthmethod)iMethod;
	Config.Username = xmail_native_view(sUsername, iUsernameSize);
	Config.Secret = xmail_native_view(sSecret, iSecretSize);
	Config.AuthorizationId = xmail_native_view(
		sAuthorizationId,
		iAuthorizationIdSize
	);
	Config.InitialResponse = InitialResponse;
	Config.AllowPlaintext = AllowPlaintext;
	return xrtImapClientAuth(pImap->Client, &Config, iDeadline, NULL);
}



/* 执行无参数常用命令。 */
bool xmail_imap_simple(
	void* pHandle,
	int64 iOperation,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;

	if ( (pImap == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	switch ( iOperation ) {
		case 0: return xrtImapClientRefresh(pImap->Client, iDeadline, NULL);
		case 1: return xrtImapClientNoop(pImap->Client, iDeadline, NULL);
		case 2: return xrtImapClientCheck(pImap->Client, iDeadline, NULL);
		case 3: return xrtImapClientUnselect(pImap->Client, iDeadline, NULL);
		case 4: return xrtImapClientCloseMailbox(pImap->Client, iDeadline, NULL);
		case 5: return xrtImapClientBeginIdle(pImap->Client, iDeadline, NULL);
		case 6: return xrtImapClientEndIdle(pImap->Client, iDeadline, NULL);
		default:
			return xmail_native_fail(XERR_ARGUMENT, "invalid IMAP helper operation");
	}
}



/* SELECT/EXAMINE 邮箱并保存零分配摘要。 */
bool xmail_imap_select(
	void* pHandle,
	cstr sMailbox,
	size_t iMailboxSize,
	bool ReadOnly,
	int64 iTimeoutMilliseconds
)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;
	bool bResult;

	if ( (pImap == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	xrtImapMailboxInfoInit(&pImap->Mailbox);
	bResult = ReadOnly ? xrtImapClientExamine(
		pImap->Client,
		xmail_native_view(sMailbox, iMailboxSize),
		&pImap->Mailbox,
		iDeadline,
		NULL
	) : xrtImapClientSelect(
		pImap->Client,
		xmail_native_view(sMailbox, iMailboxSize),
		&pImap->Mailbox,
		iDeadline,
		NULL
	);
	return bResult;
}



/* 返回最近 SELECT/EXAMINE 摘要字段。 */
uint64 xmail_imap_mailbox_value(void* pHandle, int64 iField)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);

	if ( pImap == NULL ) {
		return 0;
	}
	switch ( iField ) {
		case 0: return pImap->Mailbox.Exists;
		case 1: return pImap->Mailbox.Recent;
		case 2: return pImap->Mailbox.Unseen;
		case 3: return pImap->Mailbox.UidValidity;
		case 4: return pImap->Mailbox.UidNext;
		case 5: return pImap->Mailbox.HighestModSeq;
		case 6: return pImap->Mailbox.Present;
		case 7: return pImap->Mailbox.ReadOnly ? 1u : 0u;
		default: return 0;
	}
}



/* 发送 LOGOUT 并在成功后释放 Engine 占用。 */
bool xmail_imap_logout(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;
	bool bResult;

	if ( (pImap == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	bResult = xrtImapClientLogout(pImap->Client, iDeadline, NULL);
	if ( bResult ) {
		xmail_native_protocol_release(&pImap->Protocol);
	}
	return bResult;
}



/* 正常关闭 IMAP 传输。 */
bool xmail_imap_close(void* pHandle, int64 iTimeoutMilliseconds)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	xdeadline iDeadline;
	bool bResult;

	if ( (pImap == NULL) ||
		!xmail_native_deadline(iTimeoutMilliseconds, &iDeadline) ) {
		return false;
	}
	bResult = xrtImapClientClose(pImap->Client, iDeadline);
	if ( bResult ) {
		xmail_native_protocol_release(&pImap->Protocol);
	}
	return bResult;
}



/* 立即异常中止 IMAP 会话。 */
bool xmail_imap_abort(void* pHandle)
{
	xmail_native_imap* pImap = xmail_native_imap_require(pHandle);
	bool bResult;

	if ( pImap == NULL ) {
		return false;
	}
	bResult = xrtImapClientAbort(pImap->Client);
	if ( bResult ) {
		xmail_native_protocol_release(&pImap->Protocol);
	}
	return bResult;
}
