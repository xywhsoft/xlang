#include "../common/xl_common.h"
#include "xlrt_generated.h"

typedef struct XL_NetUdpHandle {
	SOCKET hSocket;
} XL_NetUdpHandle;

static int __xlNetUdpEnsureStartup(void)
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

static void __xlNetUdpCloseRaw(SOCKET hSocket)
{
#if defined(_WIN32) || defined(_WIN64)
	closesocket(hSocket);
#else
	close(hSocket);
#endif
}

static XL_NetUdpHandle* __xlNetUdpWrap(SOCKET hSocket)
{
	XL_NetUdpHandle* pHandle;

	if ( hSocket == (SOCKET)(~0) ) {
		return NULL;
	}
	pHandle = (XL_NetUdpHandle*)malloc(sizeof(XL_NetUdpHandle));
	if ( pHandle == NULL ) {
		__xlNetUdpCloseRaw(hSocket);
		return NULL;
	}
	pHandle->hSocket = hSocket;
	return pHandle;
}

static XL_NetUdpHandle* __xlNetUdpFromPoint(void* pValue)
{
	XL_NetUdpHandle* pHandle = (XL_NetUdpHandle*)pValue;

	if ( (pHandle == NULL) || (pHandle->hSocket == (SOCKET)(~0)) ) {
		return NULL;
	}
	return pHandle;
}

static int __xlNetUdpWaitReadable(SOCKET hSocket, int iTimeoutMs)
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

static SOCKET __xlNetUdpBindSocket(const char* sHost, int iPort)
{
	struct addrinfo hints;
	struct addrinfo* pList = NULL;
	struct addrinfo* pCur;
	char sPort[32];
	SOCKET hSocket = (SOCKET)(~0);
	struct sockaddr_in addr4;

	if ( !__xlNetUdpEnsureStartup() ) {
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
		hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if ( hSocket == (SOCKET)(~0) ) {
			return hSocket;
		}
		if ( bind(hSocket, (struct sockaddr*)&addr4, (socklen_t)sizeof(addr4)) == 0 ) {
			return hSocket;
		}
		__xlNetUdpCloseRaw(hSocket);
		return (SOCKET)(~0);
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
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
		if ( bind(hSocket, pCur->ai_addr, (socklen_t)pCur->ai_addrlen) == 0 ) {
			break;
		}
		__xlNetUdpCloseRaw(hSocket);
		hSocket = (SOCKET)(~0);
	}
	freeaddrinfo(pList);
	return hSocket;
}

static int __xlNetUdpTableSetText(xvalue value, const char* sKey, const char* sText)
{
	xvalue itemValue;

	if ( (value == NULL) || (sKey == NULL) ) {
		return 0;
	}
	itemValue = xvoCreateText((str)((sText != NULL) ? sText : ""), (uint32)strlen((sText != NULL) ? sText : ""), FALSE);
	if ( itemValue == NULL ) {
		return 0;
	}
	if ( !xvoTableSetValue(value, (str)sKey, (uint32)strlen(sKey), itemValue, TRUE) ) {
		xvoUnref(itemValue);
		return 0;
	}
	return 1;
}

void* xl_net_udp_bind(const char* sHost, int iPort)
{
	XL_NetUdpHandle* pHandle = __xlNetUdpWrap(__xlNetUdpBindSocket(sHost, iPort));

	if ( pHandle == NULL ) {
		xlrt_native_error_raise("NetUdpError", "udp bind failed", 0, sHost);
		return NULL;
	}
	return pHandle;
}

int xl_net_udp_send_to(void* pSocket, const char* sHost, int iPort, const char* sText)
{
	XL_NetUdpHandle* pHandle = __xlNetUdpFromPoint(pSocket);
	struct addrinfo hints;
	struct addrinfo* pList = NULL;
	char sPort[32];
	int iSent = -1;
	struct sockaddr_in addr4;

	if ( pHandle == NULL ) {
		return -1;
	}
	if ( !__xlNetUdpEnsureStartup() ) {
		return -1;
	}
	memset(&addr4, 0, sizeof(addr4));
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons((uint16_t)((iPort > 0) ? iPort : 0));
	if ( inet_pton(AF_INET, (sHost != NULL) ? sHost : "127.0.0.1", &addr4.sin_addr) == 1 ) {
		return (int)sendto(
			pHandle->hSocket,
			(sText != NULL) ? sText : "",
			(int)strlen((sText != NULL) ? sText : ""),
			0,
			(struct sockaddr*)&addr4,
			(socklen_t)sizeof(addr4)
		);
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	snprintf(sPort, sizeof(sPort), "%d", (iPort > 0) ? iPort : 0);
	if ( getaddrinfo((sHost != NULL) ? sHost : "127.0.0.1", sPort, &hints, &pList) != 0 ) {
		return -1;
	}
	if ( pList != NULL ) {
		iSent = (int)sendto(
			pHandle->hSocket,
			(sText != NULL) ? sText : "",
			(int)strlen((sText != NULL) ? sText : ""),
			0,
			pList->ai_addr,
			(socklen_t)pList->ai_addrlen
		);
	}
	freeaddrinfo(pList);
	return iSent;
}

xvalue xl_net_udp_recv_from(void* pSocket, int iSize, int iTimeoutMs)
{
	XL_NetUdpHandle* pHandle = __xlNetUdpFromPoint(pSocket);
	xvalue value;
	char* sBuf;
	struct sockaddr_storage addr;
	socklen_t iAddrLen = (socklen_t)sizeof(addr);
	int iRead;
	char sHost[NI_MAXHOST];
	char sPort[NI_MAXSERV];

	if ( (pHandle == NULL) || (iSize <= 0) ) {
		return NULL;
	}
	if ( __xlNetUdpWaitReadable(pHandle->hSocket, iTimeoutMs) <= 0 ) {
		return xvoCreateTable();
	}
	sBuf = (char*)malloc((size_t)iSize + 1u);
	if ( sBuf == NULL ) {
		return NULL;
	}
	iRead = (int)recvfrom(pHandle->hSocket, sBuf, iSize, 0, (struct sockaddr*)&addr, &iAddrLen);
	if ( iRead < 0 ) {
		free(sBuf);
		return xvoCreateTable();
	}
	sBuf[iRead] = '\0';
	sHost[0] = '\0';
	sPort[0] = '\0';
	(void)getnameinfo((struct sockaddr*)&addr, iAddrLen, sHost, (DWORD)sizeof(sHost), sPort, (DWORD)sizeof(sPort), NI_NUMERICHOST | NI_NUMERICSERV);
	value = xvoCreateTable();
	if ( (value == NULL) ||
		!__xlNetUdpTableSetText(value, "text", sBuf) ||
		!__xlNetUdpTableSetText(value, "host", sHost) ||
		!__xlNetUdpTableSetText(value, "port", sPort) ) {
		free(sBuf);
		if ( value != NULL ) {
			xvoUnref(value);
		}
		return NULL;
	}
	free(sBuf);
	return value;
}

int xl_net_udp_local_port(void* pSocket)
{
	XL_NetUdpHandle* pHandle = __xlNetUdpFromPoint(pSocket);
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

int xl_net_udp_close(void* pSocket)
{
	XL_NetUdpHandle* pHandle = __xlNetUdpFromPoint(pSocket);

	if ( pHandle == NULL ) {
		return 0;
	}
	__xlNetUdpCloseRaw(pHandle->hSocket);
	pHandle->hSocket = (SOCKET)(~0);
	free(pHandle);
	return 1;
}
