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
