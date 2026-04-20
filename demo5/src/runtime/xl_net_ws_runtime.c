#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static int __xlWsTableSetText(xvalue value, const char* sKey, const char* sText)
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

static int __xlWsTableSetIntText(xvalue value, const char* sKey, int iValue)
{
	char sBuf[64];

	snprintf(sBuf, sizeof(sBuf), "%d", iValue);
	return __xlWsTableSetText(value, sKey, sBuf);
}

static char* __xlWsAllocFormat(const char* sFormat, ...)
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

const char* xl_net_ws_key(void)
{
	uint8 aRandom[16];

	xrtRandomBytes(aRandom, sizeof(aRandom));
	return XLRT_CSTR(xrtBase64Encode(aRandom, sizeof(aRandom), NULL));
}

const char* xl_net_ws_accept(const char* sKey)
{
	static const char* sGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char sCombined[256];
	uint8 aDigest[20];

	if ( sKey == NULL ) {
		sKey = "";
	}
	snprintf(sCombined, sizeof(sCombined), "%s%s", sKey, sGuid);
	xrtSHA1((ptr)sCombined, strlen(sCombined), aDigest);
	return XLRT_CSTR(xrtBase64Encode(aDigest, sizeof(aDigest), NULL));
}

const char* xl_net_ws_request(const char* sHost, const char* sPath, const char* sKey)
{
	char* sOut;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		sPath = "/";
	}
	if ( sHost == NULL ) {
		sHost = "";
	}
	if ( (sKey == NULL) || (sKey[0] == '\0') ) {
		sKey = xl_net_ws_key();
	}
	sOut = __xlWsAllocFormat(
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: %s\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n",
		sPath,
		sHost,
		sKey
	);
	return (sOut != NULL) ? sOut : "";
}

const char* xl_net_ws_response(const char* sKey)
{
	const char* sAccept = xl_net_ws_accept(sKey);
	char* sOut;

	sOut = __xlWsAllocFormat(
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n",
		sAccept
	);
	return (sOut != NULL) ? sOut : "";
}

const char* xl_net_ws_frame_text(const char* sText, int bMask)
{
	size_t iLen;
	size_t iHeaderLen;
	size_t iTotal;
	uint8 aMask[4] = { 1u, 2u, 3u, 4u };
	unsigned char* pOut;
	size_t i;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	if ( iLen > 125u ) {
		xlrt_native_error_raise("NetWsError", "ws frame_text only supports payload <= 125 in stage1", 0, sText);
		return "";
	}
	iHeaderLen = bMask ? 6u : 2u;
	iTotal = iHeaderLen + iLen;
	pOut = (unsigned char*)malloc(iTotal + 1u);
	if ( pOut == NULL ) {
		return "";
	}
	pOut[0] = 0x81u;
	pOut[1] = (unsigned char)((bMask ? 0x80u : 0u) | (unsigned char)iLen);
	if ( bMask ) {
		memcpy(pOut + 2, aMask, sizeof(aMask));
		for ( i = 0u; i < iLen; ++i ) {
			pOut[6u + i] = ((unsigned char)sText[i]) ^ aMask[i % 4u];
		}
	} else if ( iLen != 0u ) {
		memcpy(pOut + 2, sText, iLen);
	}
	pOut[iTotal] = '\0';
	return (const char*)pOut;
}

xvalue xl_net_ws_unframe(const char* sFrame)
{
	xvalue value;
	const unsigned char* pFrame = (const unsigned char*)sFrame;
	size_t iLen;
	size_t iPayloadLen;
	size_t iOffset;
	int bFin;
	int bMasked;
	int iOpcode;
	uint8 aMask[4] = { 0u, 0u, 0u, 0u };
	char* sText;
	size_t i;

	if ( sFrame == NULL ) {
		sFrame = "";
		pFrame = (const unsigned char*)sFrame;
	}
	iLen = strlen(sFrame);
	value = xvoCreateTable();
	if ( value == NULL ) {
		return NULL;
	}
	if ( iLen < 2u ) {
		(void)__xlWsTableSetText(value, "text", "");
		return value;
	}
	bFin = ((pFrame[0] & 0x80u) != 0u);
	iOpcode = (int)(pFrame[0] & 0x0Fu);
	bMasked = ((pFrame[1] & 0x80u) != 0u);
	iPayloadLen = (size_t)(pFrame[1] & 0x7Fu);
	iOffset = 2u;
	if ( iPayloadLen == 126u || iPayloadLen == 127u ) {
		xvoUnref(value);
		xlrt_native_error_raise("NetWsError", "ws unframe only supports payload <= 125 in stage1", 0, NULL);
		return NULL;
	}
	if ( bMasked ) {
		if ( iLen < 6u ) {
			(void)__xlWsTableSetText(value, "text", "");
			return value;
		}
		memcpy(aMask, pFrame + 2, sizeof(aMask));
		iOffset += 4u;
	}
	if ( iLen < iOffset + iPayloadLen ) {
		(void)__xlWsTableSetText(value, "text", "");
		return value;
	}
	sText = (char*)malloc(iPayloadLen + 1u);
	if ( sText == NULL ) {
		xvoUnref(value);
		return NULL;
	}
	for ( i = 0u; i < iPayloadLen; ++i ) {
		unsigned char ch = pFrame[iOffset + i];
		if ( bMasked ) {
			ch ^= aMask[i % 4u];
		}
		sText[i] = (char)ch;
	}
	sText[iPayloadLen] = '\0';
	if ( !__xlWsTableSetText(value, "text", sText) ||
		!__xlWsTableSetText(value, "fin", bFin ? "true" : "false") ||
		!__xlWsTableSetText(value, "masked", bMasked ? "true" : "false") ||
		!__xlWsTableSetIntText(value, "opcode", iOpcode) ||
		!__xlWsTableSetIntText(value, "length", (int)iPayloadLen) ) {
		free(sText);
		xvoUnref(value);
		return NULL;
	}
	free(sText);
	return value;
}
