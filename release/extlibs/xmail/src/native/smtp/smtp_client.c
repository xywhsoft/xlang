#include <xrt/smtp_client.h>

#include "../internal/xrt_mail_net.h"
#include "../internal/xrt_smtp_client.h"



#if defined(XMAIL_FEATURE_SMTP_CLIENT)

/* 同步客户端只保存会话状态、能力和最后响应，不复制配置。 */
struct xsmtpclient {
	__xmailtransport Transport;
	xsmtpclientstate State;
	uint64 Capabilities;
	uint64 SizeLimit;
	__xmailtext Reply;
	size_t ReplyLines;
	size_t ReplyLineLimit;
	int ReplyCode;
	xmaildotwriter DataWriter;
	size_t ChunkRemaining;
	bool ChunkLast;
	bool ChunkActive;
	bool ChunkRejected;
	bool Authenticated;
};



/* 判断一个借用视图是否具有有效地址范围。 */
static bool __xrtSmtpClientViewValid(xstrview Text)
{
	return xrtMemRangeValid(Text.Data, Text.Size);
}



/* 创建 SMTP 客户端错误。 */
static void __xrtSmtpClientError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
}



/* 验证客户端仍然可以交换协议数据。 */
static bool __xrtSmtpClientUsable(const xsmtpclient* pClient)
{
	if ( (pClient == NULL) || (pClient->State == XSMTP_CLIENT_CLOSED) ||
		(pClient->State == XSMTP_CLIENT_FAILED) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP client is not usable");
		return false;
	}
	return true;
}



/* 普通命令不能插入 DATA、未完成的 BDAT 或已拒绝的 BDAT 事务。 */
static bool __xrtSmtpClientCommandMode(
	const xsmtpclient* pClient,
	bool bReset
)
{
	if ( !__xrtSmtpClientUsable(pClient) ) {
		return false;
	}
	if ( (pClient->State == XSMTP_CLIENT_DATA) ||
		pClient->ChunkActive ||
		(pClient->ChunkRejected && !bReset) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP transaction does not accept a command"
		);
		return false;
	}
	return true;
}



/* 将不可恢复的线路失败记录为会话终态。 */
static bool __xrtSmtpClientFailed(xsmtpclient* pClient)
{
	if ( pClient != NULL ) {
		pClient->State = XSMTP_CLIENT_FAILED;
	}
	return false;
}



/* 保存最后一条响应文本，借用视图稳定到下一次响应。 */
static bool __xrtSmtpClientReplySave(
	xsmtpclient* pClient,
	xstrview Text,
	int iCode,
	size_t iLines
)
{
	if ( !__xrtMailTextSet(&pClient->Reply, Text) ) {
		return false;
	}
	pClient->ReplyCode = iCode;
	pClient->ReplyLines = iLines;
	return true;
}



/* 把一条 EHLO 扩展合并到客户端能力快照。 */
static bool __xrtSmtpClientCapability(
	xsmtpclient* pClient,
	xstrview Text
)
{
	xsmtpcapabilityview Capability;

	return xrtSmtpCapabilityParse(Text, &Capability) &&
		xrtSmtpCapabilityAdd(
			&Capability,
			&pClient->Capabilities,
			&pClient->SizeLimit
		);
}



/* 读取一条响应，并可解析 EHLO 首行之后的扩展。 */
static bool __xrtSmtpClientReceiveMode(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	bool bCapabilities,
	size_t iReplyLines,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreplyparser Parser;
	xsmtpreplyline Line;
	xstrview Text;

	if ( !__xrtSmtpClientUsable(pClient) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		!xrtSmtpReplyParserInit(&Parser, iReplyLines) ) {
		return false;
	}
	for ( ;; ) {
		if ( !__xrtMailTransportLine(
			&pClient->Transport,
			&Text,
			iDeadline,
			pCancel
		) || !xrtSmtpReplyRead(&Parser, Text, &Line) ) {
			return __xrtSmtpClientFailed(pClient);
		}
		if ( bCapabilities && (Parser.Lines > 1u) &&
			!__xrtSmtpClientCapability(pClient, Line.Text) ) {
			return __xrtSmtpClientFailed(pClient);
		}
		if ( Parser.Done ) {
			break;
		}
	}
	if ( !__xrtSmtpClientReplySave(
		pClient,
		Line.Text,
		Line.Code,
		Parser.Lines
	) ) {
		return false;
	}
	pReply->Code = pClient->ReplyCode;
	pReply->Lines = pClient->ReplyLines;
	pReply->Text.Data = pClient->Reply.Data;
	pReply->Text.Size = pClient->Reply.Size;
	return true;
}



/* 设置意外服务器状态错误，同时保留 LastReply 供诊断。 */
static bool __xrtSmtpClientUnexpected(void)
{
	__xrtSmtpClientError(XERR_PROTOCOL, "unexpected SMTP reply code");
	return false;
}



/* 发送 EHLO，并在明确不支持时按配置回退 HELO。 */
static bool __xrtSmtpClientHello(
	xsmtpclient* pClient,
	const xsmtpclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;
	char sCommand[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;

	pClient->Capabilities = 0;
	pClient->SizeLimit = 0;
	if ( !xrtSmtpCommandWrite(
		XRT_STR_LITERAL("EHLO"),
		pConfig->Hello,
		sCommand,
		sizeof(sCommand),
		&iSize
	) || !__xrtMailTransportSend(
		&pClient->Transport,
		sCommand,
		iSize,
		iDeadline,
		pCancel
	) || !__xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		true,
		pConfig->ReplyLines,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code == 250 ) {
		return true;
	}
	if ( !pConfig->HeloFallback ||
		((Reply.Code != 500) && (Reply.Code != 502) &&
		 (Reply.Code != 504)) ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->Capabilities = 0;
	pClient->SizeLimit = 0;
	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("HELO"),
		pConfig->Hello,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	return Reply.Code == 250 ? true : __xrtSmtpClientUnexpected();
}



/* 在栈上构建 FROM/TO 路径参数。 */
static bool __xrtSmtpClientPathArguments(
	cstr sPrefix,
	xstrview Path,
	xstrview Parameters,
	bool bAllowEmpty,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iPrefix = strlen(sPrefix);
	size_t iRequired = iPrefix;

	if ( !xrtSmtpPathValid(Path, bAllowEmpty) ||
		!__xrtSmtpClientViewValid(Parameters) ||
		!__xrtMailSizeAdd(iRequired, Path.Size, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		((Parameters.Size != 0) &&
		 (!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		  !__xrtMailSizeAdd(iRequired, Parameters.Size, &iRequired))) ||
		(iRequired >= iCapacity) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP envelope path or parameters"
		);
		return false;
	}
	memcpy(sOutput, sPrefix, iPrefix);
	if ( Path.Size != 0 ) {
		memcpy(sOutput + iPrefix, Path.Data, Path.Size);
	}
	sOutput[iPrefix + Path.Size] = '>';
	if ( Parameters.Size != 0 ) {
		sOutput[iPrefix + Path.Size + 1u] = ' ';
		memcpy(
			sOutput + iPrefix + Path.Size + 2u,
			Parameters.Data,
			Parameters.Size
		);
	}
	sOutput[iRequired] = 0;
	*pSize = iRequired;
	return true;
}



/* 把 dot writer 输出直接提交到当前 SMTP 传输。 */
typedef struct __xsmtpclientdatasink {
	xsmtpclient* Client;
	xdeadline Deadline;
	xcancel* Cancel;
} __xsmtpclientdatasink;



/* 同步发送一个已经完成 dot transparency 的 DATA 片段。 */
static bool __xrtSmtpClientDataSink(xbytesview Data, ptr pUserData)
{
	__xsmtpclientdatasink* pSink = (__xsmtpclientdatasink*)pUserData;

	return __xrtMailTransportSend(
		&pSink->Client->Transport,
		Data.Data,
		Data.Size,
		pSink->Deadline,
		pSink->Cancel
	);
}



/* 初始化 SMTP 客户端配置。 */
XRT_API void xrtSmtpClientConfigInit(xsmtpclientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtMailNetConfigInit(&pConfig->Net);
	pConfig->Net.Port = 25u;
	pConfig->Hello = (xstrview)XRT_STR_LITERAL("localhost");
	pConfig->ReplyLines = XSMTP_REPLY_LINES_DEFAULT;
	pConfig->HeloFallback = true;
}



/* 验证 SMTP 客户端配置。 */
XRT_API bool xrtSmtpClientConfigValid(const xsmtpclientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailNetConfigValid(&pConfig->Net) ) {
		return false;
	}
	if ( !__xrtSmtpClientViewValid(pConfig->Hello) ||
		(pConfig->Hello.Size == 0) ||
		(pConfig->Hello.Size > XSMTP_HELLO_MAX) ||
		(pConfig->ReplyLines == 0) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid SMTP client configuration"
		);
		return false;
	}
	for ( size_t i = 0; i < pConfig->Hello.Size; i++ ) {
		unsigned char iByte = (unsigned char)pConfig->Hello.Data[i];

		if ( (iByte <= 32u) || (iByte >= 127u) ) {
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_CONFIG,
				"invalid SMTP EHLO name"
			);
			return false;
		}
	}
	return true;
}



/* 建立并协商 SMTP 会话。 */
XRT_API xsmtpclient* xrtSmtpClientOpen(
	const xsmtpclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpclient* pClient;
	xsmtpreply Reply;
	bool bOpened;
	bool bReceived;

	if ( !xrtSmtpClientConfigValid(pConfig) ) {
		return NULL;
	}
	pClient = (xsmtpclient*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->State = XSMTP_CLIENT_READY;
	pClient->ReplyLineLimit = pConfig->ReplyLines;
	memset(&Reply, 0, sizeof(Reply));
	bOpened = __xrtMailTransportOpen(
		&pClient->Transport,
		&pConfig->Net,
		iDeadline,
		pCancel
	);
	bReceived = bOpened && __xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	);
	if ( !bOpened || !bReceived ) {
		xrtSmtpClientDestroy(pClient);
		return NULL;
	}
	if ( Reply.Code != 220 ) {
		(void)__xrtSmtpClientUnexpected();
		xrtSmtpClientDestroy(pClient);
		return NULL;
	}
	if ( !__xrtSmtpClientHello(
		pClient,
		pConfig,
		iDeadline,
		pCancel
	) ) {
		xrtSmtpClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->Net.Security == XMAIL_SECURITY_STARTTLS ) {
		#if defined(XMAIL_FEATURE_SMTP_CLIENT_TLS)
			if ( (pClient->Capabilities & XSMTP_CAP_STARTTLS) == 0 ) {
				__xrtSmtpClientError(
					XERR_UNSUPPORTED,
					"SMTP server does not support STARTTLS"
				);
				xrtSmtpClientDestroy(pClient);
				return NULL;
			}
			if ( !xrtSmtpClientCommand(
				pClient,
				XRT_STR_LITERAL("STARTTLS"),
				XRT_STR_LITERAL(""),
				&Reply,
				iDeadline,
				pCancel
			) || (Reply.Code != 220) ||
				!__xrtMailTransportStartTls(
					&pClient->Transport,
					&pConfig->Net,
					iDeadline,
					pCancel
				) || !__xrtSmtpClientHello(
					pClient,
					pConfig,
					iDeadline,
					pCancel
				) ) {
				if ( (Reply.Code != 220) && (xrtGetError() == NULL) ) {
					(void)__xrtSmtpClientUnexpected();
				}
				xrtSmtpClientDestroy(pClient);
				return NULL;
			}
		#else
			__xrtSmtpClientError(
				XERR_UNSUPPORTED,
				"SMTP STARTTLS support is not enabled"
			);
			xrtSmtpClientDestroy(pClient);
			return NULL;
		#endif
	}
	pClient->State = XSMTP_CLIENT_READY;
	return pClient;
}



/* 返回 SMTP 客户端状态。 */
XRT_API xsmtpclientstate xrtSmtpClientState(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->State : XSMTP_CLIENT_FAILED;
}



/* 返回能力快照。 */
XRT_API uint64 xrtSmtpClientCapabilities(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->Capabilities : 0;
}



/* 返回 SIZE 快照。 */
XRT_API uint64 xrtSmtpClientSizeLimit(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->SizeLimit : 0;
}



/* 返回当前 SMTP 传输安全级别。 */
XRT_API xmailsecurity xrtSmtpClientSecurity(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->Transport.Security :
		XMAIL_SECURITY_PLAIN;
}



/* 返回当前会话的 SMTP AUTH 完成状态。 */
XRT_API bool xrtSmtpClientAuthenticated(const xsmtpclient* pClient)
{
	return (pClient != NULL) && pClient->Authenticated;
}



/* 由认证层在服务器接受凭据后记录不可逆的会话状态。 */
void __xrtSmtpClientAuthComplete(xsmtpclient* pClient)
{
	if ( pClient != NULL ) {
		pClient->Authenticated = true;
	}
}



/* 取得最后响应。 */
XRT_API bool xrtSmtpClientLastReply(
	const xsmtpclient* pClient,
	xsmtpreply* pReply
)
{
	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		(pClient->ReplyCode == 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pReply->Code = pClient->ReplyCode;
	pReply->Lines = pClient->ReplyLines;
	pReply->Text.Data = pClient->Reply.Data;
	pReply->Text.Size = pClient->Reply.Size;
	return true;
}



/* 验证并发送一条具有调用方指定上限的 SMTP 行。 */
static bool __xrtSmtpClientSendLine(
	xsmtpclient* pClient,
	xstrview Line,
	size_t iLimit,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpClientCommandMode(pClient, false) ) {
		return false;
	}
	if ( !__xrtSmtpClientViewValid(Line) ||
		(Line.Size > iLimit) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ) {
			__xrtSmtpClientError(
				XERR_ARGUMENT,
				"SMTP line contains a control separator"
			);
			return false;
		}
	}
	if ( (Line.Size != 0) && !__xrtMailTransportSend(
		&pClient->Transport,
		Line.Data,
		Line.Size,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	return true;
}



/* 发送受普通命令长度约束的低层 SMTP 行。 */
XRT_API bool xrtSmtpClientSend(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtSmtpClientSendLine(
		pClient,
		Line,
		XSMTP_COMMAND_MAX - 2u,
		iDeadline,
		pCancel
	);
}



/* 发送具有独立长度上限的 SASL continuation 响应。 */
XRT_API bool xrtSmtpClientAuthLine(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtSmtpClientSendLine(
		pClient,
		Line,
		XSMTP_AUTH_RESPONSE_MAX,
		iDeadline,
		pCancel
	);
}



/* 读取完整 SMTP 响应。 */
XRT_API bool xrtSmtpClientReceive(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpClientCommandMode(pClient, false) ) {
		return false;
	}
	return __xrtSmtpClientReceiveMode(
		pClient,
		pReply,
		false,
		pClient != NULL ? pClient->ReplyLineLimit : 0,
		iDeadline,
		pCancel
	);
}



/* 发送命令并读取响应。 */
XRT_API bool xrtSmtpClientCommand(
	xsmtpclient* pClient,
	xstrview Verb,
	xstrview Arguments,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sCommand[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;
	bool bReset;

	if ( !__xrtSmtpClientViewValid(Verb) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	bReset = __xrtMailAsciiEqualI(Verb, XRT_STR_LITERAL("RSET"));
	if ( !__xrtSmtpClientCommandMode(pClient, bReset) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		!xrtSmtpCommandWrite(
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
		return __xrtSmtpClientFailed(pClient);
	}
	return __xrtSmtpClientReceiveMode(
		pClient,
		pReply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	);
}



/* 开始 SMTP envelope。 */
XRT_API bool xrtSmtpClientMail(
	xsmtpclient* pClient,
	xstrview ReversePath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sArguments[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_READY) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP MAIL requires READY state");
		return false;
	}
	if ( !__xrtSmtpClientPathArguments(
		"FROM:<",
		ReversePath,
		Parameters,
		true,
		sArguments,
		sizeof(sArguments),
		&iSize
	) || !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("MAIL"),
		(xstrview) { sArguments, iSize },
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 250 ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = XSMTP_CLIENT_MAIL;
	return true;
}



/* 增加 SMTP envelope 收件人。 */
XRT_API bool xrtSmtpClientRcpt(
	xsmtpclient* pClient,
	xstrview ForwardPath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sArguments[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		((pClient->State != XSMTP_CLIENT_MAIL) &&
		 (pClient->State != XSMTP_CLIENT_RECIPIENT)) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP RCPT requires an active envelope"
		);
		return false;
	}
	if ( !__xrtSmtpClientPathArguments(
		"TO:<",
		ForwardPath,
		Parameters,
		false,
		sArguments,
		sizeof(sArguments),
		&iSize
	) || !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("RCPT"),
		(xstrview) { sArguments, iSize },
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( (Reply.Code != 250) && (Reply.Code != 251) &&
		(Reply.Code != 252) ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = XSMTP_CLIENT_RECIPIENT;
	return true;
}



/* 进入 SMTP DATA 模式。 */
XRT_API bool xrtSmtpClientDataBegin(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_RECIPIENT) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP DATA requires at least one accepted recipient"
		);
		return false;
	}
	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("DATA"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 354 ) {
		return __xrtSmtpClientUnexpected();
	}
	if ( !xrtMailDotWriterInit(&pClient->DataWriter) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	pClient->State = XSMTP_CLIENT_DATA;
	return true;
}



/* 发送一个 SMTP DATA 消息片段。 */
XRT_API bool xrtSmtpClientDataWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xsmtpclientdatasink Sink;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_DATA) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP DATA write requires DATA state");
		return false;
	}
	Sink.Client = pClient;
	Sink.Deadline = iDeadline;
	Sink.Cancel = pCancel;
	if ( !xrtMailDotWriterWrite(
		&pClient->DataWriter,
		Data,
		__xrtSmtpClientDataSink,
		&Sink
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	return true;
}



/* 完成 SMTP DATA 并读取最终响应。 */
XRT_API bool xrtSmtpClientDataEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xsmtpclientdatasink Sink;
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_DATA) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP DATA end requires DATA state");
		return false;
	}
	Sink.Client = pClient;
	Sink.Deadline = iDeadline;
	Sink.Cancel = pCancel;
	if ( !xrtMailDotWriterFinish(
		&pClient->DataWriter,
		__xrtSmtpClientDataSink,
		&Sink
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	if ( !__xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	pClient->State = XSMTP_CLIENT_READY;
	return Reply.Code == 250 ? true : __xrtSmtpClientUnexpected();
}



/* 发送一份已经连续存放的 SMTP DATA。 */
XRT_API bool xrtSmtpClientData(
	xsmtpclient* pClient,
	xstrview Message,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xbytesview Data;
	size_t iEncoded;

	if ( !xrtMailDotWrite(Message, true, NULL, 0, &iEncoded) ) {
		return false;
	}
	(void)iEncoded;
	Data.Data = (const unsigned char*)Message.Data;
	Data.Size = Message.Size;
	return xrtSmtpClientDataBegin(pClient, iDeadline, pCancel) &&
		xrtSmtpClientDataWrite(pClient, Data, iDeadline, pCancel) &&
		xrtSmtpClientDataEnd(pClient, iDeadline, pCancel);
}



/* 开始一个精确计数的 SMTP BDAT 块。 */
XRT_API bool xrtSmtpClientBdatBegin(
	xsmtpclient* pClient,
	size_t iChunkSize,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sLine[5u + (sizeof(size_t) * 3u) + 5u];
	size_t iSize;

	if ( !__xrtSmtpClientCommandMode(pClient, false) ||
		((pClient->State != XSMTP_CLIENT_RECIPIENT) &&
		 (pClient->State != XSMTP_CLIENT_CHUNK)) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP BDAT requires an accepted recipient"
		);
		return false;
	}
	if ( (pClient->Capabilities & XSMTP_CAP_CHUNKING) == 0 ) {
		__xrtSmtpClientError(
			XERR_UNSUPPORTED,
			"SMTP server does not advertise CHUNKING"
		);
		return false;
	}
	memcpy(sLine, "BDAT ", 5u);
	iSize = 5u + __xrtMailUint64Write(
		sLine + 5u,
		(uint64)iChunkSize
	);
	if ( Last ) {
		memcpy(sLine + iSize, " LAST", 5u);
		iSize += 5u;
	}
	if ( !xrtSmtpClientSend(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	pClient->State = XSMTP_CLIENT_CHUNK;
	pClient->ChunkRemaining = iChunkSize;
	pClient->ChunkLast = Last;
	pClient->ChunkActive = true;
	return true;
}



/* 发送当前 BDAT 块的一段原始字节。 */
XRT_API bool xrtSmtpClientBdatWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_CHUNK) ||
		!pClient->ChunkActive ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP BDAT write requires a block");
		return false;
	}
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Data.Size > pClient->ChunkRemaining ) {
		__xrtSmtpClientError(XERR_RANGE, "SMTP BDAT data exceeds chunk size");
		return false;
	}
	if ( (Data.Size != 0) && !__xrtMailTransportSend(
		&pClient->Transport,
		Data.Data,
		Data.Size,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	pClient->ChunkRemaining -= Data.Size;
	return true;
}



/* 完成当前 BDAT 块并读取服务器确认。 */
XRT_API bool xrtSmtpClientBdatEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;
	bool bLast;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_CHUNK) ||
		!pClient->ChunkActive ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP BDAT end requires a block");
		return false;
	}
	if ( pClient->ChunkRemaining != 0 ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP BDAT block is incomplete");
		return false;
	}
	bLast = pClient->ChunkLast;
	pClient->ChunkActive = false;
	pClient->ChunkLast = false;
	if ( !__xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 250 ) {
		pClient->ChunkRejected = true;
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = bLast ? XSMTP_CLIENT_READY : XSMTP_CLIENT_CHUNK;
	return true;
}



/* 发送一个连续存放的 SMTP BDAT 块。 */
XRT_API bool xrtSmtpClientBdat(
	xsmtpclient* pClient,
	xbytesview Data,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtSmtpClientBdatBegin(
		pClient,
		Data.Size,
		Last,
		iDeadline,
		pCancel
	) && xrtSmtpClientBdatWrite(
		pClient,
		Data,
		iDeadline,
		pCancel
	) && xrtSmtpClientBdatEnd(pClient, iDeadline, pCancel);
}



/* 重置 SMTP envelope。 */
XRT_API bool xrtSmtpClientReset(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("RSET"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 250 ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = XSMTP_CLIENT_READY;
	pClient->ChunkRemaining = 0;
	pClient->ChunkLast = false;
	pClient->ChunkActive = false;
	pClient->ChunkRejected = false;
	return true;
}



/* 发送 SMTP NOOP。 */
XRT_API bool xrtSmtpClientNoop(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	return xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) && (Reply.Code == 250 ? true : __xrtSmtpClientUnexpected());
}



/* 发送 QUIT 并关闭传输。 */
XRT_API bool xrtSmtpClientQuit(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("QUIT"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 221 ) {
		return __xrtSmtpClientUnexpected();
	}
	return xrtSmtpClientClose(pClient, iDeadline);
}



/* 正常关闭 SMTP 传输。 */
XRT_API bool xrtSmtpClientClose(
	xsmtpclient* pClient,
	xdeadline iDeadline
)
{
	if ( !__xrtSmtpClientUsable(pClient) ) {
		return false;
	}
	if ( !__xrtMailTransportClose(&pClient->Transport, iDeadline) ) {
		pClient->State = XSMTP_CLIENT_FAILED;
		return false;
	}
	pClient->State = XSMTP_CLIENT_CLOSED;
	return true;
}



/* 无等待异常中止 SMTP 连接，保留失败终态和响应诊断。 */
XRT_API bool xrtSmtpClientAbort(xsmtpclient* pClient)
{
	bool bFailed;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State == XSMTP_CLIENT_CLOSED ) {
		return true;
	}
	bFailed = pClient->State == XSMTP_CLIENT_FAILED;
	if ( !__xrtMailTransportAbort(&pClient->Transport) ) {
		pClient->State = XSMTP_CLIENT_FAILED;
		return false;
	}
	if ( !bFailed ) {
		pClient->State = XSMTP_CLIENT_CLOSED;
	}
	return true;
}



/* 释放 SMTP 客户端。 */
XRT_API void xrtSmtpClientDestroy(xsmtpclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	__xrtMailTransportDestroy(&pClient->Transport);
	__xrtMailTextDestroy(&pClient->Reply);
	xrtFree(pClient);
}

#endif
