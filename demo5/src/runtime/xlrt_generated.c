#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static size_t __xlTextLen(const char* sText)
{
	return (sText != NULL) ? strlen(sText) : 0u;
}

static const char* __xlHexLowerBytes(const unsigned char* pBytes, size_t iCount);
static int __xlTextBufAppend(char** psBuf, size_t* piLen, size_t* piCap, const char* sText, size_t iTextLen);
static int __xlTextBufAppendChar(char** psBuf, size_t* piLen, size_t* piCap, char ch);
static xregex* __xlRegexCreateOrRaise(const char* sPattern);
static const char* __xlRegexSliceText(const char* sText, size_t iTextLen, xregexspan tSpan);
static const char* __xlRegexReplaceInternal(const char* sPattern, const char* sText, const char* sValue, int bReplaceAll);
static int __xlAppendListText(xvalue value, const char* sText, size_t iLen);
static int __xlJsonEncodeValue(char** psBuf, size_t* piLen, size_t* piCap, xvalue value);
static int __xlQueryEncodeText(char** psBuf, size_t* piLen, size_t* piCap, const char* sText, size_t iTextLen);
static const char* __xlQueryDecodeText(const char* sText, size_t iTextLen);
static const char* __xlTemplateTrimKey(const char* sText, size_t iTextLen, size_t* piOutLen);

typedef struct XL_JsonParseCtx {
	const char* sText;
	size_t iPos;
	size_t iLen;
	const char* sError;
} XL_JsonParseCtx;

static void __xlJsonSkipSpace(XL_JsonParseCtx* pCtx);
static xvalue __xlJsonParseValue(XL_JsonParseCtx* pCtx);
static xvalue __xlJsonParseObject(XL_JsonParseCtx* pCtx);
static xvalue __xlJsonParseArray(XL_JsonParseCtx* pCtx);
static xvalue __xlJsonParseStringValue(XL_JsonParseCtx* pCtx);
static xvalue __xlJsonParseNumberValue(XL_JsonParseCtx* pCtx);

static void __xlRaiseNativeError(const char* sKind, const char* sMessage, int iCode, const char* sData)
{
	xlrt_native_error_raise(sKind, sMessage, iCode, sData);
}

static void __xlWriteText(const char* sText)
{
	fputs((sText != NULL) ? sText : "", stdout);
}

static void __xlWriteErrorText(const char* sText)
{
	fputs((sText != NULL) ? sText : "", stderr);
}

static xvalue __xlValueCloneAsyncState(xvalue value);

typedef struct XL_AsyncStateCloneDictCtx {
	xvalue value;
	int failed;
} XL_AsyncStateCloneDictCtx;

typedef struct XL_AsyncStateCloneSetCtx {
	xvalue value;
	int failed;
} XL_AsyncStateCloneSetCtx;

static bool __xlValueCloneAsyncStateDictProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	XL_AsyncStateCloneDictCtx* pCtx;
	xvalue* ppItem;
	xvalue cloned;

	pCtx = (XL_AsyncStateCloneDictCtx*)pArg;
	ppItem = (xvalue*)pVal;
	if ( (pCtx == NULL) || (pCtx->value == NULL) || (pKey == NULL) || (ppItem == NULL) ) {
		return TRUE;
	}
	cloned = __xlValueCloneAsyncState(ppItem[0]);
	if ( (ppItem[0] != NULL) && (cloned == NULL) ) {
		pCtx->failed = 1;
		return TRUE;
	}
	if ( !xvoTableSetValue(pCtx->value, pKey->Key, pKey->KeyLen, cloned, TRUE) ) {
		if ( cloned != NULL ) {
			xvoUnref(cloned);
		}
		pCtx->failed = 1;
		return TRUE;
	}
	return FALSE;
}

static bool __xlValueCloneAsyncStateSetProc(ptr pNode, ptr pArg)
{
	XL_AsyncStateCloneSetCtx* pCtx;
	Coll_Key* pKey;
	xvalue cloned;

	pCtx = (XL_AsyncStateCloneSetCtx*)pArg;
	pKey = (Coll_Key*)pNode;
	if ( (pCtx == NULL) || (pCtx->value == NULL) || (pKey == NULL) ) {
		return TRUE;
	}
	cloned = __xlValueCloneAsyncState(pKey->Value);
	if ( (pKey->Value != NULL) && (cloned == NULL) ) {
		pCtx->failed = 1;
		return TRUE;
	}
	if ( !xvoCollSetValue(pCtx->value, cloned, TRUE) ) {
		if ( cloned != NULL ) {
			xvoUnref(cloned);
		}
		pCtx->failed = 1;
		return TRUE;
	}
	return FALSE;
}

static xvalue __xlValueCloneAsyncState(xvalue value)
{
	xvalue cloned;
	uint32 i;

	if ( value == NULL ) {
		return NULL;
	}

	switch ( value->Type ) {
		case XVO_DT_ARRAY:
			cloned = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
			if ( cloned == NULL ) {
				return NULL;
			}
			for ( i = 0; i < xvoArrayItemCount(value); ++i ) {
				xvalue item = __xlValueCloneAsyncState(xvoArrayGetValue(value, i));
				if ( (xvoArrayGetValue(value, i) != NULL) && (item == NULL) ) {
					xvoUnref(cloned);
					return NULL;
				}
				if ( !xvoArrayAppendValue(cloned, item, TRUE) ) {
					if ( item != NULL ) {
						xvoUnref(item);
					}
					xvoUnref(cloned);
					return NULL;
				}
			}
			return cloned;
		case XVO_DT_LIST:
			cloned = xvoCreateListEx(XRT_OBJMODE_SHARED);
			if ( cloned == NULL ) {
				return NULL;
			}
			for ( i = 0; i < xvoListItemCount(value); ++i ) {
				xvalue item = __xlValueCloneAsyncState(xvoListGetValue(value, (int64)i));
				if ( (xvoListGetValue(value, (int64)i) != NULL) && (item == NULL) ) {
					xvoUnref(cloned);
					return NULL;
				}
				if ( !xvoListSetValue(cloned, (int64)i, item, TRUE) ) {
					if ( item != NULL ) {
						xvoUnref(item);
					}
					xvoUnref(cloned);
					return NULL;
				}
			}
			return cloned;
		case XVO_DT_TABLE:
		{
			XL_AsyncStateCloneDictCtx ctx;

			cloned = xvoCreateTableEx(XRT_OBJMODE_SHARED);
			if ( cloned == NULL ) {
				return NULL;
			}
			ctx.value = cloned;
			ctx.failed = 0;
			xrtDictWalk(value->vTable, __xlValueCloneAsyncStateDictProc, &ctx);
			if ( ctx.failed ) {
				xvoUnref(cloned);
				return NULL;
			}
			return cloned;
		}
		case XVO_DT_COLL:
		{
			XL_AsyncStateCloneSetCtx ctx;

			cloned = xvoCreateCollEx(XRT_OBJMODE_SHARED);
			if ( cloned == NULL ) {
				return NULL;
			}
			ctx.value = cloned;
			ctx.failed = 0;
			(void)xrtAVLTreeWalk(value->vColl, __xlValueCloneAsyncStateSetProc, &ctx);
			if ( ctx.failed ) {
				xvoUnref(cloned);
				return NULL;
			}
			return cloned;
		}
		default:
			return xvoCopy(value);
	}
}

xvalue xl_value_clone_async_state(xvalue value)
{
	return __xlValueCloneAsyncState(value);
}

const char* xl_debug_text(xvalue value)
{
	return xl_value_to_string(value);
}

void xl_debug_print(xvalue value)
{
	__xlWriteText(xl_value_to_string(value));
}

void xl_debug_println(xvalue value)
{
	__xlWriteText(xl_value_to_string(value));
	__xlWriteText("\n");
}

void xl_log_debug(const char* sText)
{
	__xlWriteText("[debug] ");
	__xlWriteText((sText != NULL) ? sText : "");
	__xlWriteText("\n");
}

void xl_log_info(const char* sText)
{
	__xlWriteText("[info] ");
	__xlWriteText((sText != NULL) ? sText : "");
	__xlWriteText("\n");
}

void xl_log_warn(const char* sText)
{
	__xlWriteText("[warn] ");
	__xlWriteText((sText != NULL) ? sText : "");
	__xlWriteText("\n");
}

void xl_log_error(const char* sText)
{
	__xlWriteText("[error] ");
	__xlWriteText((sText != NULL) ? sText : "");
	__xlWriteText("\n");
}

void xl_trace_write(const char* sText)
{
	__xlWriteText("[trace] ");
	__xlWriteText((sText != NULL) ? sText : "");
	__xlWriteText("\n");
}

int xl_regex_is_match(const char* sPattern, const char* sText)
{
	xregex* pRegex;
	int iRc;

	if ( sText == NULL ) {
		sText = "";
	}
	pRegex = __xlRegexCreateOrRaise(sPattern);
	if ( pRegex == NULL ) {
		return 0;
	}
	iRc = xrtRegexIsMatch(pRegex, sText, __xlTextLen(sText));
	xrtRegexDestroy(pRegex);
	return (iRc > 0) ? 1 : 0;
}

const char* xl_regex_find(const char* sPattern, const char* sText)
{
	xregex* pRegex;
	xregexspan tSpan;
	size_t iTextLen;
	int iRc;

	if ( sText == NULL ) {
		sText = "";
	}
	pRegex = __xlRegexCreateOrRaise(sPattern);
	if ( pRegex == NULL ) {
		return "";
	}
	iTextLen = __xlTextLen(sText);
	iRc = xrtRegexFind(pRegex, sText, iTextLen, &tSpan);
	xrtRegexDestroy(pRegex);
	if ( iRc <= 0 ) {
		return "";
	}
	return __xlRegexSliceText(sText, iTextLen, tSpan);
}

xvalue xl_regex_captures(const char* sPattern, const char* sText)
{
	xregex* pRegex;
	xvalue value;
	uint32 iCaptureCount;
	xregexspan* arrCaptures;
	uint32 i;
	size_t iTextLen;
	int iRc;

	if ( sText == NULL ) {
		sText = "";
	}
	value = xvoCreateList();
	if ( value == NULL ) {
		return NULL;
	}
	pRegex = __xlRegexCreateOrRaise(sPattern);
	if ( pRegex == NULL ) {
		return value;
	}
	iCaptureCount = xrtRegexCaptureCount(pRegex);
	if ( iCaptureCount == 0u ) {
		iCaptureCount = 1u;
	}
	arrCaptures = (xregexspan*)xrtMalloc(sizeof(xregexspan) * (size_t)iCaptureCount);
	if ( arrCaptures == NULL ) {
		xrtRegexDestroy(pRegex);
		__xlRaiseNativeError("RegexError", "regex captures alloc failed", 0, sPattern);
		return value;
	}
	iTextLen = __xlTextLen(sText);
	iRc = xrtRegexCaptures(pRegex, sText, iTextLen, arrCaptures, iCaptureCount);
	if ( iRc > 0 ) {
		for ( i = 0u; i < iCaptureCount; ++i ) {
			const char* sItem = __xlRegexSliceText(sText, iTextLen, arrCaptures[i]);
			(void)__xlAppendListText(value, sItem, __xlTextLen(sItem));
		}
	}
	xrtFree(arrCaptures);
	xrtRegexDestroy(pRegex);
	return value;
}

int xl_regex_capture_count(const char* sPattern)
{
	xregex* pRegex;
	int iCount;

	pRegex = __xlRegexCreateOrRaise(sPattern);
	if ( pRegex == NULL ) {
		return 0;
	}
	iCount = (int)xrtRegexCaptureCount(pRegex);
	if ( iCount > 0 ) {
		iCount -= 1;
	}
	xrtRegexDestroy(pRegex);
	return iCount;
}

const char* xl_regex_replace_first(const char* sPattern, const char* sText, const char* sValue)
{
	return __xlRegexReplaceInternal(sPattern, sText, sValue, 0);
}

const char* xl_regex_replace_all(const char* sPattern, const char* sText, const char* sValue)
{
	return __xlRegexReplaceInternal(sPattern, sText, sValue, 1);
}

const char* xl_json_encode(xvalue value)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	if ( !__xlJsonEncodeValue(&sBuf, &iLen, &iCap, value) ) {
		free(sBuf);
		__xlRaiseNativeError("JsonError", "json encode failed", 0, "");
		return "";
	}
	return (sBuf != NULL) ? sBuf : "";
}

xvalue xl_json_decode(const char* sText)
{
	XL_JsonParseCtx ctx;
	xvalue value;

	if ( sText == NULL ) {
		sText = "";
	}
	memset(&ctx, 0, sizeof(ctx));
	ctx.sText = sText;
	ctx.iLen = __xlTextLen(sText);
	value = __xlJsonParseValue(&ctx);
	if ( value == NULL ) {
		__xlRaiseNativeError("JsonError", (ctx.sError != NULL) ? ctx.sError : "json decode failed", 0, sText);
		return xvoCreateNull();
	}
	__xlJsonSkipSpace(&ctx);
	if ( ctx.iPos != ctx.iLen ) {
		xvoUnref(value);
		__xlRaiseNativeError("JsonError", "trailing json characters", 0, sText);
		return xvoCreateNull();
	}
	return value;
}

const char* xl_query_encode(xvalue value)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	xdict pTable;
	Dict_Key* pKey;
	int bFirst;

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	if ( value == NULL ) {
		return "";
	}
	pTable = xvoGetTable(value);
	bFirst = 1;
	if ( pTable != NULL ) {
		xrtAVLTB_IterBegin((xavltbase)pTable);
		for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
			xvalue itemValue = *(xvalue*)(&pKey[1]);
			const char* sValue = (const char*)xvoGetText(itemValue);
			size_t iValueLen = (size_t)xvoGetSize(itemValue);

			if ( !bFirst && !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, '&') ) {
				xrtAVLTB_IterEnd((xavltbase)pTable);
				free(sBuf);
				__xlRaiseNativeError("QueryError", "query encode failed", 0, "");
				return "";
			}
			if ( !__xlQueryEncodeText(&sBuf, &iLen, &iCap, pKey->Key, pKey->KeyLen) ||
				!__xlTextBufAppendChar(&sBuf, &iLen, &iCap, '=') ||
				!__xlQueryEncodeText(&sBuf, &iLen, &iCap, (sValue != NULL) ? sValue : "", iValueLen) ) {
				xrtAVLTB_IterEnd((xavltbase)pTable);
				free(sBuf);
				__xlRaiseNativeError("QueryError", "query encode failed", 0, "");
				return "";
			}
			bFirst = 0;
		}
		xrtAVLTB_IterEnd((xavltbase)pTable);
	}
	return (sBuf != NULL) ? sBuf : "";
}

xvalue xl_query_decode(const char* sText)
{
	xvalue value;
	const char* sCur;

	if ( sText == NULL ) {
		sText = "";
	}
	value = xvoCreateTable();
	if ( value == NULL ) {
		__xlRaiseNativeError("QueryError", "query decode failed", 0, sText);
		return NULL;
	}
	sCur = sText;
	while ( *sCur != '\0' ) {
		const char* sPartEnd = strchr(sCur, '&');
		const char* sEq;
		size_t iPartLen;
		size_t iKeyLen;
		size_t iValueLen;
		const char* sKeyDecoded;
		const char* sValueDecoded;
		xvalue itemValue;

		iPartLen = (sPartEnd != NULL) ? (size_t)(sPartEnd - sCur) : __xlTextLen(sCur);
		sEq = memchr(sCur, '=', iPartLen);
		iKeyLen = (sEq != NULL) ? (size_t)(sEq - sCur) : iPartLen;
		iValueLen = (sEq != NULL) ? (iPartLen - iKeyLen - 1u) : 0u;
		sKeyDecoded = __xlQueryDecodeText(sCur, iKeyLen);
		sValueDecoded = __xlQueryDecodeText((sEq != NULL) ? (sEq + 1) : "", iValueLen);
		itemValue = xvoCreateText((str)sValueDecoded, __xlTextLen(sValueDecoded), FALSE);
		if ( (itemValue == NULL) ||
			!xvoTableSetValue(value, (str)sKeyDecoded, (uint32)__xlTextLen(sKeyDecoded), itemValue, TRUE) ) {
			if ( itemValue != NULL ) {
				xvoUnref(itemValue);
			}
			xvoUnref(value);
			__xlRaiseNativeError("QueryError", "query decode failed", 0, sText);
			return NULL;
		}
		if ( sPartEnd == NULL ) {
			break;
		}
		sCur = sPartEnd + 1;
	}
	return value;
}

static xvalue __xlRangeCollectInternal(int start, int stop, int step)
{
	xvalue value;
	int i;

	if ( step == 0 ) {
		__xlRaiseNativeError("RangeError", "range step cannot be zero", 0, NULL);
		return NULL;
	}
	value = xvoCreateArray();
	if ( value == NULL ) {
		return NULL;
	}
	if ( step > 0 ) {
		for ( i = start; i < stop; i += step ) {
			if ( !xvoArrayAppendValue(value, xvoCreateInt((int64)i), TRUE) ) {
				xvoUnref(value);
				return NULL;
			}
		}
	} else {
		for ( i = start; i > stop; i += step ) {
			if ( !xvoArrayAppendValue(value, xvoCreateInt((int64)i), TRUE) ) {
				xvoUnref(value);
				return NULL;
			}
		}
	}
	return value;
}

xvalue xl_range_collect1(int stop)
{
	return __xlRangeCollectInternal(0, stop, (stop >= 0) ? 1 : -1);
}

xvalue xl_range_collect2(int start, int stop)
{
	return __xlRangeCollectInternal(start, stop, (start <= stop) ? 1 : -1);
}

xvalue xl_range_collect3(int start, int stop, int step)
{
	return __xlRangeCollectInternal(start, stop, step);
}

const char* xl_template_render(const char* sTemplate, xvalue value)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	size_t iPos;
	size_t iTextLen;

	if ( sTemplate == NULL ) {
		sTemplate = "";
	}
	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	iPos = 0u;
	iTextLen = __xlTextLen(sTemplate);
	while ( iPos < iTextLen ) {
		const char* sOpen;
		size_t iOpenPos;

		sOpen = strstr(sTemplate + iPos, "{{");
		if ( sOpen == NULL ) {
			if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sTemplate + iPos, iTextLen - iPos) ) {
				free(sBuf);
				__xlRaiseNativeError("TemplateError", "template render failed", 0, sTemplate);
				return "";
			}
			break;
		}
		iOpenPos = (size_t)(sOpen - sTemplate);
		if ( iOpenPos > iPos ) {
			if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sTemplate + iPos, iOpenPos - iPos) ) {
				free(sBuf);
				__xlRaiseNativeError("TemplateError", "template render failed", 0, sTemplate);
				return "";
			}
		}
		{
			const char* sClose = strstr(sOpen + 2, "}}");
			if ( sClose == NULL ) {
				if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sOpen, iTextLen - iOpenPos) ) {
					free(sBuf);
					__xlRaiseNativeError("TemplateError", "template render failed", 0, sTemplate);
					return "";
				}
				break;
			}
			{
				size_t iKeyLen = 0u;
				const char* sKey = __xlTemplateTrimKey(sOpen + 2, (size_t)(sClose - (sOpen + 2)), &iKeyLen);
				const char* sValue = "";
				if ( (value != NULL) && (iKeyLen != 0u) ) {
					sValue = (const char*)xvoTableGetText(value, (str)sKey, (uint32)iKeyLen);
					if ( sValue == NULL ) {
						sValue = "";
					}
				}
				if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sValue, __xlTextLen(sValue)) ) {
					free(sBuf);
					__xlRaiseNativeError("TemplateError", "template render failed", 0, sTemplate);
					return "";
				}
			}
			iPos = (size_t)((sClose - sTemplate) + 2u);
		}
	}
	return (sBuf != NULL) ? sBuf : "";
}

const char* xl_point_to_string(void* value)
{
	char sBuf[64];
	int iLen;

	iLen = snprintf(sBuf, sizeof(sBuf), "[point:%p]", value);
	if ( iLen < 0 ) {
		return "";
	}
	return XLRT_CSTR(xrtCopyStr((str)sBuf, (size_t)iLen));
}

const char* xl_value_to_string(xvalue value)
{
	char sBuf[64];
	const char* sText;
	int iLen;

	if ( value == NULL ) {
		return "";
	}

	switch ( value->Type ) {
		case XVO_DT_TEXT:
			sText = (const char*)xvoGetText(value);
			return XLRT_CSTR(xrtCopyStr((str)((sText != NULL) ? sText : ""), (size_t)xvoGetSize(value)));
		case XVO_DT_BOOL:
			sText = xvoGetBool(value) ? "true" : "false";
			return XLRT_CSTR(xrtCopyStr((str)sText, __xlTextLen(sText)));
		case XVO_DT_INT:
			iLen = xrtI64ToStr(xvoGetInt(value), sBuf);
			if ( iLen < 0 ) {
				return "";
			}
			return XLRT_CSTR(xrtCopyStr((str)sBuf, (size_t)iLen));
		case XVO_DT_FLOAT:
			iLen = xrtNumToStr(xvoGetFloat(value), sBuf);
			if ( iLen < 0 ) {
				return "";
			}
			return XLRT_CSTR(xrtCopyStr((str)sBuf, (size_t)iLen));
		case XVO_DT_TIME:
			sText = (const char*)xrtTimeToStr(xvoGetTime(value), XRT_TIME_FORMAT_DATETIME);
			return XLRT_CSTR(xrtCopyStr((str)((sText != NULL) ? sText : ""), __xlTextLen(sText)));
		case XVO_DT_POINT:
			return xl_point_to_string(xvoGetPoint(value));
		default:
			return XLRT_CSTR(xvoGetText(value));
	}
}

static int __xlAppendListText(xvalue value, const char* sText, size_t iLen)
{
	const char* sSlice;
	if ( value == NULL ) {
		return 0;
	}
	sSlice = (iLen == 0u) ? "" : sText;
	return xvoListSetValue(value, (int64)xvoListItemCount(value), xvoCreateText((str)sSlice, iLen, FALSE), TRUE) ? 1 : 0;
}

static int __xlTextBufGrow(char** psBuf, size_t* piCap, size_t iNeed)
{
	char* sNewBuf;
	size_t iNewCap;
	if ( (psBuf == NULL) || (piCap == NULL) ) {
		return 0;
	}
	if ( iNeed <= *piCap ) {
		return 1;
	}
	iNewCap = (*piCap == 0u) ? 32u : *piCap;
	while ( iNewCap < iNeed ) {
		iNewCap *= 2u;
	}
	sNewBuf = (char*)realloc(*psBuf, iNewCap);
	if ( sNewBuf == NULL ) {
		return 0;
	}
	*psBuf = sNewBuf;
	*piCap = iNewCap;
	return 1;
}

static int __xlTextBufAppend(char** psBuf, size_t* piLen, size_t* piCap, const char* sText, size_t iTextLen)
{
	if ( (psBuf == NULL) || (piLen == NULL) || (piCap == NULL) ) {
		return 0;
	}
	if ( !__xlTextBufGrow(psBuf, piCap, *piLen + iTextLen + 1u) ) {
		return 0;
	}
	if ( (sText != NULL) && (iTextLen != 0u) ) {
		memcpy(*psBuf + *piLen, sText, iTextLen);
	}
	*piLen += iTextLen;
	(*psBuf)[*piLen] = '\0';
	return 1;
}

static xregex* __xlRegexCreateOrRaise(const char* sPattern)
{
	xregex* pRegex;

	if ( sPattern == NULL ) {
		sPattern = "";
	}
	pRegex = xrtRegexCreate(sPattern);
	if ( pRegex == NULL ) {
		__xlRaiseNativeError("RegexError", (const char*)xrtGetError(), 0, sPattern);
		return NULL;
	}
	return pRegex;
}

static const char* __xlRegexSliceText(const char* sText, size_t iTextLen, xregexspan tSpan)
{
	if ( sText == NULL ) {
		sText = "";
	}
	if ( (tSpan.iEnd < tSpan.iBegin) || (tSpan.iBegin > iTextLen) || (tSpan.iEnd > iTextLen) ) {
		return "";
	}
	return XLRT_CSTR(xrtCopyStr((str)(sText + tSpan.iBegin), tSpan.iEnd - tSpan.iBegin));
}

static int __xlTextBufAppendChar(char** psBuf, size_t* piLen, size_t* piCap, char ch)
{
	return __xlTextBufAppend(psBuf, piLen, piCap, &ch, 1u);
}

static int __xlJsonEncodeString(char** psBuf, size_t* piLen, size_t* piCap, const char* sText, size_t iTextLen)
{
	size_t i;
	char sHex[7];

	if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, '"') ) {
		return 0;
	}
	for ( i = 0; i < iTextLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];

		switch ( ch ) {
			case '\\':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\\\", 2u) ) return 0;
				break;
			case '"':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\\"", 2u) ) return 0;
				break;
			case '\b':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\b", 2u) ) return 0;
				break;
			case '\f':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\f", 2u) ) return 0;
				break;
			case '\n':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\n", 2u) ) return 0;
				break;
			case '\r':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\r", 2u) ) return 0;
				break;
			case '\t':
				if ( !__xlTextBufAppend(psBuf, piLen, piCap, "\\t", 2u) ) return 0;
				break;
			default:
				if ( ch < 0x20u ) {
					snprintf(sHex, sizeof(sHex), "\\u%04x", (unsigned int)ch);
					if ( !__xlTextBufAppend(psBuf, piLen, piCap, sHex, 6u) ) return 0;
				} else {
					if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, (char)ch) ) return 0;
				}
				break;
		}
	}
	return __xlTextBufAppendChar(psBuf, piLen, piCap, '"');
}

static int __xlJsonEncodeDict(char** psBuf, size_t* piLen, size_t* piCap, xvalue value)
{
	xdict pTable;
	Dict_Key* pKey;
	int bFirst;

	if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, '{') ) {
		return 0;
	}
	pTable = xvoGetTable(value);
	bFirst = 1;
	if ( pTable != NULL ) {
		xrtAVLTB_IterBegin((xavltbase)pTable);
		for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
			xvalue itemValue = *(xvalue*)(&pKey[1]);

			if ( !bFirst && !__xlTextBufAppendChar(psBuf, piLen, piCap, ',') ) {
				xrtAVLTB_IterEnd((xavltbase)pTable);
				return 0;
			}
			if ( !__xlJsonEncodeString(psBuf, piLen, piCap, pKey->Key, pKey->KeyLen) ||
				!__xlTextBufAppendChar(psBuf, piLen, piCap, ':') ||
				!__xlJsonEncodeValue(psBuf, piLen, piCap, itemValue) ) {
				xrtAVLTB_IterEnd((xavltbase)pTable);
				return 0;
			}
			bFirst = 0;
		}
		xrtAVLTB_IterEnd((xavltbase)pTable);
	}
	return __xlTextBufAppendChar(psBuf, piLen, piCap, '}');
}

static int __xlJsonEncodeSet(char** psBuf, size_t* piLen, size_t* piCap, xvalue value)
{
	xavltree pTree;
	Coll_Key* pKey;
	int bFirst;

	if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, '[') ) {
		return 0;
	}
	pTree = xvoGetColl(value);
	bFirst = 1;
	if ( pTree != NULL ) {
		xrtAVLTreeIterBegin(pTree);
		for ( pKey = (Coll_Key*)xrtAVLTreeIterNext(pTree); pKey != NULL; pKey = (Coll_Key*)xrtAVLTreeIterNext(pTree) ) {
			if ( !bFirst && !__xlTextBufAppendChar(psBuf, piLen, piCap, ',') ) {
				xrtAVLTreeIterEnd(pTree);
				return 0;
			}
			if ( !__xlJsonEncodeValue(psBuf, piLen, piCap, pKey->Value) ) {
				xrtAVLTreeIterEnd(pTree);
				return 0;
			}
			bFirst = 0;
		}
		xrtAVLTreeIterEnd(pTree);
	}
	return __xlTextBufAppendChar(psBuf, piLen, piCap, ']');
}

static int __xlJsonEncodeArrayLike(char** psBuf, size_t* piLen, size_t* piCap, xvalue value, int bList)
{
	size_t i;
	size_t iCount;

	if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, '[') ) {
		return 0;
	}
	iCount = bList ? (size_t)xvoListItemCount(value) : (size_t)xvoArrayItemCount(value);
	for ( i = 0; i < iCount; ++i ) {
		xvalue item = bList ? xvoListGetValue(value, (int64)i) : xvoArrayGetValue(value, (uint32)i);

		if ( (i != 0u) && !__xlTextBufAppendChar(psBuf, piLen, piCap, ',') ) {
			return 0;
		}
		if ( !__xlJsonEncodeValue(psBuf, piLen, piCap, item) ) {
			return 0;
		}
	}
	return __xlTextBufAppendChar(psBuf, piLen, piCap, ']');
}

static int __xlJsonEncodeValue(char** psBuf, size_t* piLen, size_t* piCap, xvalue value)
{
	char sBuf[64];
	const char* sText;
	int iLen;

	if ( (value == NULL) || (value->Type == XVO_DT_NULL) ) {
		return __xlTextBufAppend(psBuf, piLen, piCap, "null", 4u);
	}

	switch ( value->Type ) {
		case XVO_DT_BOOL:
			return __xlTextBufAppend(psBuf, piLen, piCap, xvoGetBool(value) ? "true" : "false", xvoGetBool(value) ? 4u : 5u);
		case XVO_DT_INT:
			iLen = xrtI64ToStr(xvoGetInt(value), sBuf);
			return (iLen >= 0) ? __xlTextBufAppend(psBuf, piLen, piCap, sBuf, (size_t)iLen) : 0;
		case XVO_DT_FLOAT:
			iLen = xrtNumToStr(xvoGetFloat(value), sBuf);
			return (iLen >= 0) ? __xlTextBufAppend(psBuf, piLen, piCap, sBuf, (size_t)iLen) : 0;
		case XVO_DT_TEXT:
			sText = (const char*)xvoGetText(value);
			return __xlJsonEncodeString(psBuf, piLen, piCap, (sText != NULL) ? sText : "", (size_t)xvoGetSize(value));
		case XVO_DT_ARRAY:
			return __xlJsonEncodeArrayLike(psBuf, piLen, piCap, value, 0);
		case XVO_DT_LIST:
			return __xlJsonEncodeArrayLike(psBuf, piLen, piCap, value, 1);
		case XVO_DT_COLL:
			return __xlJsonEncodeSet(psBuf, piLen, piCap, value);
		case XVO_DT_TABLE:
			return __xlJsonEncodeDict(psBuf, piLen, piCap, value);
		default:
			sText = xl_value_to_string(value);
			return __xlJsonEncodeString(psBuf, piLen, piCap, (sText != NULL) ? sText : "", __xlTextLen(sText));
	}
}

static void __xlJsonSetError(XL_JsonParseCtx* pCtx, const char* sError)
{
	if ( (pCtx != NULL) && (pCtx->sError == NULL) ) {
		pCtx->sError = sError;
	}
}

static void __xlJsonSkipSpace(XL_JsonParseCtx* pCtx)
{
	while ( (pCtx != NULL) && (pCtx->iPos < pCtx->iLen) ) {
		unsigned char ch = (unsigned char)pCtx->sText[pCtx->iPos];

		if ( (ch != ' ') && (ch != '\t') && (ch != '\r') && (ch != '\n') ) {
			break;
		}
		pCtx->iPos += 1u;
	}
}

static int __xlJsonMatchLiteral(XL_JsonParseCtx* pCtx, const char* sText)
{
	size_t iLen;

	if ( (pCtx == NULL) || (sText == NULL) ) {
		return 0;
	}
	iLen = __xlTextLen(sText);
	if ( ((pCtx->iPos + iLen) > pCtx->iLen) || (memcmp(pCtx->sText + pCtx->iPos, sText, iLen) != 0) ) {
		return 0;
	}
	pCtx->iPos += iLen;
	return 1;
}

static xvalue __xlJsonParseStringValue(XL_JsonParseCtx* pCtx)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;

	if ( (pCtx == NULL) || (pCtx->iPos >= pCtx->iLen) || (pCtx->sText[pCtx->iPos] != '"') ) {
		__xlJsonSetError(pCtx, "expected string");
		return NULL;
	}

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	pCtx->iPos += 1u;
	while ( pCtx->iPos < pCtx->iLen ) {
		unsigned char ch = (unsigned char)pCtx->sText[pCtx->iPos++];

		if ( ch == '"' ) {
			xvalue value = xvoCreateText((str)((sBuf != NULL) ? sBuf : ""), iLen, FALSE);
			free(sBuf);
			return value;
		}
		if ( ch == '\\' ) {
			unsigned char esc;

			if ( pCtx->iPos >= pCtx->iLen ) {
				free(sBuf);
				__xlJsonSetError(pCtx, "invalid escape");
				return NULL;
			}
			esc = (unsigned char)pCtx->sText[pCtx->iPos++];
			switch ( esc ) {
				case '"': ch = '"'; break;
				case '\\': ch = '\\'; break;
				case '/': ch = '/'; break;
				case 'b': ch = '\b'; break;
				case 'f': ch = '\f'; break;
				case 'n': ch = '\n'; break;
				case 'r': ch = '\r'; break;
				case 't': ch = '\t'; break;
				case 'u':
				{
					unsigned int iCode = 0u;
					int i;

					if ( (pCtx->iPos + 4u) > pCtx->iLen ) {
						free(sBuf);
						__xlJsonSetError(pCtx, "invalid unicode escape");
						return NULL;
					}
					for ( i = 0; i < 4; ++i ) {
						unsigned char hex = (unsigned char)pCtx->sText[pCtx->iPos++];

						iCode <<= 4u;
						if ( (hex >= '0') && (hex <= '9') ) iCode |= (unsigned int)(hex - '0');
						else if ( (hex >= 'a') && (hex <= 'f') ) iCode |= (unsigned int)(hex - 'a' + 10u);
						else if ( (hex >= 'A') && (hex <= 'F') ) iCode |= (unsigned int)(hex - 'A' + 10u);
						else {
							free(sBuf);
							__xlJsonSetError(pCtx, "invalid unicode escape");
							return NULL;
						}
					}
					ch = (iCode <= 0x7Fu) ? (unsigned char)iCode : (unsigned char)'?';
					break;
				}
				default:
					free(sBuf);
					__xlJsonSetError(pCtx, "invalid escape");
					return NULL;
			}
		}
		if ( !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, (char)ch) ) {
			free(sBuf);
			__xlJsonSetError(pCtx, "out of memory");
			return NULL;
		}
	}

	free(sBuf);
	__xlJsonSetError(pCtx, "unterminated string");
	return NULL;
}

static xvalue __xlJsonParseNumberValue(XL_JsonParseCtx* pCtx)
{
	size_t iStart;
	size_t iLen;
	char* sTmp;
	char* pEnd;
	xvalue value;

	iStart = (pCtx != NULL) ? pCtx->iPos : 0u;
	if ( (pCtx == NULL) || (iStart >= pCtx->iLen) ) {
		__xlJsonSetError(pCtx, "expected number");
		return NULL;
	}

	if ( pCtx->sText[pCtx->iPos] == '-' ) {
		pCtx->iPos += 1u;
	}
	while ( (pCtx->iPos < pCtx->iLen) && (pCtx->sText[pCtx->iPos] >= '0') && (pCtx->sText[pCtx->iPos] <= '9') ) {
		pCtx->iPos += 1u;
	}
	if ( (pCtx->iPos < pCtx->iLen) && (pCtx->sText[pCtx->iPos] == '.') ) {
		pCtx->iPos += 1u;
		while ( (pCtx->iPos < pCtx->iLen) && (pCtx->sText[pCtx->iPos] >= '0') && (pCtx->sText[pCtx->iPos] <= '9') ) {
			pCtx->iPos += 1u;
		}
	}
	if ( (pCtx->iPos < pCtx->iLen) && ((pCtx->sText[pCtx->iPos] == 'e') || (pCtx->sText[pCtx->iPos] == 'E')) ) {
		pCtx->iPos += 1u;
		if ( (pCtx->iPos < pCtx->iLen) && ((pCtx->sText[pCtx->iPos] == '+') || (pCtx->sText[pCtx->iPos] == '-')) ) {
			pCtx->iPos += 1u;
		}
		while ( (pCtx->iPos < pCtx->iLen) && (pCtx->sText[pCtx->iPos] >= '0') && (pCtx->sText[pCtx->iPos] <= '9') ) {
			pCtx->iPos += 1u;
		}
	}

	iLen = pCtx->iPos - iStart;
	if ( iLen == 0u ) {
		__xlJsonSetError(pCtx, "expected number");
		return NULL;
	}

	sTmp = (char*)malloc(iLen + 1u);
	if ( sTmp == NULL ) {
		__xlJsonSetError(pCtx, "out of memory");
		return NULL;
	}
	memcpy(sTmp, pCtx->sText + iStart, iLen);
	sTmp[iLen] = '\0';

	if ( (strchr(sTmp, '.') != NULL) || (strchr(sTmp, 'e') != NULL) || (strchr(sTmp, 'E') != NULL) ) {
		double fValue = strtod(sTmp, &pEnd);
		free(sTmp);
		if ( (pEnd == NULL) || (*pEnd != '\0') ) {
			__xlJsonSetError(pCtx, "invalid number");
			return NULL;
		}
		value = xvoCreateFloat(fValue);
	} else {
		int64 iValue = (int64)strtoll(sTmp, &pEnd, 10);
		free(sTmp);
		if ( (pEnd == NULL) || (*pEnd != '\0') ) {
			__xlJsonSetError(pCtx, "invalid number");
			return NULL;
		}
		value = xvoCreateInt(iValue);
	}
	return value;
}

static xvalue __xlJsonParseArray(XL_JsonParseCtx* pCtx)
{
	xvalue value;

	value = xvoCreateArray();
	if ( value == NULL ) {
		__xlJsonSetError(pCtx, "out of memory");
		return NULL;
	}
	pCtx->iPos += 1u;
	__xlJsonSkipSpace(pCtx);
	if ( (pCtx->iPos < pCtx->iLen) && (pCtx->sText[pCtx->iPos] == ']') ) {
		pCtx->iPos += 1u;
		return value;
	}
	for (;;) {
		xvalue item;

		__xlJsonSkipSpace(pCtx);
		item = __xlJsonParseValue(pCtx);
		if ( item == NULL ) {
			xvoUnref(value);
			return NULL;
		}
		if ( !xvoArrayAppendValue(value, item, TRUE) ) {
			xvoUnref(item);
			xvoUnref(value);
			__xlJsonSetError(pCtx, "out of memory");
			return NULL;
		}
		__xlJsonSkipSpace(pCtx);
		if ( pCtx->iPos >= pCtx->iLen ) {
			xvoUnref(value);
			__xlJsonSetError(pCtx, "unterminated array");
			return NULL;
		}
		if ( pCtx->sText[pCtx->iPos] == ']' ) {
			pCtx->iPos += 1u;
			return value;
		}
		if ( pCtx->sText[pCtx->iPos] != ',' ) {
			xvoUnref(value);
			__xlJsonSetError(pCtx, "expected ','");
			return NULL;
		}
		pCtx->iPos += 1u;
	}
}

static xvalue __xlJsonParseObject(XL_JsonParseCtx* pCtx)
{
	xvalue value;

	value = xvoCreateTable();
	if ( value == NULL ) {
		__xlJsonSetError(pCtx, "out of memory");
		return NULL;
	}
	pCtx->iPos += 1u;
	__xlJsonSkipSpace(pCtx);
	if ( (pCtx->iPos < pCtx->iLen) && (pCtx->sText[pCtx->iPos] == '}') ) {
		pCtx->iPos += 1u;
		return value;
	}
	for (;;) {
		xvalue keyValue;
		const char* sKey;
		size_t iKeyLen;
		xvalue item;

		__xlJsonSkipSpace(pCtx);
		keyValue = __xlJsonParseStringValue(pCtx);
		if ( keyValue == NULL ) {
			xvoUnref(value);
			return NULL;
		}
		sKey = (const char*)xvoGetText(keyValue);
		iKeyLen = (size_t)xvoGetSize(keyValue);
		__xlJsonSkipSpace(pCtx);
		if ( (pCtx->iPos >= pCtx->iLen) || (pCtx->sText[pCtx->iPos] != ':') ) {
			xvoUnref(keyValue);
			xvoUnref(value);
			__xlJsonSetError(pCtx, "expected ':'");
			return NULL;
		}
		pCtx->iPos += 1u;
		__xlJsonSkipSpace(pCtx);
		item = __xlJsonParseValue(pCtx);
		if ( item == NULL ) {
			xvoUnref(keyValue);
			xvoUnref(value);
			return NULL;
		}
		if ( !xvoTableSetValue(value, (str)((sKey != NULL) ? sKey : ""), (uint32)iKeyLen, item, TRUE) ) {
			xvoUnref(item);
			xvoUnref(keyValue);
			xvoUnref(value);
			__xlJsonSetError(pCtx, "out of memory");
			return NULL;
		}
		xvoUnref(keyValue);
		__xlJsonSkipSpace(pCtx);
		if ( pCtx->iPos >= pCtx->iLen ) {
			xvoUnref(value);
			__xlJsonSetError(pCtx, "unterminated object");
			return NULL;
		}
		if ( pCtx->sText[pCtx->iPos] == '}' ) {
			pCtx->iPos += 1u;
			return value;
		}
		if ( pCtx->sText[pCtx->iPos] != ',' ) {
			xvoUnref(value);
			__xlJsonSetError(pCtx, "expected ','");
			return NULL;
		}
		pCtx->iPos += 1u;
	}
}

static xvalue __xlJsonParseValue(XL_JsonParseCtx* pCtx)
{
	__xlJsonSkipSpace(pCtx);
	if ( (pCtx == NULL) || (pCtx->iPos >= pCtx->iLen) ) {
		__xlJsonSetError(pCtx, "unexpected end of json");
		return NULL;
	}

	switch ( pCtx->sText[pCtx->iPos] ) {
		case '{':
			return __xlJsonParseObject(pCtx);
		case '[':
			return __xlJsonParseArray(pCtx);
		case '"':
			return __xlJsonParseStringValue(pCtx);
		case 't':
			if ( __xlJsonMatchLiteral(pCtx, "true") ) return xvoCreateBool(TRUE);
			break;
		case 'f':
			if ( __xlJsonMatchLiteral(pCtx, "false") ) return xvoCreateBool(FALSE);
			break;
		case 'n':
			if ( __xlJsonMatchLiteral(pCtx, "null") ) return xvoCreateNull();
			break;
		default:
			if ( (pCtx->sText[pCtx->iPos] == '-') || ((pCtx->sText[pCtx->iPos] >= '0') && (pCtx->sText[pCtx->iPos] <= '9')) ) {
				return __xlJsonParseNumberValue(pCtx);
			}
			break;
	}

	__xlJsonSetError(pCtx, "invalid json value");
	return NULL;
}

static int __xlQueryEncodeText(char** psBuf, size_t* piLen, size_t* piCap, const char* sText, size_t iTextLen)
{
	static const char sHex[] = "0123456789ABCDEF";
	size_t i;

	for ( i = 0; i < iTextLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		char sTmp[3];

		if ( ((ch >= 'A') && (ch <= 'Z')) ||
			((ch >= 'a') && (ch <= 'z')) ||
			((ch >= '0') && (ch <= '9')) ||
			(ch == '-') || (ch == '_') || (ch == '.') || (ch == '~') ) {
			if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, (char)ch) ) {
				return 0;
			}
		} else if ( ch == ' ' ) {
			if ( !__xlTextBufAppendChar(psBuf, piLen, piCap, '+') ) {
				return 0;
			}
		} else {
			sTmp[0] = '%';
			sTmp[1] = sHex[(ch >> 4) & 0x0F];
			sTmp[2] = sHex[ch & 0x0F];
			if ( !__xlTextBufAppend(psBuf, piLen, piCap, sTmp, 3u) ) {
				return 0;
			}
		}
	}
	return 1;
}

static const char* __xlQueryDecodeText(const char* sText, size_t iTextLen)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	size_t i;

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	if ( sText == NULL ) {
		sText = "";
		iTextLen = 0u;
	}
	for ( i = 0; i < iTextLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];

		if ( ch == '+' ) {
			if ( !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, ' ') ) {
				free(sBuf);
				return "";
			}
		} else if ( (ch == '%') && ((i + 2u) < iTextLen) ) {
			unsigned char hi = (unsigned char)sText[i + 1u];
			unsigned char lo = (unsigned char)sText[i + 2u];
			unsigned int iValue = 0u;

			if ( (hi >= '0') && (hi <= '9') ) iValue = (unsigned int)(hi - '0') << 4u;
			else if ( (hi >= 'A') && (hi <= 'F') ) iValue = (unsigned int)(hi - 'A' + 10u) << 4u;
			else if ( (hi >= 'a') && (hi <= 'f') ) iValue = (unsigned int)(hi - 'a' + 10u) << 4u;
			else {
				if ( !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, '%') ) {
					free(sBuf);
					return "";
				}
				continue;
			}

			if ( (lo >= '0') && (lo <= '9') ) iValue |= (unsigned int)(lo - '0');
			else if ( (lo >= 'A') && (lo <= 'F') ) iValue |= (unsigned int)(lo - 'A' + 10u);
			else if ( (lo >= 'a') && (lo <= 'f') ) iValue |= (unsigned int)(lo - 'a' + 10u);
			else {
				if ( !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, '%') ) {
					free(sBuf);
					return "";
				}
				continue;
			}
			if ( !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, (char)iValue) ) {
				free(sBuf);
				return "";
			}
			i += 2u;
		} else {
			if ( !__xlTextBufAppendChar(&sBuf, &iLen, &iCap, (char)ch) ) {
				free(sBuf);
				return "";
			}
		}
	}
	return (sBuf != NULL) ? sBuf : "";
}

static const char* __xlTemplateTrimKey(const char* sText, size_t iTextLen, size_t* piOutLen)
{
	size_t iStart;
	size_t iEnd;

	iStart = 0u;
	iEnd = iTextLen;
	while ( (iStart < iEnd) && ((sText[iStart] == ' ') || (sText[iStart] == '\t') || (sText[iStart] == '\r') || (sText[iStart] == '\n')) ) {
		iStart += 1u;
	}
	while ( (iEnd > iStart) && ((sText[iEnd - 1u] == ' ') || (sText[iEnd - 1u] == '\t') || (sText[iEnd - 1u] == '\r') || (sText[iEnd - 1u] == '\n')) ) {
		iEnd -= 1u;
	}
	if ( piOutLen != NULL ) {
		*piOutLen = iEnd - iStart;
	}
	return sText + iStart;
}

static const char* __xlRegexReplaceInternal(const char* sPattern, const char* sText, const char* sValue, int bReplaceAll)
{
	xregex* pRegex;
	size_t iTextLen;
	size_t iValueLen;
	size_t iPos;
	size_t iAppendPos;
	char* sBuf;
	size_t iLen;
	size_t iCap;
	xregexspan tSpan;
	int iRc;

	if ( sText == NULL ) {
		sText = "";
	}
	if ( sValue == NULL ) {
		sValue = "";
	}
	pRegex = __xlRegexCreateOrRaise(sPattern);
	if ( pRegex == NULL ) {
		return "";
	}

	iTextLen = __xlTextLen(sText);
	iValueLen = __xlTextLen(sValue);
	iPos = 0u;
	iAppendPos = 0u;
	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;

	for (;;) {
		iRc = xrtRegexFindAt(pRegex, sText, iTextLen, iPos, &tSpan);
		if ( iRc <= 0 ) {
			break;
		}
		if ( tSpan.iBegin < iAppendPos ) {
			break;
		}
		if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sText + iAppendPos, tSpan.iBegin - iAppendPos) ) {
			xrtRegexDestroy(pRegex);
			return (sBuf != NULL) ? sBuf : "";
		}
		if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sValue, iValueLen) ) {
			xrtRegexDestroy(pRegex);
			return (sBuf != NULL) ? sBuf : "";
		}
		iAppendPos = tSpan.iEnd;
		if ( !bReplaceAll ) {
			break;
		}
		if ( tSpan.iEnd == iPos ) {
			if ( iPos >= iTextLen ) {
				break;
			}
			if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sText + iPos, 1u) ) {
				xrtRegexDestroy(pRegex);
				return (sBuf != NULL) ? sBuf : "";
			}
			iPos += 1u;
			iAppendPos = iPos;
		} else {
			iPos = tSpan.iEnd;
		}
	}

	xrtRegexDestroy(pRegex);
	if ( sBuf == NULL ) {
		return XLRT_CSTR(xrtCopyStr((str)sText, iTextLen));
	}
	if ( iAppendPos < iTextLen ) {
		if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, sText + iAppendPos, iTextLen - iAppendPos) ) {
			return sBuf;
		}
	}
	return sBuf;
}

static const char* __xlJoinStrings(xvalue value, int bList, const char* sSep)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	size_t iCount;
	size_t iSepLen;
	size_t i;

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	iCount = bList ? (size_t)xvoListItemCount(value) : (size_t)xvoArrayItemCount(value);
	iSepLen = __xlTextLen(sSep);

	for ( i = 0u; i < iCount; ++i ) {
		xvalue item = bList ? xvoListGetValue(value, (int64)i) : xvoArrayGetValue(value, (uint32)i);
		const char* sItem = (const char*)xvoGetText(item);
		size_t iItemLen = (size_t)xvoGetSize(item);
		if ( (i != 0u) && !__xlTextBufAppend(&sBuf, &iLen, &iCap, sSep, iSepLen) ) {
			free(sBuf);
			return "";
		}
		if ( !__xlTextBufAppend(&sBuf, &iLen, &iCap, (sItem != NULL) ? sItem : "", (sItem != NULL) ? iItemLen : 0u) ) {
			free(sBuf);
			return "";
		}
	}

	if ( sBuf == NULL ) {
		sBuf = (char*)malloc(1u);
		if ( sBuf == NULL ) {
			return "";
		}
		sBuf[0] = '\0';
	}
	return sBuf;
}

void xl_core_print_int(int value) { printf("%d", value); }
void xl_core_print_float(double value) { printf("%g", value); }
void xl_core_print_string(const char* value) { __xlWriteText(value); }
void xl_core_print_bool(int value) { __xlWriteText(value ? "true" : "false"); }
void xl_core_println_void(void) { fputc('\n', stdout); }
void xl_core_println_int(int value) { xl_core_print_int(value); xl_core_println_void(); }
void xl_core_println_float(double value) { xl_core_print_float(value); xl_core_println_void(); }
void xl_core_println_string(const char* value) { xl_core_print_string(value); xl_core_println_void(); }
void xl_core_println_bool(int value) { xl_core_print_bool(value); xl_core_println_void(); }
void xl_io_eprint_int(int value) { fprintf(stderr, "%d", value); }
void xl_io_eprint_float(double value) { fprintf(stderr, "%g", value); }
void xl_io_eprint_string(const char* value) { __xlWriteErrorText(value); }
void xl_io_eprint_bool(int value) { __xlWriteErrorText(value ? "true" : "false"); }
void xl_io_eprintln_void(void) { fputc('\n', stderr); }
void xl_io_eprintln_int(int value) { xl_io_eprint_int(value); xl_io_eprintln_void(); }
void xl_io_eprintln_float(double value) { xl_io_eprint_float(value); xl_io_eprintln_void(); }
void xl_io_eprintln_string(const char* value) { xl_io_eprint_string(value); xl_io_eprintln_void(); }
void xl_io_eprintln_bool(int value) { xl_io_eprint_bool(value); xl_io_eprintln_void(); }
const char* xl_io_read_line(void)
{
	char sChunk[256];
	char* sBuf;
	size_t iLen;
	size_t iCap;
	size_t iChunkLen;
	char* sNewBuf;

	if ( fgets(sChunk, (int)sizeof(sChunk), stdin) == NULL ) {
		return "";
	}
	iChunkLen = strlen(sChunk);
	iCap = iChunkLen + 1u;
	sBuf = (char*)malloc(iCap);
	if ( sBuf == NULL ) {
		return "";
	}
	memcpy(sBuf, sChunk, iChunkLen + 1u);
	iLen = iChunkLen;
	while ( (iLen != 0u) && (sBuf[iLen - 1u] != '\n') && !feof(stdin) ) {
		if ( fgets(sChunk, (int)sizeof(sChunk), stdin) == NULL ) {
			break;
		}
		iChunkLen = strlen(sChunk);
		sNewBuf = (char*)realloc(sBuf, iLen + iChunkLen + 1u);
		if ( sNewBuf == NULL ) {
			free(sBuf);
			return "";
		}
		sBuf = sNewBuf;
		memcpy(sBuf + iLen, sChunk, iChunkLen + 1u);
		iLen += iChunkLen;
	}
	while ( (iLen != 0u) && ((sBuf[iLen - 1u] == '\n') || (sBuf[iLen - 1u] == '\r')) ) {
		--iLen;
		sBuf[iLen] = '\0';
	}
	return sBuf;
}
void xl_io_flush(void) { fflush(stdout); fflush(stderr); }
int xl_core_len_string(const char* value) { return (int)__xlTextLen(value); }
int xl_core_len_array(xvalue value) { return (int)xvoArrayItemCount(value); }
int xl_core_len_list(xvalue value) { return (int)xvoListItemCount(value); }
int xl_core_len_set(xvalue value) { return (int)xvoCollItemCount(value); }
int xl_core_len_dict(xvalue value) { return (int)xvoTableItemCount(value); }
void xl_array_append(xvalue value, xvalue item) { (void)xvoArrayAppendValue(value, item, TRUE); }
void xl_list_append(xvalue value, xvalue item) { (void)xvoListSetValue(value, (int64)xvoListItemCount(value), item, TRUE); }
const char* xl_array_join(xvalue value, const char* sSep) { return __xlJoinStrings(value, 0, (sSep != NULL) ? sSep : ""); }
const char* xl_list_join(xvalue value, const char* sSep) { return __xlJoinStrings(value, 1, (sSep != NULL) ? sSep : ""); }
void xl_array_clear(xvalue value) { (void)xvoArrayClear(value); }
void xl_list_clear(xvalue value) { (void)xvoListClear(value); }
void xl_set_clear(xvalue value) { (void)xvoCollClear(value); }
void xl_dict_clear(xvalue value) { (void)xvoTableClear(value); }
int xl_array_remove(xvalue value, int index)
{
	if ( index < 0 ) {
		return 0;
	}
	return xvoArrayRemove(value, (uint32)index, 1u) ? 1 : 0;
}
int xl_list_remove(xvalue value, int index)
{
	uint32 iCount;
	uint32 i;
	if ( index < 0 ) {
		return 0;
	}
	iCount = xvoListItemCount(value);
	if ( (uint32)index >= iCount ) {
		return 0;
	}
	for ( i = (uint32)index; (i + 1u) < iCount; ++i ) {
		/* Shift existing values left while preserving their refcounts. */
		if ( !xvoListSetValue(value, (int64)i, xvoListGetValue(value, (int64)(i + 1u)), FALSE) ) {
			return 0;
		}
	}
	return xvoListRemove(value, (int64)(iCount - 1u)) ? 1 : 0;
}
int xl_set_remove(xvalue value, xvalue item) { return xvoCollRemove(value, item) ? 1 : 0; }
int xl_dict_remove(xvalue value, const char* key)
{
	return xvoTableRemove(value, (str)key, (uint32)__xlTextLen(key)) ? 1 : 0;
}
int xl_set_has(xvalue value, xvalue item) { return xvoCollExists(value, item) ? 1 : 0; }
int xl_dict_has(xvalue value, const char* key)
{
	return xvoTableExists(value, (str)key, (uint32)__xlTextLen(key)) ? 1 : 0;
}
xvalue xl_iter_keys(xvalue value)
{
	xvalue result;
	xdict pTable;
	Dict_Key* pKey;

	result = xvoCreateArray();
	if ( result == NULL ) {
		return NULL;
	}
	pTable = xvoGetTable(value);
	if ( pTable == NULL ) {
		return result;
	}

	xrtAVLTB_IterBegin((xavltbase)pTable);
	for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
		xvalue keyValue = xvoCreateText((str)pKey->Key, (uint32)pKey->KeyLen, FALSE);

		if ( (keyValue == NULL) || !xvoArrayAppendValue(result, keyValue, TRUE) ) {
			if ( keyValue != NULL ) {
				xvoUnref(keyValue);
			}
			xrtAVLTB_IterEnd((xavltbase)pTable);
			xvoUnref(result);
			return NULL;
		}
	}
	xrtAVLTB_IterEnd((xavltbase)pTable);
	return result;
}

xvalue xl_iter_values(xvalue value)
{
	xvalue result;
	xdict pTable;
	Dict_Key* pKey;

	result = xvoCreateArray();
	if ( result == NULL ) {
		return NULL;
	}
	pTable = xvoGetTable(value);
	if ( pTable == NULL ) {
		return result;
	}

	xrtAVLTB_IterBegin((xavltbase)pTable);
	for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
		xvalue itemValue = *(xvalue*)(&pKey[1]);

		if ( !xvoArrayAppendValue(result, itemValue, TRUE) ) {
			xrtAVLTB_IterEnd((xavltbase)pTable);
			xvoUnref(result);
			return NULL;
		}
	}
	xrtAVLTB_IterEnd((xavltbase)pTable);
	return result;
}

xvalue xl_iter_items(xvalue value)
{
	xvalue result;
	xavltree pTree;
	Coll_Key* pKey;

	result = xvoCreateArray();
	if ( result == NULL ) {
		return NULL;
	}
	pTree = xvoGetColl(value);
	if ( pTree == NULL ) {
		return result;
	}

	xrtAVLTreeIterBegin(pTree);
	for ( pKey = (Coll_Key*)xrtAVLTreeIterNext(pTree); pKey != NULL; pKey = (Coll_Key*)xrtAVLTreeIterNext(pTree) ) {
		if ( !xvoArrayAppendValue(result, pKey->Value, TRUE) ) {
			xrtAVLTreeIterEnd(pTree);
			xvoUnref(result);
			return NULL;
		}
	}
	xrtAVLTreeIterEnd(pTree);
	return result;
}

xtime xl_time_now(void) { return xrtNow(); }
const char* xl_time_to_string(xtime value) { return XLRT_CSTR(xrtTimeToStr(value, XRT_TIME_FORMAT_DATETIME)); }
const char* xl_time_now_text(void) { return XLRT_CSTR(xrtNowStr()); }
const char* xl_time_date_text(void) { return XLRT_CSTR(xrtDateStr()); }
const char* xl_time_time_text(void) { return XLRT_CSTR(xrtTimeStr()); }
const char* xl_time_relative(xtime value, xtime base) { return XLRT_CSTR(xrtRelativeTime(value, base)); }
xtime xl_time_parse(const char* sText) { return xrtStrToTime((str)sText, __xlTextLen(sText)); }
const char* xl_time_format(xtime value, const char* sPattern) { return XLRT_CSTR(xrtTimeFormat(value, (str)sPattern)); }
xtime xl_time_parse_format(const char* sText, const char* sPattern) { return xrtTimeParse((str)sText, (str)sPattern); }
void xl_time_sleep(int iMs) { xrtSleep((uint32)((iMs > 0) ? iMs : 0)); }
int xl_time_is_leap_year(int iYear) { return xrtIsLeapYear(iYear) ? 1 : 0; }

const char* xl_path_join(const char* sLeft, const char* sRight) { return XLRT_CSTR(xrtPathJoin(2, sLeft, sRight)); }
const char* xl_path_name_ext(const char* sPath) { return XLRT_CSTR(xrtPathGetNameExt((str)sPath, __xlTextLen(sPath))); }
const char* xl_path_name(const char* sPath) { return XLRT_CSTR(xrtPathGetName((str)sPath, __xlTextLen(sPath))); }
const char* xl_path_ext(const char* sPath) { return XLRT_CSTR(xrtPathGetExt((str)sPath, __xlTextLen(sPath))); }
const char* xl_path_dir(const char* sPath) { return XLRT_CSTR(xrtPathGetDir((str)sPath, __xlTextLen(sPath))); }
int xl_path_exists(const char* sPath) { return xrtPathExists((str)sPath) ? 1 : 0; }
int xl_path_is_abs(const char* sPath) { return xrtPathIsAbs((str)sPath, __xlTextLen(sPath)) ? 1 : 0; }
const char* xl_path_random(const char* sHead, const char* sFoot, int iLen)
{
	return XLRT_CSTR(xrtPathRandom((str)sHead, __xlTextLen(sHead), (str)sFoot, __xlTextLen(sFoot), (size_t)((iLen > 0) ? iLen : 0)));
}

int xl_fs_exists(const char* sPath) { return xrtFileExists((str)sPath) ? 1 : 0; }
int xl_fs_is_file(const char* sPath) { return xrtFileExists((str)sPath) ? 1 : 0; }
int xl_fs_is_dir(const char* sPath) { return xrtDirExists((str)sPath) ? 1 : 0; }
int xl_fs_size(const char* sPath) { return (int)xrtFileGetSize((str)sPath); }
int xl_fs_set_size(const char* sPath, int iSize)
{
	if ( (sPath == NULL) || (iSize < 0) || !xrtFileSetSize((str)sPath, (size_t)iSize) ) {
		__xlRaiseNativeError("IOError", "fs.set_size failed", 0, sPath);
		return 0;
	}
	return 1;
}
xtime xl_fs_access_time(const char* sPath) { return (xtime)xrtFileGetAccessTime((str)sPath); }
xtime xl_fs_change_time(const char* sPath) { return (xtime)xrtFileGetChangeTime((str)sPath); }
const char* xl_fs_read_all(const char* sPath)
{
	const char* sText = XLRT_CSTR(xrtFileReadAll((str)sPath, XRT_CP_UTF8, NULL));
	if ( sText == NULL ) {
		__xlRaiseNativeError("IOError", "fs.read_all failed", 0, sPath);
	}
	return (sText != NULL) ? sText : "";
}
int xl_fs_write_all(const char* sPath, const char* sText)
{
	size_t iSize;
	if ( (sPath == NULL) || (sText == NULL) ) {
		__xlRaiseNativeError("IOError", "fs.write_all failed", 0, sPath);
		return 0;
	}
	iSize = __xlTextLen(sText);
	if ( xrtFileWriteAll((str)sPath, (str)sText, iSize, XRT_CP_UTF8) != (int)iSize ) {
		__xlRaiseNativeError("IOError", "fs.write_all failed", 0, sPath);
		return 0;
	}
	return 1;
}
int xl_fs_append_all(const char* sPath, const char* sText)
{
	size_t iSize;
	if ( (sPath == NULL) || (sText == NULL) ) {
		__xlRaiseNativeError("IOError", "fs.append_all failed", 0, sPath);
		return 0;
	}
	iSize = __xlTextLen(sText);
	if ( xrtFileAppend((str)sPath, (str)sText, iSize, XRT_CP_UTF8) != (int)iSize ) {
		__xlRaiseNativeError("IOError", "fs.append_all failed", 0, sPath);
		return 0;
	}
	return 1;
}
int xl_fs_mkdir_all(const char* sPath)
{
	if ( (sPath == NULL) || !xrtDirCreateAll((str)sPath) ) {
		__xlRaiseNativeError("IOError", "fs.mkdir_all failed", 0, sPath);
		return 0;
	}
	return 1;
}
int xl_fs_copy(const char* sSrc, const char* sDst)
{
	if ( (sSrc == NULL) || (sDst == NULL) || !xrtFileCopy((str)sSrc, (str)sDst, true) ) {
		__xlRaiseNativeError("IOError", "fs.copy failed", 0, sSrc);
		return 0;
	}
	return 1;
}
int xl_fs_move(const char* sSrc, const char* sDst)
{
	if ( (sSrc == NULL) || (sDst == NULL) || !xrtFileMove((str)sSrc, (str)sDst, true) ) {
		__xlRaiseNativeError("IOError", "fs.move failed", 0, sSrc);
		return 0;
	}
	return 1;
}
int xl_fs_delete(const char* sPath)
{
	if ( (sPath == NULL) || !xrtFileDelete((str)sPath) ) {
		__xlRaiseNativeError("IOError", "fs.delete failed", 0, sPath);
		return 0;
	}
	return 1;
}

int xl_str_contains(const char* sText, const char* sPart)
{
	size_t iPartLen;
	if ( sPart == NULL ) return 0;
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0 ) return 1;
	if ( sText == NULL ) return 0;
	return (xrtFindStr((str)sText, __xlTextLen(sText), (str)sPart, iPartLen, FALSE) != NULL) ? 1 : 0;
}
int xl_str_empty(const char* sText)
{
	return ((sText == NULL) || (__xlTextLen(sText) == 0u)) ? 1 : 0;
}
int xl_str_byte_len(const char* sText)
{
	return (int)__xlTextLen((sText != NULL) ? sText : "");
}
int xl_str_index_of(const char* sText, const char* sPart)
{
	size_t iPartLen;
	str sFound;
	if ( sPart == NULL ) return -1;
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0u ) return 0;
	if ( sText == NULL ) return -1;
	sFound = xrtFindStr((str)sText, __xlTextLen(sText), (str)sPart, iPartLen, FALSE);
	if ( sFound == NULL ) {
		return -1;
	}
	return (int)(sFound - (str)sText);
}
int xl_str_last_index_of(const char* sText, const char* sPart)
{
	size_t iTextLen;
	size_t iPartLen;
	size_t i;
	if ( sText == NULL ) return -1;
	if ( sPart == NULL ) return -1;
	iTextLen = __xlTextLen(sText);
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0u ) return (int)iTextLen;
	if ( iPartLen > iTextLen ) return -1;
	for ( i = iTextLen - iPartLen + 1u; i > 0u; --i ) {
		size_t iPos = i - 1u;
		if ( xrtStrComp((str)(sText + iPos), (str)sPart, iPartLen, FALSE) == 0 ) {
			return (int)iPos;
		}
	}
	return -1;
}
const char* xl_str_slice(const char* sText, int iStart, int iCount)
{
	size_t iTextLen;
	size_t iOffset;
	size_t iTake;
	if ( sText == NULL ) {
		sText = "";
	}
	iTextLen = __xlTextLen(sText);
	if ( iStart < 0 ) {
		iStart = 0;
	}
	if ( iCount <= 0 ) {
		return "";
	}
	iOffset = (size_t)iStart;
	if ( iOffset >= iTextLen ) {
		return "";
	}
	iTake = (size_t)iCount;
	if ( iTake > (iTextLen - iOffset) ) {
		iTake = iTextLen - iOffset;
	}
	return XLRT_CSTR(xrtCopyStr((str)(sText + iOffset), iTake));
}
const char* xl_str_repeat(const char* sText, int iCount)
{
	char* sBuf;
	size_t iTextLen;
	size_t iCap;
	size_t iRepeat;
	size_t i;

	if ( sText == NULL ) {
		sText = "";
	}
	if ( iCount <= 0 ) {
		return "";
	}

	iTextLen = __xlTextLen(sText);
	iRepeat = (size_t)iCount;
	sBuf = NULL;
	iCap = 0u;
	if ( !__xlTextBufGrow(&sBuf, &iCap, (iTextLen * iRepeat) + 1u) ) {
		free(sBuf);
		return "";
	}
	iTextLen = 0u;
	for ( i = 0u; i < iRepeat; ++i ) {
		size_t iPieceLen = __xlTextLen(sText);
		memcpy(sBuf + iTextLen, sText, iPieceLen);
		iTextLen += iPieceLen;
	}
	sBuf[iTextLen] = '\0';
	return sBuf;
}
xvalue xl_str_split(const char* sText, const char* sPart)
{
	xvalue value;
	str* arrParts;
	size_t iCount;
	size_t i;

	value = xvoCreateList();
	if ( value == NULL ) {
		return NULL;
	}

	if ( sText == NULL ) {
		sText = "";
	}
	if ( sPart == NULL ) {
		sPart = "";
	}

	arrParts = xrtSplit((str)sText, __xlTextLen(sText), (str)sPart, __xlTextLen(sPart), FALSE, &iCount);
	if ( arrParts == NULL ) {
		(void)xvoListSetValue(value, 0, xvoCreateText((str)sText, __xlTextLen(sText), FALSE), TRUE);
		return value;
	}

	for ( i = 0; i < iCount; ++i ) {
		if ( !xvoListSetValue(value, (int64)i, xvoCreateText(arrParts[i], 0u, FALSE), TRUE) ) {
			xrtFree(arrParts);
			return value;
		}
	}

	xrtFree(arrParts);
	return value;
}
xvalue xl_str_split_lines(const char* sText)
{
	xvalue value;
	size_t iLen;
	size_t iStart;
	size_t i;

	value = xvoCreateList();
	if ( value == NULL ) {
		return NULL;
	}

	if ( sText == NULL ) {
		return value;
	}

	iLen = __xlTextLen(sText);
	iStart = 0u;
	for ( i = 0u; i < iLen; ++i ) {
		if ( (sText[i] != '\r') && (sText[i] != '\n') ) {
			continue;
		}
		if ( !__xlAppendListText(value, sText + iStart, i - iStart) ) {
			return value;
		}
		if ( (sText[i] == '\r') && ((i + 1u) < iLen) && (sText[i + 1u] == '\n') ) {
			i += 1u;
		}
		iStart = i + 1u;
	}
	if ( iStart < iLen ) {
		(void)__xlAppendListText(value, sText + iStart, iLen - iStart);
	}
	return value;
}
int xl_str_starts_with(const char* sText, const char* sPart)
{
	size_t iTextLen;
	size_t iPartLen;
	if ( sPart == NULL ) return 0;
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0 ) return 1;
	if ( sText == NULL ) return 0;
	iTextLen = __xlTextLen(sText);
	if ( iTextLen < iPartLen ) return 0;
	return (xrtStrComp((str)sText, (str)sPart, iPartLen, FALSE) == 0) ? 1 : 0;
}
int xl_str_ends_with(const char* sText, const char* sPart)
{
	size_t iTextLen;
	size_t iPartLen;
	if ( sPart == NULL ) return 0;
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0 ) return 1;
	if ( sText == NULL ) return 0;
	iTextLen = __xlTextLen(sText);
	if ( iTextLen < iPartLen ) return 0;
	return (xrtStrComp((str)(sText + iTextLen - iPartLen), (str)sPart, iPartLen, FALSE) == 0) ? 1 : 0;
}
const char* xl_str_concat(const char* sLeft, const char* sRight)
{
	return XLRT_CSTR(xrtFormat("%s%s", (sLeft != NULL) ? sLeft : "", (sRight != NULL) ? sRight : ""));
}
const char* xl_str_trim(const char* sText)
{
	static const char sTrimChars[] = " \t\r\n";
	if ( sText == NULL ) {
		sText = "";
	}
	return XLRT_CSTR(xrtTrim((str)sText, __xlTextLen(sText), (str)sTrimChars, sizeof(sTrimChars) - 1u, FALSE, NULL));
}
const char* xl_str_trim_left(const char* sText)
{
	static const char sTrimChars[] = " \t\r\n";
	if ( sText == NULL ) {
		sText = "";
	}
	return XLRT_CSTR(xrtLTrim((str)sText, __xlTextLen(sText), (str)sTrimChars, sizeof(sTrimChars) - 1u, FALSE, NULL));
}
const char* xl_str_trim_right(const char* sText)
{
	static const char sTrimChars[] = " \t\r\n";
	if ( sText == NULL ) {
		sText = "";
	}
	return XLRT_CSTR(xrtRTrim((str)sText, __xlTextLen(sText), (str)sTrimChars, sizeof(sTrimChars) - 1u, FALSE, NULL));
}
const char* xl_str_lower(const char* sText) { return XLRT_CSTR(xrtLCase((str)sText, __xlTextLen(sText), FALSE)); }
const char* xl_str_upper(const char* sText) { return XLRT_CSTR(xrtUCase((str)sText, __xlTextLen(sText), FALSE)); }
const char* xl_str_replace(const char* sText, const char* sPart, const char* sValue)
{
	return XLRT_CSTR(xrtReplace((str)sText, __xlTextLen(sText), (str)sPart, __xlTextLen(sPart), (str)sValue, __xlTextLen(sValue), NULL));
}
const char* xl_str_reverse(const char* sText)
{
	size_t iLen;
	char* sBuf;
	size_t i;

	if ( sText == NULL ) {
		return "";
	}
	iLen = __xlTextLen(sText);
	sBuf = (char*)malloc(iLen + 1u);
	if ( sBuf == NULL ) {
		return "";
	}
	for ( i = 0u; i < iLen; ++i ) {
		sBuf[i] = sText[iLen - i - 1u];
	}
	sBuf[iLen] = '\0';
	return sBuf;
}
const char* xl_str_pad_left(const char* sText, int iWidth, const char* sFill)
{
	size_t iTextLen;
	size_t iFillLen;
	size_t iNeed;
	char* sBuf;
	size_t iPos;

	if ( sText == NULL ) {
		sText = "";
	}
	if ( sFill == NULL ) {
		sFill = " ";
	}
	iTextLen = __xlTextLen(sText);
	iFillLen = __xlTextLen(sFill);
	if ( (iWidth <= 0) || ((size_t)iWidth <= iTextLen) ) {
		return XLRT_CSTR(xrtCopyStr((str)sText, iTextLen));
	}
	if ( iFillLen == 0u ) {
		sFill = " ";
		iFillLen = 1u;
	}
	iNeed = (size_t)iWidth - iTextLen;
	sBuf = (char*)malloc((size_t)iWidth + 1u);
	if ( sBuf == NULL ) {
		return "";
	}
	iPos = 0u;
	while ( iPos < iNeed ) {
		size_t iTake = iFillLen;
		if ( iTake > (iNeed - iPos) ) {
			iTake = iNeed - iPos;
		}
		memcpy(sBuf + iPos, sFill, iTake);
		iPos += iTake;
	}
	memcpy(sBuf + iPos, sText, iTextLen);
	sBuf[iPos + iTextLen] = '\0';
	return sBuf;
}
const char* xl_str_pad_right(const char* sText, int iWidth, const char* sFill)
{
	size_t iTextLen;
	size_t iFillLen;
	size_t iNeed;
	char* sBuf;
	size_t iPos;

	if ( sText == NULL ) {
		sText = "";
	}
	if ( sFill == NULL ) {
		sFill = " ";
	}
	iTextLen = __xlTextLen(sText);
	iFillLen = __xlTextLen(sFill);
	if ( (iWidth <= 0) || ((size_t)iWidth <= iTextLen) ) {
		return XLRT_CSTR(xrtCopyStr((str)sText, iTextLen));
	}
	if ( iFillLen == 0u ) {
		sFill = " ";
		iFillLen = 1u;
	}
	iNeed = (size_t)iWidth - iTextLen;
	sBuf = (char*)malloc((size_t)iWidth + 1u);
	if ( sBuf == NULL ) {
		return "";
	}
	memcpy(sBuf, sText, iTextLen);
	iPos = iTextLen;
	while ( iPos < (iTextLen + iNeed) ) {
		size_t iTake = iFillLen;
		if ( iTake > ((iTextLen + iNeed) - iPos) ) {
			iTake = (iTextLen + iNeed) - iPos;
		}
		memcpy(sBuf + iPos, sFill, iTake);
		iPos += iTake;
	}
	sBuf[iPos] = '\0';
	return sBuf;
}

int xl_rand_int32(void) { return (int)xrtRand32(); }
int xl_rand_bool(void) { return (xrtRand32() & 1u) ? 1 : 0; }
int xl_rand_range(int iMin, int iMax) { return xrtRandRange(iMin, iMax); }
const char* xl_rand_text(int iLen) { return XLRT_CSTR(xrtRandStr(NULL, 0u, (size_t)((iLen > 0) ? iLen : 0))); }
const char* xl_rand_text_from(const char* sTemplate, int iLen)
{
	return XLRT_CSTR(xrtRandStr((str)sTemplate, __xlTextLen(sTemplate), (size_t)((iLen > 0) ? iLen : 0)));
}
const char* xl_host_name(void) { return xlrt_host_name(); }
const char* xl_host_ip(void) { return xlrt_host_ip(); }
const char* xl_host_user(void) { return xlrt_host_user(); }
const char* xl_host_os(void) { return xlrt_host_os(); }
const char* xl_host_arch(void) { return xlrt_host_arch(); }
const char* xl_host_executable(void) { return xlrt_host_executable(); }
const char* xl_env_get(const char* sName) { return xlrt_env_get(sName); }
int xl_env_exists(const char* sName) { return xlrt_env_exists(sName); }
int xl_env_set(const char* sName, const char* sValue) { return xlrt_env_set(sName, sValue); }
const char* xl_env_cwd(void) { return xlrt_env_cwd(); }
const char* xl_env_temp(void) { return xlrt_env_temp(); }
const char* xl_env_home(void) { return xlrt_env_home(); }

typedef struct XL_DirListBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
	int bWantFiles;
	int bWantDirs;
} XL_DirListBuffer;

static int __xlDirListGrow(XL_DirListBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;
	if ( pBuf == NULL ) return 0;
	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) return 1;
	iNewCap = (pBuf->iCap == 0u) ? 256u : pBuf->iCap;
	while ( iNewCap < (pBuf->iLen + iNeed + 1) ) iNewCap *= 2u;
	sNewData = (char*)realloc(pBuf->sData, iNewCap);
	if ( sNewData == NULL ) return 0;
	pBuf->sData = sNewData;
	pBuf->iCap = iNewCap;
	return 1;
}

static int __xlDirListAppend(XL_DirListBuffer* pBuf, const char* sText)
{
	size_t iLen;
	if ( (pBuf == NULL) || (sText == NULL) ) return 0;
	iLen = strlen(sText);
	if ( !__xlDirListGrow(pBuf, iLen + 1u) ) return 0;
	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen++] = '\n';
	pBuf->sData[pBuf->iLen] = '\0';
	return 1;
}

static int __xlDirListScanProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	XL_DirListBuffer* pBuf;
	(void)pData;
	(void)iSize;
	pBuf = (XL_DirListBuffer*)Param;
	if ( (pBuf == NULL) || (sPath == NULL) ) return 0;
	if ( (bDir == 0) && pBuf->bWantFiles ) {
		if ( !__xlDirListAppend(pBuf, (const char*)sPath) ) return 1;
	}
	if ( (bDir == 1) && pBuf->bWantDirs ) {
		if ( !__xlDirListAppend(pBuf, (const char*)sPath) ) return 1;
	}
	return 0;
}

static const char* __xlDirCollect(const char* sPath, int bRecursive, int bWantFiles, int bWantDirs)
{
	XL_DirListBuffer buf;
	if ( sPath == NULL ) return "";
	memset(&buf, 0, sizeof(buf));
	buf.bWantFiles = bWantFiles;
	buf.bWantDirs = bWantDirs;
	xrtDirScan((str)sPath, bRecursive ? TRUE : FALSE, __xlDirListScanProc, &buf);
	return (buf.sData != NULL) ? buf.sData : "";
}

int xl_dir_create(const char* sPath)
{
	if ( (sPath == NULL) || !xrtDirCreate((str)sPath) ) {
		__xlRaiseNativeError("IOError", "dir.create failed", 0, sPath);
		return 0;
	}
	return 1;
}
const char* xl_dir_list(const char* sPath) { return __xlDirCollect(sPath, 0, 1, 1); }
const char* xl_dir_files(const char* sPath) { return __xlDirCollect(sPath, 0, 1, 0); }
const char* xl_dir_dirs(const char* sPath) { return __xlDirCollect(sPath, 0, 0, 1); }
const char* xl_dir_tree(const char* sPath) { return __xlDirCollect(sPath, 1, 1, 1); }
int xl_dir_copy(const char* sSrc, const char* sDst)
{
	if ( (sSrc == NULL) || (sDst == NULL) || (xrtDirCopy((str)sSrc, (str)sDst, TRUE) < 0) ) {
		__xlRaiseNativeError("IOError", "dir.copy failed", 0, sSrc);
		return 0;
	}
	return 1;
}
int xl_dir_move(const char* sSrc, const char* sDst)
{
	if ( (sSrc == NULL) || (sDst == NULL) || (xrtDirMove((str)sSrc, (str)sDst, TRUE) < 0) ) {
		__xlRaiseNativeError("IOError", "dir.move failed", 0, sSrc);
		return 0;
	}
	return 1;
}
int xl_dir_delete(const char* sPath)
{
	if ( (sPath == NULL) || (xrtDirDelete((str)sPath) < 0) ) {
		__xlRaiseNativeError("IOError", "dir.delete failed", 0, sPath);
		return 0;
	}
	return 1;
}
const char* xl_fmt_int_text(int value, const char* sPattern) { return XLRT_CSTR(xrtIntFormat((int64)value, (str)sPattern)); }
const char* xl_fmt_float_text(double value, const char* sPattern) { return XLRT_CSTR(xrtNumFormat(value, (str)sPattern)); }
const char* xl_fmt_bool_text(int value) { return value ? "true" : "false"; }
const char* xl_crypto_hmac_sha256(const char* sKey, const char* sText)
{
	unsigned char aDigest[32];

	xrtHMAC_SHA256(
		(const uint8*)sKey,
		__xlTextLen(sKey),
		(const uint8*)sText,
		__xlTextLen(sText),
		aDigest
	);
	return __xlHexLowerBytes(aDigest, sizeof(aDigest));
}

const char* xl_crypto_hmac_sha512(const char* sKey, const char* sText)
{
	unsigned char aDigest[64];

	xrtHMAC_SHA512(
		(const uint8*)sKey,
		__xlTextLen(sKey),
		(const uint8*)sText,
		__xlTextLen(sText),
		aDigest
	);
	return __xlHexLowerBytes(aDigest, sizeof(aDigest));
}

static const char* __xlBytesStage1Text(const unsigned char* pBytes, size_t iCount, const char* sErrorKind, const char* sErrorMessage)
{
	char* sBuf;
	size_t i;

	if ( pBytes == NULL ) {
		return "";
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( pBytes[i] == 0u ) {
			__xlRaiseNativeError(sErrorKind, sErrorMessage, 0, NULL);
			return "";
		}
	}
	sBuf = (char*)malloc(iCount + 1u);
	if ( sBuf == NULL ) {
		__xlRaiseNativeError(sErrorKind, "native allocation failed", 0, NULL);
		return "";
	}
	for ( i = 0u; i < iCount; ++i ) {
		sBuf[i] = (char)pBytes[i];
	}
	sBuf[iCount] = '\0';
	return sBuf;
}

static const char* __xlHexLowerBytes(const unsigned char* pBytes, size_t iCount)
{
	static const char sHex[] = "0123456789abcdef";
	char* sBuf;
	size_t i;

	if ( pBytes == NULL ) {
		return "";
	}
	sBuf = (char*)malloc((iCount * 2u) + 1u);
	if ( sBuf == NULL ) {
		__xlRaiseNativeError("HashError", "native allocation failed", 0, NULL);
		return "";
	}
	for ( i = 0u; i < iCount; ++i ) {
		sBuf[i * 2u] = sHex[(pBytes[i] >> 4) & 0x0f];
		sBuf[(i * 2u) + 1u] = sHex[pBytes[i] & 0x0f];
	}
	sBuf[iCount * 2u] = '\0';
	return sBuf;
}

static const char* __xlHexLowerU64(uint64 value)
{
	static const char sHex[] = "0123456789abcdef";
	char sBuf[17];
	int i;

	for ( i = 15; i >= 0; --i ) {
		sBuf[i] = sHex[(unsigned int)(value & 0x0f)];
		value >>= 4;
	}
	sBuf[16] = '\0';
	return XLRT_CSTR(xrtCopyStr((str)sBuf, 16u));
}

static size_t __xlBase64DecodedSize(const char* sText, size_t iLen)
{
	size_t iPad;

	if ( (sText == NULL) || (iLen == 0u) ) {
		return 0u;
	}
	if ( (iLen % 4u) != 0u ) {
		return 0u;
	}
	iPad = 0u;
	if ( sText[iLen - 1u] == '=' ) {
		iPad += 1u;
	}
	if ( (iLen >= 2u) && (sText[iLen - 2u] == '=') ) {
		iPad += 1u;
	}
	return ((iLen / 4u) * 3u) - iPad;
}

const char* xl_encode_hex_encode(const char* sText)
{
	return XLRT_CSTR(xrtHexEncode((ptr)sText, __xlTextLen(sText)));
}

const char* xl_encode_hex_decode(const char* sText)
{
	size_t iLen;
	unsigned char* pData;
	const char* sRet;

	iLen = __xlTextLen(sText);
	if ( (iLen == 0u) || ((iLen % 2u) != 0u) ) {
		__xlRaiseNativeError("EncodeError", "hex decode requires even-length text", 0, NULL);
		return "";
	}
	pData = (unsigned char*)xrtHexDecode((str)sText, iLen);
	if ( pData == NULL ) {
		__xlRaiseNativeError("EncodeError", "hex decode failed", 0, NULL);
		return "";
	}
	sRet = __xlBytesStage1Text(pData, iLen / 2u, "EncodeError", "hex decode does not support NUL bytes in stage 1");
	xrtFree(pData);
	return sRet;
}

const char* xl_encode_base64_encode(const char* sText)
{
	return XLRT_CSTR(xrtBase64Encode((ptr)sText, __xlTextLen(sText), NULL));
}

const char* xl_encode_base64_decode(const char* sText)
{
	size_t iLen;
	size_t iOutLen;
	unsigned char* pData;
	const char* sRet;

	iLen = __xlTextLen(sText);
	iOutLen = __xlBase64DecodedSize(sText, iLen);
	if ( (iLen == 0u) || ((iOutLen == 0u) && (iLen != 0u)) ) {
		__xlRaiseNativeError("EncodeError", "base64 decode failed", 0, NULL);
		return "";
	}
	pData = (unsigned char*)xrtBase64Decode((str)sText, iLen, NULL);
	if ( pData == NULL ) {
		__xlRaiseNativeError("EncodeError", "base64 decode failed", 0, NULL);
		return "";
	}
	sRet = __xlBytesStage1Text(pData, iOutLen, "EncodeError", "base64 decode does not support NUL bytes in stage 1");
	xrtFree(pData);
	return sRet;
}

int xl_hash_hash32(const char* sText)
{
	return (int)xrtHash32((ptr)sText, __xlTextLen(sText));
}

const char* xl_hash_hash64(const char* sText)
{
	return __xlHexLowerU64(xrtHash64((ptr)sText, __xlTextLen(sText)));
}

const char* xl_hash_hash64_micro(const char* sText)
{
	return __xlHexLowerU64(xrtHash64_Micro((ptr)sText, __xlTextLen(sText)));
}

const char* xl_hash_hash64_nano(const char* sText)
{
	return __xlHexLowerU64(xrtHash64_Nano((ptr)sText, __xlTextLen(sText)));
}

const char* xl_hash_sha256(const char* sText)
{
	unsigned char aDigest[32];

	xrtSHA256((ptr)sText, __xlTextLen(sText), aDigest);
	return __xlHexLowerBytes(aDigest, sizeof(aDigest));
}

const char* xl_hash_sha512(const char* sText)
{
	unsigned char aDigest[64];

	xrtSHA512((ptr)sText, __xlTextLen(sText), aDigest);
	return __xlHexLowerBytes(aDigest, sizeof(aDigest));
}

int xl_math_abs_int(int value) { return (value < 0) ? -value : value; }
double xl_math_abs_float(double value) { return fabs(value); }
int xl_math_min_int(int left, int right) { return (left < right) ? left : right; }
double xl_math_min_float(double left, double right) { return (left < right) ? left : right; }
int xl_math_max_int(int left, int right) { return (left > right) ? left : right; }
double xl_math_max_float(double left, double right) { return (left > right) ? left : right; }
int xl_math_clamp_int(int value, int low, int high)
{
	if ( value < low ) {
		return low;
	}
	if ( value > high ) {
		return high;
	}
	return value;
}
double xl_math_clamp_float(double value, double low, double high)
{
	if ( value < low ) {
		return low;
	}
	if ( value > high ) {
		return high;
	}
	return value;
}
double xl_math_floor(double value) { return floor(value); }
double xl_math_ceil(double value) { return ceil(value); }
double xl_math_round(double value) { return round(value); }
double xl_math_trunc(double value) { return trunc(value); }
int xl_math_sqrt_int(int value) { return (int)sqrt((double)value); }
double xl_math_sqrt(double value) { return sqrt(value); }
int xl_math_pow_int(int value, int exp) { return (int)pow((double)value, (double)exp); }
double xl_math_pow(double value, double exp) { return pow(value, exp); }
double xl_math_sin(double value) { return sin(value); }
double xl_math_cos(double value) { return cos(value); }
double xl_math_tan(double value) { return tan(value); }
double xl_math_exp(double value) { return exp(value); }
double xl_math_log(double value) { return log(value); }
double xl_math_log10(double value) { return log10(value); }

static const char* __xlPackBytes(const unsigned char* pBytes, size_t iCount)
{
	return __xlBytesStage1Text(pBytes, iCount, "PackError", "pack write does not support NUL bytes in stage 1");
}

static int __xlPackCanRead(const char* sData, int iOffset, size_t iNeed)
{
	size_t iLen;

	if ( (sData == NULL) || (iOffset < 0) ) {
		return 0;
	}
	iLen = __xlTextLen(sData);
	return (((size_t)iOffset) + iNeed <= iLen) ? 1 : 0;
}

const char* xl_pack_write_u8(int value)
{
	unsigned char b;

	b = (unsigned char)(value & 0xff);
	return __xlPackBytes(&b, 1u);
}
const char* xl_pack_write_u16_le(int value)
{
	unsigned char aBytes[2];

	aBytes[0] = (unsigned char)(value & 0xff);
	aBytes[1] = (unsigned char)((value >> 8) & 0xff);
	return __xlPackBytes(aBytes, 2u);
}
const char* xl_pack_write_u16_be(int value)
{
	unsigned char aBytes[2];

	aBytes[0] = (unsigned char)((value >> 8) & 0xff);
	aBytes[1] = (unsigned char)(value & 0xff);
	return __xlPackBytes(aBytes, 2u);
}
const char* xl_pack_write_u32_le(int value)
{
	unsigned char aBytes[4];

	aBytes[0] = (unsigned char)(value & 0xff);
	aBytes[1] = (unsigned char)((value >> 8) & 0xff);
	aBytes[2] = (unsigned char)((value >> 16) & 0xff);
	aBytes[3] = (unsigned char)((value >> 24) & 0xff);
	return __xlPackBytes(aBytes, 4u);
}
const char* xl_pack_write_u32_be(int value)
{
	unsigned char aBytes[4];

	aBytes[0] = (unsigned char)((value >> 24) & 0xff);
	aBytes[1] = (unsigned char)((value >> 16) & 0xff);
	aBytes[2] = (unsigned char)((value >> 8) & 0xff);
	aBytes[3] = (unsigned char)(value & 0xff);
	return __xlPackBytes(aBytes, 4u);
}
int xl_pack_read_u8(const char* sData, int iOffset)
{
	if ( !__xlPackCanRead(sData, iOffset, 1u) ) {
		__xlRaiseNativeError("PackError", "pack read out of range", 0, NULL);
		return 0;
	}
	return (int)(unsigned char)sData[iOffset];
}
int xl_pack_read_u16_le(const char* sData, int iOffset)
{
	if ( !__xlPackCanRead(sData, iOffset, 2u) ) {
		__xlRaiseNativeError("PackError", "pack read out of range", 0, NULL);
		return 0;
	}
	return
		((int)(unsigned char)sData[iOffset]) |
		(((int)(unsigned char)sData[iOffset + 1]) << 8);
}
int xl_pack_read_u16_be(const char* sData, int iOffset)
{
	if ( !__xlPackCanRead(sData, iOffset, 2u) ) {
		__xlRaiseNativeError("PackError", "pack read out of range", 0, NULL);
		return 0;
	}
	return
		(((int)(unsigned char)sData[iOffset]) << 8) |
		((int)(unsigned char)sData[iOffset + 1]);
}
int xl_pack_read_u32_le(const char* sData, int iOffset)
{
	if ( !__xlPackCanRead(sData, iOffset, 4u) ) {
		__xlRaiseNativeError("PackError", "pack read out of range", 0, NULL);
		return 0;
	}
	return
		((int)(unsigned char)sData[iOffset]) |
		(((int)(unsigned char)sData[iOffset + 1]) << 8) |
		(((int)(unsigned char)sData[iOffset + 2]) << 16) |
		(((int)(unsigned char)sData[iOffset + 3]) << 24);
}
int xl_pack_read_u32_be(const char* sData, int iOffset)
{
	if ( !__xlPackCanRead(sData, iOffset, 4u) ) {
		__xlRaiseNativeError("PackError", "pack read out of range", 0, NULL);
		return 0;
	}
	return
		(((int)(unsigned char)sData[iOffset]) << 24) |
		(((int)(unsigned char)sData[iOffset + 1]) << 16) |
		(((int)(unsigned char)sData[iOffset + 2]) << 8) |
		((int)(unsigned char)sData[iOffset + 3]);
}

static void __xlProcessInitConfig(xprocessconfig* pConfig, const char* sCommand, size_t iMaxCaptureBytes)
{
	xrtProcessConfigInit(pConfig);
	pConfig->iTargetKind = XPROC_TARGET_SHELL;
	pConfig->sCommand = (str)sCommand;
	pConfig->bHideWindow = true;
	pConfig->bMergeStderr = true;
	pConfig->iMaxCaptureBytes = iMaxCaptureBytes;
}

static void __xlProcessInitConfigIn(xprocessconfig* pConfig, const char* sCommand, const char* sWorkDir, size_t iMaxCaptureBytes)
{
	__xlProcessInitConfig(pConfig, sCommand, iMaxCaptureBytes);
	pConfig->sWorkDir = (str)sWorkDir;
}

int xl_process_exec(const char* sCommand)
{
	xprocessconfig config;
	xprocessresult result;
	int iExitCode;
	bool bOk;
	__xlProcessInitConfig(&config, sCommand, 4096u);
	bOk = xrtExecCapture(&config, &result, 0u);
	iExitCode = result.iExitCode;
	xrtProcessResultUnit(&result);
	if ( !bOk && (iExitCode == 0) ) {
		__xlRaiseNativeError("NativeError", "process.exec failed", 0, sCommand);
		return -1;
	}
	return iExitCode;
}

const char* xl_process_capture(const char* sCommand)
{
	xprocessconfig config;
	xprocessresult result;
	const char* sOut;
	const char* sCopy;
	bool bOk;
	__xlProcessInitConfig(&config, sCommand, 65536u);
	bOk = xrtExecCapture(&config, &result, 0u);
	if ( !bOk ) {
		__xlRaiseNativeError("NativeError", "process.capture failed", 0, sCommand);
		xrtProcessResultUnit(&result);
		return "";
	}
	sOut = (result.pStdout != NULL) ? (const char*)result.pStdout : "";
	sCopy = XLRT_CSTR(xrtCopyStr((str)sOut, result.iStdoutSize));
	xrtProcessResultUnit(&result);
	return (sCopy != NULL) ? sCopy : "";
}

int xl_process_exec_in(const char* sCommand, const char* sWorkDir)
{
	xprocessconfig config;
	xprocessresult result;
	int iExitCode;
	bool bOk;
	__xlProcessInitConfigIn(&config, sCommand, sWorkDir, 4096u);
	bOk = xrtExecCapture(&config, &result, 0u);
	iExitCode = result.iExitCode;
	xrtProcessResultUnit(&result);
	if ( !bOk && (iExitCode == 0) ) {
		__xlRaiseNativeError("NativeError", "process.exec_in failed", 0, sCommand);
		return -1;
	}
	return iExitCode;
}

const char* xl_process_capture_in(const char* sCommand, const char* sWorkDir)
{
	xprocessconfig config;
	xprocessresult result;
	const char* sOut;
	const char* sCopy;
	bool bOk;
	__xlProcessInitConfigIn(&config, sCommand, sWorkDir, 65536u);
	bOk = xrtExecCapture(&config, &result, 0u);
	if ( !bOk ) {
		__xlRaiseNativeError("NativeError", "process.capture_in failed", 0, sCommand);
		xrtProcessResultUnit(&result);
		return "";
	}
	sOut = (result.pStdout != NULL) ? (const char*)result.pStdout : "";
	sCopy = XLRT_CSTR(xrtCopyStr((str)sOut, result.iStdoutSize));
	xrtProcessResultUnit(&result);
	return (sCopy != NULL) ? sCopy : "";
}
int xl_process_terminal_supported(void) { return xrtProcessTerminalSupported() ? 1 : 0; }
