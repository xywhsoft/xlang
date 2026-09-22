#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP_MESSAGE)

#define __XRT_IMAP_MESSAGE_CHUNK (16u * 1024u)



/* BODY 请求同时保留发送属性和服务器响应属性。 */
typedef struct __ximapmessageitems {
	xstrview Command;
	xstrview Response;
	str Storage;
} __ximapmessageitems;



/* owned 字节路径借用本次调用栈上的连续缓冲。 */
typedef struct __ximapmessagebuffer {
	xbuffer Buffer;
} __ximapmessagebuffer;



/* 设置 IMAP 消息便利层的稳定错误。 */
static bool __xrtImapMessageError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 保留首个错误并关闭无法安全继续解析的活动命令。 */
static bool __xrtImapMessageRecover(
	ximapclient* pClient
)
{
	xerror* pPrimaryError = xrtTakeError();
	xerror* pCloseError;
	ximapclientstate State = xrtImapClientState(pClient);

	if ( State != XIMAP_CLIENT_CLOSED ) {
		(void)xrtImapClientAbort(pClient);
	}
	pCloseError = xrtTakeError();
	if ( pPrimaryError != NULL ) {
		xrtErrorFree(pCloseError);
		xrtSetErrorTake(pPrimaryError);
	} else {
		xrtSetErrorTake(pCloseError);
	}
	return false;
}



/* 验证 section-spec 可以安全嵌入 BODY 方括号。 */
static bool __xrtImapMessageSectionValid(xstrview Section)
{
	if ( !__xrtMailViewValid(Section) ) {
		return false;
	}
	for ( size_t i = 0; i < Section.Size; i++ ) {
		unsigned char iByte = (unsigned char)Section.Data[i];

		if ( (iByte < (unsigned char)' ') || (iByte > (unsigned char)'~') ||
			(iByte == (unsigned char)'[') || (iByte == (unsigned char)']') ) {
			return false;
		}
	}
	return true;
}



/* 构建 BODY[section] 与可选 BODY.PEEK[section] 属性。 */
static bool __xrtImapMessageItemsCreate(
	ximapclient* pClient,
	xstrview Section,
	bool bPeek,
	__ximapmessageitems* pItems
)
{
	static const char sBody[] = "BODY";
	static const char sPeek[] = ".PEEK";
	static const char sBodyResponse[] = "BODY[]";
	static const char sBodyPeek[] = "BODY.PEEK[]";
	size_t iCommand;
	size_t iResponse;
	size_t iTotal;
	char* sCommand;
	char* sResponse;
	size_t iPosition = 0;

	memset(pItems, 0, sizeof(*pItems));
	if ( (pClient == NULL) || !__xrtImapMessageSectionValid(Section) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Section.Size == 0 ) {
		pItems->Command = bPeek ?
			__xrtMailView(sBodyPeek, sizeof(sBodyPeek) - 1u) :
			__xrtMailView(sBodyResponse, sizeof(sBodyResponse) - 1u);
		pItems->Response = __xrtMailView(
			sBodyResponse,
			sizeof(sBodyResponse) - 1u
		);
		return true;
	}
	if ( Section.Size > xrtImapClientCommandLimit(pClient) ) {
		return __xrtImapMessageError(
			XERR_RANGE,
			"IMAP BODY section exceeds the command line limit"
		);
	}
	iResponse = sizeof(sBody) - 1u + 2u;
	if ( !__xrtMailSizeAdd(iResponse, Section.Size, &iResponse) ) {
		return false;
	}
	iCommand = iResponse;
	if ( bPeek && !__xrtMailSizeAdd(
		iCommand,
		sizeof(sPeek) - 1u,
		&iCommand
	) ) {
		return false;
	}
	if ( !__xrtMailSizeAdd(iCommand, iResponse, &iTotal) ) {
		return false;
	}
	pItems->Storage = (str)xrtMalloc(iTotal);
	if ( pItems->Storage == NULL ) {
		return false;
	}
	sCommand = pItems->Storage;
	sResponse = sCommand + iCommand;
	memcpy(sCommand + iPosition, sBody, sizeof(sBody) - 1u);
	iPosition += sizeof(sBody) - 1u;
	if ( bPeek ) {
		memcpy(sCommand + iPosition, sPeek, sizeof(sPeek) - 1u);
		iPosition += sizeof(sPeek) - 1u;
	}
	sCommand[iPosition++] = '[';
	if ( Section.Size != 0 ) {
		memcpy(sCommand + iPosition, Section.Data, Section.Size);
		iPosition += Section.Size;
	}
	sCommand[iPosition] = ']';
	memcpy(sResponse, sBody, sizeof(sBody) - 1u);
	sResponse[sizeof(sBody) - 1u] = '[';
	if ( Section.Size != 0 ) {
		memcpy(
			sResponse + sizeof(sBody),
			Section.Data,
			Section.Size
		);
	}
	sResponse[iResponse - 1u] = ']';
	pItems->Command = (xstrview){ sCommand, iCommand };
	pItems->Response = (xstrview){ sResponse, iResponse };
	return true;
}



/* 释放 BODY 属性临时文本。 */
static void __xrtImapMessageItemsFree(__ximapmessageitems* pItems)
{
	xrtFree(pItems->Storage);
	memset(pItems, 0, sizeof(*pItems));
}



/* 验证 literal 属于本次请求的 FETCH BODY 属性。 */
static bool __xrtImapMessageLiteralValid(
	const ximapevent* pEvent,
	xstrview Expected
)
{
	ximapfetchview Fetch;
	ximapfetchcursor Cursor;
	ximapfetchitem Item;
	xmailnext Next;

	if ( (pEvent->Kind != XIMAP_EVENT_RESPONSE) ||
		(pEvent->Response.Kind != XIMAP_RESPONSE_UNTAGGED) ||
		(pEvent->Response.Status != XIMAP_STATUS_NONE) ||
		pEvent->Literal.Binary ||
		!xrtImapFetchParse(pEvent->Response.Text, &Fetch) ||
		!xrtImapFetchCursorInit(&Cursor, &Fetch) ) {
		return __xrtImapMessageError(
			XERR_PROTOCOL,
			"invalid IMAP BODY literal response"
		);
	}
	for ( ;; ) {
		Next = xrtImapFetchNext(&Cursor, &Item);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			return __xrtImapMessageError(
				XERR_PROTOCOL,
				"IMAP FETCH response did not identify the BODY literal"
			);
		}
		if ( Item.Value.Kind == XIMAP_DATA_LITERAL ) {
			if ( !__xrtMailAsciiEqualI(Item.Attribute, Expected) ||
				(Item.Value.LiteralSize != pEvent->Literal.Size) ) {
				return __xrtImapMessageError(
					XERR_PROTOCOL,
					"IMAP FETCH returned an unexpected literal"
				);
			}
			return true;
		}
	}
}



/* 调用输出 sink，并在无具体原因时补充 callback 错误。 */
static bool __xrtImapMessageOutput(
	xmailwriteproc pWrite,
	ptr pUserData,
	xbytesview Data
)
{
	if ( pWrite(Data, pUserData) ) {
		return true;
	}
	if ( xrtGetError() == NULL ) {
		__xrtMailError(
			XERR_IO,
			XMAIL_ERROR_CALLBACK,
			"IMAP message output callback failed"
		);
	}
	return false;
}



/* 读取并直接交付当前 literal。 */
static bool __xrtImapMessageLiteralWrite(
	ximapclient* pClient,
	size_t iLiteralSize,
	xmailwriteproc pWrite,
	ptr pUserData,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	unsigned char Data[__XRT_IMAP_MESSAGE_CHUNK];
	size_t iReadTotal = 0;

	while ( iReadTotal < iLiteralSize ) {
		size_t iRead;

		if ( !xrtImapClientReadLiteral(
			pClient,
			Data,
			sizeof(Data),
			&iRead,
			iDeadline,
			pCancel
		) || !__xrtImapMessageOutput(
			pWrite,
			pUserData,
			(xbytesview){ Data, iRead }
		) ) {
			return false;
		}
		iReadTotal += iRead;
	}
	return true;
}



/* 把完成状态映射为稳定错误，并区分空结果。 */
static bool __xrtImapMessageFinish(
	ximapclient* pClient,
	bool bFound
)
{
	ximapresponseview Final;

	if ( !xrtImapClientLastResponse(pClient, &Final) ) {
		return false;
	}
	if ( Final.Status != XIMAP_STATUS_OK ) {
		return __xrtImapMessageError(
			Final.Status == XIMAP_STATUS_NO ?
				XERR_PERMISSION : XERR_PROTOCOL,
			"IMAP BODY command was rejected"
		);
	}
	if ( !bFound ) {
		return __xrtImapMessageError(
			XERR_NOT_FOUND,
			"IMAP BODY command returned no message"
		);
	}
	return true;
}



/* 向连续缓冲追加一个已受外层预算约束的片段。 */
static bool __xrtImapMessageBufferWrite(xbytesview Data, ptr pUserData)
{
	__ximapmessagebuffer* pBuffer = (__ximapmessagebuffer*)pUserData;

	return xrtBufferAppend(&pBuffer->Buffer, Data);
}



/* 流式读取一个 BODY section。 */
XRT_API bool xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__ximapmessageitems Items;
	char sMessage[10];
	xstrview Message;
	bool bFound = false;
	size_t iWritten = 0;

	if ( (iMessage == 0) || (pWrite == NULL) || !xrtMemRangeValid(
		pWritten,
		pWritten != NULL ? sizeof(*pWritten) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxBytes == 0 ) {
		iMaxBytes = XIMAP_MESSAGE_BYTES_DEFAULT;
	}
	if ( !__xrtImapMessageItemsCreate(pClient, Section, bPeek, &Items) ) {
		return false;
	}
	Message = (xstrview){
		sMessage,
		__xrtMailUint64Write(sMessage, iMessage)
	};
	if ( !xrtImapClientBeginFetch(
		pClient,
		Message,
		Items.Command,
		bUid,
		iDeadline,
		pCancel
	) ) {
		__xrtImapMessageItemsFree(&Items);
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
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
		if ( Next == XMAIL_NEXT_END ) {
			bool bSuccess = __xrtImapMessageFinish(pClient, bFound);

			__xrtImapMessageItemsFree(&Items);
			if ( bSuccess && (pWritten != NULL) ) {
				*pWritten = iWritten;
			}
			return bSuccess;
		}
		if ( !Event.HasLiteral ) {
			continue;
		}
		if ( bFound || !__xrtImapMessageLiteralValid(
			&Event,
			Items.Response
		) ) {
			if ( bFound && (xrtGetError() == NULL) ) {
				(void)__xrtImapMessageError(
					XERR_PROTOCOL,
					"IMAP BODY command returned multiple literals"
				);
			}
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
		if ( !__xrtMailSizeAdd(
			iWritten,
			Event.Literal.Size,
			&iWritten
		) || ((iMaxBytes != SIZE_MAX) && (iWritten > iMaxBytes)) ) {
			if ( xrtGetError() == NULL ) {
				__xrtMailError(
					XERR_RANGE,
					XMAIL_ERROR_LIMIT,
					"IMAP BODY literal exceeds the byte limit"
				);
			}
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
		bFound = true;
		if ( (Event.Literal.Size != 0) && !__xrtImapMessageLiteralWrite(
			pClient,
			Event.Literal.Size,
			pWrite,
			pUserData,
			iDeadline,
			pCancel
		) ) {
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
	}
}



/* 收集一个 BODY section 并附加零字节。 */
XRT_API bytes xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__ximapmessagebuffer Buffer;
	bytes pData;
	size_t iSize;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtBufferInit(&Buffer.Buffer) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return NULL;
	}
	if ( !xrtImapClientBodyWrite(
		pClient,
		iMessage,
		Section,
		bUid,
		bPeek,
		iMaxBytes,
		__xrtImapMessageBufferWrite,
		&Buffer,
		&iSize,
		iDeadline,
		pCancel
	) || !xrtBufferAppendByte(&Buffer.Buffer, 0) ) {
		xrtBufferUnit(&Buffer.Buffer);
		return NULL;
	}
	pData = xrtBufferTake(&Buffer.Buffer, NULL, NULL);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iSize;
	}
	return pData;
}



/* 收集完整 BODY[] 并解析为拥有型 MIME 树。 */
XRT_API bool xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xmailtreelimits Limits;
	bytes pData;
	size_t iSize;
	bool bResult;

	if ( !xrtMemRangeValid(pTree, sizeof(*pTree)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pLimits != NULL ) {
		if ( !xrtMailTreeLimitsValid(pLimits) ) {
			return false;
		}
		Limits = *pLimits;
	} else {
		xrtMailTreeLimitsInit(&Limits);
	}
	pData = xrtImapClientBodyBytes(
		pClient,
		iMessage,
		XRT_STR_LITERAL(""),
		bUid,
		bPeek,
		Limits.MaxSourceBytes,
		&iSize,
		iDeadline,
		pCancel
	);
	if ( pData == NULL ) {
		return false;
	}
	bResult = xrtMailTreeParse(
		(xstrview){ (cstr)pData, iSize },
		&Limits,
		pTree
	);
	xrtFree(pData);
	return bResult;
}

#endif
