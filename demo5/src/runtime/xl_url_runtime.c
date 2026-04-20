#include "../common/xl_common.h"
#include "xlrt_generated.h"

static int __xlUrlSetText(xvalue table, const char* sKey, const char* sValue)
{
	xvalue itemValue;
	size_t iKeyLen;
	size_t iValueLen;

	if ( (table == NULL) || (sKey == NULL) ) {
		return 0;
	}

	if ( sValue == NULL ) {
		sValue = "";
	}
	iKeyLen = strlen(sKey);
	iValueLen = strlen(sValue);
	itemValue = xvoCreateText((str)sValue, (uint32)iValueLen, FALSE);
	if ( itemValue == NULL ) {
		return 0;
	}
	if ( !xvoTableSetValue(table, (str)sKey, (uint32)iKeyLen, itemValue, TRUE) ) {
		xvoUnref(itemValue);
		return 0;
	}
	return 1;
}

static const char* __xlUrlGetText(xvalue table, const char* sKey)
{
	xvalue itemValue;

	if ( (table == NULL) || (sKey == NULL) ) {
		return "";
	}

	itemValue = xvoTableGetValue(table, (str)sKey, (uint32)strlen(sKey));
	if ( itemValue == NULL ) {
		return "";
	}
	if ( xvoGetText(itemValue) == NULL ) {
		return "";
	}
	return (const char*)xvoGetText(itemValue);
}

static char* __xlUrlSlice(const char* sText, size_t iStart, size_t iLen)
{
	char* sOut;

	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText + iStart, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

xvalue xl_url_parse(const char* sText)
{
	xvalue value;
	const char* sRest;
	const char* sSchemeSep;
	const char* sHash;
	const char* sQuery;
	const char* sSlash;
	const char* sAt;
	const char* sColon;
	char* sTmp;
	char* sAuthority;
	char* sHostPort;

	if ( sText == NULL ) {
		sText = "";
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
		return NULL;
	}

	if ( !__xlUrlSetText(value, "scheme", "") ||
		!__xlUrlSetText(value, "userinfo", "") ||
		!__xlUrlSetText(value, "host", "") ||
		!__xlUrlSetText(value, "port", "") ||
		!__xlUrlSetText(value, "path", "") ||
		!__xlUrlSetText(value, "query", "") ||
		!__xlUrlSetText(value, "fragment", "") ||
		!__xlUrlSetText(value, "authority", "") ) {
		xvoUnref(value);
			xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
		return NULL;
	}

	sRest = sText;
	sSchemeSep = strstr(sRest, "://");
	if ( sSchemeSep != NULL ) {
		sTmp = __xlUrlSlice(sRest, 0u, (size_t)(sSchemeSep - sRest));
		if ( (sTmp == NULL) || !__xlUrlSetText(value, "scheme", sTmp) ) {
			free(sTmp);
			xvoUnref(value);
			xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
			return NULL;
		}
		free(sTmp);
		sRest = sSchemeSep + 3;
	}

	sHash = strchr(sRest, '#');
	if ( sHash != NULL ) {
		if ( !__xlUrlSetText(value, "fragment", sHash + 1) ) {
			xvoUnref(value);
			xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
			return NULL;
		}
	}

	sQuery = strchr(sRest, '?');
	if ( (sQuery != NULL) && ((sHash == NULL) || (sQuery < sHash)) ) {
		const char* sQueryEnd = (sHash != NULL) ? sHash : (sRest + strlen(sRest));
		sTmp = __xlUrlSlice(sQuery + 1, 0u, (size_t)(sQueryEnd - (sQuery + 1)));
		if ( (sTmp == NULL) || !__xlUrlSetText(value, "query", sTmp) ) {
			free(sTmp);
			xvoUnref(value);
			xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
			return NULL;
		}
		free(sTmp);
	}

	{
		const char* sAuthorityEnd = sRest + strlen(sRest);
		if ( (sQuery != NULL) && (sQuery < sAuthorityEnd) ) {
			sAuthorityEnd = sQuery;
		}
		if ( (sHash != NULL) && (sHash < sAuthorityEnd) ) {
			sAuthorityEnd = sHash;
		}
		sSlash = memchr(sRest, '/', (size_t)(sAuthorityEnd - sRest));
		if ( sSlash != NULL ) {
			sAuthority = __xlUrlSlice(sRest, 0u, (size_t)(sSlash - sRest));
			sTmp = __xlUrlSlice(sSlash, 0u, (size_t)(sAuthorityEnd - sSlash));
			if ( (sAuthority == NULL) || (sTmp == NULL) ||
				!__xlUrlSetText(value, "authority", sAuthority) ||
				!__xlUrlSetText(value, "path", sTmp) ) {
				free(sAuthority);
				free(sTmp);
				xvoUnref(value);
				xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
				return NULL;
			}
			free(sTmp);
		} else {
			sAuthority = __xlUrlSlice(sRest, 0u, (size_t)(sAuthorityEnd - sRest));
			if ( (sAuthority == NULL) || !__xlUrlSetText(value, "authority", sAuthority) ) {
				free(sAuthority);
				xvoUnref(value);
				xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
				return NULL;
			}
		}
	}

	sHostPort = sAuthority;
	sAt = strrchr(sHostPort, '@');
	if ( sAt != NULL ) {
		sTmp = __xlUrlSlice(sHostPort, 0u, (size_t)(sAt - sHostPort));
		if ( (sTmp == NULL) || !__xlUrlSetText(value, "userinfo", sTmp) ) {
			free(sTmp);
			free(sAuthority);
			xvoUnref(value);
			xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
			return NULL;
		}
		free(sTmp);
		sHostPort = (char*)(sAt + 1);
	}

	sColon = strrchr(sHostPort, ':');
	if ( sColon != NULL ) {
		sTmp = __xlUrlSlice(sHostPort, 0u, (size_t)(sColon - sHostPort));
		if ( (sTmp == NULL) || !__xlUrlSetText(value, "host", sTmp) || !__xlUrlSetText(value, "port", sColon + 1) ) {
			free(sTmp);
			free(sAuthority);
			xvoUnref(value);
			xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
			return NULL;
		}
		free(sTmp);
	} else if ( !__xlUrlSetText(value, "host", sHostPort) ) {
		free(sAuthority);
		xvoUnref(value);
		xlrt_native_error_raise("UrlError", "url parse failed", 0, sText);
		return NULL;
	}

	free(sAuthority);
	return value;
}

const char* xl_url_build(xvalue value)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	const char* sScheme;
	const char* sUserinfo;
	const char* sHost;
	const char* sPort;
	const char* sPath;
	const char* sQuery;
	const char* sFragment;

	sScheme = __xlUrlGetText(value, "scheme");
	sUserinfo = __xlUrlGetText(value, "userinfo");
	sHost = __xlUrlGetText(value, "host");
	sPort = __xlUrlGetText(value, "port");
	sPath = __xlUrlGetText(value, "path");
	sQuery = __xlUrlGetText(value, "query");
	sFragment = __xlUrlGetText(value, "fragment");

	iCap = strlen(sScheme) + strlen(sUserinfo) + strlen(sHost) + strlen(sPort) +
		strlen(sPath) + strlen(sQuery) + strlen(sFragment) + 16u;
	sBuf = (char*)malloc(iCap);
	if ( sBuf == NULL ) {
		xlrt_native_error_raise("UrlError", "url build failed", 0, NULL);
		return "";
	}
	iLen = 0u;
	sBuf[0] = '\0';

	if ( sScheme[0] != '\0' ) {
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "%s://", sScheme);
	}
	if ( sUserinfo[0] != '\0' ) {
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "%s@", sUserinfo);
	}
	if ( sHost[0] != '\0' ) {
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "%s", sHost);
	}
	if ( sPort[0] != '\0' ) {
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, ":%s", sPort);
	}
	if ( sPath[0] != '\0' ) {
		if ( (iLen != 0u) && (sPath[0] != '/') ) {
			iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "/");
		}
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "%s", sPath);
	}
	if ( sQuery[0] != '\0' ) {
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "?%s", sQuery);
	}
	if ( sFragment[0] != '\0' ) {
		iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "#%s", sFragment);
	}

	return sBuf;
}
