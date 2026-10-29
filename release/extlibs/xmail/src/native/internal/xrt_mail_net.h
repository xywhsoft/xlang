#ifndef XRT_INTERNAL_MAIL_NET_H
#define XRT_INTERNAL_MAIL_NET_H

#include "xrt_mail.h"

#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
	#include <xrt/compress.h>
#endif



#if defined(XMAIL_FEATURE_MAIL_NET)

#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) && \
	(!defined(XRT_FEATURE_DEFLATE) || !defined(XRT_FEATURE_INFLATE))
	#error "XMAIL_FEATURE_MAIL_NET_DEFLATE requires Deflate and Inflate"
#endif

/* 同步协议客户端共享的传输对象，不向公共 API 隐藏 XRT 网络对象。 */
typedef struct __xmailtransport {
	xnetstream* Tcp;
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		xtlsstream* Tls;
	#endif
	bytes Pending;
	size_t PendingSize;
	size_t PendingCapacity;
	size_t PendingConsumed;
	size_t LineLimit;
	size_t ReadChunk;
	size_t WriteChunk;
	xmailsecurity Security;
	#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
		xdeflate* Deflater;
		xinflate* Inflater;
		bytes DeflatePrefix;
		size_t DeflatePrefixSize;
		size_t DeflatePrefixConsumed;
		xnetbytes* DeflateInput;
		size_t DeflateInputConsumed;
	#endif
} __xmailtransport;



/* 协议客户端共享的动态文本只负责所有权，不解释任何协议字段。 */
typedef struct __xmailtext {
	char* Data;
	size_t Size;
	size_t Capacity;
} __xmailtext;



bool __xrtMailTextSet(__xmailtext* pText, xstrview Value);



void __xrtMailTextDestroy(__xmailtext* pText);



bool __xrtMailTransportOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportWrite(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	xdeadline iDeadline,
	xcancel* pCancel
);



xnetbytes* __xrtMailTransportRawRecv(
	__xmailtransport* pTransport,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportRawSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportReserve(
	__xmailtransport* pTransport,
	size_t iAppend
);



void __xrtMailTransportConsume(__xmailtransport* pTransport);



bool __xrtMailTransportLine(
	__xmailtransport* pTransport,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportRead(
	__xmailtransport* pTransport,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportClose(
	__xmailtransport* pTransport,
	xdeadline iDeadline
);



bool __xrtMailTransportAbort(__xmailtransport* pTransport);



void __xrtMailTransportDestroy(__xmailtransport* pTransport);



#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
bool __xrtMailTransportDeflateStart(
	__xmailtransport* pTransport,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
);



bool __xrtMailTransportDeflateSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportDeflateFill(
	__xmailtransport* pTransport,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportDeflated(const __xmailtransport* pTransport);



void __xrtMailTransportDeflateDestroy(__xmailtransport* pTransport);
#endif



#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
bool __xrtMailTransportTlsOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportStartTls(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportTlsSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



xnetbytes* __xrtMailTransportTlsRecv(
	__xmailtransport* pTransport,
	xdeadline iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportTlsClose(
	__xmailtransport* pTransport,
	xdeadline iDeadline
);



void __xrtMailTransportTlsDestroy(__xmailtransport* pTransport);
#endif

#endif

#endif
