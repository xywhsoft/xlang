#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static int __xlNetDnsArrayAppendText(xvalue value, const char* sText)
{
	xvalue itemValue;

	if ( value == NULL ) {
		return 0;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	itemValue = xvoCreateText((str)sText, (uint32)strlen(sText), FALSE);
	if ( itemValue == NULL ) {
		return 0;
	}
	if ( !xvoArrayAppendValue(value, itemValue, TRUE) ) {
		xvoUnref(itemValue);
		return 0;
	}
	return 1;
}

static int __xlNetDnsArrayHasText(xvalue value, const char* sText)
{
	uint32 i;
	uint32 iCount;

	if ( (value == NULL) || (sText == NULL) ) {
		return 0;
	}

	iCount = xvoArrayItemCount(value);
	for ( i = 0u; i < iCount; ++i ) {
		xvalue itemValue = xvoArrayGetValue(value, i);
		const char* sItem = (const char*)xvoGetText(itemValue);

		if ( (sItem != NULL) && (strcmp(sItem, sText) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xlNetDnsParseNumericHost(const char* sHost, char* sOut, size_t iOutCap)
{
	struct in_addr addr4;
	struct in6_addr addr6;
	const char* sText;
	char sLocal[512];
	size_t iLen;

	if ( (sHost == NULL) || (sOut == NULL) || (iOutCap == 0u) ) {
		return 0;
	}

	sText = sHost;
	iLen = strlen(sHost);
	if ( (iLen > 2u) && (sHost[0] == '[') ) {
		const char* sClose = strchr(sHost + 1, ']');
		if ( sClose == NULL ) {
			return 0;
		}
		iLen = (size_t)(sClose - (sHost + 1));
		if ( iLen >= sizeof(sLocal) ) {
			return 0;
		}
		memcpy(sLocal, sHost + 1, iLen);
		sLocal[iLen] = '\0';
		sText = sLocal;
	} else {
		const char* sLastColon = strrchr(sHost, ':');
		const char* sFirstColon = strchr(sHost, ':');
		if ( (sLastColon != NULL) && (sFirstColon == sLastColon) ) {
			size_t iHostLen = (size_t)(sLastColon - sHost);
			const char* sPort = sLastColon + 1;
			size_t iIdx;
			int bDigits = (sPort[0] != '\0');

			for ( iIdx = 0u; sPort[iIdx] != '\0'; ++iIdx ) {
				if ( (sPort[iIdx] < '0') || (sPort[iIdx] > '9') ) {
					bDigits = 0;
					break;
				}
			}
			if ( bDigits && (iHostLen < sizeof(sLocal)) ) {
				memcpy(sLocal, sHost, iHostLen);
				sLocal[iHostLen] = '\0';
				sText = sLocal;
			}
		}
	}

	if ( inet_pton(AF_INET, sText, &addr4) == 1 ) {
		snprintf(sOut, iOutCap, "%s", sText);
		return 1;
	}
	if ( inet_pton(AF_INET6, sText, &addr6) == 1 ) {
		snprintf(sOut, iOutCap, "%s", sText);
		return 1;
	}
	return 0;
}

const char* xl_net_dns_resolve(const char* sHost)
{
	char sNumeric[512];

	if ( sHost == NULL ) {
		return "";
	}
	if ( __xlNetDnsParseNumericHost(sHost, sNumeric, sizeof(sNumeric)) ) {
		return XLRT_CSTR(xrtCopyStr((str)sNumeric, (uint32)strlen(sNumeric)));
	}
	return xl_net_resolve(sHost);
}

xvalue xl_net_dns_resolve_all(const char* sHost)
{
	xvalue value;
	struct addrinfo hints;
	struct addrinfo* pList;
	struct addrinfo* pCur;
	char sHostBuf[NI_MAXHOST];
	size_t iCount;

	if ( sHost == NULL ) {
		sHost = "";
	}

	value = xvoCreateArray();
	if ( value == NULL ) {
		return NULL;
	}
	if ( __xlNetDnsParseNumericHost(sHost, sHostBuf, sizeof(sHostBuf)) ) {
		(void)__xlNetDnsArrayAppendText(value, sHostBuf);
		return value;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if ( getaddrinfo(sHost, NULL, &hints, &pList) != 0 ) {
		return value;
	}

	iCount = 0u;
	for ( pCur = pList; pCur != NULL; pCur = pCur->ai_next ) {
		if ( getnameinfo(pCur->ai_addr, (socklen_t)pCur->ai_addrlen, sHostBuf, (DWORD)sizeof(sHostBuf), NULL, 0, NI_NUMERICHOST) != 0 ) {
			continue;
		}
		if ( !__xlNetDnsArrayHasText(value, sHostBuf) ) {
			if ( !__xlNetDnsArrayAppendText(value, sHostBuf) ) {
				freeaddrinfo(pList);
				xvoUnref(value);
				return NULL;
			}
			++iCount;
		}
	}
	freeaddrinfo(pList);
	return value;
}

const char* xl_net_dns_reverse(const char* sIp)
{
	struct addrinfo hints;
	struct addrinfo* pList;
	char sHostBuf[NI_MAXHOST];
	char sNumeric[512];

	if ( sIp == NULL ) {
		sIp = "";
	}
	if ( !__xlNetDnsParseNumericHost(sIp, sNumeric, sizeof(sNumeric)) ) {
		return "";
	}
	if ( (strcmp(sNumeric, "127.0.0.1") == 0) || (strcmp(sNumeric, "::1") == 0) ) {
		return "localhost";
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	if ( getaddrinfo(sNumeric, NULL, &hints, &pList) != 0 ) {
		return "";
	}
	if ( getnameinfo(pList->ai_addr, (socklen_t)pList->ai_addrlen, sHostBuf, (DWORD)sizeof(sHostBuf), NULL, 0, 0) != 0 ) {
		freeaddrinfo(pList);
		return "";
	}
	freeaddrinfo(pList);
	return XLRT_CSTR(xrtCopyStr((str)sHostBuf, (uint32)strlen(sHostBuf)));
}
