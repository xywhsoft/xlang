#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_POP3_MESSAGE)

static const unsigned char __xrtPop3MessageCrlf[] = { '\r', '\n' };



/* 收集 sink 借用本次调用栈上的连续缓冲。 */
typedef struct __xpop3messagesink {
	xbuffer Buffer;
} __xpop3messagesink;



/* 保留主错误并关闭无法安全继续解析的多行会话。 */
static bool __xrtPop3MessageRecover(xpop3client* pClient)
{
	xerror* pPrimaryError = xrtTakeError();
	xerror* pCloseError;
	xpop3clientstate State = xrtPop3ClientState(pClient);

	if ( (State == XPOP3_CLIENT_MULTILINE) ||
		 (State == XPOP3_CLIENT_FAILED) ) {
		(void)xrtPop3ClientAbort(pClient);
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



/* 调用输出 sink，并在无具体错误时补充稳定的 callback 错误。 */
static bool __xrtPop3MessageOutput(
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
			"POP3 message output callback failed"
		);
	}
	return false;
}



/* 共享 RETR/TOP 的有界多行读取与 CRLF 恢复。 */
static bool __xrtPop3MessageWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	bool bTop,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	size_t iWritten = 0;
	xstrview Line;
	xmailnext Next;

	if ( (pWrite == NULL) || !xrtMemRangeValid(
		pWritten,
		pWritten != NULL ? sizeof(*pWritten) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxBytes == 0 ) {
		iMaxBytes = XPOP3_MESSAGE_BYTES_DEFAULT;
	}
	if ( bTop ) {
		if ( !xrtPop3ClientTop(
			pClient,
			iMessage,
			iLines,
			iDeadline,
			pCancel
		) ) {
			return false;
		}
	} else if ( !xrtPop3ClientRetr(
		pClient,
		iMessage,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	for ( ;; ) {
		size_t iNext;

		Next = xrtPop3ClientNext(
			pClient,
			&Line,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_END ) {
			if ( pWritten != NULL ) {
				*pWritten = iWritten;
			}
			return true;
		}
		if ( Next != XMAIL_NEXT_ITEM ) {
			return __xrtPop3MessageRecover(pClient);
		}
		if ( !__xrtMailSizeAdd(Line.Size, 2u, &iNext) ||
			 !__xrtMailSizeAdd(iWritten, iNext, &iNext) ||
			 ((iMaxBytes != SIZE_MAX) && (iNext > iMaxBytes)) ) {
			if ( xrtGetError() == NULL ) {
				__xrtMailError(
					XERR_RANGE,
					XMAIL_ERROR_LIMIT,
					"POP3 message exceeds the byte limit"
				);
			}
			return __xrtPop3MessageRecover(pClient);
		}
		if ( ((Line.Size != 0) && !__xrtPop3MessageOutput(
			pWrite,
			pUserData,
			(xbytesview){ (cbytes)Line.Data, Line.Size }
		)) || !__xrtPop3MessageOutput(
			pWrite,
			pUserData,
			(xbytesview){ __xrtPop3MessageCrlf, 2u }
		) ) {
			return __xrtPop3MessageRecover(pClient);
		}
		iWritten = iNext;
	}
}



/* 向连续缓冲追加一个已受外层预算约束的片段。 */
static bool __xrtPop3MessageBufferWrite(xbytesview Data, ptr pUserData)
{
	__xpop3messagesink* pSink = (__xpop3messagesink*)pUserData;

	return xrtBufferAppend(&pSink->Buffer, Data);
}



/* 收集 RETR/TOP 并返回末尾附零的 owned 字节。 */
static bytes __xrtPop3MessageBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	bool bTop,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xpop3messagesink Sink;
	bytes pData;
	size_t iSize;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtBufferInit(&Sink.Buffer) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return NULL;
	}
	if ( !__xrtPop3MessageWrite(
		pClient,
		iMessage,
		iLines,
		bTop,
		iMaxBytes,
		__xrtPop3MessageBufferWrite,
		&Sink,
		&iSize,
		iDeadline,
		pCancel
	) || !xrtBufferAppendByte(&Sink.Buffer, 0) ) {
		xrtBufferUnit(&Sink.Buffer);
		return NULL;
	}
	pData = xrtBufferTake(&Sink.Buffer, NULL, NULL);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iSize;
	}
	return pData;
}



/* 流式读取完整邮件。 */
XRT_API bool xrtPop3ClientRetrWrite(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageWrite(
		pClient,
		iMessage,
		0,
		false,
		iMaxBytes,
		pWrite,
		pUserData,
		pWritten,
		iDeadline,
		pCancel
	);
}



/* 流式读取 TOP 结果。 */
XRT_API bool xrtPop3ClientTopWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageWrite(
		pClient,
		iMessage,
		iLines,
		true,
		iMaxBytes,
		pWrite,
		pUserData,
		pWritten,
		iDeadline,
		pCancel
	);
}



/* 收集完整 RETR 结果。 */
XRT_API bytes xrtPop3ClientRetrBytes(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageBytes(
		pClient,
		iMessage,
		0,
		false,
		iMaxBytes,
		pOutputSize,
		iDeadline,
		pCancel
	);
}



/* 收集完整 TOP 结果。 */
XRT_API bytes xrtPop3ClientTopBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageBytes(
		pClient,
		iMessage,
		iLines,
		true,
		iMaxBytes,
		pOutputSize,
		iDeadline,
		pCancel
	);
}



/* 收集并解析一棵拥有型 MIME 树。 */
XRT_API bool xrtPop3ClientRetrTree(
	xpop3client* pClient,
	uint64 iMessage,
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
	pData = xrtPop3ClientRetrBytes(
		pClient,
		iMessage,
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
