#include "xl_clock.h"
#include "../internal/xrt_mail_net.h"



#if defined(XMAIL_FEATURE_MAIL_NET_TLS)

/* STARTTLS 接管请求只在原 TCP Stream 的 Worker 上短暂存在。 */
typedef struct __xmailtlsupgrade {
    xatomic32 Refs;
    xatomic32 Gate;
	xnetstream* Tcp;
	const xtlsclientconfig* Client;
	const xtlsstreamconfig* Stream;
	xtlsstream* Tls;
	xpromise* Promise;
} __xmailtlsupgrade;

/* The caller and queued task each own one request reference. Queued
 * abandonment must never inspect the caller-borrowed TCP/configuration. */
static void __xrtMailUpgradeDrop(void* data)
{
    __xmailtlsupgrade* request=data;
    if(xrtAtomic32FetchSub(&request->Refs,1,XMEMORY_ACQ_REL)==1)xrtFree(request);
}

/* 判断拨号主机是否是无需 SNI 的数字 IP。 */
static bool __xrtMailNetTlsHostIsIp(cstr sHost)
{
	xnetaddr Address;
	xerror* pSaved = xrtTakeError();
	bool bIp = xrtNetAddrParse(&Address, sHost, 0);
	xerror* pProbe = xrtTakeError();

	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
	} else {
		xrtClearError();
	}
	xrtErrorFree(pSaved);
	xrtErrorFree(pProbe);
	return bIp;
}



/* 等待 TLS Future 并把失败终态恢复为当前结构化错误。 */
static bool __xrtMailNetTlsFuture(
	xfuture* pFuture,
	XL6_DeadlineV1 iDeadline,
	xcancel* pCancel
)
{
	xwaitresult Wait = xrtFutureWaitForCancel(
		pFuture,xl6DeadlineRemainingMillisecondsV1(iDeadline),
		pCancel
	);
	xfuturestate State;

	if ( Wait != XWAIT_OK ) {
		(void)xrtFutureCancel(pFuture);
		if ( Wait == XWAIT_TIMEOUT ) {
			__xrtMailError(
				XERR_TIMEOUT,
				XMAIL_ERROR_PROTOCOL,
				"mail TLS operation timed out"
			);
		} else if ( Wait == XWAIT_CANCELLED ) {
			__xrtMailError(
				XERR_CANCELLED,
				XMAIL_ERROR_PROTOCOL,
				"mail TLS operation was cancelled"
			);
		}
		return false;
	}
	State = xrtFutureState(pFuture);
	if ( State == XFUTURE_RESOLVED ) {
		return true;
	}
	if ( State == XFUTURE_FAILED ) {
		xrtSetError(xrtFutureError(pFuture));
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtMailError(
			XERR_CANCELLED,
			XMAIL_ERROR_PROTOCOL,
			"mail TLS operation was cancelled"
		);
	} else {
		__xrtMailError(
			XERR_CLOSED,
			XMAIL_ERROR_PROTOCOL,
			"mail TLS operation closed without a result"
		);
	}
	return false;
}



/* 为 TLS 拨号补齐默认验证名称，并避免向数字 IP 发送 SNI。 */
static void __xrtMailNetTlsClient(
	const xmailnetconfig* pConfig,
	xtlsclientconfig* pTls
)
{
	xstrview Host;

	*pTls = pConfig->Tls;
	Host.Data = pConfig->Host;
	Host.Size = strlen(pConfig->Host);
	if ( pTls->VerifyName.Size == 0 ) {
		pTls->VerifyName = Host;
	}
	if ( (pTls->ServerName.Size == 0) &&
		!__xrtMailNetTlsHostIsIp(pConfig->Host) ) {
		pTls->ServerName = Host;
	}
}



/* 在 TCP 所属 Worker 上原子接管传输引用。 */
static void __xrtMailNetTlsUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	__xmailtlsupgrade* pUpgrade = (__xmailtlsupgrade*)pData;

    (void)pWorker;
    xpromise* promise=pUpgrade->Promise;
    uint32 expected=0;
    if(!xrtAtomic32CompareExchange(&pUpgrade->Gate,&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
        xrtPromiseDestroy(promise);__xrtMailUpgradeDrop(pUpgrade);return;
    }
	if ( xrtTlsStreamClient(
		pUpgrade->Tcp,
		pUpgrade->Client,
		pUpgrade->Stream,
		NULL,
		NULL,
		&pUpgrade->Tls
	) ) {
		(void)xrtPromiseResolve(promise, NULL);
	} else if ( xrtGetError() != NULL ) {
		(void)xrtPromiseReject(promise, xrtGetError());
	} else {
		(void)xrtPromiseClose(promise);
	}
	/* No access to caller-owned Upgrade storage after publication. */
    xrtClearError();
    xrtPromiseDestroy(promise);
    __xrtMailUpgradeDrop(pUpgrade);
}



/* 完成隐式 TLS 拨号。 */
bool __xrtMailTransportTlsOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	XL6_DeadlineV1 iDeadline,
	xcancel* pCancel
)
{
	xtlsclientconfig Tls;
	xtlsdialconfig Dial;
	xfuture* pFuture;

	__xrtMailNetTlsClient(pConfig, &Tls);
	xrtTlsDialConfigInit(&Dial);
	Dial.Transport = pConfig->Dial;
	Dial.Stream = pConfig->TlsStream;
	Dial.Timeout = pConfig->TlsTimeout;
	Dial.ServerNameFromHost = false;
	pFuture = xrtTlsDialAsync(
		pConfig->Engine,
		pConfig->Resolver,
		pConfig->Host,
		pConfig->Port,
		&Tls,
		&Dial,
		NULL,
		NULL
	);
	if ( pFuture == NULL ) {
		return false;
	}
	if ( !__xrtMailNetTlsFuture(pFuture, iDeadline, pCancel) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pTransport->Tls = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
	xrtFutureDestroy(pFuture);
	return pTransport->Tls != NULL;
}



/* 把已经完成协议协商的明文 TCP Stream 接管为 TLS Stream。 */
bool __xrtMailTransportStartTls(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	XL6_DeadlineV1 iDeadline,
	xcancel* pCancel
)
{
	__xmailtlsupgrade* pUpgrade;
	xtlsclientconfig Client;
	xnetworker* pWorker;
	xfuture* pFuture;
	xfuture* pOpen;
	xwaitresult Wait;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMailNetConfigValid(pConfig) ||
		(pConfig->Security != XMAIL_SECURITY_STARTTLS) ||
		(pTransport->Tcp == NULL) || (pTransport->Tls != NULL) ||
		(pTransport->PendingConsumed > pTransport->PendingSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pTransport->PendingSize - pTransport->PendingConsumed) != 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"STARTTLS response left unconsumed plaintext"
		);
		return false;
	}
	if ( xl6DeadlineExpiredV1(iDeadline) || xrtCancelRequested(pCancel) ) {
		__xrtMailError(
			xl6DeadlineExpiredV1(iDeadline) ? XERR_TIMEOUT : XERR_CANCELLED,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS was not started"
		);
		return false;
	}
	pWorker = xrtNetStreamWorker(pTransport->Tcp);
	if ( (pWorker == NULL) || xrtNetWorkerIsCurrent(pWorker) ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS cannot block its transport worker"
		);
		return false;
	}
	__xrtMailNetTlsClient(pConfig, &Client);
    pUpgrade=xrtCalloc(1,sizeof(*pUpgrade));if(!pUpgrade)return false;
    xrtAtomic32Init(&pUpgrade->Refs,2);xrtAtomic32Init(&pUpgrade->Gate,0);
	pUpgrade->Tcp = pTransport->Tcp;
	pUpgrade->Client = &Client;
	pUpgrade->Stream = &pConfig->TlsStream;
	pUpgrade->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pUpgrade->Promise == NULL ) {
        xrtFree(pUpgrade);
		return false;
	}
    bool posted;
    if(pTransport->Post){
        posted=pTransport->Post(pWorker,__xrtMailNetTlsUpgradeTask,pUpgrade,
            pUpgrade->Promise,__xrtMailUpgradeDrop,pTransport->PostContext);
    }else{
        posted=xrtNetEnginePost(xrtNetWorkerEngine(pWorker),xrtNetWorkerIndex(pWorker),
            __xrtMailNetTlsUpgradeTask,pUpgrade);
        if(!posted){xrtPromiseDestroy(pUpgrade->Promise);__xrtMailUpgradeDrop(pUpgrade);}
    }
    if(!posted){xrtFutureDestroy(pFuture);__xrtMailUpgradeDrop(pUpgrade);return false;}

    Wait=xrtFutureWaitForCancel(pFuture,xl6DeadlineRemainingMillisecondsV1(iDeadline),pCancel);
    if(Wait!=XWAIT_OK){
        uint32 expected=0;
        if(!xrtAtomic32CompareExchange(&pUpgrade->Gate,&expected,2,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
            /* Running handoff must return ownership before borrowed inputs die. */
            (void)xrtFutureWait(pFuture);
            if(pUpgrade->Tls){pTransport->Tcp=NULL;(void)xrtTlsStreamAbort(pUpgrade->Tls);xrtTlsStreamDestroy(pUpgrade->Tls);}
        }
        xrtFutureDestroy(pFuture);__xrtMailUpgradeDrop(pUpgrade);
        __xrtMailError(Wait==XWAIT_TIMEOUT?XERR_TIMEOUT:Wait==XWAIT_CANCELLED?XERR_CANCELLED:XERR_IO,
            XMAIL_ERROR_PROTOCOL,"mail STARTTLS worker handoff interrupted");return false;
    }
	if ( (Wait != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ||
		(pUpgrade->Tls == NULL) ) {
		if ( xrtFutureState(pFuture) == XFUTURE_FAILED ) {
			xrtSetError(xrtFutureError(pFuture));
		} else if ( Wait != XWAIT_OK ) {
			__xrtMailError(
				XERR_IO,
				XMAIL_ERROR_PROTOCOL,
				"mail STARTTLS worker handoff failed"
			);
		}
		if(!xrtGetError())__xrtMailError(XERR_CLOSED,XMAIL_ERROR_PROTOCOL,"mail STARTTLS handoff closed");
		xrtFutureDestroy(pFuture);
        __xrtMailUpgradeDrop(pUpgrade);
		return false;
	}
	xrtFutureDestroy(pFuture);
	pTransport->Tcp = NULL;
	pTransport->Tls = pUpgrade->Tls;
    __xrtMailUpgradeDrop(pUpgrade);
	pTransport->PendingSize = 0;
	pTransport->PendingConsumed = 0;
	pOpen = xrtTlsStreamWaitAsync(
		pTransport->Tls,
		XTLS_STREAM_WAIT_OPEN
	);
	if ( pOpen == NULL ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
		xrtTlsStreamDestroy(pTransport->Tls);
		pTransport->Tls = NULL;
		return false;
	}
	if ( !__xrtMailNetTlsFuture(pOpen, iDeadline, pCancel) ) {
		xrtFutureDestroy(pOpen);
		(void)xrtTlsStreamAbort(pTransport->Tls);
		xrtTlsStreamDestroy(pTransport->Tls);
		pTransport->Tls = NULL;
		return false;
	}
	xrtFutureDestroy(pOpen);
	pTransport->Security = XMAIL_SECURITY_TLS;
	return true;
}



/* 发送一个完整 TLS 明文分片。 */
bool __xrtMailTransportTlsSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	XL6_DeadlineV1 iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTlsStreamSendAsync(
		pTransport->Tls,
		pData,
		iSize
	);
	bool bSuccess;

	if ( pFuture == NULL ) {
		return false;
	}
	bSuccess = __xrtMailNetTlsFuture(pFuture, iDeadline, pCancel);
	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 取得一块拥有型 TLS 明文。 */
xnetbytes* __xrtMailTransportTlsRecv(
	__xmailtransport* pTransport,
	XL6_DeadlineV1 iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pTransport->Tls,
		pTransport->ReadChunk
	);
	xnetbytes* pBytes;

	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( !__xrtMailNetTlsFuture(pFuture, iDeadline, pCancel) ) {
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	return pBytes;
}



/* 等待 TLS Stream 进入关闭终态。 */
bool __xrtMailTransportTlsClose(
	__xmailtransport* pTransport,
	XL6_DeadlineV1 iDeadline
)
{
	xfuture* pFuture;
	bool bSuccess;

	if ( !xrtTlsStreamClose(pTransport->Tls) ) {
		return false;
	}
	pFuture = xrtTlsStreamWaitAsync(
		pTransport->Tls,
		XTLS_STREAM_WAIT_CLOSE
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bSuccess = __xrtMailNetTlsFuture(pFuture, iDeadline, NULL);
	if ( !bSuccess ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
	}
	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 异常中止并释放 TLS Stream 引用。 */
void __xrtMailTransportTlsDestroy(__xmailtransport* pTransport)
{
	if ( (xrtTlsStreamState(pTransport->Tls) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pTransport->Tls) != XTLS_STREAM_FAILED) ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
	}
	xrtTlsStreamDestroy(pTransport->Tls);
	pTransport->Tls = NULL;
}

#endif
