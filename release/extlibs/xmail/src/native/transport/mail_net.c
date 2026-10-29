#include "../internal/xrt_mail_net.h"



#if defined(XMAIL_FEATURE_MAIL_NET)

/* 保存一段末尾补零的动态文本，并复用已有容量。 */
bool __xrtMailTextSet(__xmailtext* pText, xstrview Value)
{
	char* sText;

	if ( !xrtMemRangeValid(pText, sizeof(*pText)) ||
		!__xrtMailViewValid(Value) || (Value.Size >= SIZE_MAX) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Value.Size + 1u > pText->Capacity ) {
		size_t iCapacity = pText->Capacity != 0 ? pText->Capacity : 64u;

		while ( iCapacity < Value.Size + 1u ) {
			size_t iNext = iCapacity <= (SIZE_MAX / 2u) ?
				iCapacity * 2u : Value.Size + 1u;

			if ( iNext <= iCapacity ) {
				iCapacity = Value.Size + 1u;
				break;
			}
			iCapacity = iNext;
		}
		sText = (char*)xrtRealloc(pText->Data, iCapacity);
		if ( sText == NULL ) {
			return false;
		}
		pText->Data = sText;
		pText->Capacity = iCapacity;
	}
	if ( Value.Size != 0 ) {
		memcpy(pText->Data, Value.Data, Value.Size);
	}
	pText->Data[Value.Size] = 0;
	pText->Size = Value.Size;
	return true;
}



/* 释放协议客户端共享的动态文本。 */
void __xrtMailTextDestroy(__xmailtext* pText)
{
	if ( pText == NULL ) {
		return;
	}
	xrtFree(pText->Data);
	memset(pText, 0, sizeof(*pText));
}




/* 返回有界零结尾主机名长度。 */
static bool __xrtMailNetHostSize(cstr sHost, size_t* pSize)
{
	if ( !xrtMemRangeValid(sHost, 1u) ) {
		return false;
	}
	for ( size_t i = 0; i <= XMAIL_NET_HOST_MAX; i++ ) {
		if ( sHost[i] == 0 ) {
			*pSize = i;
			return i != 0;
		}
	}
	return false;
}



/* 初始化邮件网络配置。 */
XRT_API void xrtMailNetConfigInit(xmailnetconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Security = XMAIL_SECURITY_PLAIN;
	pConfig->LineLimit = XMAIL_WIRE_LINE_DEFAULT;
	pConfig->ReadChunk = XMAIL_NET_READ_CHUNK_DEFAULT;
	pConfig->WriteChunk = XMAIL_NET_WRITE_CHUNK_DEFAULT;
	xrtNetDialConfigInit(&pConfig->Dial);
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		xrtTlsClientConfigInit(&pConfig->Tls);
		xrtTlsStreamConfigInit(&pConfig->TlsStream);
		pConfig->TlsTimeout = pConfig->Dial.Timeout;
	#endif
}



/* 验证邮件网络配置。 */
XRT_API bool xrtMailNetConfigValid(const xmailnetconfig* pConfig)
{
	size_t iHostSize;
	size_t iLineLimit;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		(pConfig->Engine == NULL) || (pConfig->Resolver == NULL) ||
		!__xrtMailNetHostSize(pConfig->Host, &iHostSize) ||
		(pConfig->Port == 0) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid mail network owner, host or port"
		);
		return false;
	}
	(void)iHostSize;
	if ( (pConfig->Security != XMAIL_SECURITY_PLAIN) &&
		(pConfig->Security != XMAIL_SECURITY_TLS) &&
		(pConfig->Security != XMAIL_SECURITY_STARTTLS) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid mail network security mode"
		);
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( (pConfig->Security != XMAIL_SECURITY_PLAIN) &&
			(pConfig->Tls.Verifier == NULL) ) {
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_CONFIG,
				"verified TLS requires a mail network verifier"
			);
			return false;
		}
	#else
		if ( pConfig->Security != XMAIL_SECURITY_PLAIN ) {
			__xrtMailError(
				XERR_UNSUPPORTED,
				XMAIL_ERROR_CONFIG,
				"mail TLS support is not enabled"
			);
			return false;
		}
	#endif
	iLineLimit = pConfig->LineLimit != 0 ?
		pConfig->LineLimit : XMAIL_WIRE_LINE_DEFAULT;
	if ( (pConfig->ReadChunk == 0) || (pConfig->WriteChunk == 0) ||
		(pConfig->ReadChunk > (SIZE_MAX - 2u)) ||
		(iLineLimit > (SIZE_MAX - pConfig->ReadChunk - 2u)) ||
		(pConfig->WriteChunk > pConfig->Dial.Stream.WriteLimit) ||
		!xrtNetDialConfigValid(&pConfig->Dial) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"invalid mail network buffering or dial limit"
		);
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( (pConfig->Security != XMAIL_SECURITY_PLAIN) &&
			(pConfig->WriteChunk > pConfig->TlsStream.AsyncBytesLimit) ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail TLS write chunk exceeds the asynchronous byte limit"
			);
			return false;
		}
	#endif
	return true;
}



/* 打开明文或隐式 TLS 传输；STARTTLS 首先建立明文连接。 */
bool __xrtMailTransportOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMailNetConfigValid(pConfig) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(pTransport, 0, sizeof(*pTransport));
	pTransport->LineLimit = pConfig->LineLimit != 0 ?
		pConfig->LineLimit : XMAIL_WIRE_LINE_DEFAULT;
	pTransport->ReadChunk = pConfig->ReadChunk;
	pTransport->WriteChunk = pConfig->WriteChunk;
	pTransport->Security = pConfig->Security;
	if ( pConfig->Security == XMAIL_SECURITY_TLS ) {
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
			return __xrtMailTransportTlsOpen(
				pTransport,
				pConfig,
				iDeadline,
				pCancel
			);
		#else
			return false;
		#endif
	} else {
		pTransport->Tcp = xrtNetConnect(
			pConfig->Engine,
			pConfig->Resolver,
			pConfig->Host,
			pConfig->Port,
			&pConfig->Dial,
			NULL,
			NULL,
			iDeadline,
			pCancel
		);
		return pTransport->Tcp != NULL;
	}
}



/* 绕过可选内容编码，有界发送完整底层字节序列。 */
bool __xrtMailTransportRawSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	const unsigned char* pBytes = (const unsigned char*)pData;
	size_t iOffset = 0;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pData, iSize) ||
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
			((pTransport->Tcp == NULL) && (pTransport->Tls == NULL))
		#else
			(pTransport->Tcp == NULL)
		#endif
		) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	while ( iOffset < iSize ) {
		size_t iChunk = iSize - iOffset;

		if ( iChunk > pTransport->WriteChunk ) {
			iChunk = pTransport->WriteChunk;
		}
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			if ( !__xrtMailTransportTlsSend(
				pTransport,
				pBytes + iOffset,
				iChunk,
				iDeadline,
				pCancel
			) ) {
				return false;
			}
		} else {
		#endif
			xnetresult Result;

			for ( ;; ) {
				Result = xrtNetStreamSend(
					pTransport->Tcp,
					pBytes + iOffset,
					iChunk
				);
				if ( Result == XNET_RESULT_OK ) {
					break;
				}
				if ( (Result != XNET_RESULT_AGAIN) ||
					!xrtNetStreamWait(
						pTransport->Tcp,
						XNET_STREAM_WAIT_WRITE,
						iDeadline,
						pCancel
					) ) {
					return false;
				}
			}
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		}
		#endif
		iOffset += iChunk;
	}
	return true;
}



/* 写入可选压缩层，并由调用方决定是否建立同步刷新边界。 */
bool __xrtMailTransportWrite(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
		if ( __xrtMailTransportDeflated(pTransport) ) {
			return __xrtMailTransportDeflateSend(
				pTransport,
				pData,
				iSize,
				bFlush,
				iDeadline,
				pCancel
			);
		}
	#else
		(void)bFlush;
	#endif
	return __xrtMailTransportRawSend(
		pTransport,
		pData,
		iSize,
		iDeadline,
		pCancel
	);
}



/* 发送完整协议片段，并保证压缩数据对端可立即消费。 */
bool __xrtMailTransportSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtMailTransportWrite(
		pTransport,
		pData,
		iSize,
		true,
		iDeadline,
		pCancel
	);
}



/* 绕过可选内容解码，取得一块拥有型传输字节。 */
xnetbytes* __xrtMailTransportRawRecv(
	__xmailtransport* pTransport,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			return __xrtMailTransportTlsRecv(
				pTransport,
				iDeadline,
				pCancel
			);
		}
	#endif
	return xrtNetStreamRecv(
			pTransport->Tcp,
			pTransport->ReadChunk,
			iDeadline,
			pCancel
		);
}



/* 在读取下一条线路前提交上一条借用线路的消费。 */
void __xrtMailTransportConsume(__xmailtransport* pTransport)
{
	if ( pTransport->PendingConsumed == 0 ) {
		return;
	}
	pTransport->PendingSize -= pTransport->PendingConsumed;
	if ( pTransport->PendingSize != 0 ) {
		memmove(
			pTransport->Pending,
			pTransport->Pending + pTransport->PendingConsumed,
			pTransport->PendingSize
		);
	}
	pTransport->PendingConsumed = 0;
}



/* 为下一块接收数据扩展动态线路缓冲。 */
bool __xrtMailTransportReserve(
	__xmailtransport* pTransport,
	size_t iAppend
)
{
	size_t iRequired;
	size_t iCapacity;
	bytes pPending;

	if ( iAppend > (SIZE_MAX - pTransport->PendingSize) ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	iRequired = pTransport->PendingSize + iAppend;
	if ( iRequired <= pTransport->PendingCapacity ) {
		return true;
	}
	iCapacity = pTransport->PendingCapacity != 0 ?
		pTransport->PendingCapacity : pTransport->ReadChunk;
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity <= (SIZE_MAX / 2u) ?
			iCapacity * 2u : SIZE_MAX;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pPending = (bytes)xrtRealloc(pTransport->Pending, iCapacity);
	if ( pPending == NULL ) {
		return false;
	}
	pTransport->Pending = pPending;
	pTransport->PendingCapacity = iCapacity;
	return true;
}



/* 增量读取一条严格 CRLF 线路，返回视图在下一次 Line 调用前有效。 */
bool __xrtMailTransportLine(
	__xmailtransport* pTransport,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	size_t iConsumed;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pLine, sizeof(*pLine)) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pLine, sizeof(*pLine)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailTransportConsume(pTransport);
	for ( ;; ) {
		xstrview Data = __xrtMailView(
			(const char*)pTransport->Pending,
			pTransport->PendingSize
		);
		xmailnext Next = xrtMailLineRead(
			Data,
			pTransport->LineLimit,
			pLine,
			&iConsumed
		);

		if ( Next == XMAIL_NEXT_ITEM ) {
			pTransport->PendingConsumed = iConsumed;
			return true;
		}
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		{
			#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
				if ( __xrtMailTransportDeflated(pTransport) ) {
					if ( !__xrtMailTransportDeflateFill(
						pTransport,
						iDeadline,
						pCancel
					) ) {
						return false;
					}
					continue;
				}
			#endif
			xnetbytes* pReceived = __xrtMailTransportRawRecv(
				pTransport,
				iDeadline,
				pCancel
			);
			xbytesview Received;

			if ( pReceived == NULL ) {
				return false;
			}
			Received = xrtNetBytesView(pReceived);
			if ( (Received.Size == 0) ||
				!__xrtMailTransportReserve(pTransport, Received.Size) ) {
				xrtNetBytesDestroy(pReceived);
				if ( Received.Size == 0 ) {
					__xrtMailError(
						XERR_CLOSED,
						XMAIL_ERROR_PROTOCOL,
						"mail transport closed before a complete line"
					);
				}
				return false;
			}
			memcpy(
				pTransport->Pending + pTransport->PendingSize,
				Received.Data,
				Received.Size
			);
			pTransport->PendingSize += Received.Size;
			xrtNetBytesDestroy(pReceived);
		}
	}
}



/* 优先消费线路解析后的缓存，再把网络块直接复制到调用方缓冲区。 */
bool __xrtMailTransportRead(
	__xmailtransport* pTransport,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	bytes pOutput = (bytes)pBuffer;
	size_t iCopy;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pBuffer, iCapacity) || (iCapacity == 0) ||
		!xrtMemRangeValid(pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pBuffer, iCapacity) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(pRead, sizeof(*pRead), pBuffer, iCapacity) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailTransportConsume(pTransport);
	if ( pTransport->PendingSize != 0 ) {
		iCopy = pTransport->PendingSize < iCapacity ?
			pTransport->PendingSize : iCapacity;
		memcpy(pOutput, pTransport->Pending, iCopy);
		pTransport->PendingConsumed = iCopy;
		*pRead = iCopy;
		return true;
	}
	{
		#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
			if ( __xrtMailTransportDeflated(pTransport) ) {
				while ( pTransport->PendingSize == 0 ) {
					if ( !__xrtMailTransportDeflateFill(
						pTransport,
						iDeadline,
						pCancel
					) ) {
						return false;
					}
				}
				iCopy = pTransport->PendingSize < iCapacity ?
					pTransport->PendingSize : iCapacity;
				memcpy(pOutput, pTransport->Pending, iCopy);
				pTransport->PendingConsumed = iCopy;
				*pRead = iCopy;
				return true;
			}
		#endif
		xnetbytes* pReceived = __xrtMailTransportRawRecv(
			pTransport,
			iDeadline,
			pCancel
		);
		xbytesview Received;

		if ( pReceived == NULL ) {
			return false;
		}
		Received = xrtNetBytesView(pReceived);
		if ( Received.Size == 0 ) {
			xrtNetBytesDestroy(pReceived);
			__xrtMailError(
				XERR_CLOSED,
				XMAIL_ERROR_PROTOCOL,
				"mail transport closed before the requested bytes"
			);
			return false;
		}
		iCopy = Received.Size < iCapacity ? Received.Size : iCapacity;
		memcpy(pOutput, Received.Data, iCopy);
		if ( Received.Size > iCopy ) {
			size_t iRemain = Received.Size - iCopy;

			if ( !__xrtMailTransportReserve(pTransport, iRemain) ) {
				xrtNetBytesDestroy(pReceived);
				return false;
			}
			memcpy(
				pTransport->Pending,
				(const unsigned char*)Received.Data + iCopy,
				iRemain
			);
			pTransport->PendingSize = iRemain;
		}
		xrtNetBytesDestroy(pReceived);
	}
	*pRead = iCopy;
	return true;
}



/* 正常关闭传输；超时后转为异常关闭。 */
bool __xrtMailTransportClose(
	__xmailtransport* pTransport,
	xdeadline iDeadline
)
{
	bool bSuccess = true;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			bSuccess = __xrtMailTransportTlsClose(pTransport, iDeadline);
		} else
	#endif
	if ( pTransport->Tcp != NULL ) {
		bSuccess = xrtNetStreamClose(pTransport->Tcp) &&
			xrtNetStreamWait(
				pTransport->Tcp,
				XNET_STREAM_WAIT_CLOSE,
				iDeadline,
				NULL
			);
		if ( !bSuccess ) {
			(void)xrtNetStreamAbort(pTransport->Tcp);
		}
	}
	return bSuccess;
}



/* 立即异常中止活动连接，保留传输对象中的配置和诊断快照。 */
bool __xrtMailTransportAbort(__xmailtransport* pTransport)
{
	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			return xrtTlsStreamAbort(pTransport->Tls);
		}
	#endif
	if ( pTransport->Tcp != NULL ) {
		return xrtNetStreamAbort(pTransport->Tcp);
	}
	return true;
}



/* 释放传输及动态线路缓冲；未关闭连接会先异常中止。 */
void __xrtMailTransportDestroy(__xmailtransport* pTransport)
{
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
		__xrtMailTransportDeflateDestroy(pTransport);
	#endif
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			__xrtMailTransportTlsDestroy(pTransport);
		} else
	#endif
	if ( pTransport->Tcp != NULL ) {
		if ( xrtNetStreamState(pTransport->Tcp) != XNET_STREAM_CLOSED ) {
			(void)xrtNetStreamAbort(pTransport->Tcp);
		}
		xrtNetStreamDestroy(pTransport->Tcp);
	}
	xrtFree(pTransport->Pending);
	memset(pTransport, 0, sizeof(*pTransport));
}

#endif
