#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static int __xlNetIpTableSetText(xvalue value, const char* sKey, const char* sText)
{
	xvalue itemValue;

	if ( (value == NULL) || (sKey == NULL) ) {
		return 0;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	itemValue = xvoCreateText((str)sText, (uint32)strlen(sText), FALSE);
	if ( itemValue == NULL ) {
		return 0;
	}
	if ( !xvoTableSetValue(value, (str)sKey, (uint32)strlen(sKey), itemValue, TRUE) ) {
		xvoUnref(itemValue);
		return 0;
	}
	return 1;
}

static int __xlNetIpTryParse(const char* sText, xnetaddr* pAddr, char* sHostBuf, size_t iHostCap, int* pbBracketed)
{
	const char* sHost;
	size_t iLen;
	uint16 iPort;
	char sLocal[512];

	if ( pbBracketed != NULL ) {
		*pbBracketed = 0;
	}
	if ( (sText == NULL) || (pAddr == NULL) ) {
		return 0;
	}

	sHost = sText;
	iLen = strlen(sText);
	iPort = 0u;

	if ( (iLen > 2u) && (sText[0] == '[') ) {
		const char* sClose = strchr(sText + 1, ']');
		if ( sClose == NULL ) {
			return 0;
		}
		iLen = (size_t)(sClose - (sText + 1));
		if ( iLen >= sizeof(sLocal) ) {
			return 0;
		}
		memcpy(sLocal, sText + 1, iLen);
		sLocal[iLen] = '\0';
		sHost = sLocal;
		if ( pbBracketed != NULL ) {
			*pbBracketed = 1;
		}
		if ( sClose[1] == ':' ) {
			iPort = (uint16)atoi(sClose + 2);
		} else if ( sClose[1] != '\0' ) {
			return 0;
		}
	} else {
		const char* sLastColon = strrchr(sText, ':');
		const char* sFirstColon = strchr(sText, ':');
		if ( (sLastColon != NULL) && (sFirstColon == sLastColon) ) {
			size_t iHostLen = (size_t)(sLastColon - sText);
			const char* sPortText = sLastColon + 1;
			size_t iPortLen = strlen(sPortText);
			size_t iIdx;
			int bDigits = (iPortLen != 0u);

			for ( iIdx = 0u; iIdx < iPortLen; ++iIdx ) {
				if ( (sPortText[iIdx] < '0') || (sPortText[iIdx] > '9') ) {
					bDigits = 0;
					break;
				}
			}
			if ( bDigits && (iHostLen != 0u) && (iHostLen < sizeof(sLocal)) ) {
				memcpy(sLocal, sText, iHostLen);
				sLocal[iHostLen] = '\0';
				sHost = sLocal;
				iPort = (uint16)atoi(sPortText);
			}
		}
	}

	memset(pAddr, 0, sizeof(*pAddr));
	pAddr->iPort = iPort;
	if ( inet_pton(AF_INET, sHost, pAddr->aAddr) == 1 ) {
		pAddr->iFamily = AF_INET;
	}
	else if ( inet_pton(AF_INET6, sHost, pAddr->aAddr) == 1 ) {
		pAddr->iFamily = AF_INET6;
	}
	else {
		return 0;
	}

	if ( (sHostBuf != NULL) && (iHostCap != 0u) ) {
		snprintf(sHostBuf, iHostCap, "%s", sHost);
	}
	return 1;
}

xvalue xl_net_ip_parse(const char* sText)
{
	xnetaddr addr;
	xvalue value;
	char sHost[512];
	char sPort[32];
	const char* sFamily;
	const char* sTextNorm;
	int bBracketed;

	if ( sText == NULL ) {
		sText = "";
	}

	if ( !__xlNetIpTryParse(sText, &addr, sHost, sizeof(sHost), &bBracketed) ) {
		xlrt_native_error_raise("NetIpError", "ip parse failed", 0, sText);
		return NULL;
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		xlrt_native_error_raise("NetIpError", "ip parse failed", 0, sText);
		return NULL;
	}

	sFamily = (addr.iFamily == AF_INET6) ? "ipv6" : "ipv4";
	sTextNorm = xrtNetAddrToStr(&addr);
	snprintf(sPort, sizeof(sPort), "%u", (unsigned)addr.iPort);
	if ( !__xlNetIpTableSetText(value, "text", (sTextNorm != NULL) ? sTextNorm : "") ||
		!__xlNetIpTableSetText(value, "host", sHost) ||
		!__xlNetIpTableSetText(value, "port", sPort) ||
		!__xlNetIpTableSetText(value, "family", sFamily) ||
		!__xlNetIpTableSetText(value, "is_ipv4", (addr.iFamily == AF_INET) ? "true" : "false") ||
		!__xlNetIpTableSetText(value, "is_ipv6", (addr.iFamily == AF_INET6) ? "true" : "false") ||
		!__xlNetIpTableSetText(value, "bracketed", bBracketed ? "true" : "false") ) {
		xvoUnref(value);
		xlrt_native_error_raise("NetIpError", "ip parse failed", 0, sText);
		return NULL;
	}
	return value;
}

const char* xl_net_ip_build(xvalue value)
{
	xvalue hostValue;
	xvalue portValue;
	const char* sHost;
	const char* sPort;
	size_t iNeed;
	char* sOut;
	int bWrap;

	if ( value == NULL ) {
		return "";
	}

	hostValue = xvoTableGetValue(value, (str)"host", 4u);
	portValue = xvoTableGetValue(value, (str)"port", 4u);
	sHost = xl_value_to_string(hostValue);
	sPort = xl_value_to_string(portValue);
	if ( sHost == NULL ) {
		sHost = "";
	}
	if ( sPort == NULL ) {
		sPort = "";
	}

	bWrap = (strchr(sHost, ':') != NULL);
	if ( sPort[0] == '\0' || strcmp(sPort, "0") == 0 ) {
		return XLRT_CSTR(xrtCopyStr((str)sHost, (uint32)strlen(sHost)));
	}

	iNeed = strlen(sHost) + strlen(sPort) + 4u;
	sOut = (char*)malloc(iNeed);
	if ( sOut == NULL ) {
		xlrt_native_error_raise("NetIpError", "ip build failed", 0, sHost);
		return "";
	}
	if ( bWrap ) {
		snprintf(sOut, iNeed, "[%s]:%s", sHost, sPort);
	} else {
		snprintf(sOut, iNeed, "%s:%s", sHost, sPort);
	}
	return sOut;
}

const char* xl_net_ip_normalize(const char* sText)
{
	xnetaddr addr;
	const char* sOut;

	if ( sText == NULL ) {
		sText = "";
	}
	if ( !__xlNetIpTryParse(sText, &addr, NULL, 0u, NULL) ) {
		return "";
	}
	sOut = xrtNetAddrToStr(&addr);
	if ( sOut == NULL ) {
		return "";
	}
	return sOut;
}

int xl_net_ip_is_ip(const char* sText)
{
	xnetaddr addr;

	if ( sText == NULL ) {
		return 0;
	}
	return __xlNetIpTryParse(sText, &addr, NULL, 0u, NULL) ? 1 : 0;
}

int xl_net_ip_is_ipv4(const char* sText)
{
	xnetaddr addr;

	if ( sText == NULL ) {
		return 0;
	}
	if ( !__xlNetIpTryParse(sText, &addr, NULL, 0u, NULL) ) {
		return 0;
	}
	return (addr.iFamily == AF_INET) ? 1 : 0;
}

int xl_net_ip_is_ipv6(const char* sText)
{
	xnetaddr addr;

	if ( sText == NULL ) {
		return 0;
	}
	if ( !__xlNetIpTryParse(sText, &addr, NULL, 0u, NULL) ) {
		return 0;
	}
	return (addr.iFamily == AF_INET6) ? 1 : 0;
}
