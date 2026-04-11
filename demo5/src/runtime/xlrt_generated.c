#include "xlrt_generated.h"

#define XLRT_CSTR(value) ((const char*)(value))

static size_t __xlTextLen(const char* sText)
{
	return (sText != NULL) ? strlen(sText) : 0u;
}

static void __xlRaiseNativeError(const char* sKind, const char* sMessage, int iCode, const char* sData)
{
	xlrt_native_error_raise(sKind, sMessage, iCode, sData);
}

static void __xlWriteText(const char* sText)
{
	fputs((sText != NULL) ? sText : "", stdout);
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

xtime xl_time_now(void) { return xrtNow(); }
const char* xl_time_to_string(xtime value) { return XLRT_CSTR(xrtTimeToStr(value, XRT_TIME_FORMAT_DATETIME)); }
const char* xl_time_now_text(void) { return XLRT_CSTR(xrtNowStr()); }
const char* xl_time_date_text(void) { return XLRT_CSTR(xrtDateStr()); }
const char* xl_time_time_text(void) { return XLRT_CSTR(xrtTimeStr()); }
const char* xl_time_relative(xtime value, xtime base) { return XLRT_CSTR(xrtRelativeTime(value, base)); }
xtime xl_time_parse(const char* sText) { return xrtStrToTime((str)sText, __xlTextLen(sText)); }

const char* xl_path_join(const char* sLeft, const char* sRight) { return XLRT_CSTR(xrtPathJoin(2, sLeft, sRight)); }
const char* xl_path_name_ext(const char* sPath) { return XLRT_CSTR(xrtPathGetNameExt((str)sPath, __xlTextLen(sPath))); }
const char* xl_path_name(const char* sPath) { return XLRT_CSTR(xrtPathGetName((str)sPath, __xlTextLen(sPath))); }
const char* xl_path_ext(const char* sPath) { return XLRT_CSTR(xrtPathGetExt((str)sPath, __xlTextLen(sPath))); }
const char* xl_path_dir(const char* sPath) { return XLRT_CSTR(xrtPathGetDir((str)sPath, __xlTextLen(sPath))); }
int xl_path_is_abs(const char* sPath) { return xrtPathIsAbs((str)sPath, __xlTextLen(sPath)) ? 1 : 0; }
const char* xl_path_random(const char* sHead, const char* sFoot, int iLen)
{
	return XLRT_CSTR(xrtPathRandom((str)sHead, __xlTextLen(sHead), (str)sFoot, __xlTextLen(sFoot), (size_t)((iLen > 0) ? iLen : 0)));
}

int xl_fs_exists(const char* sPath) { return xrtFileExists((str)sPath) ? 1 : 0; }
int xl_fs_is_file(const char* sPath) { return xrtFileExists((str)sPath) ? 1 : 0; }
int xl_fs_is_dir(const char* sPath) { return xrtDirExists((str)sPath) ? 1 : 0; }
int xl_fs_size(const char* sPath) { return (int)xrtFileGetSize((str)sPath); }
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

int xl_rand_int32(void) { return (int)xrtRand32(); }
int xl_rand_range(int iMin, int iMax) { return xrtRandRange(iMin, iMax); }
const char* xl_rand_text(int iLen) { return XLRT_CSTR(xrtRandStr(NULL, 0u, (size_t)((iLen > 0) ? iLen : 0))); }
const char* xl_host_name(void) { return xlrt_host_name(); }
const char* xl_host_ip(void) { return xlrt_host_ip(); }
const char* xl_env_get(const char* sName) { return xlrt_env_get(sName); }
int xl_env_exists(const char* sName) { return xlrt_env_exists(sName); }
const char* xl_env_cwd(void) { return xlrt_env_cwd(); }

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

const char* xl_dir_list(const char* sPath) { return __xlDirCollect(sPath, 0, 1, 1); }
const char* xl_dir_files(const char* sPath) { return __xlDirCollect(sPath, 0, 1, 0); }
const char* xl_dir_dirs(const char* sPath) { return __xlDirCollect(sPath, 0, 0, 1); }
const char* xl_dir_tree(const char* sPath) { return __xlDirCollect(sPath, 1, 1, 1); }
const char* xl_fmt_int_text(int value, const char* sPattern) { return XLRT_CSTR(xrtIntFormat((int64)value, (str)sPattern)); }
const char* xl_fmt_float_text(double value, const char* sPattern) { return XLRT_CSTR(xrtNumFormat(value, (str)sPattern)); }

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
