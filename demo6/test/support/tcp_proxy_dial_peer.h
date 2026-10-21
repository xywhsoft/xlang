/* Fixed valid-peer helpers; legacy driver and main are omitted. */

#if !defined(TEST_PROXY_DIAL_HTTP_CONNECT) || \
	!(TEST_PROXY_DIAL_HTTP_CONNECT)
	#include "tcp_proxy_socks5_peer.h"
#endif



#if !defined(TEST_PROXY_DIAL_BACKEND)
	#define TEST_PROXY_DIAL_BACKEND XNET_PORT_SELECT
#endif

#if !defined(TEST_PROXY_DIAL_HTTP_CONNECT)
	#define TEST_PROXY_DIAL_HTTP_CONNECT 0
#endif



typedef enum testproxymode {
	TEST_PROXY_SUCCESS = 0,
	TEST_PROXY_MALFORMED,
	TEST_PROXY_REJECTED,
	TEST_PROXY_STALL
} testproxymode;



typedef enum testproxystage {
	TEST_PROXY_GREETING = 0,
	TEST_PROXY_AUTH,
	TEST_PROXY_CONNECT,
	TEST_PROXY_TUNNEL,
	TEST_PROXY_STOPPED
} testproxystage;



typedef struct testproxycontext {
	xatomic32 Accepted;
	xatomic32 ClientOpen;
	xatomic32 ClientRead;
	xatomic32 ReceivedBytes;
	xatomic32 Done;
	xatomic32 Order;
	xatomic32 ListenerClosed;
	xnetstream* Client;
	xnetstream* Server;
	xnetresult Result;
	xerrkind ErrorKind;
	int32 ErrorCode;
	testproxymode Mode;
	testproxystage Stage;
	uint32 OpenOrder;
	uint32 ReadOrder;
	uint32 DoneOrder;
	uint16 TargetPort;
	char TargetHost[256];
	uint8 Received[128];
	size_t ReceivedSize;
	bool RequireAuth;
	bool AuthSeen;
} testproxycontext;



/* 在测试截止时间内等待原子计数达到下限。 */
static inline void testProxyDialWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待一个已经公开的 Stream 完成关闭。 */
static inline void testProxyDialWaitClosed(
	const xnetstream* pStream,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待累计接收字节，并在失败时输出协议阶段快照。 */
static inline void testProxyDialWaitBytes(
	const testproxycontext* pContext,
	uint32 iExpected
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	uint32 iReceived;

	for ( ;; ) {
		iReceived = xrtAtomic32Load(
			&pContext->ReceivedBytes,
			XMEMORY_ACQUIRE
		);
		if ( iReceived >= iExpected ) {
			return;
		}
		if ( xrtDeadlineExpired(Deadline) ) {
			fprintf(
				stderr,
				"[INFO] received=%u expected=%u stage=%d result=%d "
				"client=%d server=%d\n",
				(unsigned)iReceived,
				(unsigned)iExpected,
				(int)pContext->Stage,
				(int)pContext->Result,
				pContext->Client != NULL ?
					(int)xrtNetStreamState(pContext->Client) : -1,
				pContext->Server != NULL ?
					(int)xrtNetStreamState(pContext->Server) : -1
			);
			testRequire(false,
				"proxy Dial tunnel echo was not received");
		}
		xrtThreadYield();
	}
}



/* 所有测试主机都解析到当前回环 Listener。 */
static inline xnetaddrlist* testProxyDialLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)sHost;
	(void)Family;
	(void)pData;
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



#if !TEST_PROXY_DIAL_HTTP_CONNECT

/* 验证问候方法，并发送匿名或密码认证选择。 */
static inline bool testProxyDialGreeting(
	testproxycontext* pContext,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	uint8 iMethod = pContext->RequireAuth ? 0x02 : 0x00;

	if ( !testSocks5ProxyGreetingRequest(
		pBuffer,
		iMethod
	) ) {
		return false;
	}
	if ( pContext->Mode == TEST_PROXY_STALL ) {
		pContext->Stage = TEST_PROXY_STOPPED;
		return false;
	}
	if ( pContext->Mode == TEST_PROXY_MALFORMED ) {
		pContext->Stage = TEST_PROXY_STOPPED;
		testSocks5ProxyMethodReply(pStream, 0x04, iMethod);
	} else {
		pContext->Stage = pContext->RequireAuth ?
			TEST_PROXY_AUTH : TEST_PROXY_CONNECT;
		testSocks5ProxyMethodReply(pStream, 0x05, iMethod);
	}
	return true;
}



/* 验证 RFC 1929 凭据并发送认证成功。 */
static inline bool testProxyDialAuth(
	testproxycontext* pContext,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	if ( !testSocks5ProxyAuthRequest(
		pBuffer,
		XRT_BYTES_LITERAL("user"),
		XRT_BYTES_LITERAL("password")
	) ) {
		return false;
	}
	pContext->AuthSeen = true;
	pContext->Stage = TEST_PROXY_CONNECT;
	testSocks5ProxyAuthReply(pStream, 0);
	return true;
}



/* 验证域名 CONNECT 目标，并把绑定回复和应用前缀同批发送。 */
static inline bool testProxyDialConnect(
	testproxycontext* pContext,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	static const uint8 Preface[] = "preface";
	test_socks5_target Target;

	if ( !testSocks5ProxyConnectRequest(
		pBuffer,
		&Target
	) ) {
		return false;
	}
	memcpy(
		pContext->TargetHost,
		Target.Host,
		strlen(Target.Host) + 1u
	);
	pContext->TargetPort = Target.Port;
	pContext->Stage = TEST_PROXY_TUNNEL;
	testSocks5ProxyConnectReply(
		pStream,
		XNET_SOCKS5_SUCCEEDED,
		(xbytesview){
			Preface,
			sizeof(Preface) - 1u
		}
	);
	return true;
}

#else

/* 验证完整 HTTP CONNECT 请求，并按用例发送成功、拒绝或畸形响应。 */
static inline bool testProxyDialConnect(
	testproxycontext* pContext,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	static const char Anonymous[] =
		"CONNECT origin.test:8443 HTTP/1.1\r\n"
		"Host: origin.test:8443\r\n\r\n";
	static const char Authenticated[] =
		"CONNECT origin.test:8443 HTTP/1.1\r\n"
		"Host: origin.test:8443\r\n"
		"Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n\r\n";
	static const char Success[] =
		"HTTP/1.1 204 Tunnel Ready\r\n"
		"Proxy-Agent: xrt-test\r\n\r\npreface";
	static const char Malformed[] =
		"HTTP/1.1 200 OK\nBad: value\r\n\r\n";
	static const char Rejected[] =
		"HTTP/1.1 407 Proxy Authentication Required\r\n\r\n";
	cstr sExpected = pContext->RequireAuth ?
		Authenticated : Anonymous;
	size_t iExpected = strlen(sExpected);
	char Request[192];
	cstr sResponse;
	size_t iResponse;

	if ( xrtNetBufSize(pBuffer) < iExpected ) {
		return false;
	}
	testRequire(iExpected <= sizeof(Request),
		"HTTP proxy Dial request fixture overflow");
	testRequire(xrtNetBufPeek(
		pBuffer, 0, Request, iExpected
	) == iExpected, "HTTP proxy Dial request peek failed");
	testRequire(memcmp(Request, sExpected, iExpected) == 0,
		"HTTP proxy Dial CONNECT request mismatch");
	(void)xrtNetBufConsume(pBuffer, iExpected);
	memcpy(pContext->TargetHost, "origin.test", 12);
	pContext->TargetPort = 8443;
	pContext->AuthSeen = pContext->RequireAuth;

	if ( pContext->Mode == TEST_PROXY_STALL ) {
		pContext->Stage = TEST_PROXY_STOPPED;
		return false;
	}
	if ( pContext->Mode == TEST_PROXY_MALFORMED ) {
		sResponse = Malformed;
		iResponse = sizeof(Malformed) - 1u;
		pContext->Stage = TEST_PROXY_STOPPED;
	} else if ( pContext->Mode == TEST_PROXY_REJECTED ) {
		sResponse = Rejected;
		iResponse = sizeof(Rejected) - 1u;
		pContext->Stage = TEST_PROXY_STOPPED;
	} else {
		sResponse = Success;
		iResponse = sizeof(Success) - 1u;
		pContext->Stage = TEST_PROXY_TUNNEL;
	}
	testRequire(xrtNetStreamSend(
		pStream,
		sResponse,
		iResponse
	) == XNET_RESULT_OK, "HTTP proxy Dial response send failed");
	return true;
}

#endif



/* 隧道阶段逐批复制回显应用数据。 */
static inline bool testProxyDialTunnel(
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	uint8 Data[64];
	size_t iSize = xrtNetBufSize(pBuffer);

	if ( iSize == 0 ) {
		return false;
	}
	if ( iSize > sizeof(Data) ) {
		iSize = sizeof(Data);
	}
	(void)xrtNetBufPeek(pBuffer, 0, Data, iSize);
	(void)xrtNetBufConsume(pBuffer, iSize);
	testRequire(xrtNetStreamSend(
		pStream,
		Data,
		iSize
	) == XNET_RESULT_OK, "proxy Dial tunnel echo failed");
	return true;
}



/* 按增量协议阶段消费代理服务器输入。 */
static inline void testProxyDialServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testproxycontext* pContext = (testproxycontext*)pData;
	bool bProgress;

	do {
		bProgress = false;
		switch ( pContext->Stage ) {
			#if !TEST_PROXY_DIAL_HTTP_CONNECT
			case TEST_PROXY_GREETING:
				bProgress = testProxyDialGreeting(
					pContext, pStream, pBuffer
				);
				break;
			case TEST_PROXY_AUTH:
				bProgress = testProxyDialAuth(
					pContext, pStream, pBuffer
				);
				break;
			#endif
			case TEST_PROXY_CONNECT:
				bProgress = testProxyDialConnect(
					pContext, pStream, pBuffer
				);
				break;
			case TEST_PROXY_TUNNEL:
				bProgress = testProxyDialTunnel(pStream, pBuffer);
				break;
			default:
				return;
		}
	} while ( bProgress && !xrtNetBufEmpty(pBuffer) );
}



/* 接管唯一代理服务端连接，并安装当前用例上下文。 */
static inline bool testProxyDialAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testproxycontext* pContext = (testproxycontext*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"proxy Dial accepted stream data setup failed");
	pContext->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 发布 Listener 唯一关闭事件。 */
static inline void testProxyDialListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testproxycontext* pContext = (testproxycontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录最终用户 Open 的协议边界顺序。 */
static inline void testProxyDialClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	testproxycontext* pContext = (testproxycontext*)pData;

	(void)pStream;
	pContext->OpenOrder = xrtAtomic32FetchAdd(
		&pContext->Order,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	(void)xrtAtomic32FetchAdd(
		&pContext->ClientOpen,
		1,
		XMEMORY_RELEASE
	);
}



/* 消费隧道预读和回显数据，并记录第一次 Read 顺序。 */
static inline void testProxyDialClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testproxycontext* pContext = (testproxycontext*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iAvailable = sizeof(pContext->Received) -
		pContext->ReceivedSize;

	(void)pStream;
	testRequire(iSize <= iAvailable,
		"proxy Dial client receive fixture overflow");
	(void)xrtNetBufPeek(
		pBuffer,
		0,
		pContext->Received + pContext->ReceivedSize,
		iSize
	);
	(void)xrtNetBufConsume(pBuffer, iSize);
	pContext->ReceivedSize += iSize;
	xrtAtomic32Store(
		&pContext->ReceivedBytes,
		(uint32)pContext->ReceivedSize,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pContext->ClientRead,
		XMEMORY_ACQUIRE
	) == 0 ) {
		pContext->ReadOrder = xrtAtomic32FetchAdd(
			&pContext->Order,
			1,
			XMEMORY_ACQ_REL
		) + 1u;
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->ClientRead,
		1,
		XMEMORY_RELEASE
	);
}



/* 捕获唯一 Dial 终态，并在成功后立即验证可用隧道。 */
static inline void testProxyDialDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	static const uint8 Payload[] = "ok";
	testproxycontext* pContext = (testproxycontext*)pData;

	pContext->Result = Result;
	pContext->DoneOrder = xrtAtomic32FetchAdd(
		&pContext->Order,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	if ( Result == XNET_RESULT_OK ) {
		testRequire((pStream != NULL) && (pError == NULL) &&
			(xrtNetProxyDialState(pDial) == XNET_PROXY_DIAL_CONNECTED),
			"proxy Dial success terminal mismatch");
		pContext->Client = pStream;
		testRequire(xrtNetStreamSend(
			pStream,
			Payload,
			sizeof(Payload) - 1u
		) == XNET_RESULT_OK, "proxy Dial post-open send failed");
	} else {
		testRequire((pStream == NULL) && (pError != NULL),
			"proxy Dial failure terminal mismatch");
		pContext->ErrorKind = xrtErrorKind(pError);
		pContext->ErrorCode = xrtErrorCode(pError);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}
