#include <xrt/pop3_client.h>

#include "../internal/xrt_pop3_client.h"



#if defined(XMAIL_FEATURE_POP3_CLIENT)

/* 设置稳定的 POP3 客户端错误。 */
static bool __xrtPop3ClientError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 验证客户端仍可交换协议数据。 */
static bool __xrtPop3ClientUsable(const xpop3client* pClient)
{
	if ( (pClient == NULL) || (pClient->State == XPOP3_CLIENT_CLOSED) ||
		(pClient->State == XPOP3_CLIENT_FAILED) ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 client is not usable"
		);
	}
	return true;
}



/* 把不可恢复的线路失败记录为终态。 */
bool __xrtPop3ClientFail(xpop3client* pClient)
{
	if ( pClient != NULL ) {
		pClient->State = XPOP3_CLIENT_FAILED;
	}
	return false;
}



/* 保存并重新解析最后状态行，使公开视图指向稳定缓冲。 */
bool __xrtPop3ClientReplySave(
	xpop3client* pClient,
	xstrview Line,
	xpop3reply* pReply
)
{
	xpop3replyview Parsed;
	xstrview Stable;

	if ( !__xrtMailTextSet(&pClient->Reply, Line) ) {
		return false;
	}
	Stable.Data = pClient->Reply.Data;
	Stable.Size = pClient->Reply.Size;
	if ( !xrtPop3ReplyParse(Stable, &Parsed) ) {
		return false;
	}
	pReply->Ok = Parsed.Ok;
	pReply->Source = Parsed.Source;
	pReply->Text = Parsed.Text;
	return true;
}



/* 把 -ERR 转换为保留 LastReply 的命令拒绝错误。 */
static bool __xrtPop3ClientRejected(void)
{
	return __xrtPop3ClientError(XERR_PROTOCOL, "POP3 command was rejected");
}



/* 构建一到两个无符号十进制参数。 */
static xstrview __xrtPop3ClientNumbers(
	char* sOutput,
	uint64 iFirst,
	bool bSecond,
	uint64 iSecond
)
{
	size_t iSize = __xrtMailUint64Write(sOutput, iFirst);

	if ( bSecond ) {
		sOutput[iSize++] = ' ';
		iSize += __xrtMailUint64Write(sOutput + iSize, iSecond);
	}
	sOutput[iSize] = 0;
	return (xstrview) { sOutput, iSize };
}



/* 把 SASL 能力参数合并为已知机制位集。 */
static uint32 __xrtPop3ClientSaslMechanisms(xstrview Parameters)
{
	uint32 iMechanisms = 0;
	size_t iPosition = 0;

	while ( iPosition < Parameters.Size ) {
		size_t iStart;

		while ( (iPosition < Parameters.Size) &&
			(Parameters.Data[iPosition] == ' ') ) {
			iPosition++;
		}
		iStart = iPosition;
		while ( (iPosition < Parameters.Size) &&
			(Parameters.Data[iPosition] != ' ') ) {
			iPosition++;
		}
		if ( iPosition != iStart ) {
			iMechanisms |= xrtPop3SaslMechanism(__xrtMailSlice(
				Parameters,
				iStart,
				iPosition - iStart
			));
		}
	}
	return iMechanisms;
}



/* 读取 CAPA 多行响应并合并已知能力。 */
static bool __xrtPop3ClientCapa(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;
	xstrview Line;
	xmailnext Next;

	pClient->Capabilities = 0;
	pClient->SaslMechanisms = 0;
	if ( !xrtPop3ClientCommand(
		pClient,
		XRT_STR_LITERAL("CAPA"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return true;
	}
	pClient->ReturnState = pClient->State;
	pClient->State = XPOP3_CLIENT_MULTILINE;
	for ( ;; ) {
		xpop3capabilityview Capability;

		Next = xrtPop3ClientNext(
			pClient,
			&Line,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_END ) {
			return true;
		}
		if ( (Next != XMAIL_NEXT_ITEM) ||
			!xrtPop3CapabilityParse(Line, &Capability) ) {
			return __xrtPop3ClientFail(pClient);
		}
		pClient->Capabilities |= xrtPop3Capability(Capability.Name);
		if ( __xrtMailAsciiEqualI(
			Capability.Name,
			XRT_STR_LITERAL("SASL")
		) ) {
			pClient->SaslMechanisms |= __xrtPop3ClientSaslMechanisms(
				Capability.Parameters
			);
		}
	}
}



/* 初始化 POP3 客户端配置。 */
XRT_API void xrtPop3ClientConfigInit(xpop3clientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtMailNetConfigInit(&pConfig->Net);
	pConfig->Net.Port = 110u;
	pConfig->ReadCapabilities = true;
}



/* 验证 POP3 客户端配置。 */
XRT_API bool xrtPop3ClientConfigValid(const xpop3clientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailNetConfigValid(&pConfig->Net) ) {
		return false;
	}
	if ( (pConfig->Net.Security == XMAIL_SECURITY_STARTTLS) &&
		!pConfig->ReadCapabilities ) {
		return __xrtPop3ClientError(
			XERR_ARGUMENT,
			"POP3 STLS requires capability discovery"
		);
	}
	return true;
}



/* 建立并协商 POP3 会话。 */
XRT_API xpop3client* xrtPop3ClientOpen(
	const xpop3clientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3client* pClient;
	xpop3reply Reply;

	if ( !xrtPop3ClientConfigValid(pConfig) ) {
		return NULL;
	}
	pClient = (xpop3client*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->State = XPOP3_CLIENT_AUTHORIZATION;
	if ( !__xrtMailTransportOpen(
		&pClient->Transport,
		&pConfig->Net,
		iDeadline,
		pCancel
	) || !xrtPop3ClientReceive(
		pClient,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		xrtPop3ClientDestroy(pClient);
		return NULL;
	}
	if ( !Reply.Ok ) {
		(void)__xrtPop3ClientRejected();
		xrtPop3ClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->ReadCapabilities && !__xrtPop3ClientCapa(
		pClient,
		iDeadline,
		pCancel
	) ) {
		xrtPop3ClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->Net.Security == XMAIL_SECURITY_STARTTLS ) {
		#if defined(XMAIL_FEATURE_POP3_CLIENT_TLS)
			if ( (pClient->Capabilities & XPOP3_CAP_STLS) == 0 ) {
				(void)__xrtPop3ClientError(
					XERR_UNSUPPORTED,
					"POP3 server does not support STLS"
				);
				xrtPop3ClientDestroy(pClient);
				return NULL;
			}
			if ( !xrtPop3ClientCommand(
				pClient,
				XRT_STR_LITERAL("STLS"),
				XRT_STR_LITERAL(""),
				&Reply,
				iDeadline,
				pCancel
			) || !Reply.Ok || !__xrtMailTransportStartTls(
				&pClient->Transport,
				&pConfig->Net,
				iDeadline,
				pCancel
			) || !__xrtPop3ClientCapa(
				pClient,
				iDeadline,
				pCancel
			) ) {
				if ( !Reply.Ok ) {
					(void)__xrtPop3ClientRejected();
				}
				xrtPop3ClientDestroy(pClient);
				return NULL;
			}
		#else
			(void)__xrtPop3ClientError(
				XERR_UNSUPPORTED,
				"POP3 STLS support is not enabled"
			);
			xrtPop3ClientDestroy(pClient);
			return NULL;
		#endif
	}
	return pClient;
}



/* 返回 POP3 客户端状态。 */
XRT_API xpop3clientstate xrtPop3ClientState(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->State : XPOP3_CLIENT_FAILED;
}



/* 返回 POP3 能力快照。 */
XRT_API uint32 xrtPop3ClientCapabilities(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->Capabilities : 0;
}



/* 返回 POP3 SASL 机制快照。 */
XRT_API uint32 xrtPop3ClientSaslMechanisms(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->SaslMechanisms : 0;
}



/* 返回 POP3 传输安全级别。 */
XRT_API xmailsecurity xrtPop3ClientSecurity(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->Transport.Security :
		XMAIL_SECURITY_PLAIN;
}



/* 取得最后 POP3 状态行。 */
XRT_API bool xrtPop3ClientLastReply(
	const xpop3client* pClient,
	xpop3reply* pReply
)
{
	xpop3replyview Parsed;
	xstrview Stable;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		(pClient->Reply.Data == NULL) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Stable.Data = pClient->Reply.Data;
	Stable.Size = pClient->Reply.Size;
	if ( !xrtPop3ReplyParse(Stable, &Parsed) ) {
		return false;
	}
	pReply->Ok = Parsed.Ok;
	pReply->Source = Parsed.Source;
	pReply->Text = Parsed.Text;
	return true;
}



/* 按调用场景的独立上限发送一条 POP3 线路。 */
static bool __xrtPop3ClientSendLine(
	xpop3client* pClient,
	xstrview Line,
	size_t iMaxLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_MULTILINE ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is active"
		);
	}
	if ( !__xrtMailViewValid(Line) || (iMaxLine < 2u) ||
		(Line.Size > (iMaxLine - 2u)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ) {
			return __xrtPop3ClientError(
				XERR_ARGUMENT,
				"POP3 line contains a control separator"
			);
		}
	}
	if ( (Line.Size != 0) && !__xrtMailTransportSend(
		&pClient->Transport,
		Line.Data,
		Line.Size,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 发送低层 POP3 命令行。 */
XRT_API bool xrtPop3ClientSend(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientSendLine(
		pClient,
		Line,
		XPOP3_COMMAND_MAX,
		iDeadline,
		pCancel
	);
}



/* 发送较长的 POP3 SASL 响应行。 */
XRT_API bool xrtPop3ClientAuthLine(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State != XPOP3_CLIENT_AUTHORIZATION ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 SASL response requires AUTHORIZATION state"
		);
	}
	return __xrtPop3ClientSendLine(
		pClient,
		Line,
		XPOP3_AUTH_RESPONSE_MAX,
		iDeadline,
		pCancel
	);
}



/* 读取一条未解释的 POP3 线路。 */
XRT_API bool xrtPop3ClientLine(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_MULTILINE ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is active"
		);
	}
	if ( !xrtMemRangeValid(pLine, sizeof(*pLine)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailTransportLine(
		&pClient->Transport,
		pLine,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 读取 POP3 状态行。 */
XRT_API bool xrtPop3ClientReceive(
	xpop3client* pClient,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;

	if ( !xrtMemRangeValid(pReply, sizeof(*pReply)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientLine(
		pClient,
		&Line,
		iDeadline,
		pCancel
	) || !__xrtPop3ClientReplySave(pClient, Line, pReply) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 发送 POP3 命令并读取状态。 */
XRT_API bool xrtPop3ClientCommand(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sCommand[XPOP3_COMMAND_MAX + 1u];
	size_t iSize;

	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_MULTILINE ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is active"
		);
	}
	if ( !xrtMemRangeValid(pReply, sizeof(*pReply)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3CommandWrite(
			Verb,
			Arguments,
			sCommand,
			sizeof(sCommand),
			&iSize
		) ) {
		return false;
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		sCommand,
		iSize,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return xrtPop3ClientReceive(
		pClient,
		pReply,
		iDeadline,
		pCancel
	);
}



/* 开始 POP3 多行响应。 */
XRT_API bool xrtPop3ClientBegin(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !xrtPop3ClientCommand(
		pClient,
		Verb,
		Arguments,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	pClient->ReturnState = pClient->State;
	pClient->State = XPOP3_CLIENT_MULTILINE;
	return true;
}



/* 读取下一条 POP3 多行数据。 */
XRT_API xmailnext xrtPop3ClientNext(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;
	xmailnext Next;

	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State != XPOP3_CLIENT_MULTILINE) ||
		!xrtMemRangeValid(pLine, sizeof(*pLine)) ) {
		(void)__xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is not active"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailTransportLine(
		&pClient->Transport,
		&Line,
		iDeadline,
		pCancel
	) ) {
		(void)__xrtPop3ClientFail(pClient);
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtMailDotLine(Line, pLine);
	if ( Next == XMAIL_NEXT_END ) {
		pClient->State = pClient->ReturnState;
	} else if ( Next == XMAIL_NEXT_ERROR ) {
		(void)__xrtPop3ClientFail(pClient);
	}
	return Next;
}



/* 认证扩展成功后原子进入事务状态。 */
bool __xrtPop3ClientAuthorize(xpop3client* pClient)
{
	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State != XPOP3_CLIENT_AUTHORIZATION) ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 authorization state is not active"
		);
	}
	pClient->State = XPOP3_CLIENT_TRANSACTION;
	return true;
}



/* 验证命令只能在事务状态执行。 */
static bool __xrtPop3ClientTransaction(const xpop3client* pClient)
{
	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State != XPOP3_CLIENT_TRANSACTION) ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 command requires TRANSACTION state"
		);
	}
	return true;
}



/* 执行事务中的无参数单行命令。 */
static bool __xrtPop3ClientSimple(
	xpop3client* pClient,
	xstrview Verb,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	return __xrtPop3ClientTransaction(pClient) &&
		xrtPop3ClientCommand(
			pClient,
			Verb,
			XRT_STR_LITERAL(""),
			&Reply,
			iDeadline,
			pCancel
		) && (Reply.Ok ? true : __xrtPop3ClientRejected());
}



/* 查询 POP3 STAT。 */
XRT_API bool xrtPop3ClientStat(
	xpop3client* pClient,
	xpop3stat* pStat,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pStat, sizeof(*pStat)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("STAT"),
			XRT_STR_LITERAL(""),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	return Reply.Ok ? xrtPop3StatParse(Reply.Source, pStat) :
		__xrtPop3ClientRejected();
}



/* 查询一条 POP3 LIST。 */
XRT_API bool xrtPop3ClientList(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3listview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( (iMessage == 0) || !xrtMemRangeValid(pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("LIST"),
			__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	return Reply.Ok ? xrtPop3ListParse(Reply.Text, pItem) :
		__xrtPop3ClientRejected();
}



/* 开始读取全部 POP3 LIST。 */
XRT_API bool xrtPop3ClientListAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientTransaction(pClient) && xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("LIST"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	);
}



/* 查询一条 POP3 UIDL。 */
XRT_API bool xrtPop3ClientUidl(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3uidlview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( (iMessage == 0) || !xrtMemRangeValid(pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("UIDL"),
			__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	return Reply.Ok ? xrtPop3UidlParse(Reply.Text, pItem) :
		__xrtPop3ClientRejected();
}



/* 开始读取全部 POP3 UIDL。 */
XRT_API bool xrtPop3ClientUidlAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientTransaction(pClient) && xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("UIDL"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	);
}



/* 开始读取完整邮件。 */
XRT_API bool xrtPop3ClientRetr(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( iMessage == 0 ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("RETR"),
		__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
		iDeadline,
		pCancel
	);
}



/* 开始读取邮件字段和有限正文行。 */
XRT_API bool xrtPop3ClientTop(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumbers[42];

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( iMessage == 0 ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("TOP"),
		__xrtPop3ClientNumbers(sNumbers, iMessage, true, iLines),
		iDeadline,
		pCancel
	);
}



/* 标记一条 POP3 消息删除。 */
XRT_API bool xrtPop3ClientDelete(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( iMessage == 0 ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("DELE"),
			__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	return Reply.Ok ? true : __xrtPop3ClientRejected();
}



/* 清除 POP3 删除标记。 */
XRT_API bool xrtPop3ClientReset(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientSimple(
		pClient,
		XRT_STR_LITERAL("RSET"),
		iDeadline,
		pCancel
	);
}



/* 发送 POP3 NOOP。 */
XRT_API bool xrtPop3ClientNoop(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientSimple(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		iDeadline,
		pCancel
	);
}



/* 发送 POP3 QUIT 并关闭传输。 */
XRT_API bool xrtPop3ClientQuit(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State == XPOP3_CLIENT_MULTILINE) ||
		!xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("QUIT"),
			XRT_STR_LITERAL(""),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	pClient->State = XPOP3_CLIENT_UPDATE;
	return xrtPop3ClientClose(pClient, iDeadline);
}



/* 正常关闭 POP3 传输。 */
XRT_API bool xrtPop3ClientClose(
	xpop3client* pClient,
	xdeadline iDeadline
)
{
	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( !__xrtMailTransportClose(&pClient->Transport, iDeadline) ) {
		pClient->State = XPOP3_CLIENT_FAILED;
		return false;
	}
	pClient->State = XPOP3_CLIENT_CLOSED;
	return true;
}



/* 无等待异常中止 POP3 连接，保留失败终态和响应诊断。 */
XRT_API bool xrtPop3ClientAbort(xpop3client* pClient)
{
	bool bFailed;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_CLOSED ) {
		return true;
	}
	bFailed = pClient->State == XPOP3_CLIENT_FAILED;
	if ( !__xrtMailTransportAbort(&pClient->Transport) ) {
		pClient->State = XPOP3_CLIENT_FAILED;
		return false;
	}
	if ( !bFailed ) {
		pClient->State = XPOP3_CLIENT_CLOSED;
	}
	return true;
}



/* 释放 POP3 客户端。 */
XRT_API void xrtPop3ClientDestroy(xpop3client* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	__xrtMailTransportDestroy(&pClient->Transport);
	__xrtMailTextDestroy(&pClient->Reply);
	xrtFree(pClient);
}

#endif
