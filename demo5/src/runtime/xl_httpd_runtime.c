#include "../common/xl_common.h"
#include "xlrt_generated.h"

static char* __xlHttpdSlice(const char* sText, size_t iLen)
{
	char* sOut;

	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( (sText != NULL) && (iLen != 0u) ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static int __xlHttpdTableSetText(xvalue value, const char* sKey, const char* sText)
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

static const char* __xlHttpdReason(int iStatus)
{
	switch ( iStatus ) {
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 409: return "Conflict";
		case 415: return "Unsupported Media Type";
		case 422: return "Unprocessable Entity";
		case 429: return "Too Many Requests";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		default: return "OK";
	}
}

static int __xlHttpdSegmentEquals(const char* sLeft, size_t iLeftLen, const char* sRight, size_t iRightLen)
{
	if ( iLeftLen != iRightLen ) {
		return 0;
	}
	if ( iLeftLen == 0u ) {
		return 1;
	}
	return memcmp(sLeft, sRight, iLeftLen) == 0;
}

xvalue xl_httpd_request(const char* sText)
{
	xvalue value;
	const char* sLineEnd;
	const char* sFirstSpace;
	const char* sSecondSpace;
	const char* sHeadersEnd;
	const char* sBodyStart;
	char* sMethod;
	char* sTarget;
	char* sVersion;
	char* sHeadersText;
	char* sBody;
	xvalue urlParts;

	if ( sText == NULL ) {
		sText = "";
	}

	sLineEnd = strstr(sText, "\r\n");
	if ( sLineEnd == NULL ) {
		sLineEnd = strchr(sText, '\n');
	}
	if ( sLineEnd == NULL ) {
		xlrt_native_error_raise("HttpdError", "httpd request parse failed", 0, sText);
		return NULL;
	}

	sFirstSpace = strchr(sText, ' ');
	if ( (sFirstSpace == NULL) || (sFirstSpace >= sLineEnd) ) {
		xlrt_native_error_raise("HttpdError", "httpd request parse failed", 0, sText);
		return NULL;
	}
	sSecondSpace = strchr(sFirstSpace + 1, ' ');
	if ( (sSecondSpace == NULL) || (sSecondSpace >= sLineEnd) ) {
		xlrt_native_error_raise("HttpdError", "httpd request parse failed", 0, sText);
		return NULL;
	}

	sHeadersEnd = strstr(sLineEnd, "\r\n\r\n");
	if ( sHeadersEnd != NULL ) {
		sBodyStart = sHeadersEnd + 4;
		sHeadersText = __xlHttpdSlice(sLineEnd + 2, (size_t)(sHeadersEnd - (sLineEnd + 2)));
	} else {
		sHeadersEnd = strstr(sLineEnd, "\n\n");
		if ( sHeadersEnd == NULL ) {
			sHeadersEnd = sText + strlen(sText);
			sBodyStart = sHeadersEnd;
			sHeadersText = __xlHttpdSlice(sLineEnd + 1, (size_t)(sHeadersEnd - (sLineEnd + 1)));
		} else {
			sBodyStart = sHeadersEnd + 2;
			sHeadersText = __xlHttpdSlice(sLineEnd + 1, (size_t)(sHeadersEnd - (sLineEnd + 1)));
		}
	}

	sMethod = __xlHttpdSlice(sText, (size_t)(sFirstSpace - sText));
	sTarget = __xlHttpdSlice(sFirstSpace + 1, (size_t)(sSecondSpace - (sFirstSpace + 1)));
	sVersion = __xlHttpdSlice(sSecondSpace + 1, (size_t)(sLineEnd - (sSecondSpace + 1)));
	sBody = __xlHttpdSlice(sBodyStart, strlen(sBodyStart));
	value = xvoCreateTable();
	urlParts = xl_url_parse((sTarget != NULL) ? sTarget : "");
	if ( (sMethod == NULL) || (sTarget == NULL) || (sVersion == NULL) || (sHeadersText == NULL) || (sBody == NULL) || (value == NULL) || (urlParts == NULL) ) {
		free(sMethod);
		free(sTarget);
		free(sVersion);
		free(sHeadersText);
		free(sBody);
		if ( value != NULL ) {
			xvoUnref(value);
		}
		if ( urlParts != NULL ) {
			xvoUnref(urlParts);
		}
		xlrt_native_error_raise("HttpdError", "httpd request parse failed", 0, sText);
		return NULL;
	}

	if ( !__xlHttpdTableSetText(value, "method", sMethod) ||
		!__xlHttpdTableSetText(value, "target", sTarget) ||
		!__xlHttpdTableSetText(value, "version", sVersion) ||
		!__xlHttpdTableSetText(value, "headers_text", sHeadersText) ||
		!__xlHttpdTableSetText(value, "body", sBody) ||
		!__xlHttpdTableSetText(value, "path", xl_value_to_string(xvoTableGetValue(urlParts, (str)"path", 4u))) ||
		!__xlHttpdTableSetText(value, "query", xl_value_to_string(xvoTableGetValue(urlParts, (str)"query", 5u))) ||
		!__xlHttpdTableSetText(value, "fragment", xl_value_to_string(xvoTableGetValue(urlParts, (str)"fragment", 8u))) ) {
		free(sMethod);
		free(sTarget);
		free(sVersion);
		free(sHeadersText);
		free(sBody);
		xvoUnref(urlParts);
		xvoUnref(value);
		xlrt_native_error_raise("HttpdError", "httpd request parse failed", 0, sText);
		return NULL;
	}

	free(sMethod);
	free(sTarget);
	free(sVersion);
	free(sHeadersText);
	free(sBody);
	xvoUnref(urlParts);
	return value;
}

xvalue xl_httpd_match(const char* sPattern, const char* sPath)
{
	xvalue value;
	const char* sPat;
	const char* sCur;

	if ( sPattern == NULL ) {
		sPattern = "";
	}
	if ( sPath == NULL ) {
		sPath = "";
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		xlrt_native_error_raise("HttpdError", "httpd route match failed", 0, sPattern);
		return NULL;
	}

	sPat = sPattern;
	sCur = sPath;
	while ( 1 ) {
		const char* sPatNext;
		const char* sCurNext;
		size_t iPatLen;
		size_t iCurLen;

		while ( *sPat == '/' ) {
			++sPat;
		}
		while ( *sCur == '/' ) {
			++sCur;
		}

		if ( (*sPat == '\0') && (*sCur == '\0') ) {
			return value;
		}
		if ( (*sPat == '\0') || (*sCur == '\0') ) {
			xvoUnref(value);
			return xvoCreateTable();
		}

		sPatNext = strchr(sPat, '/');
		sCurNext = strchr(sCur, '/');
		iPatLen = (sPatNext != NULL) ? (size_t)(sPatNext - sPat) : strlen(sPat);
		iCurLen = (sCurNext != NULL) ? (size_t)(sCurNext - sCur) : strlen(sCur);

		if ( (iPatLen > 1u) && (sPat[0] == ':') ) {
			char* sKey = __xlHttpdSlice(sPat + 1, iPatLen - 1u);
			char* sVal = __xlHttpdSlice(sCur, iCurLen);
			int bOk = (sKey != NULL) && (sVal != NULL) && __xlHttpdTableSetText(value, sKey, sVal);

			free(sKey);
			free(sVal);
			if ( !bOk ) {
				xvoUnref(value);
				xlrt_native_error_raise("HttpdError", "httpd route match failed", 0, sPattern);
				return NULL;
			}
		} else if ( !__xlHttpdSegmentEquals(sPat, iPatLen, sCur, iCurLen) ) {
			xvoUnref(value);
			return xvoCreateTable();
		}

		sPat = (sPatNext != NULL) ? (sPatNext + 1) : (sPat + iPatLen);
		sCur = (sCurNext != NULL) ? (sCurNext + 1) : (sCur + iCurLen);
	}
}

const char* xl_httpd_response(int iStatus, xvalue headers, const char* sBody)
{
	return xl_http_response(iStatus, __xlHttpdReason(iStatus), headers, sBody);
}

const char* xl_httpd_text(int iStatus, const char* sBody)
{
	xvalue headers;
	const char* sText;

	headers = xvoCreateTable();
	if ( headers == NULL ) {
		xlrt_native_error_raise("HttpdError", "httpd text response failed", 0, NULL);
		return "";
	}
	if ( !__xlHttpdTableSetText(headers, "Content-Type", "text/plain; charset=utf-8") ) {
		xvoUnref(headers);
		xlrt_native_error_raise("HttpdError", "httpd text response failed", 0, NULL);
		return "";
	}
	sText = xl_httpd_response(iStatus, headers, sBody);
	xvoUnref(headers);
	return sText;
}
