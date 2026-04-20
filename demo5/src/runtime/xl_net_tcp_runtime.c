#include "../common/xl_common.h"
#include "xlrt_generated.h"

typedef struct XL_NetSocketHandle {
	SOCKET hSocket;
	int iKind;
} XL_NetSocketHandle;

static int __xlNetSocketEnsureStartup(void)
{
#if defined(_WIN32) || defined(_WIN64)
	static int sStarted = 0;
	WSADATA data;

	if ( sStarted ) {
		return 1;
	}
	if ( WSAStartup(MAKEWORD(2, 2), &data) != 0 ) {
		return 0;
	}
	sStarted = 1;
#endif
	return 1;
}

static void __xlNetSocketCloseRaw(SOCKET hSocket)
{
#if defined(_WIN32) || defined(_WIN64)
	closesocket(hSocket);
#else
	close(hSocket);
#endif
}

static XL_NetSocketHandle* __xlNetSocketWrap(SOCKET hSocket, int iKind)
{
	XL_NetSocketHandle* pHandle;

	if ( hSocket == (SOCKET)(~0) ) {
		return NULL;
	}
	pHandle = (XL_NetSocketHandle*)malloc(sizeof(XL_NetSocketHandle));
	if ( pHandle == NULL ) {
		__xlNetSocketCloseRaw(hSocket);
		return NULL;
	}
	pHandle->hSocket = hSocket;
	pHandle->iKind = iKind;
	return pHandle;
}

static XL_NetSocketHandle* __xlNetSocketFromPoint(void* pValue, int iKind)
{
	XL_NetSocketHandle* pHandle = (XL_NetSocketHandle*)pValue;

	if ( (pHandle == NULL) || (pHandle->hSocket == (SOCKET)(~0)) ) {
		return NULL;
	}
	if ( (iKind != 0) && (pHandle->iKind != iKind) ) {
		return NULL;
	}
	return pHandle;
}

static int __xlNetSocketWaitReadable(SOCKET hSocket, int iTimeoutMs)
{
	fd_set readSet;
	struct timeval tv;
	struct timeval* pTv = NULL;

	FD_ZERO(&readSet);
	FD_SET(hSocket, &readSet);
	if ( iTimeoutMs >= 0 ) {
		tv.tv_sec = iTimeoutMs / 1000;
		tv.tv_usec = (iTimeoutMs % 1000) * 1000;
		pTv = &tv;
	}
	return select((int)hSocket + 1, &readSet, NULL, NULL, pTv);
}

static SOCKET __xlNetConnectSocket(const char* sHost, int iPort)
{
	struct addrinfo hints;
	struct addrinfo* pList = NULL;
	struct addrinfo* pCur;
	char sPort[32];
	SOCKET hSocket = (SOCKET)(~0);
	struct sockaddr_in addr4;

	if ( !__xlNetSocketEnsureStartup() ) {
		return (SOCKET)(~0);
	}
	memset(&addr4, 0, sizeof(addr4));
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons((uint16_t)((iPort > 0) ? iPort : 0));
	if ( inet_pton(AF_INET, (sHost != NULL) ? sHost : "127.0.0.1", &addr4.sin_addr) == 1 ) {
		hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if ( hSocket == (SOCKET)(~0) ) {
			return hSocket;
		}
		if ( connect(hSocket, (struct sockaddr*)&addr4, (socklen_t)sizeof(addr4)) == 0 ) {
			return hSocket;
		}
		__xlNetSocketCloseRaw(hSocket);
		return (SOCKET)(~0);
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	snprintf(sPort, sizeof(sPort), "%d", (iPort > 0) ? iPort : 0);
	if ( getaddrinfo((sHost != NULL) ? sHost : "127.0.0.1", sPort, &hints, &pList) != 0 ) {
		return (SOCKET)(~0);
	}
	for ( pCur = pList; pCur != NULL; pCur = pCur->ai_next ) {
		hSocket = socket(pCur->ai_family, pCur->ai_socktype, pCur->ai_protocol);
		if ( hSocket == (SOCKET)(~0) ) {
			continue;
		}
		if ( connect(hSocket, pCur->ai_addr, (socklen_t)pCur->ai_addrlen) == 0 ) {
			break;
		}
		__xlNetSocketCloseRaw(hSocket);
		hSocket = (SOCKET)(~0);
	}
	freeaddrinfo(pList);
	return hSocket;
}

static SOCKET __xlNetListenSocket(const char* sHost, int iPort)
{
	struct addrinfo hints;
	struct addrinfo* pList = NULL;
	struct addrinfo* pCur;
	char sPort[32];
	SOCKET hSocket = (SOCKET)(~0);
	int iYes = 1;
	struct sockaddr_in addr4;

	if ( !__xlNetSocketEnsureStartup() ) {
		return (SOCKET)(~0);
	}
	memset(&addr4, 0, sizeof(addr4));
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons((uint16_t)((iPort > 0) ? iPort : 0));
	if ( ((sHost == NULL) || (sHost[0] == '\0')) ) {
		addr4.sin_addr.s_addr = htonl(INADDR_ANY);
	} else if ( inet_pton(AF_INET, sHost, &addr4.sin_addr) != 1 ) {
		addr4.sin_addr.s_addr = 0u;
	}
	if ( ((sHost == NULL) || (sHost[0] == '\0')) || (addr4.sin_addr.s_addr != 0u) ) {
		hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if ( hSocket == (SOCKET)(~0) ) {
			return hSocket;
		}
		setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&iYes, (socklen_t)sizeof(iYes));
		if ( bind(hSocket, (struct sockaddr*)&addr4, (socklen_t)sizeof(addr4)) == 0 && listen(hSocket, 16) == 0 ) {
			return hSocket;
		}
		__xlNetSocketCloseRaw(hSocket);
		return (SOCKET)(~0);
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	snprintf(sPort, sizeof(sPort), "%d", (iPort > 0) ? iPort : 0);
	if ( getaddrinfo(((sHost != NULL) && (sHost[0] != '\0')) ? sHost : NULL, sPort, &hints, &pList) != 0 ) {
		return (SOCKET)(~0);
	}
	for ( pCur = pList; pCur != NULL; pCur = pCur->ai_next ) {
		hSocket = socket(pCur->ai_family, pCur->ai_socktype, pCur->ai_protocol);
		if ( hSocket == (SOCKET)(~0) ) {
			continue;
		}
		setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&iYes, (socklen_t)sizeof(iYes));
		if ( bind(hSocket, pCur->ai_addr, (socklen_t)pCur->ai_addrlen) == 0 && listen(hSocket, 16) == 0 ) {
			break;
		}
		__xlNetSocketCloseRaw(hSocket);
		hSocket = (SOCKET)(~0);
	}
	freeaddrinfo(pList);
	return hSocket;
}

void* xl_net_tcp_listen(const char* sHost, int iPort)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketWrap(__xlNetListenSocket(sHost, iPort), 1);

	if ( pHandle == NULL ) {
		xlrt_native_error_raise("NetTcpError", "tcp listen failed", 0, sHost);
		return NULL;
	}
	return pHandle;
}

void* xl_net_tcp_accept(void* pListener, int iTimeoutMs)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketFromPoint(pListener, 1);
	SOCKET hClient;

	if ( pHandle == NULL ) {
		return NULL;
	}
	if ( __xlNetSocketWaitReadable(pHandle->hSocket, iTimeoutMs) <= 0 ) {
		return NULL;
	}
	hClient = accept(pHandle->hSocket, NULL, NULL);
	if ( hClient == (SOCKET)(~0) ) {
		return NULL;
	}
	return __xlNetSocketWrap(hClient, 2);
}

void* xl_net_tcp_connect(const char* sHost, int iPort)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketWrap(__xlNetConnectSocket(sHost, iPort), 2);

	if ( pHandle == NULL ) {
		xlrt_native_error_raise("NetTcpError", "tcp connect failed", 0, sHost);
		return NULL;
	}
	return pHandle;
}

int xl_net_tcp_send(void* pSocket, const char* sText)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketFromPoint(pSocket, 2);
	size_t iLen;

	if ( pHandle == NULL ) {
		return -1;
	}
	iLen = (sText != NULL) ? strlen(sText) : 0u;
	return (int)send(pHandle->hSocket, (sText != NULL) ? sText : "", (int)iLen, 0);
}

const char* xl_net_tcp_recv(void* pSocket, int iSize, int iTimeoutMs)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketFromPoint(pSocket, 2);
	char* sBuf;
	int iRead;

	if ( (pHandle == NULL) || (iSize <= 0) ) {
		return "";
	}
	if ( __xlNetSocketWaitReadable(pHandle->hSocket, iTimeoutMs) <= 0 ) {
		return "";
	}
	sBuf = (char*)malloc((size_t)iSize + 1u);
	if ( sBuf == NULL ) {
		return "";
	}
	iRead = (int)recv(pHandle->hSocket, sBuf, iSize, 0);
	if ( iRead <= 0 ) {
		free(sBuf);
		return "";
	}
	sBuf[iRead] = '\0';
	return sBuf;
}

int xl_net_tcp_local_port(void* pSocket)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketFromPoint(pSocket, 0);
	struct sockaddr_storage addr;
	socklen_t iLen = (socklen_t)sizeof(addr);

	if ( pHandle == NULL ) {
		return 0;
	}
	if ( getsockname(pHandle->hSocket, (struct sockaddr*)&addr, &iLen) != 0 ) {
		return 0;
	}
	if (((struct sockaddr*)&addr)->sa_family == AF_INET) {
		return (int)ntohs(((struct sockaddr_in*)&addr)->sin_port);
	}
	if (((struct sockaddr*)&addr)->sa_family == AF_INET6) {
		return (int)ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
	}
	return 0;
}

int xl_net_tcp_close(void* pSocket)
{
	XL_NetSocketHandle* pHandle = __xlNetSocketFromPoint(pSocket, 0);

	if ( pHandle == NULL ) {
		return 0;
	}
	__xlNetSocketCloseRaw(pHandle->hSocket);
	pHandle->hSocket = (SOCKET)(~0);
	free(pHandle);
	return 1;
}
