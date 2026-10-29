#include <xrt/imap_append.h>

#include "../internal/xrt_imap_client.h"
#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP_APPEND)

/* 设置 APPEND 层稳定错误。 */
static bool __xrtImapAppendError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 解析严格的无符号十进制数。 */
static bool __xrtImapAppendNumber(xstrview Text, uint64* pValue)
{
	uint64 iValue = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint64 iDigit;

		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iDigit = (uint64)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	*pValue = iValue;
	return true;
}



/* 把 size_t 写成不依赖格式化库的 literal 标记。 */
static xstrview __xrtImapAppendMarker(
	char* sMarker,
	size_t iSize,
	bool bNonSynchronizing
)
{
	char arrDigits[(sizeof(size_t) * 3u) + 1u];
	size_t iDigits = 0;
	size_t iOffset = 0;

	do {
		arrDigits[iDigits++] = (char)('0' + (iSize % 10u));
		iSize /= 10u;
	} while ( iSize != 0 );
	sMarker[iOffset++] = '{';
	while ( iDigits != 0 ) {
		sMarker[iOffset++] = arrDigits[--iDigits];
	}
	if ( bNonSynchronizing ) {
		sMarker[iOffset++] = '+';
	}
	sMarker[iOffset++] = '}';
	return __xrtMailView(sMarker, iOffset);
}



/* 验证 Flags 的外层结构，内部语法保留给服务器和扩展处理。 */
static bool __xrtImapAppendFlagsValid(xstrview Flags)
{
	return (Flags.Size == 0) ||
		(__xrtMailViewValid(Flags) && (Flags.Size >= 2u) &&
		 (Flags.Data[0] == '(') && (Flags.Data[Flags.Size - 1u] == ')'));
}



/* 根据能力、上限和调用方策略选择 literal 形式。 */
static bool __xrtImapAppendLiteralMode(
	const ximapclient* pClient,
	const ximapappendconfig* pConfig,
	bool* pNonSynchronizing
)
{
	uint64 iCapabilities = xrtImapClientCapabilities(pClient);
	uint64 iLimit = xrtImapClientAppendLimit(pClient);
	bool bUnlimited = (iCapabilities & XIMAP_CAP_LITERAL_PLUS) != 0;
	bool bLimited = ((iCapabilities & XIMAP_CAP_LITERAL_MINUS) != 0) ||
		((iCapabilities & XIMAP_CAP_IMAP4REV2) != 0);
	bool bSupported = bUnlimited || (bLimited && (pConfig->Size <= 4096u));

	if ( (iLimit != XIMAP_APPEND_LIMIT_UNKNOWN) &&
		((iLimit == 0) || ((uint64)pConfig->Size > iLimit)) ) {
		return __xrtImapAppendError(
			XERR_RANGE,
			"IMAP APPEND exceeds the advertised upload limit"
		);
	}
	if ( pConfig->Literal == XIMAP_LITERAL_SYNC ) {
		*pNonSynchronizing = false;
		return true;
	}
	if ( pConfig->Literal == XIMAP_LITERAL_NONSYNC ) {
		if ( !bSupported ) {
			return __xrtImapAppendError(
				XERR_UNSUPPORTED,
				"IMAP server does not support this non-synchronizing literal"
			);
		}
		*pNonSynchronizing = true;
		return true;
	}
	if ( pConfig->Literal != XIMAP_LITERAL_AUTO ) {
		return __xrtImapAppendError(
			XERR_ARGUMENT,
			"invalid IMAP APPEND literal mode"
		);
	}
	*pNonSynchronizing = bSupported &&
		(iLimit != XIMAP_APPEND_LIMIT_UNKNOWN);
	return true;
}



/* 等待同步 literal continuation，允许普通未请求响应穿过。 */
static bool __xrtImapAppendWait(
	ximapclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			return __xrtImapAppendError(
				XERR_PERMISSION,
				"IMAP APPEND was rejected before literal upload"
			);
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"unexpected server literal while waiting for IMAP APPEND"
			);
		}
		if ( (Event.Kind == XIMAP_EVENT_RESPONSE) &&
			(Event.Response.Kind == XIMAP_RESPONSE_CONTINUATION) ) {
			return true;
		}
	}
}



/* 解析 tagged OK 中的单消息 APPENDUID。 */
static bool __xrtImapAppendResult(
	xstrview Text,
	ximapappendresult* pResult
)
{
	ximapcodeview Code;
	ximapatomcursor Cursor;
	xstrview UidValidity;
	xstrview Uid;
	xstrview Extra;
	xmailnext Next;

	Next = xrtImapCodeParse(Text, &Code);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( (Next == XMAIL_NEXT_END) || !__xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("APPENDUID")
	) ) {
		return true;
	}
	if ( !xrtImapAtomCursorInit(&Cursor, Code.Arguments) ||
		xrtImapAtomNext(&Cursor, &UidValidity) != XMAIL_NEXT_ITEM ||
		xrtImapAtomNext(&Cursor, &Uid) != XMAIL_NEXT_ITEM ||
		xrtImapAtomNext(&Cursor, &Extra) != XMAIL_NEXT_END ||
		!__xrtImapAppendNumber(UidValidity, &pResult->UidValidity) ||
		!__xrtImapAppendNumber(Uid, &pResult->Uid) ||
		(pResult->UidValidity == 0) || (pResult->Uid == 0) ||
		(pResult->UidValidity > UINT32_MAX) || (pResult->Uid > UINT32_MAX) ) {
		return __xrtImapAppendError(
			XERR_PROTOCOL,
			"invalid IMAP APPENDUID response code"
		);
	}
	pResult->Present = true;
	return true;
}



/* 初始化 APPEND 配置。 */
XRT_API void xrtImapAppendConfigInit(ximapappendconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Literal = XIMAP_LITERAL_AUTO;
}



/* 初始化 APPEND 结果。 */
XRT_API void xrtImapAppendResultInit(ximapappendresult* pResult)
{
	if ( !xrtMemRangeValid(pResult, sizeof(*pResult)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pResult, 0, sizeof(*pResult));
}



/* 发送 APPEND 命令头并进入受约束的 literal 写阶段。 */
XRT_API bool xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sMarker[(sizeof(size_t) * 3u) + 4u];
	xstrview Parts[4];
	str sMailbox = NULL;
	str sDate = NULL;
	size_t iCount = 0;
	bool bNonSynchronizing;
	bool bSuccess = false;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!__xrtMailViewValid(pConfig->Mailbox) ||
		(pConfig->Mailbox.Size == 0) ||
		!__xrtMailViewValid(pConfig->InternalDate) ||
		!__xrtImapAppendFlagsValid(pConfig->Flags) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (xrtImapClientState(pClient) != XIMAP_CLIENT_AUTHENTICATED) &&
		(xrtImapClientState(pClient) != XIMAP_CLIENT_SELECTED) ) {
		return __xrtImapAppendError(
			XERR_STATE,
			"IMAP APPEND requires an authenticated session"
		);
	}
	if ( !__xrtImapAppendLiteralMode(
		pClient,
		pConfig,
		&bNonSynchronizing
	) ) {
		return false;
	}
	sMailbox = xrtImapQuote(pConfig->Mailbox, &Parts[iCount].Size);
	if ( sMailbox == NULL ) {
		goto cleanup;
	}
	Parts[iCount++].Data = sMailbox;
	if ( pConfig->Flags.Size != 0 ) {
		Parts[iCount++] = pConfig->Flags;
	}
	if ( pConfig->InternalDate.Size != 0 ) {
		sDate = xrtImapQuote(pConfig->InternalDate, &Parts[iCount].Size);
		if ( sDate == NULL ) {
			goto cleanup;
		}
		Parts[iCount++].Data = sDate;
	}
	Parts[iCount++] = __xrtImapAppendMarker(
		sMarker,
		pConfig->Size,
		bNonSynchronizing
	);
	if ( !xrtImapClientBeginParts(
		pClient,
		XRT_STR_LITERAL("APPEND"),
		Parts,
		iCount,
		iDeadline,
		pCancel
	) ) {
		goto cleanup;
	}
	if ( !bNonSynchronizing && !__xrtImapAppendWait(
		pClient,
		iDeadline,
		pCancel
	) ) {
		goto cleanup;
	}
	bSuccess = __xrtImapClientAppendStart(pClient, pConfig->Size);

cleanup:
	xrtFree(sDate);
	xrtFree(sMailbox);
	return bSuccess;
}



/* 返回活动 literal 剩余字节。 */
XRT_API size_t xrtImapClientAppendRemaining(const ximapclient* pClient)
{
	return __xrtImapClientAppendRemaining(pClient);
}



/* 写入一块 APPEND literal。 */
XRT_API bool xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( (__xrtImapClientAppendRemaining(pClient) == 0) ||
		(iSize == 0) ) {
		return __xrtImapAppendError(
			XERR_STATE,
			"IMAP APPEND has no remaining literal bytes"
		);
	}
	return xrtImapClientWrite(
		pClient,
		pData,
		iSize,
		iDeadline,
		pCancel
	);
}



/* 结束 APPEND 并解析 tagged completion。 */
XRT_API bool xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	ximapappendresult Result;

	xrtImapAppendResultInit(&Result);
	if ( !xrtMemRangeValid(
		pResult,
		pResult != NULL ? sizeof(*pResult) : 0
	) || !__xrtImapClientAppendEnd(pClient) ||
		!xrtImapClientWrite(pClient, "\r\n", 2u, iDeadline, pCancel) ) {
		return false;
	}
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = xrtImapClientNext(
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
			if ( Final.Status != XIMAP_STATUS_OK ) {
				return __xrtImapAppendError(
					XERR_PERMISSION,
					"IMAP APPEND was rejected"
				);
			}
			if ( !__xrtImapAppendResult(Final.Text, &Result) ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"invalid IMAP APPEND completion"
				);
			}
			if ( pResult != NULL ) {
				*pResult = Result;
			}
			return true;
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"unexpected server literal in IMAP APPEND response"
			);
		}
	}
}



/* 执行内存消息的一次 APPEND。 */
XRT_API bool xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xrtMemRangeValid(pData, pConfig != NULL ? pConfig->Size : 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapClientAppendBegin(
		pClient,
		pConfig,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( (pConfig->Size != 0) && !xrtImapClientAppendWrite(
		pClient,
		pData,
		pConfig->Size,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	return xrtImapClientAppendEnd(
		pClient,
		pResult,
		iDeadline,
		pCancel
	);
}

#endif
