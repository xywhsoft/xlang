#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static int __xlTlsTextEqNoCaseN(const char* sLeft, const char* sRight, size_t iLen)
{
	size_t i;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return 0;
	}
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char a = (unsigned char)sLeft[i];
		unsigned char b = (unsigned char)sRight[i];

		if ( (a >= 'A') && (a <= 'Z') ) {
			a = (unsigned char)(a - 'A' + 'a');
		}
		if ( (b >= 'A') && (b <= 'Z') ) {
			b = (unsigned char)(b - 'A' + 'a');
		}
		if ( a != b ) {
			return 0;
		}
	}
	return 1;
}

static int __xlTlsArrayAppendText(xvalue value, const char* sText)
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

static int __xlTlsTableSetText(xvalue value, const char* sKey, const char* sText)
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

static const char* __xlTlsSchemeEnd(const char* sUrl)
{
	return (sUrl != NULL) ? strstr(sUrl, "://") : NULL;
}

static int __xlTlsSchemeIs(const char* sUrl, const char* sScheme)
{
	const char* sEnd = __xlTlsSchemeEnd(sUrl);
	size_t iLen;

	if ( (sEnd == NULL) || (sScheme == NULL) ) {
		return 0;
	}
	iLen = (size_t)(sEnd - sUrl);
	return (strlen(sScheme) == iLen) && __xlTlsTextEqNoCaseN(sUrl, sScheme, iLen);
}

static char* __xlTlsSlice(const char* sText, size_t iLen)
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

xvalue xl_net_tls_versions(void)
{
	xvalue value = xvoCreateArray();

	if ( value == NULL ) {
		return NULL;
	}
	if ( !__xlTlsArrayAppendText(value, "tls1.2") || !__xlTlsArrayAppendText(value, "tls1.3") ) {
		xvoUnref(value);
		return NULL;
	}
	return value;
}

int xl_net_tls_is_tls_url(const char* sUrl)
{
	return __xlTlsSchemeIs(sUrl, "https") ||
		__xlTlsSchemeIs(sUrl, "wss") ||
		__xlTlsSchemeIs(sUrl, "tls") ||
		__xlTlsSchemeIs(sUrl, "ssl") ||
		__xlTlsSchemeIs(sUrl, "smtps") ||
		__xlTlsSchemeIs(sUrl, "imaps") ||
		__xlTlsSchemeIs(sUrl, "pop3s");
}

int xl_net_tls_default_port(const char* sScheme)
{
	if ( sScheme == NULL ) {
		return 443;
	}
	if ( __xlTlsTextEqNoCaseN(sScheme, "https", 5u) && (strlen(sScheme) == 5u) ) {
		return 443;
	}
	if ( __xlTlsTextEqNoCaseN(sScheme, "wss", 3u) && (strlen(sScheme) == 3u) ) {
		return 443;
	}
	if ( __xlTlsTextEqNoCaseN(sScheme, "tls", 3u) && (strlen(sScheme) == 3u) ) {
		return 443;
	}
	if ( __xlTlsTextEqNoCaseN(sScheme, "smtps", 5u) && (strlen(sScheme) == 5u) ) {
		return 465;
	}
	if ( __xlTlsTextEqNoCaseN(sScheme, "imaps", 5u) && (strlen(sScheme) == 5u) ) {
		return 993;
	}
	if ( __xlTlsTextEqNoCaseN(sScheme, "pop3s", 5u) && (strlen(sScheme) == 5u) ) {
		return 995;
	}
	return 443;
}

const char* xl_net_tls_sni_host(const char* sUrl)
{
	const char* sHost;
	const char* sEnd;
	const char* sAt;
	const char* sColon;
	const char* sSlash;
	size_t iLen;

	if ( sUrl == NULL ) {
		return "";
	}
	sHost = __xlTlsSchemeEnd(sUrl);
	sHost = (sHost != NULL) ? (sHost + 3) : sUrl;
	sSlash = strchr(sHost, '/');
	sEnd = (sSlash != NULL) ? sSlash : (sHost + strlen(sHost));
	sAt = memchr(sHost, '@', (size_t)(sEnd - sHost));
	if ( sAt != NULL ) {
		sHost = sAt + 1;
	}
	if ( (sHost < sEnd) && (*sHost == '[') ) {
		const char* sClose = memchr(sHost + 1, ']', (size_t)(sEnd - (sHost + 1)));
		if ( sClose == NULL ) {
			return "";
		}
		return __xlTlsSlice(sHost + 1, (size_t)(sClose - (sHost + 1)));
	}
	sColon = memchr(sHost, ':', (size_t)(sEnd - sHost));
	iLen = (sColon != NULL) ? (size_t)(sColon - sHost) : (size_t)(sEnd - sHost);
	return __xlTlsSlice(sHost, iLen);
}

const char* xl_net_tls_fingerprint_sha256(const char* sData)
{
	uint8 aDigest[32];
	char sHex[65];
	static const char* sChars = "0123456789abcdef";
	size_t i;

	if ( sData == NULL ) {
		sData = "";
	}
	xrtSHA256((ptr)sData, strlen(sData), aDigest);
	for ( i = 0u; i < sizeof(aDigest); ++i ) {
		sHex[i * 2u] = sChars[(aDigest[i] >> 4) & 0x0Fu];
		sHex[i * 2u + 1u] = sChars[aDigest[i] & 0x0Fu];
	}
	sHex[64] = '\0';
	return XLRT_CSTR(xrtCopyStr((str)sHex, 64u));
}

xvalue xl_net_tls_client_config(const char* sHost, int bVerify)
{
	xvalue value = xvoCreateTable();

	if ( value == NULL ) {
		return NULL;
	}
	if ( sHost == NULL ) {
		sHost = "";
	}
	if ( !__xlTlsTableSetText(value, "host", sHost) ||
		!__xlTlsTableSetText(value, "sni", sHost) ||
		!__xlTlsTableSetText(value, "verify", bVerify ? "true" : "false") ||
		!__xlTlsTableSetText(value, "min_version", "tls1.2") ||
		!__xlTlsTableSetText(value, "max_version", "tls1.3") ) {
		xvoUnref(value);
		return NULL;
	}
	return value;
}
