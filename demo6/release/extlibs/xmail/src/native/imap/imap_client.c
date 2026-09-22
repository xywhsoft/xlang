#include <xrt/imap_client.h>

#include "../internal/xrt_imap_client.h"
#include "../internal/xrt_mail_net.h"



#if defined(XMAIL_FEATURE_IMAP_CLIENT)

/* 客户端只保存协议状态、能力快照和当前顺序命令，不缓存完整响应。 */
struct ximapclient {
	__xmailtransport Transport;
	__xmailtext Last;
	ximapclientstate State;
	uint64 Capabilities;
	uint64 AppendLimit;
	size_t CommandLineLimit;
	size_t LiteralRemaining;
	size_t AppendRemaining;
	uint32 TagCounter;
	char ActiveTag[XIMAP_CLIENT_TAG_MAX + 1u];
	size_t ActiveTagSize;
	bool Active;
	bool ExpectFragment;
	bool Idle;
	bool IdleDone;
	bool Append;
};



/* 设置 IMAP 客户端稳定错误。 */
static bool __xrtImapClientError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 验证客户端仍可交换协议数据。 */
static bool __xrtImapClientUsable(const ximapclient* pClient)
{
	if ( (pClient == NULL) || (pClient->State == XIMAP_CLIENT_CLOSED) ||
		(pClient->State == XIMAP_CLIENT_FAILED) ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP client is not usable"
		);
	}
	return true;
}



/* 把不可恢复的传输或协议失败记录为终态。 */
static bool __xrtImapClientFailed(ximapclient* pClient)
{
	if ( pClient != NULL ) {
		pClient->State = XIMAP_CLIENT_FAILED;
	}
	return false;
}



/* 比较两个 IMAP tag；tag 按字节区分大小写。 */
static bool __xrtImapClientTagEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 保存 greeting 或 tagged completion，供错误诊断和高层状态转换使用。 */
static bool __xrtImapClientResponseSave(
	ximapclient* pClient,
	const ximapresponseview* pResponse
)
{
	return __xrtMailTextSet(&pClient->Last, pResponse->Source);
}



/* 生成固定宽度十六进制 tag，避免每条命令分配内存。 */
static xstrview __xrtImapClientTagNext(ximapclient* pClient)
{
	static const char sHex[] = "0123456789ABCDEF";
	uint32 iValue;

	pClient->TagCounter++;
	if ( pClient->TagCounter == 0 ) {
		pClient->TagCounter = 1;
	}
	iValue = pClient->TagCounter;
	pClient->ActiveTag[0] = 'A';
	for ( size_t i = 0; i < 8u; i++ ) {
		pClient->ActiveTag[8u - i] = sHex[iValue & 0x0Fu];
		iValue >>= 4u;
	}
	pClient->ActiveTag[9] = 0;
	pClient->ActiveTagSize = 9u;
	return __xrtMailView(pClient->ActiveTag, pClient->ActiveTagSize);
}



/* 验证参数片只含可直接放入命令行的可见字节。 */
static bool __xrtImapClientPartValid(xstrview Part)
{
	if ( !__xrtMailViewValid(Part) || (Part.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Part.Size; i++ ) {
		unsigned char iByte = (unsigned char)Part.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 验证线路可发送后，逐片写出完整 tagged 命令。 */
static bool __xrtImapClientSendCommandParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Tag) || !__xrtMailViewValid(Command) ||
		(iCount > (SIZE_MAX / sizeof(*pArguments))) ||
		!xrtMemRangeValid(pArguments, iCount * sizeof(*pArguments)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapAtomValid(Tag) || (Tag.Data[0] == '+') ||
		!xrtImapAtomValid(Command) ) {
		return __xrtImapClientError(XERR_ARGUMENT, "invalid IMAP command");
	}
	if ( !__xrtMailSizeAdd(Tag.Size, 1u, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, Command.Size, &iRequired) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"IMAP command size overflow"
		);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtImapClientPartValid(pArguments[i]) ) {
			return __xrtImapClientError(
				XERR_ARGUMENT,
				"invalid IMAP command argument"
			);
		}
		if ( !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
			!__xrtMailSizeAdd(
				iRequired,
				pArguments[i].Size,
				&iRequired
			) ) {
			return __xrtImapClientError(
				XERR_RANGE,
				"IMAP command size overflow"
			);
		}
	}
	if ( !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ||
		(iRequired > pClient->CommandLineLimit) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"IMAP command exceeds the line limit"
		);
	}
	if ( !__xrtMailTransportWrite(
		&pClient->Transport,
		Tag.Data,
		Tag.Size,
		false,
		iDeadline,
		pCancel
	) || !__xrtMailTransportWrite(
		&pClient->Transport,
		" ",
		1u,
		false,
		iDeadline,
		pCancel
	) || !__xrtMailTransportWrite(
		&pClient->Transport,
		Command.Data,
		Command.Size,
		false,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtMailTransportWrite(
			&pClient->Transport,
			" ",
			1u,
			false,
			iDeadline,
			pCancel
		) || !__xrtMailTransportWrite(
			&pClient->Transport,
			pArguments[i].Data,
			pArguments[i].Size,
			false,
			iDeadline,
			pCancel
		) ) {
			return __xrtImapClientFailed(pClient);
		}
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	return true;
}



/* 把旧的单参数入口映射到分片发送核心。 */
static bool __xrtImapClientSendCommand(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapClientSendCommandParts(
		pClient,
		Tag,
		Command,
		Arguments.Size != 0 ? &Arguments : NULL,
		Arguments.Size != 0 ? 1u : 0,
		iDeadline,
		pCancel
	);
}



/* 把一条 CAPABILITY 响应合并到局部快照。 */
static bool __xrtImapClientCapabilityLine(
	xstrview Text,
	uint64* pCapabilities,
	uint64* pAppendLimit,
	bool* pSeen
)
{
	ximapatomcursor Cursor;
	xstrview Atom;
	xmailnext Next;

	if ( !xrtImapAtomCursorInit(&Cursor, Text) ||
		xrtImapAtomNext(&Cursor, &Atom) != XMAIL_NEXT_ITEM ) {
		return false;
	}
	if ( !__xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("CAPABILITY")) ) {
		return true;
	}
	*pSeen = true;
	for ( ;; ) {
		Next = xrtImapAtomNext(&Cursor, &Atom);
		if ( Next == XMAIL_NEXT_END ) {
			return true;
		}
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		*pCapabilities |= xrtImapCapability(Atom);
		if ( (Atom.Size > sizeof("APPENDLIMIT=") - 1u) &&
			__xrtMailAsciiEqualI(
				__xrtMailSlice(
					Atom,
					0,
					sizeof("APPENDLIMIT=") - 1u
				),
				XRT_STR_LITERAL("APPENDLIMIT=")
			) ) {
			uint64 iLimit = 0;

			for ( size_t i = sizeof("APPENDLIMIT=") - 1u;
				i < Atom.Size; i++ ) {
				uint64 iDigit;

				if ( (Atom.Data[i] < '0') || (Atom.Data[i] > '9') ) {
					return __xrtImapClientError(
						XERR_PROTOCOL,
						"invalid IMAP APPENDLIMIT capability"
					);
				}
				iDigit = (uint64)(Atom.Data[i] - '0');
				if ( iLimit > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
					return __xrtImapClientError(
						XERR_RANGE,
						"IMAP APPENDLIMIT capability is too large"
					);
				}
				iLimit = (iLimit * UINT64_C(10)) + iDigit;
			}
			*pAppendLimit = iLimit;
		}
	}
}



/* 等待当前顺序命令完成，不接受 literal，并返回最终状态。 */
static bool __xrtImapClientFinishSimple(
	ximapclient* pClient,
	bool bCapabilities,
	uint64* pCapabilities,
	uint64* pAppendLimit,
	bool* pCapabilitySeen,
	bool* pBye,
	ximapstatus* pStatus,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	ximapevent Event;
	xmailnext Next;

	for ( ;; ) {
		Next = xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			*pStatus = Final.Status;
			return true;
		}
		if ( Event.HasLiteral ) {
			(void)__xrtImapClientError(
				XERR_PROTOCOL,
				"unexpected IMAP literal in a simple command"
			);
			return __xrtImapClientFailed(pClient);
		}
		if ( Event.Kind != XIMAP_EVENT_RESPONSE ) {
			continue;
		}
		if ( (pBye != NULL) &&
			(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
			(Event.Response.Status == XIMAP_STATUS_BYE) ) {
			*pBye = true;
		}
		if ( bCapabilities &&
			(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
			(Event.Response.Status == XIMAP_STATUS_NONE) &&
			!__xrtImapClientCapabilityLine(
				Event.Response.Text,
				pCapabilities,
				pAppendLimit,
				pCapabilitySeen
			) ) {
			return __xrtImapClientFailed(pClient);
		}
	}
}



/* 初始化 IMAP 客户端配置。 */
XRT_API void xrtImapClientConfigInit(ximapclientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtMailNetConfigInit(&pConfig->Net);
	pConfig->Net.Port = 143u;
	pConfig->Net.LineLimit = XIMAP_COMMAND_LINE_DEFAULT;
	pConfig->CommandLineLimit = XIMAP_COMMAND_LINE_DEFAULT;
}



/* 验证 IMAP 客户端配置。 */
XRT_API bool xrtImapClientConfigValid(const ximapclientconfig* pConfig)
{
	size_t iLimit;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iLimit = pConfig->CommandLineLimit != 0 ?
		pConfig->CommandLineLimit : XIMAP_COMMAND_LINE_DEFAULT;
	if ( (iLimit < 16u) || (iLimit == SIZE_MAX) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"invalid IMAP command line limit"
		);
	}
	return xrtMailNetConfigValid(&pConfig->Net);
}



/* 建立 IMAP 会话并完成 greeting、CAPABILITY 与可选 STARTTLS。 */
XRT_API ximapclient* xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	ximapclient* pClient;
	ximapevent Event;
	ximapstatus Status = XIMAP_STATUS_NONE;

	if ( !xrtImapClientConfigValid(pConfig) ) {
		return NULL;
	}
	pClient = (ximapclient*)xrtCalloc(1u, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->State = XIMAP_CLIENT_NOT_AUTHENTICATED;
	pClient->AppendLimit = XIMAP_APPEND_LIMIT_UNKNOWN;
	pClient->CommandLineLimit = pConfig->CommandLineLimit != 0 ?
		pConfig->CommandLineLimit : XIMAP_COMMAND_LINE_DEFAULT;
	if ( !__xrtMailTransportOpen(
		&pClient->Transport,
		&pConfig->Net,
		iDeadline,
		pCancel
	) || !xrtImapClientReceive(
		pClient,
		&Event,
		iDeadline,
		pCancel
	) ) {
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	if ( (Event.Kind != XIMAP_EVENT_RESPONSE) || Event.HasLiteral ||
		(Event.Response.Kind != XIMAP_RESPONSE_UNTAGGED) ||
		((Event.Response.Status != XIMAP_STATUS_OK) &&
		 (Event.Response.Status != XIMAP_STATUS_PREAUTH)) ||
		!__xrtImapClientResponseSave(pClient, &Event.Response) ) {
		(void)__xrtImapClientError(
			XERR_PROTOCOL,
			"invalid IMAP server greeting"
		);
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	pClient->State = Event.Response.Status == XIMAP_STATUS_PREAUTH ?
		XIMAP_CLIENT_AUTHENTICATED : XIMAP_CLIENT_NOT_AUTHENTICATED;
	if ( !xrtImapClientRefresh(pClient, iDeadline, pCancel) ) {
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->Net.Security == XMAIL_SECURITY_STARTTLS ) {
		if ( (pClient->State != XIMAP_CLIENT_NOT_AUTHENTICATED) ||
			((pClient->Capabilities & XIMAP_CAP_STARTTLS) == 0) ) {
			(void)__xrtImapClientError(
				XERR_UNSUPPORTED,
				"IMAP server does not permit STARTTLS in this session"
			);
			xrtImapClientDestroy(pClient);
			return NULL;
		}
		if ( !xrtImapClientBegin(
			pClient,
			XRT_STR_LITERAL("STARTTLS"),
			XRT_STR_LITERAL(""),
			iDeadline,
			pCancel
		) || !__xrtImapClientFinishSimple(
			pClient,
			false,
			NULL,
			NULL,
			NULL,
			NULL,
			&Status,
			iDeadline,
			pCancel
		) ) {
			xrtImapClientDestroy(pClient);
			return NULL;
		}
		if ( Status != XIMAP_STATUS_OK ) {
			(void)__xrtImapClientError(
				XERR_PERMISSION,
				"IMAP STARTTLS was rejected"
			);
			xrtImapClientDestroy(pClient);
			return NULL;
		}
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
			if ( !__xrtMailTransportStartTls(
				&pClient->Transport,
				&pConfig->Net,
				iDeadline,
				pCancel
			) ) {
				xrtImapClientDestroy(pClient);
				return NULL;
			}
		#else
			xrtImapClientDestroy(pClient);
			return NULL;
		#endif
		pClient->Capabilities = 0;
		if ( !xrtImapClientRefresh(pClient, iDeadline, pCancel) ) {
			xrtImapClientDestroy(pClient);
			return NULL;
		}
	}
	return pClient;
}



/* 返回 IMAP 客户端状态。 */
XRT_API ximapclientstate xrtImapClientState(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->State : XIMAP_CLIENT_FAILED;
}



/* 返回能力快照。 */
XRT_API uint64 xrtImapClientCapabilities(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->Capabilities : 0;
}



/* 返回实际传输安全级别。 */
XRT_API xmailsecurity xrtImapClientSecurity(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->Transport.Security :
		XMAIL_SECURITY_PLAIN;
}



#if defined(XMAIL_FEATURE_IMAP_COMPRESS)
/* 查询 IMAP 客户端内部传输的压缩状态。 */
bool __xrtImapClientCompressed(const ximapclient* pClient)
{
	return (pClient != NULL) &&
		__xrtMailTransportDeflated(&pClient->Transport);
}



/* 在 COMPRESS completion 后安装 raw DEFLATE 传输层。 */
bool __xrtImapClientCompressStart(
	ximapclient* pClient,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
)
{
	if ( !__xrtImapClientUsable(pClient) || pClient->Active ||
		pClient->Append || pClient->Idle ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP client is not ready to enable compression"
		);
	}
	if ( !__xrtMailTransportDeflateStart(
		&pClient->Transport,
		pDeflate,
		pInflate
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	return true;
}
#endif



/* 返回当前自动 tag。 */
XRT_API xstrview xrtImapClientTag(const ximapclient* pClient)
{
	return (pClient != NULL) && pClient->Active ?
		__xrtMailView(pClient->ActiveTag, pClient->ActiveTagSize) :
		__xrtMailView(NULL, 0);
}



/* 返回当前 literal 剩余字节数。 */
XRT_API size_t xrtImapClientLiteralRemaining(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->LiteralRemaining : 0;
}



/* 返回命令线路预算。 */
XRT_API size_t xrtImapClientCommandLimit(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->CommandLineLimit : 0;
}



/* 返回当前全局 APPEND 上传上限。 */
XRT_API uint64 xrtImapClientAppendLimit(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->AppendLimit :
		XIMAP_APPEND_LIMIT_UNKNOWN;
}



/* 解析最近保存的稳定响应。 */
XRT_API bool xrtImapClientLastResponse(
	const ximapclient* pClient,
	ximapresponseview* pResponse
)
{
	xstrview Line;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pResponse, sizeof(*pResponse)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Line = __xrtMailView(pClient->Last.Data, pClient->Last.Size);
	return (Line.Data != NULL) && xrtImapResponseParse(Line, pResponse);
}



/* 发送显式 tag 的低层命令。 */
XRT_API bool xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	return __xrtImapClientSendCommand(
		pClient,
		Tag,
		Command,
		Arguments,
		iDeadline,
		pCancel
	);
}



/* 发送显式 tag 的零拼接多参数命令。 */
XRT_API bool xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	return __xrtImapClientSendCommandParts(
		pClient,
		Tag,
		Command,
		pArguments,
		iCount,
		iDeadline,
		pCancel
	);
}



/* 发送原始命令续传字节。 */
XRT_API bool xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pClient->LiteralRemaining != 0) || pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP server literal response must be consumed first"
		);
	}
	if ( pClient->Append && (iSize > pClient->AppendRemaining) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"IMAP APPEND write exceeds the remaining literal size"
		);
	}
	if ( !__xrtMailTransportWrite(
		&pClient->Transport,
		pData,
		iSize,
		!pClient->Append,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( pClient->Append ) {
		pClient->AppendRemaining -= iSize;
	}
	return true;
}



/* 发送不带 tag 的单行 continuation 数据。 */
XRT_API bool xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtMailViewValid(Data) ||
		(Data.Size > (pClient != NULL ?
		 pClient->CommandLineLimit - 2u : 0)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Data.Size; i++ ) {
		unsigned char iByte = (unsigned char)Data.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ) {
			return __xrtImapClientError(
				XERR_ARGUMENT,
				"IMAP continuation contains a line separator"
			);
		}
	}
	return xrtImapClientWrite(
		pClient,
		Data.Data,
		Data.Size,
		iDeadline,
		pCancel
	) && xrtImapClientWrite(
		pClient,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	);
}



/* 读取并分类下一条 IMAP 响应线路。 */
XRT_API bool xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;
	xmailnext Literal;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pEvent, sizeof(*pEvent)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->LiteralRemaining != 0 ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP literal bytes remain unread"
		);
	}
	if ( !__xrtMailTransportLine(
		&pClient->Transport,
		&Line,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->Source = Line;
	if ( pClient->ExpectFragment ) {
		pEvent->Kind = XIMAP_EVENT_FRAGMENT;
		pClient->ExpectFragment = false;
	} else {
		pEvent->Kind = XIMAP_EVENT_RESPONSE;
		if ( !xrtImapResponseParse(Line, &pEvent->Response) ) {
			return __xrtImapClientFailed(pClient);
		}
	}
	Literal = xrtImapLiteralParse(Line, &pEvent->Literal);
	if ( Literal == XMAIL_NEXT_ERROR ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( Literal == XMAIL_NEXT_ITEM ) {
		pEvent->HasLiteral = true;
		pClient->LiteralRemaining = pEvent->Literal.Size;
		if ( pClient->LiteralRemaining == 0 ) {
			pClient->ExpectFragment = true;
		}
	}
	if ( (pEvent->Kind == XIMAP_EVENT_RESPONSE) &&
		(pEvent->Response.Kind == XIMAP_RESPONSE_TAGGED) &&
		!__xrtImapClientResponseSave(pClient, &pEvent->Response) ) {
		return __xrtImapClientFailed(pClient);
	}
	return true;
}



/* 按调用方容量流式读取 literal。 */
XRT_API bool xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	size_t iRequest;
	size_t iRead;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pBuffer, iCapacity) || (iCapacity == 0) ||
		!xrtMemRangeValid(pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(pRead, sizeof(*pRead), pBuffer, iCapacity) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->LiteralRemaining == 0 ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP literal is not active"
		);
	}
	iRequest = pClient->LiteralRemaining < iCapacity ?
		pClient->LiteralRemaining : iCapacity;
	if ( !__xrtMailTransportRead(
		&pClient->Transport,
		pBuffer,
		iRequest,
		&iRead,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( (iRead == 0) || (iRead > iRequest) ) {
		(void)__xrtImapClientError(
			XERR_PROTOCOL,
			"invalid IMAP literal transport progress"
		);
		return __xrtImapClientFailed(pClient);
	}
	pClient->LiteralRemaining -= iRead;
	if ( pClient->LiteralRemaining == 0 ) {
		pClient->ExpectFragment = true;
	}
	*pRead = iRead;
	return true;
}



/* 开始一个由客户端管理 tag 的顺序命令。 */
XRT_API bool xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Tag;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	Tag = __xrtImapClientTagNext(pClient);
	if ( !__xrtImapClientSendCommand(
		pClient,
		Tag,
		Command,
		Arguments,
		iDeadline,
		pCancel
	) ) {
		pClient->ActiveTagSize = 0;
		return false;
	}
	pClient->Active = true;
	return true;
}



/* 开始由客户端管理 tag 的零拼接多参数命令。 */
XRT_API bool xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Tag;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	Tag = __xrtImapClientTagNext(pClient);
	if ( !__xrtImapClientSendCommandParts(
		pClient,
		Tag,
		Command,
		pArguments,
		iCount,
		iDeadline,
		pCancel
	) ) {
		pClient->ActiveTagSize = 0;
		return false;
	}
	pClient->Active = true;
	return true;
}



/* 读取顺序命令事件并识别匹配 tag 的 completion。 */
XRT_API xmailnext xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Active;

	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		pClient->Append ) {
		(void)__xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command is not active"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( !xrtMemRangeValid(pEvent, sizeof(*pEvent)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( !xrtImapClientReceive(
		pClient,
		pEvent,
		iDeadline,
		pCancel
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( (pEvent->Kind != XIMAP_EVENT_RESPONSE) ||
		(pEvent->Response.Kind != XIMAP_RESPONSE_TAGGED) ) {
		return XMAIL_NEXT_ITEM;
	}
	Active = __xrtMailView(pClient->ActiveTag, pClient->ActiveTagSize);
	if ( !__xrtImapClientTagEqual(pEvent->Response.Tag, Active) ) {
		return XMAIL_NEXT_ITEM;
	}
	pClient->Active = false;
	pClient->ActiveTagSize = 0;
	pClient->Idle = false;
	pClient->IdleDone = false;
	return XMAIL_NEXT_END;
}



/* 激活受长度约束的 APPEND literal 写入。 */
bool __xrtImapClientAppendStart(ximapclient* pClient, size_t iSize)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		pClient->Append || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP APPEND literal is not ready to start"
		);
	}
	pClient->Append = true;
	pClient->AppendRemaining = iSize;
	return true;
}



/* 返回当前 APPEND literal 尚未写入的字节数。 */
size_t __xrtImapClientAppendRemaining(const ximapclient* pClient)
{
	return (pClient != NULL) && pClient->Append ?
		pClient->AppendRemaining : 0;
}



/* 在 literal 完整写入后关闭 APPEND 写阶段。 */
bool __xrtImapClientAppendEnd(ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		!pClient->Append ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP APPEND literal is not active"
		);
	}
	if ( pClient->AppendRemaining != 0 ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP APPEND literal is incomplete"
		);
	}
	pClient->Append = false;
	return true;
}



/* 在 tagged exchange 完成后提交认证或邮箱选择状态。 */
bool __xrtImapClientStateCommit(
	ximapclient* pClient,
	ximapclientstate State
)
{
	if ( !__xrtImapClientUsable(pClient) ||
		((State != XIMAP_CLIENT_AUTHENTICATED) &&
		 (State != XIMAP_CLIENT_SELECTED)) ||
		pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP client state is not ready to commit"
		);
	}
	pClient->State = State;
	return true;
}



/* 让扩展命令层以统一错误和终态结束不可恢复的协议失败。 */
bool __xrtImapClientProtocolFail(
	ximapclient* pClient,
	cstr sMessage
)
{
	(void)__xrtImapClientError(XERR_PROTOCOL, sMessage);
	return __xrtImapClientFailed(pClient);
}



/* 把刚开始的顺序命令标记为 IDLE。 */
bool __xrtImapClientIdleStart(ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		pClient->Idle ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP IDLE command is not ready to start"
		);
	}
	pClient->Idle = true;
	pClient->IdleDone = false;
	return true;
}



/* 验证活动 IDLE 并原子记录 DONE 已发送。 */
bool __xrtImapClientIdleEnd(ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		!pClient->Idle || pClient->IdleDone ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP IDLE command is not waiting for DONE"
		);
	}
	pClient->IdleDone = true;
	return true;
}



/* 重新获取并原子替换能力快照。 */
XRT_API bool xrtImapClientRefresh(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	uint64 iCapabilities = 0;
	uint64 iAppendLimit = XIMAP_APPEND_LIMIT_UNKNOWN;
	bool bSeen = false;
	ximapstatus Status = XIMAP_STATUS_NONE;

	if ( !xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("CAPABILITY"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) || !__xrtImapClientFinishSimple(
		pClient,
		true,
		&iCapabilities,
		&iAppendLimit,
		&bSeen,
		NULL,
		&Status,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( (Status != XIMAP_STATUS_OK) || !bSeen ||
		((iCapabilities &
		 (XIMAP_CAP_IMAP4REV1 | XIMAP_CAP_IMAP4REV2)) == 0) ) {
		return __xrtImapClientError(
			XERR_PROTOCOL,
			"IMAP CAPABILITY response is missing or rejected"
		);
	}
	pClient->Capabilities = iCapabilities;
	pClient->AppendLimit = iAppendLimit;
	return true;
}



/* 完成 LOGOUT 并关闭传输。 */
XRT_API bool xrtImapClientLogout(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	bool bBye = false;
	ximapstatus Status = XIMAP_STATUS_NONE;
	bool bSuccess;

	if ( !xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("LOGOUT"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) || !__xrtImapClientFinishSimple(
		pClient,
		false,
		NULL,
		NULL,
		NULL,
		&bBye,
		&Status,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !bBye || (Status != XIMAP_STATUS_OK) ) {
		(void)__xrtImapClientError(
			XERR_PROTOCOL,
			"invalid IMAP LOGOUT response"
		);
		return __xrtImapClientFailed(pClient);
	}
	bSuccess = __xrtMailTransportClose(&pClient->Transport, iDeadline);
	pClient->State = bSuccess ? XIMAP_CLIENT_CLOSED : XIMAP_CLIENT_FAILED;
	return bSuccess;
}



/* 不发送协议命令，直接正常关闭传输。 */
XRT_API bool xrtImapClientClose(
	ximapclient* pClient,
	xdeadline iDeadline
)
{
	bool bSuccess;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	bSuccess = __xrtMailTransportClose(&pClient->Transport, iDeadline);
	pClient->State = bSuccess ? XIMAP_CLIENT_CLOSED : XIMAP_CLIENT_FAILED;
	return bSuccess;
}



/* 无等待异常中止 IMAP 连接，保留失败终态和响应诊断。 */
XRT_API bool xrtImapClientAbort(ximapclient* pClient)
{
	bool bFailed;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State == XIMAP_CLIENT_CLOSED ) {
		return true;
	}
	bFailed = pClient->State == XIMAP_CLIENT_FAILED;
	if ( !__xrtMailTransportAbort(&pClient->Transport) ) {
		pClient->State = XIMAP_CLIENT_FAILED;
		return false;
	}
	if ( !bFailed ) {
		pClient->State = XIMAP_CLIENT_CLOSED;
	}
	return true;
}



/* 释放 IMAP 客户端及所有内部缓冲。 */
XRT_API void xrtImapClientDestroy(ximapclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	__xrtMailTextDestroy(&pClient->Last);
	__xrtMailTransportDestroy(&pClient->Transport);
	memset(pClient, 0, sizeof(*pClient));
	xrtFree(pClient);
}

#endif
