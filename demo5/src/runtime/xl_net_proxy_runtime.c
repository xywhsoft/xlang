#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static char* __xlProxySlice(const char* sText, size_t iLen)
{
	char* sOut = (char*)malloc(iLen + 1u);

	if ( sOut == NULL ) {
		return NULL;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static int __xlProxyTableSetText(xvalue value, const char* sKey, const char* sText)
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

static char* __xlProxyAllocFormat(const char* sFormat, ...)
{
	va_list args;
	va_list args2;
	int iNeed;
	char* sOut;

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return NULL;
	}
	sOut = (char*)malloc((size_t)iNeed + 1u);
	if ( sOut == NULL ) {
		va_end(args2);
		return NULL;
	}
	vsnprintf(sOut, (size_t)iNeed + 1u, sFormat, args2);
	va_end(args2);
	return sOut;
}

static void __xlProxyHexByte(char* sOut, size_t* piOffset, unsigned int iValue)
{
	static const char* sHex = "0123456789abcdef";

	sOut[*piOffset] = sHex[(iValue >> 4) & 0x0Fu];
	sOut[*piOffset + 1u] = sHex[iValue & 0x0Fu];
	*piOffset += 2u;
}

xvalue xl_net_proxy_parse(const char* sUrl)
{
	xvalue value;
	const char* sSchemeEnd;
	const char* sRest;
	const char* sAt;
	const char* sHostStart;
	const char* sPortStart;
	const char* sPathStart;
	char* sScheme;
	char* sUser;
	char* sPass;
	char* sHost;
	char* sPort;
	size_t iHostLen;

	if ( sUrl == NULL ) {
		sUrl = "";
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		return NULL;
	}
	sScheme = NULL;
	sUser = NULL;
	sPass = NULL;
	sHost = NULL;
	sPort = NULL;

	sSchemeEnd = strstr(sUrl, "://");
	if ( sSchemeEnd != NULL ) {
		sScheme = __xlProxySlice(sUrl, (size_t)(sSchemeEnd - sUrl));
		sRest = sSchemeEnd + 3;
	} else {
		sScheme = __xlProxySlice("http", 4u);
		sRest = sUrl;
	}
	sPathStart = strchr(sRest, '/');
	sAt = strchr(sRest, '@');
	if ( (sAt != NULL) && ((sPathStart == NULL) || (sAt < sPathStart)) ) {
		const char* sColon = memchr(sRest, ':', (size_t)(sAt - sRest));
		if ( sColon != NULL ) {
			sUser = __xlProxySlice(sRest, (size_t)(sColon - sRest));
			sPass = __xlProxySlice(sColon + 1, (size_t)(sAt - (sColon + 1)));
		} else {
			sUser = __xlProxySlice(sRest, (size_t)(sAt - sRest));
			sPass = __xlProxySlice("", 0u);
		}
		sHostStart = sAt + 1;
	} else {
		sUser = __xlProxySlice("", 0u);
		sPass = __xlProxySlice("", 0u);
		sHostStart = sRest;
	}

	sPathStart = strchr(sHostStart, '/');
	sPortStart = strchr(sHostStart, ':');
	if ( (sPortStart != NULL) && ((sPathStart == NULL) || (sPortStart < sPathStart)) ) {
		iHostLen = (size_t)(sPortStart - sHostStart);
		sHost = __xlProxySlice(sHostStart, iHostLen);
		if ( sPathStart != NULL ) {
			sPort = __xlProxySlice(sPortStart + 1, (size_t)(sPathStart - (sPortStart + 1)));
		} else {
			sPort = __xlProxySlice(sPortStart + 1, strlen(sPortStart + 1));
		}
	} else {
		iHostLen = (sPathStart != NULL) ? (size_t)(sPathStart - sHostStart) : strlen(sHostStart);
		sHost = __xlProxySlice(sHostStart, iHostLen);
		if ( (sScheme != NULL) && (strcmp(sScheme, "socks5") == 0) ) {
			sPort = __xlProxySlice("1080", 4u);
		} else {
			sPort = __xlProxySlice("8080", 4u);
		}
	}

	if ( (sScheme == NULL) || (sUser == NULL) || (sPass == NULL) || (sHost == NULL) || (sPort == NULL) ||
		!__xlProxyTableSetText(value, "scheme", sScheme) ||
		!__xlProxyTableSetText(value, "host", sHost) ||
		!__xlProxyTableSetText(value, "port", sPort) ||
		!__xlProxyTableSetText(value, "user", sUser) ||
		!__xlProxyTableSetText(value, "pass", sPass) ) {
		free(sScheme);
		free(sUser);
		free(sPass);
		free(sHost);
		free(sPort);
		xvoUnref(value);
		return NULL;
	}

	free(sScheme);
	free(sUser);
	free(sPass);
	free(sHost);
	free(sPort);
	return value;
}

const char* xl_net_proxy_basic_auth(const char* sUser, const char* sPass)
{
	char* sCred;
	str sEncoded;
	char* sOut;

	if ( sUser == NULL ) {
		sUser = "";
	}
	if ( sPass == NULL ) {
		sPass = "";
	}
	sCred = __xlProxyAllocFormat("%s:%s", sUser, sPass);
	if ( sCred == NULL ) {
		return "";
	}
	sEncoded = xrtBase64Encode((ptr)sCred, strlen(sCred), NULL);
	free(sCred);
	if ( sEncoded == NULL ) {
		return "";
	}
	sOut = __xlProxyAllocFormat("Basic %s", (const char*)sEncoded);
	xrtFree(sEncoded);
	return (sOut != NULL) ? sOut : "";
}

const char* xl_net_proxy_http_connect(const char* sHost, int iPort, const char* sAuth)
{
	char* sOut;

	if ( sHost == NULL ) {
		sHost = "";
	}
	if ( (sAuth != NULL) && (sAuth[0] != '\0') ) {
		sOut = __xlProxyAllocFormat(
			"CONNECT %s:%d HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Proxy-Authorization: %s\r\n"
			"\r\n",
			sHost,
			iPort,
			sHost,
			iPort,
			sAuth
		);
	} else {
		sOut = __xlProxyAllocFormat(
			"CONNECT %s:%d HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"\r\n",
			sHost,
			iPort,
			sHost,
			iPort
		);
	}
	return (sOut != NULL) ? sOut : "";
}

const char* xl_net_proxy_socks5_greeting_hex(int bAuth)
{
	return bAuth ? "05020002" : "050100";
}

const char* xl_net_proxy_socks5_connect_hex(const char* sHost, int iPort)
{
	size_t iHostLen;
	size_t iCap;
	size_t iOffset;
	char* sOut;
	size_t i;

	if ( sHost == NULL ) {
		sHost = "";
	}
	iHostLen = strlen(sHost);
	if ( iHostLen > 255u ) {
		xlrt_native_error_raise("NetProxyError", "socks5 host too long", 0, sHost);
		return "";
	}
	iCap = 2u * (7u + iHostLen) + 1u;
	sOut = (char*)malloc(iCap);
	if ( sOut == NULL ) {
		return "";
	}
	iOffset = 0u;
	__xlProxyHexByte(sOut, &iOffset, 0x05u);
	__xlProxyHexByte(sOut, &iOffset, 0x01u);
	__xlProxyHexByte(sOut, &iOffset, 0x00u);
	__xlProxyHexByte(sOut, &iOffset, 0x03u);
	__xlProxyHexByte(sOut, &iOffset, (unsigned int)iHostLen);
	for ( i = 0u; i < iHostLen; ++i ) {
		__xlProxyHexByte(sOut, &iOffset, (unsigned char)sHost[i]);
	}
	__xlProxyHexByte(sOut, &iOffset, (unsigned int)(((iPort > 0) ? iPort : 0) >> 8) & 0xFFu);
	__xlProxyHexByte(sOut, &iOffset, (unsigned int)((iPort > 0) ? iPort : 0) & 0xFFu);
	sOut[iOffset] = '\0';
	return sOut;
}
