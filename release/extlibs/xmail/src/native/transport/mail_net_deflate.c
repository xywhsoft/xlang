#include "../internal/xrt_mail_net.h"



#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)

#define __XMAIL_DEFLATE_INPUT_CHUNK 256u



/* Deflate 输出回调同步写入原始 TCP 或 TLS 传输。 */
typedef struct __xmaildeflatesend {
	__xmailtransport* Transport;
	xdeadline Deadline;
	xcancel* Cancel;
} __xmaildeflatesend;



/* 直接发送编码器产生的一块压缩数据。 */
static bool __xrtMailTransportDeflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xmaildeflatesend* pSend = (__xmaildeflatesend*)pData;

	return __xrtMailTransportRawSend(
		pSend->Transport,
		Data.Data,
		Data.Size,
		pSend->Deadline,
		pSend->Cancel
	);
}



/* 把 inflater 的一块明文追加到共享读取缓冲。 */
static bool __xrtMailTransportInflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xmailtransport* pTransport = (__xmailtransport*)pData;

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( !__xrtMailTransportReserve(pTransport, Data.Size) ) {
		return false;
	}
	memcpy(
		pTransport->Pending + pTransport->PendingSize,
		Data.Data,
		Data.Size
	);
	pTransport->PendingSize += Data.Size;
	return true;
}



/* 返回传输是否已经切换到 IMAP raw DEFLATE。 */
bool __xrtMailTransportDeflated(const __xmailtransport* pTransport)
{
	return (pTransport != NULL) && (pTransport->Deflater != NULL) &&
		(pTransport->Inflater != NULL);
}



/* 在 COMPRESS 的明文 OK 后原子安装双向 raw DEFLATE。 */
bool __xrtMailTransportDeflateStart(
	__xmailtransport* pTransport,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
)
{
	xdeflate* pEncoder;
	xinflate* pDecoder;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtDeflateConfigValid(pDeflate) ||
		!xrtInflateConfigValid(pInflate) ||
		__xrtMailTransportDeflated(pTransport) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pEncoder = xrtDeflateCreate(pDeflate);
	if ( pEncoder == NULL ) {
		return false;
	}
	pDecoder = xrtInflateCreate(pInflate);
	if ( pDecoder == NULL ) {
		xrtDeflateDestroy(pEncoder);
		return false;
	}
	__xrtMailTransportConsume(pTransport);
	pTransport->DeflatePrefix = pTransport->Pending;
	pTransport->DeflatePrefixSize = pTransport->PendingSize;
	pTransport->DeflatePrefixConsumed = 0;
	pTransport->Pending = NULL;
	pTransport->PendingSize = 0;
	pTransport->PendingCapacity = 0;
	pTransport->PendingConsumed = 0;
	pTransport->Deflater = pEncoder;
	pTransport->Inflater = pDecoder;
	return true;
}



/* 压缩一块协议数据，并按命令边界选择同步刷新。 */
bool __xrtMailTransportDeflateSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xmaildeflatesend Send;

	if ( !__xrtMailTransportDeflated(pTransport) ||
		!xrtMemRangeValid(pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Send.Transport = pTransport;
	Send.Deadline = iDeadline;
	Send.Cancel = pCancel;
	return xrtDeflateWrite(
		pTransport->Deflater,
		(xbytesview) { (cbytes)pData, iSize },
		bFlush ? XDEFLATE_FLUSH_SYNC : XDEFLATE_FLUSH_NONE,
		__xrtMailTransportDeflateOutput,
		&Send
	);
}



/* 释放已经消费完的预读前缀或网络块。 */
static void __xrtMailTransportDeflateInputConsume(
	__xmailtransport* pTransport
)
{
	if ( pTransport->DeflatePrefixConsumed ==
		pTransport->DeflatePrefixSize ) {
		xrtFree(pTransport->DeflatePrefix);
		pTransport->DeflatePrefix = NULL;
		pTransport->DeflatePrefixSize = 0;
		pTransport->DeflatePrefixConsumed = 0;
	}
	if ( pTransport->DeflateInput != NULL ) {
		xbytesview Input = xrtNetBytesView(pTransport->DeflateInput);

		if ( pTransport->DeflateInputConsumed == Input.Size ) {
			xrtNetBytesDestroy(pTransport->DeflateInput);
			pTransport->DeflateInput = NULL;
			pTransport->DeflateInputConsumed = 0;
		}
	}
}



/* 取得下一小块压缩输入，避免高压缩比正文一次性膨胀。 */
static bool __xrtMailTransportDeflateInput(
	__xmailtransport* pTransport,
	xdeadline iDeadline,
	xcancel* pCancel,
	xbytesview* pInput
)
{
	xbytesview Input;
	size_t iRemain;
	size_t iChunk;

	__xrtMailTransportDeflateInputConsume(pTransport);
	if ( pTransport->DeflatePrefix != NULL ) {
		Input.Data = pTransport->DeflatePrefix +
			pTransport->DeflatePrefixConsumed;
		Input.Size = pTransport->DeflatePrefixSize -
			pTransport->DeflatePrefixConsumed;
	} else {
		if ( pTransport->DeflateInput == NULL ) {
			pTransport->DeflateInput = __xrtMailTransportRawRecv(
				pTransport,
				iDeadline,
				pCancel
			);
			if ( pTransport->DeflateInput == NULL ) {
				return false;
			}
		}
		Input = xrtNetBytesView(pTransport->DeflateInput);
		if ( (Input.Size == 0) ||
			(pTransport->DeflateInputConsumed >= Input.Size) ) {
			__xrtMailError(
				XERR_CLOSED,
				XMAIL_ERROR_PROTOCOL,
				"compressed mail transport returned no input"
			);
			return false;
		}
		Input.Data += pTransport->DeflateInputConsumed;
		Input.Size -= pTransport->DeflateInputConsumed;
	}
	iRemain = Input.Size;
	iChunk = iRemain < __XMAIL_DEFLATE_INPUT_CHUNK ?
		iRemain : __XMAIL_DEFLATE_INPUT_CHUNK;
	pInput->Data = Input.Data;
	pInput->Size = iChunk;
	return true;
}



/* 提交刚才交给 inflater 的压缩输入长度。 */
static void __xrtMailTransportDeflateAdvance(
	__xmailtransport* pTransport,
	size_t iSize
)
{
	if ( pTransport->DeflatePrefix != NULL ) {
		pTransport->DeflatePrefixConsumed += iSize;
	} else {
		pTransport->DeflateInputConsumed += iSize;
	}
}



/* 推进压缩输入，直到至少产生一块可消费明文。 */
bool __xrtMailTransportDeflateFill(
	__xmailtransport* pTransport,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtMailTransportDeflated(pTransport) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( ;; ) {
		xbytesview Input;
		size_t iBefore = pTransport->PendingSize;

		if ( !__xrtMailTransportDeflateInput(
			pTransport,
			iDeadline,
			pCancel,
			&Input
		) || !xrtInflateWrite(
			pTransport->Inflater,
			Input,
			false,
			__xrtMailTransportInflateOutput,
			pTransport
		) ) {
			return false;
		}
		__xrtMailTransportDeflateAdvance(pTransport, Input.Size);
		if ( pTransport->PendingSize != iBefore ) {
			return true;
		}
	}
}



/* 释放压缩状态和尚未消费的原始输入。 */
void __xrtMailTransportDeflateDestroy(__xmailtransport* pTransport)
{
	if ( pTransport == NULL ) {
		return;
	}
	xrtDeflateDestroy(pTransport->Deflater);
	xrtInflateDestroy(pTransport->Inflater);
	xrtFree(pTransport->DeflatePrefix);
	xrtNetBytesDestroy(pTransport->DeflateInput);
	pTransport->Deflater = NULL;
	pTransport->Inflater = NULL;
	pTransport->DeflatePrefix = NULL;
	pTransport->DeflatePrefixSize = 0;
	pTransport->DeflatePrefixConsumed = 0;
	pTransport->DeflateInput = NULL;
	pTransport->DeflateInputConsumed = 0;
}

#endif
