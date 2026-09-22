#include "ssh_channel_request_internal.h"



#if defined(XSSH_FEATURE_CHANNEL_REQUEST)

/* 写入只含一个 SSH string 的 request。 */
static xsshcode xsshRequestOneStringWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	xbytesview Value
)
{
	xsshwriter Writer;
	size_t iFieldsSize = 0u;
	xsshcode Code = xsshRequestAddString(Value, &iFieldsSize);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		Type,
		bWantReply,
		iFieldsSize,
		&Value,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteString(&Writer, Value) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取只含一个 SSH string 的 request。 */
static xsshcode xsshRequestOneStringRead(
	const xsshchannelrequest* pRequest,
	const char* pType,
	size_t iTypeSize,
	int iWantReply,
	xbytesview* pValue
)
{
	xsshreader Reader;
	xbytesview Value;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		pType,
		iTypeSize,
		iWantReply,
		pValue,
		sizeof(*pValue),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pValue = Value;
	return XSSH_OK;
}



/* 校验信号名称并拒绝 RFC 明确省略的 SIG 前缀。 */
bool xrtSshChannelSignalValid(xstrview Signal)
{
	return xrtSshNameValid(Signal) &&
		!((Signal.Size >= 3u) &&
		  (memcmp(Signal.Data, "SIG", 3u) == 0));
}



/* 写入无专用字段的 shell request。 */
xsshcode xrtSshChannelShellWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SHELL),
		bWantReply,
		0u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取无专用字段的 shell request。 */
xsshcode xrtSshChannelShellRead(const xsshchannelrequest* pRequest)
{
	xsshreader Reader;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_SHELL,
		sizeof(XSSH_CHANNEL_REQUEST_SHELL) - 1u,
		-1,
		NULL,
		0u,
		&Reader
	);

	return Code == XSSH_OK ? xsshRequestReadEnd(&Reader) : Code;
}



/* 写入不限定编码的 exec command。 */
xsshcode xrtSshChannelExecWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Command
)
{
	return xsshRequestOneStringWrite(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXEC),
		bWantReply,
		Command
	);
}



/* 严格读取不限定编码的 exec command。 */
xsshcode xrtSshChannelExecRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pCommand
)
{
	return xsshRequestOneStringRead(
		pRequest,
		XSSH_CHANNEL_REQUEST_EXEC,
		sizeof(XSSH_CHANNEL_REQUEST_EXEC) - 1u,
		-1,
		pCommand
	);
}



/* 写入不限定编码的 subsystem 名称。 */
xsshcode xrtSshChannelSubsystemWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Subsystem
)
{
	return xsshRequestOneStringWrite(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SUBSYSTEM),
		bWantReply,
		Subsystem
	);
}



/* 严格读取不限定编码的 subsystem 名称。 */
xsshcode xrtSshChannelSubsystemRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pSubsystem
)
{
	return xsshRequestOneStringRead(
		pRequest,
		XSSH_CHANNEL_REQUEST_SUBSYSTEM,
		sizeof(XSSH_CHANNEL_REQUEST_SUBSYSTEM) - 1u,
		-1,
		pSubsystem
	);
}



/* 写入 env 名称和值。 */
xsshcode xrtSshChannelEnvWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Name,
	xbytesview Value
)
{
	xbytesview arrInputs[2] = { Name, Value };
	xsshwriter Writer;
	size_t iFieldsSize = 0u;
	xsshcode Code;

	if ( ((Code = xsshRequestAddString(Name, &iFieldsSize)) != XSSH_OK) ||
		((Code = xsshRequestAddString(Value, &iFieldsSize)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_ENV),
		bWantReply,
		iFieldsSize,
		arrInputs,
		2u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Name) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Value) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 env 名称和值。 */
xsshcode xrtSshChannelEnvRead(
	const xsshchannelrequest* pRequest,
	xsshchannelenv* pEnv
)
{
	xsshreader Reader;
	xsshchannelenv Env;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_ENV,
		sizeof(XSSH_CHANNEL_REQUEST_ENV) - 1u,
		-1,
		pEnv,
		sizeof(*pEnv),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Env.Name)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Env.Value)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pEnv = Env;
	return XSSH_OK;
}



/* 写入不要求回复的 xon-xoff 通知。 */
xsshcode xrtSshChannelXonXoffWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bClientCanDo
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_XON_XOFF),
		false,
		1u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteBool(&Writer, bClientCanDo) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的 xon-xoff 通知。 */
xsshcode xrtSshChannelXonXoffRead(
	const xsshchannelrequest* pRequest,
	bool* pClientCanDo
)
{
	xsshreader Reader;
	bool bClientCanDo;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_XON_XOFF,
		sizeof(XSSH_CHANNEL_REQUEST_XON_XOFF) - 1u,
		0,
		pClientCanDo,
		sizeof(*pClientCanDo),
		&Reader
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBool(&Reader, &bClientCanDo);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pClientCanDo = bClientCanDo;
	return XSSH_OK;
}



/* 写入不要求回复的终端尺寸变更。 */
xsshcode xrtSshChannelWindowChangeWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_WINDOW_CHANGE),
		false,
		16u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteU32(&Writer, iColumns) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRows) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelWidth) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelHeight) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的终端尺寸变更。 */
xsshcode xrtSshChannelWindowChangeRead(
	const xsshchannelrequest* pRequest,
	xsshchannelwindowchange* pChange
)
{
	xsshreader Reader;
	xsshchannelwindowchange Change;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_WINDOW_CHANGE,
		sizeof(XSSH_CHANNEL_REQUEST_WINDOW_CHANGE) - 1u,
		0,
		pChange,
		sizeof(*pChange),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Change.Columns)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Change.Rows)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Change.PixelWidth)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Change.PixelHeight)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pChange = Change;
	return XSSH_OK;
}



/* 写入不要求回复的信号通知。 */
xsshcode xrtSshChannelSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal
)
{
	if ( !xrtSshChannelSignalValid(Signal) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshRequestOneStringWrite(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SIGNAL),
		false,
		xsshRequestBytes(Signal)
	);
}



/* 严格读取不要求回复的信号通知。 */
xsshcode xrtSshChannelSignalRead(
	const xsshchannelrequest* pRequest,
	xstrview* pSignal
)
{
	xbytesview Value;
	xstrview Signal;
	xsshcode Code;

	if ( (pSignal == NULL) || (pRequest == NULL) ||
		xrtMemRangesOverlap(
			pRequest->Fields.Data,
			pRequest->Fields.Size,
			pSignal,
			sizeof(*pSignal)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshRequestOneStringRead(
		pRequest,
		XSSH_CHANNEL_REQUEST_SIGNAL,
		sizeof(XSSH_CHANNEL_REQUEST_SIGNAL) - 1u,
		0,
		&Value
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Signal = xsshRequestText(Value);
	if ( !xrtSshChannelSignalValid(Signal) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignal = Signal;
	return XSSH_OK;
}



/* 写入 RFC 4335 break request。 */
xsshcode xrtSshChannelBreakWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	uint32 iLengthMs
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_BREAK),
		bWantReply,
		4u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteU32(&Writer, iLengthMs) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 RFC 4335 break request。 */
xsshcode xrtSshChannelBreakRead(
	const xsshchannelrequest* pRequest,
	uint32* pLengthMs
)
{
	xsshreader Reader;
	uint32 iLengthMs;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_BREAK,
		sizeof(XSSH_CHANNEL_REQUEST_BREAK) - 1u,
		-1,
		pLengthMs,
		sizeof(*pLengthMs),
		&Reader
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iLengthMs);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pLengthMs = iLengthMs;
	return XSSH_OK;
}



/* 写入不要求回复的进程退出状态。 */
xsshcode xrtSshChannelExitStatusWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iStatus
)
{
	xsshwriter Writer;
	xsshcode Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXIT_STATUS),
		false,
		4u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteU32(&Writer, iStatus) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的进程退出状态。 */
xsshcode xrtSshChannelExitStatusRead(
	const xsshchannelrequest* pRequest,
	uint32* pStatus
)
{
	xsshreader Reader;
	uint32 iStatus;
	xsshcode Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_EXIT_STATUS,
		sizeof(XSSH_CHANNEL_REQUEST_EXIT_STATUS) - 1u,
		0,
		pStatus,
		sizeof(*pStatus),
		&Reader
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iStatus);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshRequestReadEnd(&Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pStatus = iStatus;
	return XSSH_OK;
}



/* 写入不要求回复的进程退出信号。 */
xsshcode xrtSshChannelExitSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal,
	bool bCoreDumped,
	xstrview Message,
	xstrview Language
)
{
	xbytesview arrInputs[3] = {
		xsshRequestBytes(Signal),
		xsshRequestBytes(Message),
		xsshRequestBytes(Language)
	};
	xsshwriter Writer;
	size_t iFieldsSize = 1u;
	xsshcode Code;

	if ( !xrtSshChannelSignalValid(Signal) ||
		!xrtUtf8Valid(Message, NULL) ||
		!xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshRequestAddString(arrInputs[0], &iFieldsSize)) !=
		XSSH_OK) || ((Code = xsshRequestAddString(
		arrInputs[1],
		&iFieldsSize
	)) != XSSH_OK) || ((Code = xsshRequestAddString(
		arrInputs[2],
		&iFieldsSize
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXIT_SIGNAL),
		false,
		iFieldsSize,
		arrInputs,
		3u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bCoreDumped) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[2]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取不要求回复的进程退出信号。 */
xsshcode xrtSshChannelExitSignalRead(
	const xsshchannelrequest* pRequest,
	xsshchannelexitsignal* pSignal
)
{
	xsshreader Reader;
	xsshchannelexitsignal Signal;
	xbytesview Value;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_EXIT_SIGNAL,
		sizeof(XSSH_CHANNEL_REQUEST_EXIT_SIGNAL) - 1u,
		0,
		pSignal,
		sizeof(*pSignal),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Signal.Signal = xsshRequestText(Value);
	if ( ((Code = xrtSshReadBool(&Reader, &Signal.CoreDumped)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Value)) != XSSH_OK) ) {
		return Code;
	}
	Signal.Message = xsshRequestText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Signal.Language = xsshRequestText(Value);
	if ( !xrtSshChannelSignalValid(Signal.Signal) ||
		!xrtUtf8Valid(Signal.Message, NULL) ||
		!xrtSshLanguageValid(Signal.Language) ||
		(xsshRequestReadEnd(&Reader) != XSSH_OK) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSignal = Signal;
	return XSSH_OK;
}

#endif
