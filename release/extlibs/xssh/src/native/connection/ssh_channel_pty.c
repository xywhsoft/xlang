#include "ssh_channel_request_internal.h"

#include <xrt/ssh_channel_pty.h>



#if defined(XSSH_FEATURE_CHANNEL_PTY)

/* 写入一个有参数的 terminal mode。 */
xsshcode xrtSshTerminalModeWrite(
	xsshwriter* pWriter,
	uint8 iOpcode,
	uint32 iValue
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (iOpcode == XSSH_TTY_OP_END) ||
		(iOpcode >= XSSH_TTY_OP_UNSUPPORTED_MIN) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(pWriter, 5u, NULL, 0u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, iOpcode) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iValue) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 写入 terminal mode 结束标记。 */
xsshcode xrtSshTerminalModeEnd(xsshwriter* pWriter)
{
	xsshwriter Writer;
	xsshcode Code = xrtSshWriterReserveInputs(pWriter, 1u, NULL, 0u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteByte(&Writer, XSSH_TTY_OP_END) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 完整预验证 mode stream，未知新 opcode 按 RFC 停止解析。 */
xsshcode xrtSshTerminalModesRead(
	xbytesview Modes,
	xsshterminalmodes* pModes
)
{
	xsshterminalmodes Result;
	xsshreader Reader;
	uint32 iValue;
	uint8 iOpcode;
	xsshcode Code;

	if ( (pModes == NULL) || !xrtMemRangeValid(Modes.Data, Modes.Size) ||
		xrtMemRangesOverlap(
			Modes.Data,
			Modes.Size,
			pModes,
			sizeof(*pModes)
		) || !xrtSshReaderInit(&Reader, Modes) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Result.Reader = Reader;
	Result.Count = 0u;
	Result.Index = 0u;
	Result.Unsupported = false;
	if ( Modes.Size == 0u ) {
		*pModes = Result;
		return XSSH_OK;
	}
	while ( xrtSshReaderRemaining(&Reader) != 0u ) {
		Code = xrtSshReadByte(&Reader, &iOpcode);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( iOpcode == XSSH_TTY_OP_END ) {
			if ( xrtSshReaderRemaining(&Reader) != 0u ) {
				return XSSH_ERROR_PROTOCOL;
			}
			*pModes = Result;
			return XSSH_OK;
		}
		if ( iOpcode >= XSSH_TTY_OP_UNSUPPORTED_MIN ) {
			Result.Unsupported = true;
			*pModes = Result;
			return XSSH_OK;
		}
		Code = xrtSshReadU32(&Reader, &iValue);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		(void)iValue;
		++Result.Count;
	}
	return XSSH_ERROR_PROTOCOL;
}



/* 迭代一项已经完整验证的 terminal mode。 */
bool xrtSshTerminalModesNext(
	xsshterminalmodes* pModes,
	xsshterminalmode* pMode
)
{
	xsshterminalmodes Modes;
	xsshterminalmode Mode;

	if ( (pModes == NULL) || (pMode == NULL) ||
		(pModes->Index >= pModes->Count) ) {
		return false;
	}
	Modes = *pModes;
	if ( (xrtSshReadByte(&Modes.Reader, &Mode.Opcode) != XSSH_OK) ||
		(xrtSshReadU32(&Modes.Reader, &Mode.Value) != XSSH_OK) ) {
		return false;
	}
	++Modes.Index;
	*pModes = Modes;
	*pMode = Mode;
	return true;
}



/* 写入不复制 mode stream 的 PTY request。 */
xsshcode xrtSshChannelPtyWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes
)
{
	xbytesview arrInputs[2] = { Terminal, Modes };
	xsshterminalmodes Parsed;
	xsshwriter Writer;
	size_t iFieldsSize = 16u;
	xsshcode Code;

	Code = xrtSshTerminalModesRead(Modes, &Parsed);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_PROTOCOL ? XSSH_ERROR_ARGUMENT : Code;
	}
	if ( ((Code = xsshRequestAddString(Terminal, &iFieldsSize)) != XSSH_OK) ||
		((Code = xsshRequestAddString(Modes, &iFieldsSize)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshRequestWriteBegin(
		pWriter,
		iRecipient,
		XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_PTY),
		bWantReply,
		iFieldsSize,
		arrInputs,
		2u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Terminal) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iColumns) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRows) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelWidth) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPixelHeight) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Modes) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 PTY request 并验证 mode stream。 */
xsshcode xrtSshChannelPtyRead(
	const xsshchannelrequest* pRequest,
	xsshchannelpty* pPty
)
{
	xsshterminalmodes Modes;
	xsshchannelpty Pty;
	xsshreader Reader;
	xsshcode Code;

	Code = xsshRequestReadBegin(
		pRequest,
		XSSH_CHANNEL_REQUEST_PTY,
		sizeof(XSSH_CHANNEL_REQUEST_PTY) - 1u,
		-1,
		pPty,
		sizeof(*pPty),
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Pty.Terminal)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.Columns)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.Rows)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.PixelWidth)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Pty.PixelHeight)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Pty.Modes)) != XSSH_OK) ) {
		return Code;
	}
	if ( xsshRequestReadEnd(&Reader) != XSSH_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshTerminalModesRead(Pty.Modes, &Modes);
	if ( Code != XSSH_OK ) {
		return Code == XSSH_ERROR_ARGUMENT ? Code : XSSH_ERROR_PROTOCOL;
	}
	*pPty = Pty;
	return XSSH_OK;
}

#endif
