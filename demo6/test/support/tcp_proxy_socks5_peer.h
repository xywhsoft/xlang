#ifndef XRT_TEST_SOCKS5_PROXY_H
#define XRT_TEST_SOCKS5_PROXY_H




/* SOCKS5 测试目标保留代理实际收到的远端域名和端口。 */
typedef struct test_socks5_target {
	char Host[256];
	uint16 Port;
} test_socks5_target;



/* 从缓冲前缀复制确定长度的数据。 */
static inline bool testSocks5ProxyPeek(
	const xnetbuf* pBuffer,
	void* pOutput,
	size_t iSize
)
{
	return (xrtNetBufSize(pBuffer) >= iSize) &&
		(xrtNetBufPeek(pBuffer, 0, pOutput, iSize) == iSize);
}



/*
	增量验证并消费一条 SOCKS5 问候。
	输入不足返回 false；请求必须提供 ExpectedMethod。
*/
static inline bool testSocks5ProxyGreetingRequest(
	xnetbuf* pBuffer,
	uint8 ExpectedMethod
)
{
	uint8 Header[2];
	uint8 Methods[UINT8_MAX];
	bool bOffered = false;

	if ( !testSocks5ProxyPeek(
		pBuffer,
		Header,
		sizeof(Header)
	) ) {
		return false;
	}
	testRequire(
		(Header[0] == 0x05) && (Header[1] != 0),
		"SOCKS5 fixture greeting header mismatch"
	);
	if ( xrtNetBufSize(pBuffer) < (2u + Header[1]) ) {
		return false;
	}
	(void)xrtNetBufPeek(
		pBuffer,
		2,
		Methods,
		Header[1]
	);
	for ( size_t i = 0; i < Header[1]; i++ ) {
		if ( Methods[i] == ExpectedMethod ) {
			bOffered = true;
		}
	}
	testRequire(
		bOffered,
		"SOCKS5 fixture required method was not offered"
	);
	(void)xrtNetBufConsume(pBuffer, 2u + Header[1]);
	return true;
}



/* 发送 SOCKS5 方法选择回复。 */
static inline void testSocks5ProxyMethodReply(
	xnetstream* pStream,
	uint8 iVersion,
	uint8 iMethod
)
{
	uint8 Reply[2] = { iVersion, iMethod };

	testRequire(
		xrtNetStreamSend(
			pStream,
			Reply,
			sizeof(Reply)
		) == XNET_RESULT_OK,
		"SOCKS5 fixture method reply failed"
	);
}



/*
	增量验证并消费一条 RFC 1929 用户名密码请求。
	输入不足返回 false；凭据按二进制长度精确比较。
*/
static inline bool testSocks5ProxyAuthRequest(
	xnetbuf* pBuffer,
	xbytesview Username,
	xbytesview Password
)
{
	uint8 Header[2];
	uint8 iPasswordSize;
	uint8 Packet[2u + UINT8_MAX + 1u + UINT8_MAX];
	size_t iPasswordOffset;
	size_t iNeed;

	if ( !testSocks5ProxyPeek(
		pBuffer,
		Header,
		sizeof(Header)
	) ) {
		return false;
	}
	testRequire(
		Header[0] == 0x01,
		"SOCKS5 fixture auth version mismatch"
	);
	iPasswordOffset = 2u + Header[1];
	if ( xrtNetBufSize(pBuffer) < (iPasswordOffset + 1u) ) {
		return false;
	}
	(void)xrtNetBufPeek(
		pBuffer,
		iPasswordOffset,
		&iPasswordSize,
		1
	);
	iNeed = iPasswordOffset + 1u + iPasswordSize;
	if ( xrtNetBufSize(pBuffer) < iNeed ) {
		return false;
	}
	testRequire(
		iNeed <= sizeof(Packet),
		"SOCKS5 fixture auth packet overflowed"
	);
	(void)xrtNetBufPeek(pBuffer, 0, Packet, iNeed);
	testRequire(
		(Header[1] == Username.Size) &&
		(iPasswordSize == Password.Size) &&
		(memcmp(
			Packet + 2,
			Username.Data,
			Username.Size
		) == 0) &&
		(memcmp(
			Packet + iPasswordOffset + 1u,
			Password.Data,
			Password.Size
		) == 0),
		"SOCKS5 fixture credentials mismatch"
	);
	(void)xrtNetBufConsume(pBuffer, iNeed);
	return true;
}



/* 发送 RFC 1929 认证回复。 */
static inline void testSocks5ProxyAuthReply(
	xnetstream* pStream,
	uint8 iStatus
)
{
	uint8 Reply[2] = { 0x01, iStatus };

	testRequire(
		xrtNetStreamSend(
			pStream,
			Reply,
			sizeof(Reply)
		) == XNET_RESULT_OK,
		"SOCKS5 fixture auth reply failed"
	);
}



/*
	增量验证并消费一条使用远端域名的 SOCKS5 CONNECT。
	输入不足返回 false；Target 在成功时取得零结尾域名和主机序端口。
*/
static inline bool testSocks5ProxyConnectRequest(
	xnetbuf* pBuffer,
	test_socks5_target* pTarget
)
{
	uint8 Header[5];
	uint8 Packet[4u + 1u + UINT8_MAX + 2u];
	size_t iHostSize;
	size_t iNeed;

	if ( !testSocks5ProxyPeek(pBuffer, Header, 4) ) {
		return false;
	}
	testRequire(
		(Header[0] == 0x05) &&
		(Header[1] == 0x01) &&
		(Header[2] == 0) &&
		(Header[3] == 0x03),
		"SOCKS5 fixture CONNECT header mismatch"
	);
	if ( !testSocks5ProxyPeek(
		pBuffer,
		Header,
		sizeof(Header)
	) ) {
		return false;
	}
	iHostSize = Header[4];
	iNeed = 7u + iHostSize;
	if ( xrtNetBufSize(pBuffer) < iNeed ) {
		return false;
	}
	testRequire(
		(pTarget != NULL) &&
		(iHostSize < sizeof(pTarget->Host)) &&
		(iNeed <= sizeof(Packet)),
		"SOCKS5 fixture target overflowed"
	);
	(void)xrtNetBufPeek(pBuffer, 0, Packet, iNeed);
	memcpy(pTarget->Host, Packet + 5, iHostSize);
	pTarget->Host[iHostSize] = 0;
	pTarget->Port = (uint16)(
		((uint16)Packet[iNeed - 2u] << 8) |
		(uint16)Packet[iNeed - 1u]
	);
	(void)xrtNetBufConsume(pBuffer, iNeed);
	return true;
}



/* 发送 IPv4 绑定端点的 SOCKS5 CONNECT 回复和可选隧道前缀。 */
static inline void testSocks5ProxyConnectReply(
	xnetstream* pStream,
	uint8 iCode,
	xbytesview Preface
)
{
	uint8 Reply[10] = {
		0x05, 0x00, 0x00, 0x01,
		127, 0, 0, 1, 0x1F, 0x90
	};

	Reply[1] = iCode;
	testRequire(
		xrtNetStreamSend(
			pStream,
			Reply,
			sizeof(Reply)
		) == XNET_RESULT_OK,
		"SOCKS5 fixture CONNECT reply failed"
	);
	if ( Preface.Size != 0 ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				Preface.Data,
				Preface.Size
			) == XNET_RESULT_OK,
			"SOCKS5 fixture tunnel preface failed"
		);
	}
}



#endif
