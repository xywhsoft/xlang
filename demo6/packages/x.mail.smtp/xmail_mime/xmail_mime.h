#ifndef XRT_EXTLIB_XMAIL_MIME_H
#define XRT_EXTLIB_XMAIL_MIME_H

#include <string.h>
#include <ctype.h>
#include <time.h>

#define XMAIL_MIME_QP_LINE_MAX     76u
#define XMAIL_MIME_BASE64_LINE_MAX 76u
#define XMAIL_MIME_HEADER_LINE_MAX  78u

typedef struct {
	const char* sEmail;
	const char* sName;
} xmailaddr;

typedef struct {
	const char* sName;
	const char* sValue;
} xmailheader;

typedef struct {
	const char* sFileName;
	const char* sContentType;
	const char* sContentId;
	const void* pData;
	size_t iDataLen;
	bool bInline;
} xmailmimeattachment;

typedef struct {
	xmailaddr tFrom;
	xmailaddr tReplyTo;
	const xmailaddr* arrTo;
	size_t iToCount;
	const xmailaddr* arrCc;
	size_t iCcCount;
	const xmailaddr* arrBcc;
	size_t iBccCount;
	const char* sSubject;
	const char* sTextBody;
	const char* sHtmlBody;
	const xmailmimeattachment* arrAttachments;
	size_t iAttachmentCount;
	const xmailheader* arrHeaders;
	size_t iHeaderCount;
	const char* sMessageIdDomain;
} xmailmessage;

typedef struct {
	char* sData;
	size_t iLen;
	size_t iCap;
} xmailmimebuf;

typedef struct xmailparsedmessage_struct xmailparsedmessage;

typedef struct xmailmimepart_struct {
	xmailheader* arrHeaders;
	size_t iHeaderCount;
	char sContentType[128];
	char sMediaType[64];
	char sSubType[64];
	char sBoundary[128];
	char sTransferEncoding[32];
	char sDisposition[32];
	char sFileName[256];
	char sContentId[256];
	bool bAttachment;
	bool bInline;
	str sBody;
	str sDecodedBody;
	xmailparsedmessage* pMessage;
	struct xmailmimepart_struct* arrChildren;
	size_t iChildCount;
} xmailmimepart;

struct xmailparsedmessage_struct {
	xmailheader* arrHeaders;
	size_t iHeaderCount;
	str sHeaderBlock;
	str sBody;
	xmailmimepart* pRootPart;
};

static str xmailMimeQuotedPrintableEncode(const char* sText);
static str xmailMimeBuildPart(const xmailmimepart* pPart);
static void xmailMimePartFree(xmailmimepart* pPart);
static bool xmailMimeParseMessage(const char* sRaw, xmailparsedmessage* pOut);

static const char* xmailMimeStrOrEmpty(const char* sText)
{
	return sText ? sText : "";
}

static bool xmailMimeBufReserve(xmailmimebuf* pBuf, size_t iNeed)
{
	char* sNew;
	size_t iCap;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( pBuf->iLen + iNeed + 1u <= pBuf->iCap ) {
		return TRUE;
	}

	iCap = pBuf->iCap ? pBuf->iCap : 256u;
	while ( iCap < pBuf->iLen + iNeed + 1u ) {
		iCap *= 2u;
	}
	sNew = (char*)xrtRealloc(pBuf->sData, iCap);
	if ( sNew == NULL ) {
		return FALSE;
	}
	pBuf->sData = sNew;
	pBuf->iCap = iCap;
	return TRUE;
}

static bool xmailMimeBufAppendN(xmailmimebuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( (sText == NULL) && (iLen != 0u) ) {
		return FALSE;
	}
	if ( !xmailMimeBufReserve(pBuf, iLen) ) {
		return FALSE;
	}
	if ( iLen != 0u ) {
		memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	}
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return TRUE;
}

static bool xmailMimeBufAppend(xmailmimebuf* pBuf, const char* sText)
{
	return xmailMimeBufAppendN(pBuf, xmailMimeStrOrEmpty(sText), strlen(xmailMimeStrOrEmpty(sText)));
}

static str xmailMimeCopyN(const char* sText, size_t iLen)
{
	str sOut;

	if ( sText == NULL && iLen != 0u ) {
		return NULL;
	}
	sOut = (str)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static void xmailMimeBufFree(xmailmimebuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->sData != NULL ) {
		xrtFree(pBuf->sData);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}

static void xmailMimeFree(str sText)
{
	if ( sText != NULL ) {
		xrtFree(sText);
	}
}

static bool xmailMimeHasHeaderInjection(const char* sText)
{
	const unsigned char* p;

	if ( sText == NULL ) {
		return FALSE;
	}
	p = (const unsigned char*)sText;
	while ( *p != '\0' ) {
		if ( (*p == '\r') || (*p == '\n') ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static bool xmailMimeIsHeaderNameSafe(const char* sName)
{
	const unsigned char* p;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return FALSE;
	}
	p = (const unsigned char*)sName;
	while ( *p != '\0' ) {
		if ( (*p <= 0x20u) || (*p >= 0x7Fu) || (*p == ':') ) {
			return FALSE;
		}
		p++;
	}
	return TRUE;
}

static bool xmailMimeHasNonAscii(const char* sText)
{
	const unsigned char* p;

	if ( sText == NULL ) {
		return FALSE;
	}
	p = (const unsigned char*)sText;
	while ( *p != '\0' ) {
		if ( *p >= 0x80u ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static bool xmailMimeIsValidUtf8Text(const char* sText)
{
	if ( sText == NULL || sText[0] == '\0' || !xmailMimeHasNonAscii(sText) ) {
		return TRUE;
	}
	return xrtIsUTF8((str)sText, strlen(sText));
}

static bool xmailMimeNeedsQuotedName(const char* sText)
{
	const unsigned char* p;

	if ( (sText == NULL) || (sText[0] == '\0') ) {
		return FALSE;
	}
	p = (const unsigned char*)sText;
	while ( *p != '\0' ) {
		if ( (*p < 0x20u) || (*p == '"') || (*p == '\\') || (*p == ',') || (*p == '<') || (*p == '>') || (*p == '@') || (*p == ';') ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static bool xmailMimeAppendQuotedText(xmailmimebuf* pBuf, const char* sText)
{
	const char* p;

	if ( !xmailMimeBufAppend(pBuf, "\"") ) {
		return FALSE;
	}
	for ( p = xmailMimeStrOrEmpty(sText); *p != '\0'; p++ ) {
		if ( (*p == '"') || (*p == '\\') ) {
			if ( !xmailMimeBufAppendN(pBuf, "\\", 1u) ) {
				return FALSE;
			}
		}
		if ( !xmailMimeBufAppendN(pBuf, p, 1u) ) {
			return FALSE;
		}
	}
	return xmailMimeBufAppend(pBuf, "\"");
}

static bool xmailMimeAppendPercentEncoded(xmailmimebuf* pBuf, const char* sText)
{
	static const char* sHex = "0123456789ABCDEF";
	const unsigned char* p;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	for ( p = (const unsigned char*)xmailMimeStrOrEmpty(sText); *p != '\0'; p++ ) {
		bool bSafe = ((*p >= 'A') && (*p <= 'Z'))
			|| ((*p >= 'a') && (*p <= 'z'))
			|| ((*p >= '0') && (*p <= '9'))
			|| (*p == '.') || (*p == '-') || (*p == '_') || (*p == '~');
		if ( bSafe ) {
			if ( !xmailMimeBufAppendN(pBuf, (const char*)p, 1u) ) {
				return FALSE;
			}
		} else {
			char sEnc[3];
			sEnc[0] = '%';
			sEnc[1] = sHex[(*p >> 4) & 0x0Fu];
			sEnc[2] = sHex[*p & 0x0Fu];
			if ( !xmailMimeBufAppendN(pBuf, sEnc, 3u) ) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

static str xmailMimeNormalizeCrlf(const char* sText)
{
	xmailmimebuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}

	for ( p = sText; *p != '\0'; p++ ) {
		if ( *p == '\r' ) {
			if ( p[1] == '\n' ) {
				if ( !xmailMimeBufAppend(&tBuf, "\r\n") ) {
					xmailMimeBufFree(&tBuf);
					return NULL;
				}
				p++;
			} else if ( !xmailMimeBufAppend(&tBuf, "\r\n") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
		} else if ( *p == '\n' ) {
			if ( !xmailMimeBufAppend(&tBuf, "\r\n") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
		} else if ( !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	}

	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xmailMimeBase64Wrap(const void* pData, size_t iLen, size_t iLineMax)
{
	xmailmimebuf tBuf;
	str sBase64;
	size_t i;
	size_t iBase64Len;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( iLineMax == 0u ) {
		iLineMax = XMAIL_MIME_BASE64_LINE_MAX;
	}
	sBase64 = xrtBase64Encode((ptr)pData, iLen, NULL);
	if ( sBase64 == NULL ) {
		return NULL;
	}
	iBase64Len = strlen((const char*)sBase64);
	for ( i = 0u; i < iBase64Len; i += iLineMax ) {
		size_t iChunk = iBase64Len - i;
		if ( iChunk > iLineMax ) {
			iChunk = iLineMax;
		}
		if ( !xmailMimeBufAppendN(&tBuf, (const char*)sBase64 + i, iChunk)
			|| !xmailMimeBufAppend(&tBuf, "\r\n") ) {
			xrtFree(sBase64);
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	}
	xrtFree(sBase64);
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xmailMimeEncodeHeaderWord(const char* sText)
{
	str sBase64;
	str sOut;
	size_t iNeed;

	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	if ( xmailMimeHasHeaderInjection(sText) ) {
		return NULL;
	}
	if ( !xmailMimeIsValidUtf8Text(sText) ) {
		return NULL;
	}
	if ( !xmailMimeHasNonAscii(sText) ) {
		return xrtCopyStr((str)sText, 0);
	}

	sBase64 = xrtBase64Encode((ptr)sText, strlen(sText), NULL);
	if ( sBase64 == NULL ) {
		return NULL;
	}
	iNeed = strlen((const char*)sBase64) + 13u;
	sOut = (str)xrtMalloc(iNeed);
	if ( sOut != NULL ) {
		snprintf((char*)sOut, iNeed, "=?UTF-8?B?%s?=", (const char*)sBase64);
	}
	xrtFree(sBase64);
	return sOut;
}

static int xmailMimeHexValue(char ch)
{
	if ( (ch >= '0') && (ch <= '9') ) {
		return ch - '0';
	}
	if ( (ch >= 'A') && (ch <= 'F') ) {
		return ch - 'A' + 10;
	}
	if ( (ch >= 'a') && (ch <= 'f') ) {
		return ch - 'a' + 10;
	}
	return -1;
}

static str xmailMimeDecodeHeaderWord(const char* sText)
{
	const char* sCharsetEnd;
	const char* sEncodingEnd;
	const char* sPayloadEnd;
	char chEncoding;

	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	if ( strncmp(sText, "=?", 2u) != 0 ) {
		return xrtCopyStr((str)sText, 0);
	}
	sCharsetEnd = strstr(sText + 2, "?");
	if ( sCharsetEnd == NULL ) {
		return NULL;
	}
	sEncodingEnd = strstr(sCharsetEnd + 1, "?");
	if ( (sEncodingEnd == NULL) || (sEncodingEnd == sCharsetEnd + 1) ) {
		return NULL;
	}
	sPayloadEnd = strstr(sEncodingEnd + 1, "?=");
	if ( sPayloadEnd == NULL ) {
		return NULL;
	}
	chEncoding = (char)toupper((unsigned char)sCharsetEnd[1]);
	if ( chEncoding == 'B' ) {
		size_t iPayloadLen = (size_t)(sPayloadEnd - (sEncodingEnd + 1));
		char* sPayload = (char*)xrtMalloc(iPayloadLen + 1u);
		ptr pDecoded;
		size_t iOutLen;
		if ( sPayload == NULL ) {
			return NULL;
		}
		memcpy(sPayload, sEncodingEnd + 1, iPayloadLen);
		sPayload[iPayloadLen] = '\0';
		pDecoded = xrtBase64Decode((str)sPayload, iPayloadLen, NULL);
		xrtFree(sPayload);
		if ( pDecoded == NULL ) {
			return NULL;
		}
		iOutLen = strlen((const char*)pDecoded);
		{
			str sOut = (str)xrtMalloc(iOutLen + 1u);
			if ( sOut == NULL ) {
				xrtFree(pDecoded);
				return NULL;
			}
			memcpy(sOut, pDecoded, iOutLen + 1u);
			xrtFree(pDecoded);
			return sOut;
		}
	}
	if ( chEncoding == 'Q' ) {
		xmailmimebuf tBuf;
		const char* p;
		memset(&tBuf, 0, sizeof(tBuf));
		for ( p = sEncodingEnd + 1; p < sPayloadEnd; p++ ) {
			if ( *p == '_' ) {
				if ( !xmailMimeBufAppend(&tBuf, " ") ) {
					xmailMimeBufFree(&tBuf);
					return NULL;
				}
			} else if ( (*p == '=') && (p + 2 < sPayloadEnd) ) {
				int iHi = xmailMimeHexValue(p[1]);
				int iLo = xmailMimeHexValue(p[2]);
				char ch;
				if ( (iHi < 0) || (iLo < 0) ) {
					xmailMimeBufFree(&tBuf);
					return NULL;
				}
				ch = (char)((iHi << 4) | iLo);
				if ( !xmailMimeBufAppendN(&tBuf, &ch, 1u) ) {
					xmailMimeBufFree(&tBuf);
					return NULL;
				}
				p += 2;
			} else if ( !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
		}
		if ( tBuf.sData == NULL ) {
			return xrtCopyStr((str)"", 0);
		}
		return (str)tBuf.sData;
	}
	return NULL;
}

static str xmailMimeFoldHeaderLine(const char* sName, const char* sValue, size_t iLineMax)
{
	xmailmimebuf tBuf;
	size_t iCol;
	const char* p;
	bool bHasWordOnLine;

	if ( (sName == NULL) || (sName[0] == '\0') || xmailMimeHasHeaderInjection(sName) || xmailMimeHasHeaderInjection(sValue) ) {
		return NULL;
	}
	if ( iLineMax == 0u ) {
		iLineMax = XMAIL_MIME_HEADER_LINE_MAX;
	}

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xmailMimeBufAppend(&tBuf, sName) || !xmailMimeBufAppend(&tBuf, ": ") ) {
		xmailMimeBufFree(&tBuf);
		return NULL;
	}
	iCol = strlen(sName) + 2u;
	bHasWordOnLine = FALSE;

	p = xmailMimeStrOrEmpty(sValue);
	while ( *p != '\0' ) {
		const char* sNextSpace;
		size_t iWordLen;

		while ( (*p == ' ') || (*p == '\t') ) {
			p++;
		}
		if ( *p == '\0' ) {
			break;
		}
		sNextSpace = p;
		while ( (*sNextSpace != '\0') && (*sNextSpace != ' ') && (*sNextSpace != '\t') ) {
			sNextSpace++;
		}
		iWordLen = (size_t)(sNextSpace - p);
		if ( bHasWordOnLine && (iCol + 1u + iWordLen > iLineMax) ) {
			if ( !xmailMimeBufAppend(&tBuf, "\r\n ") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			iCol = 1u;
			bHasWordOnLine = FALSE;
		} else if ( bHasWordOnLine ) {
			if ( !xmailMimeBufAppend(&tBuf, " ") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			iCol++;
		}
		if ( !xmailMimeBufAppendN(&tBuf, p, iWordLen) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		iCol += iWordLen;
		bHasWordOnLine = TRUE;
		p = sNextSpace;
	}
	if ( !xmailMimeBufAppend(&tBuf, "\r\n") ) {
		xmailMimeBufFree(&tBuf);
		return NULL;
	}
	return (str)tBuf.sData;
}

static str xmailMimeUnfoldHeaderBlock(const char* sText)
{
	xmailmimebuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}

	p = sText;
	while ( *p != '\0' ) {
		if ( (p[0] == '\r') && (p[1] == '\n') && ((p[2] == ' ') || (p[2] == '\t')) ) {
			if ( !xmailMimeBufAppend(&tBuf, " ") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			p += 2;
			while ( (*p == ' ') || (*p == '\t') ) {
				p++;
			}
			continue;
		} else if ( (p[0] == '\n') && ((p[1] == ' ') || (p[1] == '\t')) ) {
			if ( !xmailMimeBufAppend(&tBuf, " ") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			p += 1;
			while ( (*p == ' ') || (*p == '\t') ) {
				p++;
			}
			continue;
		} else if ( !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		p++;
	}
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xmailMimeRfc2822DateNow(void)
{
	static const char* arrWeekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char* arrMonth[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	time_t tNow;
	struct tm tLocal;
	int iOffset;
	char chSign;
	int iAbsOffset;

	tNow = time(NULL);
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tLocal, &tNow);
#else
	localtime_r(&tNow, &tLocal);
#endif
	iOffset = xrtTimezoneOffset();
	chSign = iOffset >= 0 ? '+' : '-';
	iAbsOffset = iOffset >= 0 ? iOffset : -iOffset;
	return xrtFormat("%s, %02d %s %04d %02d:%02d:%02d %c%02d%02d",
		arrWeekday[tLocal.tm_wday],
		tLocal.tm_mday,
		arrMonth[tLocal.tm_mon],
		tLocal.tm_year + 1900,
		tLocal.tm_hour,
		tLocal.tm_min,
		tLocal.tm_sec,
		chSign,
		iAbsOffset / 3600,
		(iAbsOffset % 3600) / 60);
}

static str xmailMimeBuildMessageId(const char* sDomain)
{
	uint8 arrRandom[12];
	char sHex[25];
	size_t i;
	const char* sUseDomain;

	sUseDomain = ((sDomain != NULL) && (sDomain[0] != '\0')) ? sDomain : "localhost";
	if ( xmailMimeHasHeaderInjection(sUseDomain) ) {
		return NULL;
	}
	xrtRandomBytes(arrRandom, sizeof(arrRandom));
	for ( i = 0u; i < sizeof(arrRandom); i++ ) {
		static const char* sDigits = "0123456789abcdef";
		sHex[i * 2u] = sDigits[(arrRandom[i] >> 4) & 0x0Fu];
		sHex[i * 2u + 1u] = sDigits[arrRandom[i] & 0x0Fu];
	}
	sHex[24] = '\0';
	return xrtFormat("<%llu.%s@%s>", (unsigned long long)time(NULL), sHex, sUseDomain);
}

static str xmailMimeRandomBoundary(const char* sPrefix)
{
	uint8 arrRandom[12];
	char sHex[25];
	size_t i;

	xrtRandomBytes(arrRandom, sizeof(arrRandom));
	for ( i = 0u; i < sizeof(arrRandom); i++ ) {
		static const char* sDigits = "0123456789abcdef";
		sHex[i * 2u] = sDigits[(arrRandom[i] >> 4) & 0x0Fu];
		sHex[i * 2u + 1u] = sDigits[arrRandom[i] & 0x0Fu];
	}
	sHex[24] = '\0';
	return xrtFormat("%s-%s", ((sPrefix != NULL) && (sPrefix[0] != '\0')) ? sPrefix : "xmail", sHex);
}

static str xmailMimeFormatAddress(const xmailaddr* pAddr)
{
	xmailmimebuf tBuf;
	str sName;

	if ( (pAddr == NULL) || (pAddr->sEmail == NULL) || (pAddr->sEmail[0] == '\0') || xmailMimeHasHeaderInjection(pAddr->sEmail) ) {
		return NULL;
	}
	if ( (pAddr->sName == NULL) || (pAddr->sName[0] == '\0') ) {
		return xrtCopyStr((str)pAddr->sEmail, 0);
	}
	if ( xmailMimeHasHeaderInjection(pAddr->sName) ) {
		return NULL;
	}
	if ( !xmailMimeIsValidUtf8Text(pAddr->sName) ) {
		return NULL;
	}

	memset(&tBuf, 0, sizeof(tBuf));
	if ( xmailMimeHasNonAscii(pAddr->sName) ) {
		sName = xmailMimeEncodeHeaderWord(pAddr->sName);
		if ( sName == NULL ) {
			return NULL;
		}
		if ( !xmailMimeBufAppend(&tBuf, (const char*)sName) ) {
			xrtFree(sName);
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		xrtFree(sName);
	} else if ( xmailMimeNeedsQuotedName(pAddr->sName) ) {
		if ( !xmailMimeAppendQuotedText(&tBuf, pAddr->sName) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	} else if ( !xmailMimeBufAppend(&tBuf, pAddr->sName) ) {
		xmailMimeBufFree(&tBuf);
		return NULL;
	}
	if ( !xmailMimeBufAppend(&tBuf, " <") || !xmailMimeBufAppend(&tBuf, pAddr->sEmail) || !xmailMimeBufAppend(&tBuf, ">") ) {
		xmailMimeBufFree(&tBuf);
		return NULL;
	}
	return (str)tBuf.sData;
}

static str xmailMimeFormatAddressList(const xmailaddr* arrAddr, size_t iCount)
{
	xmailmimebuf tBuf;
	size_t i;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( (arrAddr == NULL) || (iCount == 0u) ) {
		return xrtCopyStr((str)"", 0);
	}
	for ( i = 0u; i < iCount; i++ ) {
		str sItem = xmailMimeFormatAddress(&arrAddr[i]);
		if ( sItem == NULL ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		if ( (i > 0u) && !xmailMimeBufAppend(&tBuf, ", ") ) {
			xrtFree(sItem);
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		if ( !xmailMimeBufAppend(&tBuf, (const char*)sItem) ) {
			xrtFree(sItem);
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		xrtFree(sItem);
	}
	return (str)tBuf.sData;
}

static char* xmailMimeTrimCopy(const char* sText, size_t iLen)
{
	const char* sStart;
	const char* sEnd;

	if ( sText == NULL && iLen != 0u ) {
		return NULL;
	}
	sStart = sText;
	sEnd = sText + iLen;
	while ( sStart < sEnd && isspace((unsigned char)*sStart) ) {
		sStart++;
	}
	while ( sEnd > sStart && isspace((unsigned char)sEnd[-1]) ) {
		sEnd--;
	}
	return (char*)xmailMimeCopyN(sStart, (size_t)(sEnd - sStart));
}

static str xmailMimeUnquoteText(const char* sText)
{
	xmailmimebuf tBuf;
	const char* p;
	size_t iLen;

	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	iLen = strlen(sText);
	if ( iLen < 2u || sText[0] != '"' || sText[iLen - 1u] != '"' ) {
		return xrtCopyStr((str)sText, 0);
	}
	memset(&tBuf, 0, sizeof(tBuf));
	for ( p = sText + 1; p < sText + iLen - 1u; p++ ) {
		if ( *p == '\\' && p + 1 < sText + iLen - 1u ) {
			p++;
		}
		if ( !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	}
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static bool xmailMimeParseAddress(const char* sText, xmailaddr* pOut)
{
	char* sWork = NULL;
	char* sName = NULL;
	char* sEmail = NULL;
	const char* pLt;
	const char* pGt;

	if ( pOut == NULL ) {
		return FALSE;
	}
	memset(pOut, 0, sizeof(*pOut));
	sWork = xmailMimeTrimCopy(xmailMimeStrOrEmpty(sText), strlen(xmailMimeStrOrEmpty(sText)));
	if ( sWork == NULL || sWork[0] == '\0' ) {
		xrtFree(sWork);
		return FALSE;
	}
	pLt = strchr(sWork, '<');
	pGt = pLt ? strchr(pLt + 1, '>') : NULL;
	if ( pLt != NULL && pGt != NULL && pGt > pLt + 1 ) {
		char* sRawName = xmailMimeTrimCopy(sWork, (size_t)(pLt - sWork));
		char* sRawEmail = xmailMimeTrimCopy(pLt + 1, (size_t)(pGt - pLt - 1));
		str sUnquoted = NULL;
		str sDecoded = NULL;
		if ( sRawName == NULL || sRawEmail == NULL || sRawEmail[0] == '\0' ) {
			xrtFree(sRawName);
			xrtFree(sRawEmail);
			xrtFree(sWork);
			return FALSE;
		}
		sUnquoted = xmailMimeUnquoteText(sRawName);
		if ( sUnquoted == NULL ) {
			xrtFree(sRawName);
			xrtFree(sRawEmail);
			xrtFree(sWork);
			return FALSE;
		}
		sDecoded = xmailMimeDecodeHeaderWord((const char*)sUnquoted);
		sName = (char*)xrtCopyStr(sDecoded ? sDecoded : sUnquoted, 0);
		sEmail = sRawEmail;
		xrtFree(sRawName);
		xrtFree(sUnquoted);
		xrtFree(sDecoded);
	} else {
		sEmail = xmailMimeTrimCopy(sWork, strlen(sWork));
		sName = (char*)xmailMimeCopyN("", 0u);
	}
	xrtFree(sWork);
	if ( sEmail == NULL || sEmail[0] == '\0' || sName == NULL || xmailMimeHasHeaderInjection(sEmail) || xmailMimeHasHeaderInjection(sName) ) {
		xrtFree(sEmail);
		xrtFree(sName);
		return FALSE;
	}
	pOut->sEmail = sEmail;
	pOut->sName = sName;
	return TRUE;
}

static void xmailMimeAddressFree(xmailaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return;
	}
	if ( pAddr->sEmail != NULL ) {
		xrtFree((ptr)pAddr->sEmail);
	}
	if ( pAddr->sName != NULL ) {
		xrtFree((ptr)pAddr->sName);
	}
	memset(pAddr, 0, sizeof(*pAddr));
}

static void xmailMimeAddressListFree(xmailaddr* arrAddr, size_t iCount)
{
	size_t i;

	if ( arrAddr == NULL ) {
		return;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xmailMimeAddressFree(&arrAddr[i]);
	}
	xrtFree(arrAddr);
}

static bool xmailMimeParseAddressList(const char* sText, xmailaddr** ppAddr, size_t* pCount)
{
	const char* p;
	const char* sItemStart;
	bool bQuoted = FALSE;
	bool bEsc = FALSE;
	int iAngleDepth = 0;
	xmailaddr* arrOut = NULL;
	size_t iCount = 0u;
	size_t iCap = 0u;

	if ( ppAddr == NULL || pCount == NULL ) {
		return FALSE;
	}
	*ppAddr = NULL;
	*pCount = 0u;
	if ( sText == NULL || sText[0] == '\0' ) {
		return TRUE;
	}
	sItemStart = sText;
	for ( p = sText; ; p++ ) {
		bool bEnd = (*p == '\0');
		bool bSplit = FALSE;
		if ( !bEnd ) {
			if ( bQuoted ) {
				if ( bEsc ) bEsc = FALSE;
				else if ( *p == '\\' ) bEsc = TRUE;
				else if ( *p == '"' ) bQuoted = FALSE;
			} else if ( *p == '"' ) {
				bQuoted = TRUE;
			} else if ( *p == '<' ) {
				iAngleDepth++;
			} else if ( *p == '>' ) {
				if ( iAngleDepth > 0 ) iAngleDepth--;
			} else if ( *p == ',' && iAngleDepth == 0 ) {
				bSplit = TRUE;
			}
		}
		if ( bEnd || bSplit ) {
			char* sItem = xmailMimeTrimCopy(sItemStart, (size_t)(p - sItemStart));
			if ( sItem == NULL ) {
				goto fail;
			}
			if ( sItem[0] != '\0' ) {
				xmailaddr tAddr;
				xmailaddr* arrNew;
				if ( !xmailMimeParseAddress(sItem, &tAddr) ) {
					xrtFree(sItem);
					goto fail;
				}
				if ( iCount + 1u > iCap ) {
					size_t iNewCap = iCap ? iCap * 2u : 4u;
					arrNew = arrOut ? (xmailaddr*)xrtRealloc(arrOut, sizeof(xmailaddr) * iNewCap) : (xmailaddr*)xrtMalloc(sizeof(xmailaddr) * iNewCap);
					if ( arrNew == NULL ) {
						xmailMimeAddressFree(&tAddr);
						xrtFree(sItem);
						goto fail;
					}
					arrOut = arrNew;
					memset(arrOut + iCap, 0, sizeof(xmailaddr) * (iNewCap - iCap));
					iCap = iNewCap;
				}
				arrOut[iCount++] = tAddr;
			}
			xrtFree(sItem);
			if ( bEnd ) {
				break;
			}
			sItemStart = p + 1;
		}
	}
	*ppAddr = arrOut;
	*pCount = iCount;
	return TRUE;

fail:
	xmailMimeAddressListFree(arrOut, iCount);
	return FALSE;
}

static bool xmailMimeAppendPayloadWithCrlf(xmailmimebuf* pBuf, const char* sPayload)
{
	size_t iLen;

	if ( !xmailMimeBufAppend(pBuf, xmailMimeStrOrEmpty(sPayload)) ) {
		return FALSE;
	}
	iLen = strlen(xmailMimeStrOrEmpty(sPayload));
	if ( (iLen >= 2u) && (sPayload[iLen - 2u] == '\r') && (sPayload[iLen - 1u] == '\n') ) {
		return TRUE;
	}
	return xmailMimeBufAppend(pBuf, "\r\n");
}

static bool xmailMimeAppendTextPart(xmailmimebuf* pBuf, const char* sContentType, const char* sBody)
{
	str sPayload;
	bool bOK;

	if ( !xmailMimeIsValidUtf8Text(xmailMimeStrOrEmpty(sBody)) ) {
		return FALSE;
	}
	sPayload = xmailMimeQuotedPrintableEncode(xmailMimeStrOrEmpty(sBody));
	if ( sPayload == NULL ) {
		return FALSE;
	}
	bOK = xmailMimeBufAppend(pBuf, "Content-Type: ")
		&& xmailMimeBufAppend(pBuf, ((sContentType != NULL) && (sContentType[0] != '\0')) ? sContentType : "text/plain")
		&& xmailMimeBufAppend(pBuf, "; charset=UTF-8\r\n")
		&& xmailMimeBufAppend(pBuf, "Content-Transfer-Encoding: quoted-printable\r\n\r\n")
		&& xmailMimeAppendPayloadWithCrlf(pBuf, (const char*)sPayload);
	xrtFree(sPayload);
	return bOK;
}

static bool xmailMimeAppendAttachmentPart(xmailmimebuf* pBuf, const xmailmimeattachment* pAttachment)
{
	str sPayload;
	const char* sType;
	const char* sFileName;
	const char* sContentId;
	const char* sDisposition;

	if ( (pBuf == NULL) || (pAttachment == NULL) || (pAttachment->pData == NULL) ) {
		return FALSE;
	}
	sFileName = xmailMimeStrOrEmpty(pAttachment->sFileName);
	if ( xmailMimeHasHeaderInjection(sFileName) ) {
		return FALSE;
	}
	if ( !xmailMimeIsValidUtf8Text(sFileName) ) {
		return FALSE;
	}
	sContentId = xmailMimeStrOrEmpty(pAttachment->sContentId);
	if ( xmailMimeHasHeaderInjection(sContentId) ) {
		return FALSE;
	}
	sType = ((pAttachment->sContentType != NULL) && (pAttachment->sContentType[0] != '\0')) ? pAttachment->sContentType : "application/octet-stream";
	if ( xmailMimeHasHeaderInjection(sType) ) {
		return FALSE;
	}
	sDisposition = pAttachment->bInline ? "inline" : "attachment";
	sPayload = xmailMimeBase64Wrap(pAttachment->pData, pAttachment->iDataLen, XMAIL_MIME_BASE64_LINE_MAX);
	if ( sPayload == NULL ) {
		return FALSE;
	}
	if ( !xmailMimeBufAppend(pBuf, "Content-Type: ")
		|| !xmailMimeBufAppend(pBuf, sType)
		|| !xmailMimeBufAppend(pBuf, "; name=")
		|| !xmailMimeAppendQuotedText(pBuf, sFileName)
		|| !xmailMimeBufAppend(pBuf, "\r\nContent-Transfer-Encoding: base64\r\n")
		|| !xmailMimeBufAppend(pBuf, "Content-Disposition: ")
		|| !xmailMimeBufAppend(pBuf, sDisposition)
		|| !xmailMimeBufAppend(pBuf, "; filename=")
		|| !xmailMimeAppendQuotedText(pBuf, sFileName)
		|| !xmailMimeBufAppend(pBuf, "; filename*=UTF-8''")
		|| !xmailMimeAppendPercentEncoded(pBuf, sFileName) ) {
		xrtFree(sPayload);
		return FALSE;
	}
	if ( sContentId[0] != '\0' ) {
		if ( !xmailMimeBufAppend(pBuf, "\r\nContent-ID: <") || !xmailMimeBufAppend(pBuf, sContentId) || !xmailMimeBufAppend(pBuf, ">") ) {
			xrtFree(sPayload);
			return FALSE;
		}
	}
	if ( !xmailMimeBufAppend(pBuf, "\r\n\r\n") || !xmailMimeAppendPayloadWithCrlf(pBuf, (const char*)sPayload) ) {
		xrtFree(sPayload);
		return FALSE;
	}
	xrtFree(sPayload);
	return TRUE;
}

static bool xmailMimeAppendHtmlRelatedPart(xmailmimebuf* pBuf, const char* sHtmlBody, const xmailmimeattachment* arrAttachments, size_t iAttachmentCount)
{
	str sBoundary;
	bool bOK;
	size_t i;

	sBoundary = xmailMimeRandomBoundary("xmail-related");
	if ( sBoundary == NULL ) {
		return FALSE;
	}
	bOK = xmailMimeBufAppend(pBuf, "Content-Type: multipart/related; boundary=\"")
		&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
		&& xmailMimeBufAppend(pBuf, "\"\r\n\r\n--")
		&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
		&& xmailMimeBufAppend(pBuf, "\r\n")
		&& xmailMimeAppendTextPart(pBuf, "text/html", sHtmlBody);
	for ( i = 0u; bOK && (i < iAttachmentCount); i++ ) {
		if ( arrAttachments[i].bInline ) {
			bOK = xmailMimeBufAppend(pBuf, "--")
				&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
				&& xmailMimeBufAppend(pBuf, "\r\n")
				&& xmailMimeAppendAttachmentPart(pBuf, &arrAttachments[i]);
		}
	}
	if ( bOK ) {
		bOK = xmailMimeBufAppend(pBuf, "--")
			&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
			&& xmailMimeBufAppend(pBuf, "--\r\n");
	}
	xrtFree(sBoundary);
	return bOK;
}

static bool xmailMimeAppendAlternativePartEx(xmailmimebuf* pBuf, const char* sTextBody, const char* sHtmlBody, const xmailmimeattachment* arrAttachments, size_t iAttachmentCount)
{
	str sBoundary;
	bool bOK;
	bool bHasInline;
	size_t i;

	bHasInline = FALSE;
	for ( i = 0u; (arrAttachments != NULL) && (i < iAttachmentCount); i++ ) {
		if ( arrAttachments[i].bInline ) {
			bHasInline = TRUE;
			break;
		}
	}

	sBoundary = xmailMimeRandomBoundary("xmail-alt");
	if ( sBoundary == NULL ) {
		return FALSE;
	}
	bOK = xmailMimeBufAppend(pBuf, "Content-Type: multipart/alternative; boundary=\"")
		&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
		&& xmailMimeBufAppend(pBuf, "\"\r\n\r\n")
		&& xmailMimeBufAppend(pBuf, "--")
		&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
		&& xmailMimeBufAppend(pBuf, "\r\n")
		&& xmailMimeAppendTextPart(pBuf, "text/plain", sTextBody)
		&& xmailMimeBufAppend(pBuf, "--")
		&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
		&& xmailMimeBufAppend(pBuf, "\r\n");
	if ( bOK ) {
		if ( bHasInline ) {
			bOK = xmailMimeAppendHtmlRelatedPart(pBuf, sHtmlBody, arrAttachments, iAttachmentCount);
		} else {
			bOK = xmailMimeAppendTextPart(pBuf, "text/html", sHtmlBody);
		}
	}
	if ( bOK ) {
		bOK = xmailMimeBufAppend(pBuf, "--")
			&& xmailMimeBufAppend(pBuf, (const char*)sBoundary)
			&& xmailMimeBufAppend(pBuf, "--\r\n");
	}
	xrtFree(sBoundary);
	return bOK;
}

static bool xmailMimeAppendMessageBody(xmailmimebuf* pBuf, const xmailmessage* pMsg)
{
	bool bHasText;
	bool bHasHtml;
	bool bHasInline;
	bool bHasRegularAttachments;
	size_t i;

	if ( (pBuf == NULL) || (pMsg == NULL) ) {
		return FALSE;
	}
	bHasText = (pMsg->sTextBody != NULL) && (pMsg->sTextBody[0] != '\0');
	bHasHtml = (pMsg->sHtmlBody != NULL) && (pMsg->sHtmlBody[0] != '\0');
	bHasInline = FALSE;
	bHasRegularAttachments = FALSE;
	for ( i = 0u; (pMsg->arrAttachments != NULL) && (i < pMsg->iAttachmentCount); i++ ) {
		if ( pMsg->arrAttachments[i].bInline ) {
			bHasInline = TRUE;
		} else {
			bHasRegularAttachments = TRUE;
		}
	}

	if ( bHasRegularAttachments ) {
		str sBoundary = xmailMimeRandomBoundary("xmail-mixed");
		if ( sBoundary == NULL ) {
			return FALSE;
		}
		if ( !xmailMimeBufAppend(pBuf, "Content-Type: multipart/mixed; boundary=\"")
			|| !xmailMimeBufAppend(pBuf, (const char*)sBoundary)
			|| !xmailMimeBufAppend(pBuf, "\"\r\n\r\n--")
			|| !xmailMimeBufAppend(pBuf, (const char*)sBoundary)
			|| !xmailMimeBufAppend(pBuf, "\r\n") ) {
			xrtFree(sBoundary);
			return FALSE;
		}
		if ( bHasText && bHasHtml ) {
			if ( !xmailMimeAppendAlternativePartEx(pBuf, pMsg->sTextBody, pMsg->sHtmlBody, pMsg->arrAttachments, pMsg->iAttachmentCount) ) {
				xrtFree(sBoundary);
				return FALSE;
			}
		} else if ( bHasHtml ) {
			if ( bHasInline ) {
				if ( !xmailMimeAppendHtmlRelatedPart(pBuf, pMsg->sHtmlBody, pMsg->arrAttachments, pMsg->iAttachmentCount) ) {
					xrtFree(sBoundary);
					return FALSE;
				}
			} else if ( !xmailMimeAppendTextPart(pBuf, "text/html", pMsg->sHtmlBody) ) {
				xrtFree(sBoundary);
				return FALSE;
			}
		} else {
			if ( !xmailMimeAppendTextPart(pBuf, "text/plain", xmailMimeStrOrEmpty(pMsg->sTextBody)) ) {
				xrtFree(sBoundary);
				return FALSE;
			}
		}
		for ( i = 0u; i < pMsg->iAttachmentCount; i++ ) {
			if ( pMsg->arrAttachments[i].bInline ) {
				continue;
			}
			if ( !xmailMimeBufAppend(pBuf, "--")
				|| !xmailMimeBufAppend(pBuf, (const char*)sBoundary)
				|| !xmailMimeBufAppend(pBuf, "\r\n")
				|| !xmailMimeAppendAttachmentPart(pBuf, &pMsg->arrAttachments[i]) ) {
				xrtFree(sBoundary);
				return FALSE;
			}
		}
		if ( !xmailMimeBufAppend(pBuf, "--") || !xmailMimeBufAppend(pBuf, (const char*)sBoundary) || !xmailMimeBufAppend(pBuf, "--\r\n") ) {
			xrtFree(sBoundary);
			return FALSE;
		}
		xrtFree(sBoundary);
		return TRUE;
	}

	if ( bHasText && bHasHtml ) {
		return xmailMimeAppendAlternativePartEx(pBuf, pMsg->sTextBody, pMsg->sHtmlBody, pMsg->arrAttachments, pMsg->iAttachmentCount);
	}
	if ( bHasHtml ) {
		if ( bHasInline ) {
			return xmailMimeAppendHtmlRelatedPart(pBuf, pMsg->sHtmlBody, pMsg->arrAttachments, pMsg->iAttachmentCount);
		}
		return xmailMimeAppendTextPart(pBuf, "text/html", pMsg->sHtmlBody);
	}
	return xmailMimeAppendTextPart(pBuf, "text/plain", xmailMimeStrOrEmpty(pMsg->sTextBody));
}

static str xmailMimeBuildMessage(const xmailmessage* pMsg)
{
	xmailmimebuf tBuf;
	str sDate = NULL;
	str sMessageId = NULL;
	str sFrom = NULL;
	str sReplyTo = NULL;
	str sTo = NULL;
	str sCc = NULL;
	str sSubject = NULL;
	size_t i;

	if ( pMsg == NULL ) {
		return NULL;
	}
	memset(&tBuf, 0, sizeof(tBuf));
	sDate = xmailMimeRfc2822DateNow();
	sMessageId = xmailMimeBuildMessageId(pMsg->sMessageIdDomain);
	sFrom = xmailMimeFormatAddress(&pMsg->tFrom);
	if ( (pMsg->tReplyTo.sEmail != NULL) && (pMsg->tReplyTo.sEmail[0] != '\0') ) {
		sReplyTo = xmailMimeFormatAddress(&pMsg->tReplyTo);
		if ( sReplyTo == NULL ) {
			goto fail;
		}
	}
	sTo = xmailMimeFormatAddressList(pMsg->arrTo, pMsg->iToCount);
	sCc = xmailMimeFormatAddressList(pMsg->arrCc, pMsg->iCcCount);
	sSubject = xmailMimeEncodeHeaderWord(xmailMimeStrOrEmpty(pMsg->sSubject));
	if ( (sDate == NULL) || (sMessageId == NULL) || (sFrom == NULL) || (sTo == NULL) || (sSubject == NULL) ) {
		goto fail;
	}
	if ( !xmailMimeBufAppend(&tBuf, "Date: ") || !xmailMimeBufAppend(&tBuf, (const char*)sDate) || !xmailMimeBufAppend(&tBuf, "\r\n")
		|| !xmailMimeBufAppend(&tBuf, "Message-ID: ") || !xmailMimeBufAppend(&tBuf, (const char*)sMessageId) || !xmailMimeBufAppend(&tBuf, "\r\n")
		|| !xmailMimeBufAppend(&tBuf, "From: ") || !xmailMimeBufAppend(&tBuf, (const char*)sFrom) || !xmailMimeBufAppend(&tBuf, "\r\n")
		|| !xmailMimeBufAppend(&tBuf, "To: ") || !xmailMimeBufAppend(&tBuf, (const char*)sTo) || !xmailMimeBufAppend(&tBuf, "\r\n") ) {
		goto fail;
	}
	if ( (sCc != NULL) && (sCc[0] != '\0') && (!xmailMimeBufAppend(&tBuf, "Cc: ") || !xmailMimeBufAppend(&tBuf, (const char*)sCc) || !xmailMimeBufAppend(&tBuf, "\r\n")) ) {
		goto fail;
	}
	if ( (sReplyTo != NULL) && (!xmailMimeBufAppend(&tBuf, "Reply-To: ") || !xmailMimeBufAppend(&tBuf, (const char*)sReplyTo) || !xmailMimeBufAppend(&tBuf, "\r\n")) ) {
		goto fail;
	}
	if ( !xmailMimeBufAppend(&tBuf, "Subject: ") || !xmailMimeBufAppend(&tBuf, (const char*)sSubject) || !xmailMimeBufAppend(&tBuf, "\r\n")
		|| !xmailMimeBufAppend(&tBuf, "MIME-Version: 1.0\r\n") ) {
		goto fail;
	}
	for ( i = 0u; (pMsg->arrHeaders != NULL) && (i < pMsg->iHeaderCount); i++ ) {
		const char* sHeaderName = xmailMimeStrOrEmpty(pMsg->arrHeaders[i].sName);
		const char* sHeaderValue = xmailMimeStrOrEmpty(pMsg->arrHeaders[i].sValue);
		char sHeaderLine[1024];
		size_t iHeaderLineLen = 0u;
		if ( !xmailMimeIsHeaderNameSafe(sHeaderName) || xmailMimeHasHeaderInjection(sHeaderValue) ) {
			goto fail;
		}
		if ( !xmailMimeIsValidUtf8Text(sHeaderValue) ) {
			goto fail;
		}
		if ( !xrtHttpHeaderBuildLineTo(sHeaderName, strlen(sHeaderName), sHeaderValue, strlen(sHeaderValue), sHeaderLine, sizeof(sHeaderLine), &iHeaderLineLen) ) {
			goto fail;
		}
		if ( !xmailMimeBufAppendN(&tBuf, sHeaderLine, iHeaderLineLen) ) {
			goto fail;
		}
	}
	if ( !xmailMimeAppendMessageBody(&tBuf, pMsg) ) {
		goto fail;
	}
	xrtFree(sDate);
	xrtFree(sMessageId);
	xrtFree(sFrom);
	if ( sReplyTo ) xrtFree(sReplyTo);
	xrtFree(sTo);
	xrtFree(sCc);
	xrtFree(sSubject);
	return (str)tBuf.sData;

fail:
	xmailMimeBufFree(&tBuf);
	if ( sDate ) xrtFree(sDate);
	if ( sMessageId ) xrtFree(sMessageId);
	if ( sFrom ) xrtFree(sFrom);
	if ( sReplyTo ) xrtFree(sReplyTo);
	if ( sTo ) xrtFree(sTo);
	if ( sCc ) xrtFree(sCc);
	if ( sSubject ) xrtFree(sSubject);
	return NULL;
}

static bool xmailMimeQpIsPrintable(unsigned char ch)
{
	return ((ch >= 33u) && (ch <= 60u)) || ((ch >= 62u) && (ch <= 126u));
}

static bool xmailMimeQpAppendEncoded(xmailmimebuf* pBuf, unsigned char ch)
{
	static const char* sHex = "0123456789ABCDEF";
	char sEnc[3];

	sEnc[0] = '=';
	sEnc[1] = sHex[(ch >> 4) & 0x0Fu];
	sEnc[2] = sHex[ch & 0x0Fu];
	return xmailMimeBufAppendN(pBuf, sEnc, 3u);
}

static str xmailMimeQuotedPrintableEncode(const char* sText)
{
	xmailmimebuf tBuf;
	str sNorm;
	const unsigned char* p;
	size_t iLineLen;

	memset(&tBuf, 0, sizeof(tBuf));
	sNorm = xmailMimeNormalizeCrlf(sText);
	if ( sNorm == NULL ) {
		return NULL;
	}

	iLineLen = 0u;
	for ( p = (const unsigned char*)sNorm; *p != '\0'; p++ ) {
		size_t iNeed;
		bool bEncode;
		unsigned char ch = *p;

		if ( (ch == '\r') && (p[1] == '\n') ) {
			if ( !xmailMimeBufAppend(&tBuf, "\r\n") ) {
				xrtFree(sNorm);
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			p++;
			iLineLen = 0u;
			continue;
		}

		bEncode = !xmailMimeQpIsPrintable(ch);
		if ( ((ch == ' ') || (ch == '\t')) && ((p[1] == '\r') || (p[1] == '\n') || (p[1] == '\0')) ) {
			bEncode = TRUE;
		}
		iNeed = bEncode ? 3u : 1u;
		if ( iLineLen + iNeed > XMAIL_MIME_QP_LINE_MAX - 1u ) {
			if ( !xmailMimeBufAppend(&tBuf, "=\r\n") ) {
				xrtFree(sNorm);
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			iLineLen = 0u;
		}
		if ( bEncode ) {
			if ( !xmailMimeQpAppendEncoded(&tBuf, ch) ) {
				xrtFree(sNorm);
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
		} else if ( !xmailMimeBufAppendN(&tBuf, (const char*)p, 1u) ) {
			xrtFree(sNorm);
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		iLineLen += iNeed;
	}
	xrtFree(sNorm);
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static str xmailMimeQuotedPrintableDecode(const char* sText)
{
	xmailmimebuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( sText == NULL ) {
		return xrtCopyStr((str)"", 0);
	}

	for ( p = sText; *p != '\0'; p++ ) {
		if ( *p == '=' ) {
			if ( (p[1] == '\r') && (p[2] == '\n') ) {
				p += 2;
				continue;
			}
			if ( p[1] == '\n' ) {
				p += 1;
				continue;
			}
			if ( (isxdigit((unsigned char)p[1])) && (isxdigit((unsigned char)p[2])) ) {
				int iHi = xmailMimeHexValue(p[1]);
				int iLo = xmailMimeHexValue(p[2]);
				char ch = (char)((iHi << 4) | iLo);
				if ( !xmailMimeBufAppendN(&tBuf, &ch, 1u) ) {
					xmailMimeBufFree(&tBuf);
					return NULL;
				}
				p += 2;
				continue;
			}
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
		if ( !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	}

	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static bool xmailMimeAsciiEqI(const char* a, const char* b)
{
	size_t i;

	if ( a == NULL || b == NULL ) {
		return FALSE;
	}
	for ( i = 0u; a[i] != '\0' && b[i] != '\0'; i++ ) {
		if ( tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]) ) {
			return FALSE;
		}
	}
	return a[i] == '\0' && b[i] == '\0';
}

static bool xmailMimeStartsWithI(const char* sText, const char* sPrefix)
{
	size_t i;

	if ( sText == NULL || sPrefix == NULL ) {
		return FALSE;
	}
	for ( i = 0u; sPrefix[i] != '\0'; i++ ) {
		if ( sText[i] == '\0' || tolower((unsigned char)sText[i]) != tolower((unsigned char)sPrefix[i]) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xmailMimePartHeaderIsManaged(const char* sName)
{
	return xmailMimeAsciiEqI(sName, "Content-Type")
		|| xmailMimeAsciiEqI(sName, "Content-Transfer-Encoding")
		|| xmailMimeAsciiEqI(sName, "Content-Disposition")
		|| xmailMimeAsciiEqI(sName, "Content-ID");
}

static bool xmailMimeBuildPartAppendHeader(xmailmimebuf* pBuf, const char* sName, const char* sValue)
{
	char sHeaderLine[1024];
	size_t iHeaderLineLen = 0u;

	if ( !xmailMimeIsHeaderNameSafe(sName) || xmailMimeHasHeaderInjection(sValue) || !xmailMimeIsValidUtf8Text(sValue) ) {
		return FALSE;
	}
	if ( !xrtHttpHeaderBuildLineTo(sName, strlen(sName), xmailMimeStrOrEmpty(sValue), strlen(xmailMimeStrOrEmpty(sValue)), sHeaderLine, sizeof(sHeaderLine), &iHeaderLineLen) ) {
		return FALSE;
	}
	return xmailMimeBufAppendN(pBuf, sHeaderLine, iHeaderLineLen);
}

static bool xmailMimeBuildPartAppendCustomHeaders(xmailmimebuf* pBuf, const xmailmimepart* pPart)
{
	size_t i;

	for ( i = 0u; (pPart != NULL) && (i < pPart->iHeaderCount); i++ ) {
		const char* sName = xmailMimeStrOrEmpty(pPart->arrHeaders[i].sName);
		if ( xmailMimePartHeaderIsManaged(sName) ) {
			continue;
		}
		if ( !xmailMimeBuildPartAppendHeader(pBuf, sName, xmailMimeStrOrEmpty(pPart->arrHeaders[i].sValue)) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xmailMimeBuildPartAppendContentType(xmailmimebuf* pBuf, const xmailmimepart* pPart, const char* sBoundary)
{
	const char* sMedia;
	const char* sSubType;
	bool bMultipart;

	if ( pBuf == NULL || pPart == NULL ) {
		return FALSE;
	}
	bMultipart = (pPart->iChildCount > 0u) || xmailMimeAsciiEqI(pPart->sMediaType, "multipart");
	if ( bMultipart ) {
		sSubType = pPart->sSubType[0] ? pPart->sSubType : "mixed";
		return xmailMimeBufAppend(pBuf, "Content-Type: multipart/")
			&& xmailMimeBufAppend(pBuf, sSubType)
			&& xmailMimeBufAppend(pBuf, "; boundary=\"")
			&& xmailMimeBufAppend(pBuf, sBoundary)
			&& xmailMimeBufAppend(pBuf, "\"\r\n");
	}
	if ( pPart->sContentType[0] != '\0' ) {
		return xmailMimeBuildPartAppendHeader(pBuf, "Content-Type", pPart->sContentType);
	}
	sMedia = pPart->sMediaType[0] ? pPart->sMediaType : "text";
	sSubType = pPart->sSubType[0] ? pPart->sSubType : "plain";
	return xmailMimeBufAppend(pBuf, "Content-Type: ")
		&& xmailMimeBufAppend(pBuf, sMedia)
		&& xmailMimeBufAppend(pBuf, "/")
		&& xmailMimeBufAppend(pBuf, sSubType)
		&& xmailMimeBufAppend(pBuf, "\r\n");
}

static bool xmailMimeBuildPartAppendDisposition(xmailmimebuf* pBuf, const xmailmimepart* pPart)
{
	const char* sDisposition;
	const char* sFileName;

	if ( pBuf == NULL || pPart == NULL ) {
		return FALSE;
	}
	sDisposition = pPart->sDisposition[0] ? pPart->sDisposition : (pPart->bAttachment ? "attachment" : (pPart->bInline ? "inline" : ""));
	sFileName = pPart->sFileName;
	if ( sDisposition[0] == '\0' && sFileName[0] == '\0' ) {
		return TRUE;
	}
	if ( xmailMimeHasHeaderInjection(sDisposition) || xmailMimeHasHeaderInjection(sFileName) || !xmailMimeIsValidUtf8Text(sFileName) ) {
		return FALSE;
	}
	if ( !xmailMimeBufAppend(pBuf, "Content-Disposition: ")
		|| !xmailMimeBufAppend(pBuf, sDisposition[0] ? sDisposition : "attachment") ) {
		return FALSE;
	}
	if ( sFileName[0] != '\0' ) {
		if ( !xmailMimeBufAppend(pBuf, "; filename=")
			|| !xmailMimeAppendQuotedText(pBuf, sFileName)
			|| !xmailMimeBufAppend(pBuf, "; filename*=UTF-8''")
			|| !xmailMimeAppendPercentEncoded(pBuf, sFileName) ) {
			return FALSE;
		}
	}
	return xmailMimeBufAppend(pBuf, "\r\n");
}

static bool xmailMimeBuildPartAppendContentId(xmailmimebuf* pBuf, const xmailmimepart* pPart)
{
	if ( pBuf == NULL || pPart == NULL || pPart->sContentId[0] == '\0' ) {
		return TRUE;
	}
	if ( xmailMimeHasHeaderInjection(pPart->sContentId) ) {
		return FALSE;
	}
	return xmailMimeBufAppend(pBuf, "Content-ID: <")
		&& xmailMimeBufAppend(pBuf, pPart->sContentId)
		&& xmailMimeBufAppend(pBuf, ">\r\n");
}

static bool xmailMimeBuildPartAppendLeafBody(xmailmimebuf* pBuf, const xmailmimepart* pPart)
{
	const char* sBody;
	str sPayload = NULL;
	bool bOK;

	if ( pBuf == NULL || pPart == NULL ) {
		return FALSE;
	}
	sBody = pPart->sBody ? (const char*)pPart->sBody : (pPart->sDecodedBody ? (const char*)pPart->sDecodedBody : "");
	if ( xmailMimeAsciiEqI(pPart->sTransferEncoding, "quoted-printable") ) {
		if ( !xmailMimeIsValidUtf8Text(sBody) ) {
			return FALSE;
		}
		sPayload = xmailMimeQuotedPrintableEncode(sBody);
		if ( sPayload == NULL ) {
			return FALSE;
		}
		bOK = xmailMimeAppendPayloadWithCrlf(pBuf, (const char*)sPayload);
		xrtFree(sPayload);
		return bOK;
	}
	if ( xmailMimeAsciiEqI(pPart->sTransferEncoding, "base64") ) {
		sPayload = xmailMimeBase64Wrap(sBody, strlen(sBody), XMAIL_MIME_BASE64_LINE_MAX);
		if ( sPayload == NULL ) {
			return FALSE;
		}
		bOK = xmailMimeAppendPayloadWithCrlf(pBuf, (const char*)sPayload);
		xrtFree(sPayload);
		return bOK;
	}
	sPayload = xmailMimeNormalizeCrlf(sBody);
	if ( sPayload == NULL ) {
		return FALSE;
	}
	bOK = xmailMimeAppendPayloadWithCrlf(pBuf, (const char*)sPayload);
	xrtFree(sPayload);
	return bOK;
}

static bool xmailMimeBuildPartAppendEntity(xmailmimebuf* pBuf, const xmailmimepart* pPart, int iDepth)
{
	str sBoundary = NULL;
	const char* sBoundaryUse;
	bool bMultipart;
	size_t i;

	if ( pBuf == NULL || pPart == NULL || iDepth > 32 ) {
		return FALSE;
	}
	bMultipart = (pPart->iChildCount > 0u) || xmailMimeAsciiEqI(pPart->sMediaType, "multipart");
	if ( bMultipart ) {
		sBoundaryUse = pPart->sBoundary[0] ? pPart->sBoundary : NULL;
		if ( sBoundaryUse == NULL ) {
			sBoundary = xmailMimeRandomBoundary("xmail-part");
			if ( sBoundary == NULL ) {
				return FALSE;
			}
			sBoundaryUse = (const char*)sBoundary;
		}
	} else {
		sBoundaryUse = NULL;
	}
	if ( !xmailMimeBuildPartAppendContentType(pBuf, pPart, sBoundaryUse)
		|| !xmailMimeBuildPartAppendCustomHeaders(pBuf, pPart)
		|| (!bMultipart && pPart->sTransferEncoding[0] != '\0' && !xmailMimeBuildPartAppendHeader(pBuf, "Content-Transfer-Encoding", pPart->sTransferEncoding))
		|| !xmailMimeBuildPartAppendDisposition(pBuf, pPart)
		|| !xmailMimeBuildPartAppendContentId(pBuf, pPart)
		|| !xmailMimeBufAppend(pBuf, "\r\n") ) {
		if ( sBoundary ) xrtFree(sBoundary);
		return FALSE;
	}
	if ( bMultipart ) {
		for ( i = 0u; i < pPart->iChildCount; i++ ) {
			if ( !xmailMimeBufAppend(pBuf, "--")
				|| !xmailMimeBufAppend(pBuf, sBoundaryUse)
				|| !xmailMimeBufAppend(pBuf, "\r\n")
				|| !xmailMimeBuildPartAppendEntity(pBuf, &pPart->arrChildren[i], iDepth + 1) ) {
				if ( sBoundary ) xrtFree(sBoundary);
				return FALSE;
			}
		}
		if ( !xmailMimeBufAppend(pBuf, "--")
			|| !xmailMimeBufAppend(pBuf, sBoundaryUse)
			|| !xmailMimeBufAppend(pBuf, "--\r\n") ) {
			if ( sBoundary ) xrtFree(sBoundary);
			return FALSE;
		}
		if ( sBoundary ) xrtFree(sBoundary);
		return TRUE;
	}
	if ( sBoundary ) xrtFree(sBoundary);
	return xmailMimeBuildPartAppendLeafBody(pBuf, pPart);
}

static str xmailMimeBuildPart(const xmailmimepart* pPart)
{
	xmailmimebuf tBuf;

	if ( pPart == NULL ) {
		return NULL;
	}
	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xmailMimeBuildPartAppendEntity(&tBuf, pPart, 0) ) {
		xmailMimeBufFree(&tBuf);
		return NULL;
	}
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static void xmailMimeParsedMessageFree(xmailparsedmessage* pMsg)
{
	size_t i;

	if ( pMsg == NULL ) {
		return;
	}
	for ( i = 0u; i < pMsg->iHeaderCount; i++ ) {
		if ( pMsg->arrHeaders[i].sName != NULL ) {
			xrtFree((ptr)pMsg->arrHeaders[i].sName);
		}
		if ( pMsg->arrHeaders[i].sValue != NULL ) {
			xrtFree((ptr)pMsg->arrHeaders[i].sValue);
		}
	}
	if ( pMsg->arrHeaders != NULL ) {
		xrtFree(pMsg->arrHeaders);
	}
	if ( pMsg->sHeaderBlock != NULL ) {
		xrtFree(pMsg->sHeaderBlock);
	}
	if ( pMsg->sBody != NULL ) {
		xrtFree(pMsg->sBody);
	}
	if ( pMsg->pRootPart != NULL ) {
		xmailMimePartFree(pMsg->pRootPart);
		xrtFree(pMsg->pRootPart);
	}
	memset(pMsg, 0, sizeof(*pMsg));
}

static const char* xmailMimeParsedGetHeader(const xmailparsedmessage* pMsg, const char* sName)
{
	size_t i;

	if ( pMsg == NULL || sName == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < pMsg->iHeaderCount; i++ ) {
		if ( xmailMimeAsciiEqI(pMsg->arrHeaders[i].sName, sName) ) {
			return pMsg->arrHeaders[i].sValue;
		}
	}
	return NULL;
}

static size_t xmailMimeParsedGetHeaderCount(const xmailparsedmessage* pMsg, const char* sName)
{
	size_t i;
	size_t iCount = 0u;

	if ( pMsg == NULL || sName == NULL ) {
		return 0u;
	}
	for ( i = 0u; i < pMsg->iHeaderCount; i++ ) {
		if ( xmailMimeAsciiEqI(pMsg->arrHeaders[i].sName, sName) ) {
			iCount++;
		}
	}
	return iCount;
}

static const char* xmailMimeParsedGetHeaderAt(const xmailparsedmessage* pMsg, const char* sName, size_t iMatchIndex)
{
	size_t i;
	size_t iSeen = 0u;

	if ( pMsg == NULL || sName == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < pMsg->iHeaderCount; i++ ) {
		if ( xmailMimeAsciiEqI(pMsg->arrHeaders[i].sName, sName) ) {
			if ( iSeen == iMatchIndex ) {
				return pMsg->arrHeaders[i].sValue;
			}
			iSeen++;
		}
	}
	return NULL;
}

static str xmailMimeParsedJoinHeaders(const xmailparsedmessage* pMsg, const char* sName, const char* sSep)
{
	xmailmimebuf tBuf;
	size_t i;
	bool bAny = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( pMsg == NULL || sName == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < pMsg->iHeaderCount; i++ ) {
		if ( xmailMimeAsciiEqI(pMsg->arrHeaders[i].sName, sName) ) {
			if ( bAny && !xmailMimeBufAppend(&tBuf, sSep ? sSep : ", ") ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			if ( !xmailMimeBufAppend(&tBuf, xmailMimeStrOrEmpty(pMsg->arrHeaders[i].sValue)) ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			bAny = TRUE;
		}
	}
	if ( !bAny ) {
		return NULL;
	}
	return (str)tBuf.sData;
}

static bool xmailMimeParseHeaderLine(const char* sLine, xmailheader* pHeader)
{
	xrtheaderpair tPair;
	const char* pColon;
	const char* pValue;
	size_t iNameLen;
	size_t iValueLen;

	if ( sLine == NULL || pHeader == NULL ) {
		return FALSE;
	}
	if ( xrtHttpHeaderSplitLine(sLine, &tPair) ) {
		pHeader->sName = (const char*)xmailMimeCopyN(tPair.tName.sPtr, tPair.tName.iLen);
		pHeader->sValue = (const char*)xmailMimeCopyN(tPair.tValue.sPtr, tPair.tValue.iLen);
		if ( pHeader->sName == NULL || pHeader->sValue == NULL ) {
			if ( pHeader->sName != NULL ) xrtFree((ptr)pHeader->sName);
			if ( pHeader->sValue != NULL ) xrtFree((ptr)pHeader->sValue);
			memset(pHeader, 0, sizeof(*pHeader));
			return FALSE;
		}
		return TRUE;
	}
	pColon = strchr(sLine, ':');
	if ( pColon == NULL || pColon == sLine ) {
		return FALSE;
	}
	iNameLen = (size_t)(pColon - sLine);
	while ( iNameLen > 0u && isspace((unsigned char)sLine[iNameLen - 1u]) ) {
		iNameLen--;
	}
	pValue = pColon + 1;
	while ( *pValue != '\0' && isspace((unsigned char)*pValue) ) {
		pValue++;
	}
	iValueLen = strlen(pValue);
	while ( iValueLen > 0u && isspace((unsigned char)pValue[iValueLen - 1u]) ) {
		iValueLen--;
	}
	pHeader->sName = (const char*)xmailMimeCopyN(sLine, iNameLen);
	pHeader->sValue = (const char*)xmailMimeCopyN(pValue, iValueLen);
	if ( pHeader->sName == NULL || pHeader->sValue == NULL ) {
		if ( pHeader->sName != NULL ) xrtFree((ptr)pHeader->sName);
		if ( pHeader->sValue != NULL ) xrtFree((ptr)pHeader->sValue);
		memset(pHeader, 0, sizeof(*pHeader));
		return FALSE;
	}
	return TRUE;
}

static const char* xmailMimePartGetHeader(const xmailmimepart* pPart, const char* sName)
{
	size_t i;

	if ( pPart == NULL || sName == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < pPart->iHeaderCount; i++ ) {
		if ( xmailMimeAsciiEqI(pPart->arrHeaders[i].sName, sName) ) {
			return pPart->arrHeaders[i].sValue;
		}
	}
	return NULL;
}

static void xmailMimeLowerCopy(char* sOut, size_t iOutCap, const char* sText, size_t iLen)
{
	size_t i;

	if ( sOut == NULL || iOutCap == 0u ) {
		return;
	}
	for ( i = 0u; i + 1u < iOutCap && i < iLen; i++ ) {
		sOut[i] = (char)tolower((unsigned char)sText[i]);
	}
	sOut[i] = '\0';
}

static str xmailMimePercentDecode(const char* sText)
{
	xmailmimebuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	for ( p = xmailMimeStrOrEmpty(sText); *p != '\0'; p++ ) {
		if ( p[0] == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2]) ) {
			char ch = (char)((xmailMimeHexValue(p[1]) << 4) | xmailMimeHexValue(p[2]));
			if ( !xmailMimeBufAppendN(&tBuf, &ch, 1u) ) {
				xmailMimeBufFree(&tBuf);
				return NULL;
			}
			p += 2;
		} else if ( !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	}
	if ( tBuf.sData == NULL ) {
		return xmailMimeCopyN("", 0u);
	}
	return (str)tBuf.sData;
}

static void xmailMimeCopyToField(char* sOut, size_t iOutCap, const char* sText)
{
	if ( sOut == NULL || iOutCap == 0u ) {
		return;
	}
	snprintf(sOut, iOutCap, "%s", xmailMimeStrOrEmpty(sText));
}

static void xmailMimeViewCopyToField(char* sOut, size_t iOutCap, xrtstrview tView)
{
	size_t iLen;

	if ( sOut == NULL || iOutCap == 0u ) {
		return;
	}
	iLen = tView.iLen;
	if ( tView.sPtr == NULL ) {
		sOut[0] = '\0';
		return;
	}
	if ( iLen >= iOutCap ) {
		iLen = iOutCap - 1u;
	}
	memcpy(sOut, tView.sPtr, iLen);
	sOut[iLen] = '\0';
}

static void xmailMimeLowerViewCopy(char* sOut, size_t iOutCap, xrtstrview tView)
{
	if ( tView.sPtr == NULL ) {
		if ( sOut != NULL && iOutCap > 0u ) sOut[0] = '\0';
		return;
	}
	xmailMimeLowerCopy(sOut, iOutCap, tView.sPtr, tView.iLen);
}

static str xmailMimeDecodeParameterValue(const char* sValue, bool bExtended)
{
	str sUnquoted;
	str sDecoded;
	const char* sPayload;
	const char* sQuote1;
	const char* sQuote2;

	sUnquoted = xmailMimeUnquoteText(xmailMimeStrOrEmpty(sValue));
	if ( sUnquoted == NULL ) {
		return NULL;
	}
	if ( !bExtended ) {
		sDecoded = xmailMimeDecodeHeaderWord((const char*)sUnquoted);
		if ( sDecoded != NULL ) {
			xrtFree(sUnquoted);
			return sDecoded;
		}
		return sUnquoted;
	}
	sPayload = (const char*)sUnquoted;
	sQuote1 = strchr(sPayload, '\'');
	sQuote2 = sQuote1 ? strchr(sQuote1 + 1, '\'') : NULL;
	if ( sQuote2 != NULL ) {
		sPayload = sQuote2 + 1;
	}
	sDecoded = xmailMimePercentDecode(sPayload);
	xrtFree(sUnquoted);
	return sDecoded;
}

static str xmailMimeHeaderParamValue(const char* sValue, const char* sName)
{
	const char* p;
	size_t iNameLen;

	if ( sValue == NULL || sName == NULL ) {
		return NULL;
	}
	iNameLen = strlen(sName);
	for ( p = strchr(sValue, ';'); p != NULL; ) {
		const char* sParamStart;
		const char* sParamEnd;
		const char* sEq;
		char* sRaw;
		str sDecoded;
		bool bQuoted;
		bool bEsc;

		p++;
		while ( *p != '\0' && isspace((unsigned char)*p) ) {
			p++;
		}
		sParamStart = p;
		bQuoted = FALSE;
		bEsc = FALSE;
		while ( *p != '\0' ) {
			if ( bQuoted ) {
				if ( bEsc ) bEsc = FALSE;
				else if ( *p == '\\' ) bEsc = TRUE;
				else if ( *p == '"' ) bQuoted = FALSE;
			} else if ( *p == '"' ) {
				bQuoted = TRUE;
			} else if ( *p == ';' ) {
				break;
			}
			p++;
		}
		sParamEnd = p;
		sEq = memchr(sParamStart, '=', (size_t)(sParamEnd - sParamStart));
		if ( sEq != NULL ) {
			const char* sNameEnd = sEq;
			while ( sNameEnd > sParamStart && isspace((unsigned char)sNameEnd[-1]) ) {
				sNameEnd--;
			}
			if ( (size_t)(sNameEnd - sParamStart) == iNameLen && xmailMimeStartsWithI(sParamStart, sName) ) {
				const char* sVal = sEq + 1;
				while ( sVal < sParamEnd && isspace((unsigned char)*sVal) ) {
					sVal++;
				}
				while ( sParamEnd > sVal && isspace((unsigned char)sParamEnd[-1]) ) {
					sParamEnd--;
				}
				sRaw = xmailMimeTrimCopy(sVal, (size_t)(sParamEnd - sVal));
				if ( sRaw == NULL ) {
					return NULL;
				}
				sDecoded = xmailMimeDecodeParameterValue(sRaw, sName[iNameLen - 1u] == '*');
				xrtFree(sRaw);
				return sDecoded;
			}
		}
		if ( *p == ';' ) {
			continue;
		}
		break;
	}
	return NULL;
}

static bool xmailMimeParseContentTypeValue(const char* sValue, xmailmimepart* pPart)
{
	const char* p;
	const char* sSemi;
	const char* sSlash;
	size_t iTypeLen;
	size_t iFullLen;
	xrtmediatypeview tMediaType;
	xrthttpparam tBoundary;

	if ( pPart == NULL ) {
		return FALSE;
	}
	if ( sValue == NULL || sValue[0] == '\0' ) {
		snprintf(pPart->sContentType, sizeof(pPart->sContentType), "%s", "text/plain");
		snprintf(pPart->sMediaType, sizeof(pPart->sMediaType), "%s", "text");
		snprintf(pPart->sSubType, sizeof(pPart->sSubType), "%s", "plain");
		return TRUE;
	}
	if ( xrtHttpMediaTypeParse(sValue, &tMediaType) ) {
		xmailMimeViewCopyToField(pPart->sContentType, sizeof(pPart->sContentType), tMediaType.tType);
		if ( tMediaType.tSubType.sPtr != NULL && tMediaType.tSubType.iLen != 0u
			&& strlen(pPart->sContentType) + tMediaType.tSubType.iLen + 2u < sizeof(pPart->sContentType) ) {
			size_t iOff = strlen(pPart->sContentType);
			pPart->sContentType[iOff++] = '/';
			memcpy(pPart->sContentType + iOff, tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen);
			pPart->sContentType[iOff + tMediaType.tSubType.iLen] = '\0';
		}
		xmailMimeLowerViewCopy(pPart->sMediaType, sizeof(pPart->sMediaType), tMediaType.tType);
		xmailMimeLowerViewCopy(pPart->sSubType, sizeof(pPart->sSubType), tMediaType.tSubType);
		if ( xrtHttpMediaTypeFindParam(&tMediaType, "boundary", &tBoundary)
			&& (tBoundary.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
			xmailMimeViewCopyToField(pPart->sBoundary, sizeof(pPart->sBoundary), tBoundary.tValue);
		}
		return TRUE;
	}
	sSemi = strchr(sValue, ';');
	iFullLen = sSemi ? (size_t)(sSemi - sValue) : strlen(sValue);
	while ( iFullLen > 0u && isspace((unsigned char)sValue[iFullLen - 1u]) ) {
		iFullLen--;
	}
	while ( *sValue && isspace((unsigned char)*sValue) ) {
		sValue++;
		if ( iFullLen > 0u ) iFullLen--;
	}
	if ( iFullLen >= sizeof(pPart->sContentType) ) {
		iFullLen = sizeof(pPart->sContentType) - 1u;
	}
	memcpy(pPart->sContentType, sValue, iFullLen);
	pPart->sContentType[iFullLen] = '\0';
	sSlash = memchr(sValue, '/', iFullLen);
	if ( sSlash != NULL ) {
		iTypeLen = (size_t)(sSlash - sValue);
		xmailMimeLowerCopy(pPart->sMediaType, sizeof(pPart->sMediaType), sValue, iTypeLen);
		xmailMimeLowerCopy(pPart->sSubType, sizeof(pPart->sSubType), sSlash + 1, iFullLen - iTypeLen - 1u);
	}
	for ( p = sSemi ? sSemi + 1 : sValue + iFullLen; *p; ) {
		while ( *p && (isspace((unsigned char)*p) || *p == ';') ) {
			p++;
		}
		if ( xmailMimeStartsWithI(p, "boundary=") ) {
			const char* sBoundary = p + strlen("boundary=");
			size_t iBoundaryLen = 0u;
			if ( *sBoundary == '"' ) {
				sBoundary++;
				while ( sBoundary[iBoundaryLen] && sBoundary[iBoundaryLen] != '"' ) {
					iBoundaryLen++;
				}
			} else {
				while ( sBoundary[iBoundaryLen] && sBoundary[iBoundaryLen] != ';' && !isspace((unsigned char)sBoundary[iBoundaryLen]) ) {
					iBoundaryLen++;
				}
			}
			if ( iBoundaryLen >= sizeof(pPart->sBoundary) ) {
				iBoundaryLen = sizeof(pPart->sBoundary) - 1u;
			}
			memcpy(pPart->sBoundary, sBoundary, iBoundaryLen);
			pPart->sBoundary[iBoundaryLen] = '\0';
			break;
		}
		while ( *p && *p != ';' ) {
			p++;
		}
	}
	return TRUE;
}

static bool xmailMimeParsePartMetadata(xmailmimepart* pPart)
{
	const char* sDisposition;
	const char* sContentId;
	str sFileNameExt = NULL;
	str sFileName = NULL;
	str sName = NULL;
	xrtcontentdispositionview tDisposition;
	xrtmediatypeview tContentType;
	xrthttpparam tNameParam;

	if ( pPart == NULL ) {
		return FALSE;
	}
	sDisposition = xmailMimePartGetHeader(pPart, "Content-Disposition");
	if ( sDisposition != NULL && sDisposition[0] != '\0' ) {
		if ( xrtHttpContentDispositionParse(sDisposition, &tDisposition) ) {
			size_t iFileNameLen = 0u;
			xmailMimeLowerViewCopy(pPart->sDisposition, sizeof(pPart->sDisposition), tDisposition.tType);
			pPart->bAttachment = xmailMimeAsciiEqI(pPart->sDisposition, "attachment");
			pPart->bInline = xmailMimeAsciiEqI(pPart->sDisposition, "inline");
			if ( xrtHttpContentDispositionDecodeFileNameTo(&tDisposition, pPart->sFileName, sizeof(pPart->sFileName), &iFileNameLen) ) {
				pPart->sFileName[iFileNameLen < sizeof(pPart->sFileName) ? iFileNameLen : sizeof(pPart->sFileName) - 1u] = '\0';
			}
		} else {
			const char* sSemi = strchr(sDisposition, ';');
			size_t iLen = sSemi ? (size_t)(sSemi - sDisposition) : strlen(sDisposition);
			while ( iLen > 0u && isspace((unsigned char)sDisposition[iLen - 1u]) ) {
				iLen--;
			}
			while ( *sDisposition && isspace((unsigned char)*sDisposition) ) {
				sDisposition++;
				if ( iLen > 0u ) iLen--;
			}
			xmailMimeLowerCopy(pPart->sDisposition, sizeof(pPart->sDisposition), sDisposition, iLen);
			pPart->bAttachment = xmailMimeAsciiEqI(pPart->sDisposition, "attachment");
			pPart->bInline = xmailMimeAsciiEqI(pPart->sDisposition, "inline");
			sFileNameExt = xmailMimeHeaderParamValue(sDisposition, "filename*");
			sFileName = xmailMimeHeaderParamValue(sDisposition, "filename");
		}
	}
	if ( pPart->sFileName[0] != '\0' ) {
		/* xrtHttpContentDispositionDecodeFileNameTo already filled it. */
	} else if ( sFileNameExt != NULL && sFileNameExt[0] != '\0' ) {
		xmailMimeCopyToField(pPart->sFileName, sizeof(pPart->sFileName), (const char*)sFileNameExt);
	} else if ( sFileName != NULL && sFileName[0] != '\0' ) {
		xmailMimeCopyToField(pPart->sFileName, sizeof(pPart->sFileName), (const char*)sFileName);
	} else {
		const char* sContentType = xmailMimePartGetHeader(pPart, "Content-Type");
		if ( sContentType != NULL && xrtHttpMediaTypeParse(sContentType, &tContentType)
			&& xrtHttpMediaTypeFindParam(&tContentType, "name", &tNameParam)
			&& (tNameParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
			xmailMimeViewCopyToField(pPart->sFileName, sizeof(pPart->sFileName), tNameParam.tValue);
		} else {
			sName = xmailMimeHeaderParamValue(sContentType, "name");
			if ( sName != NULL && sName[0] != '\0' ) {
				xmailMimeCopyToField(pPart->sFileName, sizeof(pPart->sFileName), (const char*)sName);
			}
		}
	}
	sContentId = xmailMimePartGetHeader(pPart, "Content-ID");
	if ( sContentId != NULL && sContentId[0] != '\0' ) {
		const char* sStart = sContentId;
		size_t iLen;
		while ( *sStart && isspace((unsigned char)*sStart) ) {
			sStart++;
		}
		iLen = strlen(sStart);
		while ( iLen > 0u && isspace((unsigned char)sStart[iLen - 1u]) ) {
			iLen--;
		}
		if ( iLen >= 2u && sStart[0] == '<' && sStart[iLen - 1u] == '>' ) {
			sStart++;
			iLen -= 2u;
		}
		if ( iLen >= sizeof(pPart->sContentId) ) {
			iLen = sizeof(pPart->sContentId) - 1u;
		}
		memcpy(pPart->sContentId, sStart, iLen);
		pPart->sContentId[iLen] = '\0';
	}
	if ( sFileNameExt != NULL ) xrtFree(sFileNameExt);
	if ( sFileName != NULL ) xrtFree(sFileName);
	if ( sName != NULL ) xrtFree(sName);
	return TRUE;
}

static str xmailMimeCopyWithoutWhitespace(const char* sText)
{
	xmailmimebuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	for ( p = xmailMimeStrOrEmpty(sText); *p != '\0'; p++ ) {
		if ( !isspace((unsigned char)*p) && !xmailMimeBufAppendN(&tBuf, p, 1u) ) {
			xmailMimeBufFree(&tBuf);
			return NULL;
		}
	}
	if ( tBuf.sData == NULL ) {
		return xmailMimeCopyN("", 0u);
	}
	return (str)tBuf.sData;
}

static bool xmailMimePartDecodeBody(xmailmimepart* pPart)
{
	const char* sEncoding;

	if ( pPart == NULL ) {
		return FALSE;
	}
	sEncoding = xmailMimePartGetHeader(pPart, "Content-Transfer-Encoding");
	if ( sEncoding == NULL || sEncoding[0] == '\0' ) {
		snprintf(pPart->sTransferEncoding, sizeof(pPart->sTransferEncoding), "%s", "7bit");
		pPart->sDecodedBody = xmailMimeCopyN(xmailMimeStrOrEmpty((const char*)pPart->sBody), strlen(xmailMimeStrOrEmpty((const char*)pPart->sBody)));
		return pPart->sDecodedBody != NULL;
	}
	xmailMimeLowerCopy(pPart->sTransferEncoding, sizeof(pPart->sTransferEncoding), sEncoding, strlen(sEncoding));
	if ( xmailMimeAsciiEqI(pPart->sTransferEncoding, "quoted-printable") ) {
		pPart->sDecodedBody = xmailMimeQuotedPrintableDecode((const char*)pPart->sBody);
		return pPart->sDecodedBody != NULL;
	}
	if ( xmailMimeAsciiEqI(pPart->sTransferEncoding, "base64") ) {
		str sCompact = xmailMimeCopyWithoutWhitespace((const char*)pPart->sBody);
		ptr pDecoded;
		if ( sCompact == NULL ) {
			return FALSE;
		}
		pDecoded = xrtBase64Decode(sCompact, strlen((const char*)sCompact), NULL);
		xrtFree(sCompact);
		if ( pDecoded == NULL ) {
			return FALSE;
		}
		pPart->sDecodedBody = (str)pDecoded;
		return TRUE;
	}
	pPart->sDecodedBody = xmailMimeCopyN(xmailMimeStrOrEmpty((const char*)pPart->sBody), strlen(xmailMimeStrOrEmpty((const char*)pPart->sBody)));
	return pPart->sDecodedBody != NULL;
}

static bool xmailMimeParsePartRaw(const char* sRaw, xmailmimepart* pPart, int iDepth);

static bool xmailMimePartAppendChild(xmailmimepart* pPart, const char* sRaw, size_t iLen, int iDepth)
{
	xmailmimepart* arrNew;
	char* sChildRaw;

	if ( pPart == NULL || sRaw == NULL ) {
		return FALSE;
	}
	sChildRaw = (char*)xmailMimeCopyN(sRaw, iLen);
	if ( sChildRaw == NULL ) {
		return FALSE;
	}
	arrNew = pPart->arrChildren
		? (xmailmimepart*)xrtRealloc(pPart->arrChildren, sizeof(xmailmimepart) * (pPart->iChildCount + 1u))
		: (xmailmimepart*)xrtMalloc(sizeof(xmailmimepart));
	if ( arrNew == NULL ) {
		xrtFree(sChildRaw);
		return FALSE;
	}
	pPart->arrChildren = arrNew;
	memset(&pPart->arrChildren[pPart->iChildCount], 0, sizeof(xmailmimepart));
	if ( !xmailMimeParsePartRaw(sChildRaw, &pPart->arrChildren[pPart->iChildCount], iDepth + 1) ) {
		xmailMimePartFree(&pPart->arrChildren[pPart->iChildCount]);
		xrtFree(sChildRaw);
		return FALSE;
	}
	pPart->iChildCount++;
	xrtFree(sChildRaw);
	return TRUE;
}

static bool xmailMimePartAppendChildFromView(xmailmimepart* pPart, xrtstrview tHeaders, xrtstrview tBody, int iDepth)
{
	xmailmimebuf tRaw;
	bool bOk;

	if ( pPart == NULL || (tHeaders.iLen > 0u && tHeaders.sPtr == NULL) || (tBody.iLen > 0u && tBody.sPtr == NULL) ) {
		return FALSE;
	}
	memset(&tRaw, 0, sizeof(tRaw));
	bOk = xmailMimeBufAppendN(&tRaw, tHeaders.sPtr ? tHeaders.sPtr : "", tHeaders.iLen)
		&& xmailMimeBufAppend(&tRaw, "\r\n\r\n")
		&& xmailMimeBufAppendN(&tRaw, tBody.sPtr ? tBody.sPtr : "", tBody.iLen)
		&& xmailMimePartAppendChild(pPart, tRaw.sData ? tRaw.sData : "", tRaw.iLen, iDepth);
	xmailMimeBufFree(&tRaw);
	return bOk;
}

static bool xmailMimeParseMultipartChildrenWithXrt(xmailmimepart* pPart, const char* sBody, int iDepth, bool* pUsed)
{
	xmailmimepart tTmp;
	size_t iBodyLen;
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtmultipartpartview tView;

	if ( pUsed != NULL ) {
		*pUsed = FALSE;
	}
	if ( pPart == NULL || sBody == NULL || pPart->sBoundary[0] == '\0' ) {
		return FALSE;
	}
	memset(&tTmp, 0, sizeof(tTmp));
	iBodyLen = strlen(sBody);
	while ( xrtMultipartNextN(sBody, iBodyLen, pPart->sBoundary, strlen(pPart->sBoundary), &iOffset, &tView) ) {
		if ( !xmailMimePartAppendChildFromView(&tTmp, tView.tHeaders, tView.tBody, iDepth) ) {
			xmailMimePartFree(&tTmp);
			return FALSE;
		}
		iCount++;
	}
	if ( iCount == 0u ) {
		xmailMimePartFree(&tTmp);
		return TRUE;
	}
	if ( iOffset < iBodyLen ) {
		xmailMimePartFree(&tTmp);
		return TRUE;
	}
	pPart->arrChildren = tTmp.arrChildren;
	pPart->iChildCount = tTmp.iChildCount;
	tTmp.arrChildren = NULL;
	tTmp.iChildCount = 0u;
	if ( pUsed != NULL ) {
		*pUsed = TRUE;
	}
	return TRUE;
}

static bool xmailMimeParseMultipartChildrenManual(xmailmimepart* pPart, const char* sBody, int iDepth)
{
	xmailmimebuf tNeedle;
	const char* p;
	const char* sPartStart = NULL;

	if ( pPart == NULL || sBody == NULL || pPart->sBoundary[0] == '\0' ) {
		return FALSE;
	}
	memset(&tNeedle, 0, sizeof(tNeedle));
	if ( !xmailMimeBufAppend(&tNeedle, "--") || !xmailMimeBufAppend(&tNeedle, pPart->sBoundary) ) {
		xmailMimeBufFree(&tNeedle);
		return FALSE;
	}
	p = sBody;
	while ( *p ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen >= tNeedle.iLen && strncmp(p, tNeedle.sData, tNeedle.iLen) == 0 ) {
			bool bClose = (iLineLen >= tNeedle.iLen + 2u && p[tNeedle.iLen] == '-' && p[tNeedle.iLen + 1u] == '-');
			if ( sPartStart != NULL ) {
				const char* sPartEnd = p;
				if ( sPartEnd >= sPartStart + 2 && sPartEnd[-2] == '\r' && sPartEnd[-1] == '\n' ) {
					sPartEnd -= 2;
				}
				if ( sPartEnd > sPartStart && !xmailMimePartAppendChild(pPart, sPartStart, (size_t)(sPartEnd - sPartStart), iDepth) ) {
					xmailMimeBufFree(&tNeedle);
					return FALSE;
				}
			}
			if ( bClose ) {
				break;
			}
			sPartStart = sLineEnd ? sLineEnd + 2 : p + iLineLen;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	xmailMimeBufFree(&tNeedle);
	return TRUE;
}

static bool xmailMimeParseMultipartChildren(xmailmimepart* pPart, const char* sBody, int iDepth)
{
	bool bUsedXrt = FALSE;

	if ( pPart == NULL || sBody == NULL || pPart->sBoundary[0] == '\0' ) {
		return FALSE;
	}
	if ( !xmailMimeParseMultipartChildrenWithXrt(pPart, sBody, iDepth, &bUsedXrt) ) {
		return FALSE;
	}
	if ( bUsedXrt ) {
		return TRUE;
	}
	return xmailMimeParseMultipartChildrenManual(pPart, sBody, iDepth);
}

static void xmailMimePartFree(xmailmimepart* pPart)
{
	size_t i;

	if ( pPart == NULL ) {
		return;
	}
	for ( i = 0u; i < pPart->iHeaderCount; i++ ) {
		if ( pPart->arrHeaders[i].sName != NULL ) xrtFree((ptr)pPart->arrHeaders[i].sName);
		if ( pPart->arrHeaders[i].sValue != NULL ) xrtFree((ptr)pPart->arrHeaders[i].sValue);
	}
	if ( pPart->arrHeaders != NULL ) {
		xrtFree(pPart->arrHeaders);
	}
	if ( pPart->sBody != NULL ) {
		xrtFree(pPart->sBody);
	}
	if ( pPart->sDecodedBody != NULL ) {
		xrtFree(pPart->sDecodedBody);
	}
	if ( pPart->pMessage != NULL ) {
		xmailMimeParsedMessageFree(pPart->pMessage);
		xrtFree(pPart->pMessage);
	}
	if ( pPart->arrChildren != NULL ) {
		for ( i = 0u; i < pPart->iChildCount; i++ ) {
			xmailMimePartFree(&pPart->arrChildren[i]);
		}
		xrtFree(pPart->arrChildren);
	}
	memset(pPart, 0, sizeof(*pPart));
}

static bool xmailMimeParsePartRaw(const char* sRaw, xmailmimepart* pPart, int iDepth)
{
	str sNorm = NULL;
	str sHeaderBlock = NULL;
	str sUnfolded = NULL;
	const char* sSep;
	const char* p;
	size_t iHeaderLen;
	size_t iCount = 0u;
	size_t iIndex = 0u;
	const char* sContentType;

	if ( pPart == NULL || iDepth > 16 ) {
		return FALSE;
	}
	memset(pPart, 0, sizeof(*pPart));
	sNorm = xmailMimeNormalizeCrlf(sRaw);
	if ( sNorm == NULL ) {
		return FALSE;
	}
	sSep = strstr((const char*)sNorm, "\r\n\r\n");
	iHeaderLen = sSep ? (size_t)(sSep - (const char*)sNorm) : strlen((const char*)sNorm);
	sHeaderBlock = xmailMimeCopyN((const char*)sNorm, iHeaderLen);
	if ( sHeaderBlock == NULL ) {
		goto fail;
	}
	sUnfolded = xmailMimeUnfoldHeaderBlock((const char*)sHeaderBlock);
	if ( sUnfolded == NULL ) {
		goto fail;
	}
	for ( p = (const char*)sUnfolded; *p != '\0'; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0u ) iCount++;
		if ( sLineEnd == NULL ) break;
		p = sLineEnd + 2;
	}
	if ( iCount > 0u ) {
		pPart->arrHeaders = (xmailheader*)xrtMalloc(sizeof(xmailheader) * iCount);
		if ( pPart->arrHeaders == NULL ) goto fail;
		memset(pPart->arrHeaders, 0, sizeof(xmailheader) * iCount);
	}
	for ( p = (const char*)sUnfolded; *p != '\0'; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0u ) {
			char* sLine = (char*)xmailMimeCopyN(p, iLineLen);
			if ( sLine == NULL ) goto fail;
			if ( !xmailMimeParseHeaderLine(sLine, &pPart->arrHeaders[iIndex]) ) {
				xrtFree(sLine);
				goto fail;
			}
			xrtFree(sLine);
			iIndex++;
		}
		if ( sLineEnd == NULL ) break;
		p = sLineEnd + 2;
	}
	pPart->iHeaderCount = iIndex;
	pPart->sBody = xmailMimeCopyN(sSep ? sSep + 4 : "", sSep ? strlen(sSep + 4) : 0u);
	if ( pPart->sBody == NULL ) goto fail;
	sContentType = xmailMimePartGetHeader(pPart, "Content-Type");
	if ( !xmailMimeParseContentTypeValue(sContentType, pPart) ) goto fail;
	if ( !xmailMimeParsePartMetadata(pPart) ) goto fail;
	if ( xmailMimeAsciiEqI(pPart->sMediaType, "multipart") && pPart->sBoundary[0] != '\0' ) {
		if ( !xmailMimeParseMultipartChildren(pPart, (const char*)pPart->sBody, iDepth) ) goto fail;
	} else {
		if ( !xmailMimePartDecodeBody(pPart) ) goto fail;
		if ( xmailMimeAsciiEqI(pPart->sMediaType, "message") && xmailMimeAsciiEqI(pPart->sSubType, "rfc822") ) {
			pPart->pMessage = (xmailparsedmessage*)xrtMalloc(sizeof(xmailparsedmessage));
			if ( pPart->pMessage == NULL ) goto fail;
			memset(pPart->pMessage, 0, sizeof(xmailparsedmessage));
			if ( !xmailMimeParseMessage((const char*)pPart->sDecodedBody, pPart->pMessage) ) goto fail;
		}
	}
	xrtFree(sNorm);
	xrtFree(sHeaderBlock);
	xrtFree(sUnfolded);
	return TRUE;

fail:
	if ( sNorm != NULL ) xrtFree(sNorm);
	if ( sHeaderBlock != NULL ) xrtFree(sHeaderBlock);
	if ( sUnfolded != NULL ) xrtFree(sUnfolded);
	xmailMimePartFree(pPart);
	return FALSE;
}

static bool xmailMimeParseMessage(const char* sRaw, xmailparsedmessage* pOut)
{
	str sNorm = NULL;
	str sUnfolded = NULL;
	const char* sSep;
	const char* p;
	size_t iHeaderLen;
	size_t iCount = 0u;
	size_t iIndex = 0u;

	if ( pOut == NULL ) {
		return FALSE;
	}
	memset(pOut, 0, sizeof(*pOut));
	sNorm = xmailMimeNormalizeCrlf(sRaw);
	if ( sNorm == NULL ) {
		return FALSE;
	}
	sSep = strstr((const char*)sNorm, "\r\n\r\n");
	iHeaderLen = sSep ? (size_t)(sSep - (const char*)sNorm) : strlen((const char*)sNorm);
	pOut->sHeaderBlock = xmailMimeCopyN((const char*)sNorm, iHeaderLen);
	pOut->sBody = xmailMimeCopyN(sSep ? sSep + 4 : "", sSep ? strlen(sSep + 4) : 0u);
	if ( pOut->sHeaderBlock == NULL || pOut->sBody == NULL ) {
		goto fail;
	}
	sUnfolded = xmailMimeUnfoldHeaderBlock((const char*)pOut->sHeaderBlock);
	if ( sUnfolded == NULL ) {
		goto fail;
	}
	for ( p = (const char*)sUnfolded; *p != '\0'; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0u ) {
			iCount++;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	if ( iCount > 0u ) {
		pOut->arrHeaders = (xmailheader*)xrtMalloc(sizeof(xmailheader) * iCount);
		if ( pOut->arrHeaders == NULL ) {
			goto fail;
		}
		memset(pOut->arrHeaders, 0, sizeof(xmailheader) * iCount);
	}
	for ( p = (const char*)sUnfolded; *p != '\0'; ) {
		char* sLine;
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0u ) {
			sLine = (char*)xmailMimeCopyN(p, iLineLen);
			if ( sLine == NULL ) {
				goto fail;
			}
			if ( !xmailMimeParseHeaderLine(sLine, &pOut->arrHeaders[iIndex]) ) {
				xrtFree(sLine);
				goto fail;
			}
			xrtFree(sLine);
			iIndex++;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	pOut->iHeaderCount = iIndex;
	pOut->pRootPart = (xmailmimepart*)xrtMalloc(sizeof(xmailmimepart));
	if ( pOut->pRootPart == NULL ) {
		goto fail;
	}
	if ( !xmailMimeParsePartRaw((const char*)sNorm, pOut->pRootPart, 0) ) {
		goto fail;
	}
	xrtFree(sNorm);
	xrtFree(sUnfolded);
	return TRUE;

fail:
	if ( sNorm != NULL ) xrtFree(sNorm);
	if ( sUnfolded != NULL ) xrtFree(sUnfolded);
	xmailMimeParsedMessageFree(pOut);
	return FALSE;
}

#endif
