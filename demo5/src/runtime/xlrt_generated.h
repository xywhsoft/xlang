#ifndef XLRT_GENERATED_H
#define XLRT_GENERATED_H

extern const char* xlrt_host_name(void);
extern const char* xlrt_host_ip(void);
extern const char* xlrt_env_get(const char* sName);
extern int xlrt_env_exists(const char* sName);
extern const char* xlrt_env_cwd(void);

static int xlrt_attach_generated_runtime(void)
{
	if ( xrtThreadIsAttached() ) {
		return 0;
	}

	return (xrtThreadAttachCurrent() != NULL) ? 1 : -1;
}

static void xlrt_detach_generated_runtime(int bAttachedHere)
{
	if ( (bAttachedHere > 0) && xrtThreadIsAttached() ) {
		xrtThreadDetachCurrent();
	}
}

static size_t __xlTextLen(const char* sText)
{
	return (sText != NULL) ? strlen(sText) : 0u;
}

static void __xlWriteText(const char* sText)
{
	fputs((sText != NULL) ? sText : "", stdout);
}

static void xl_core_print_int(int value)
{
	printf("%d", value);
}

static void xl_core_print_float(double value)
{
	printf("%g", value);
}

static void xl_core_print_string(const char* value)
{
	__xlWriteText(value);
}

static void xl_core_print_bool(int value)
{
	__xlWriteText(value ? "true" : "false");
}

static void xl_core_println_void(void)
{
	fputc('\n', stdout);
}

static void xl_core_println_int(int value)
{
	xl_core_print_int(value);
	xl_core_println_void();
}

static void xl_core_println_float(double value)
{
	xl_core_print_float(value);
	xl_core_println_void();
}

static void xl_core_println_string(const char* value)
{
	xl_core_print_string(value);
	xl_core_println_void();
}

static void xl_core_println_bool(int value)
{
	xl_core_print_bool(value);
	xl_core_println_void();
}

static xtime xl_time_now(void)
{
	return xrtNow();
}

static const char* xl_time_to_string(xtime value)
{
	return xrtTimeToStr(value, XRT_TIME_FORMAT_DATETIME);
}

static const char* xl_time_now_text(void)
{
	return xrtNowStr();
}

static const char* xl_time_date_text(void)
{
	return xrtDateStr();
}

static const char* xl_time_time_text(void)
{
	return xrtTimeStr();
}

static const char* xl_time_relative(xtime value, xtime base)
{
	return xrtRelativeTime(value, base);
}

static xtime xl_time_parse(const char* sText)
{
	return xrtStrToTime((str)sText, __xlTextLen(sText));
}

static const char* xl_path_join(const char* sLeft, const char* sRight)
{
	return xrtPathJoin(2, sLeft, sRight);
}

static const char* xl_path_name_ext(const char* sPath)
{
	return xrtPathGetNameExt((str)sPath, __xlTextLen(sPath));
}

static const char* xl_path_name(const char* sPath)
{
	return xrtPathGetName((str)sPath, __xlTextLen(sPath));
}

static const char* xl_path_ext(const char* sPath)
{
	return xrtPathGetExt((str)sPath, __xlTextLen(sPath));
}

static const char* xl_path_dir(const char* sPath)
{
	return xrtPathGetDir((str)sPath, __xlTextLen(sPath));
}

static int xl_path_is_abs(const char* sPath)
{
	return xrtPathIsAbs((str)sPath, __xlTextLen(sPath)) ? 1 : 0;
}

static const char* xl_path_random(const char* sHead, const char* sFoot, int iLen)
{
	return xrtPathRandom((str)sHead, __xlTextLen(sHead), (str)sFoot, __xlTextLen(sFoot), (size_t)((iLen > 0) ? iLen : 0));
}

static int xl_fs_exists(const char* sPath)
{
	return xrtFileExists((str)sPath) ? 1 : 0;
}

static int xl_fs_is_file(const char* sPath)
{
	return xrtFileExists((str)sPath) ? 1 : 0;
}

static int xl_fs_is_dir(const char* sPath)
{
	return xrtDirExists((str)sPath) ? 1 : 0;
}

static int xl_fs_size(const char* sPath)
{
	return (int)xrtFileGetSize((str)sPath);
}

static const char* xl_fs_read_all(const char* sPath)
{
	const char* sText;

	sText = xrtFileReadAll((str)sPath, XRT_CP_UTF8, NULL);
	return (sText != NULL) ? sText : "";
}

static int xl_fs_write_all(const char* sPath, const char* sText)
{
	size_t iSize;

	if ( (sPath == NULL) || (sText == NULL) ) {
		return 0;
	}

	iSize = __xlTextLen(sText);
	return (xrtFileWriteAll((str)sPath, (str)sText, iSize, XRT_CP_UTF8) == (int)iSize) ? 1 : 0;
}

static int xl_fs_append_all(const char* sPath, const char* sText)
{
	size_t iSize;

	if ( (sPath == NULL) || (sText == NULL) ) {
		return 0;
	}

	iSize = __xlTextLen(sText);
	return (xrtFileAppend((str)sPath, (str)sText, iSize, XRT_CP_UTF8) == (int)iSize) ? 1 : 0;
}

static int xl_fs_mkdir_all(const char* sPath)
{
	if ( sPath == NULL ) {
		return 0;
	}

	return xrtDirCreateAll((str)sPath) ? 1 : 0;
}

static int xl_fs_copy(const char* sSrc, const char* sDst)
{
	if ( (sSrc == NULL) || (sDst == NULL) ) {
		return 0;
	}

	return xrtFileCopy((str)sSrc, (str)sDst, true) ? 1 : 0;
}

static int xl_fs_move(const char* sSrc, const char* sDst)
{
	if ( (sSrc == NULL) || (sDst == NULL) ) {
		return 0;
	}

	return xrtFileMove((str)sSrc, (str)sDst, true) ? 1 : 0;
}

static int xl_fs_delete(const char* sPath)
{
	if ( sPath == NULL ) {
		return 0;
	}

	return xrtFileDelete((str)sPath) ? 1 : 0;
}

static int xl_str_contains(const char* sText, const char* sPart)
{
	size_t iPartLen;

	if ( sPart == NULL ) {
		return 0;
	}
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0 ) {
		return 1;
	}
	if ( sText == NULL ) {
		return 0;
	}
	return (xrtFindStr((str)sText, __xlTextLen(sText), (str)sPart, iPartLen, FALSE) != NULL) ? 1 : 0;
}

static int xl_str_starts_with(const char* sText, const char* sPart)
{
	size_t iTextLen;
	size_t iPartLen;

	if ( sPart == NULL ) {
		return 0;
	}
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0 ) {
		return 1;
	}
	if ( sText == NULL ) {
		return 0;
	}
	iTextLen = __xlTextLen(sText);
	if ( iTextLen < iPartLen ) {
		return 0;
	}
	return (xrtStrComp((str)sText, (str)sPart, iPartLen, FALSE) == 0) ? 1 : 0;
}

static int xl_str_ends_with(const char* sText, const char* sPart)
{
	size_t iTextLen;
	size_t iPartLen;

	if ( sPart == NULL ) {
		return 0;
	}
	iPartLen = __xlTextLen(sPart);
	if ( iPartLen == 0 ) {
		return 1;
	}
	if ( sText == NULL ) {
		return 0;
	}
	iTextLen = __xlTextLen(sText);
	if ( iTextLen < iPartLen ) {
		return 0;
	}
	return (xrtStrComp((str)(sText + iTextLen - iPartLen), (str)sPart, iPartLen, FALSE) == 0) ? 1 : 0;
}

static const char* xl_str_lower(const char* sText)
{
	return xrtLCase((str)sText, __xlTextLen(sText), FALSE);
}

static const char* xl_str_upper(const char* sText)
{
	return xrtUCase((str)sText, __xlTextLen(sText), FALSE);
}

static const char* xl_str_replace(const char* sText, const char* sPart, const char* sValue)
{
	return xrtReplace(
		(str)sText,
		__xlTextLen(sText),
		(str)sPart,
		__xlTextLen(sPart),
		(str)sValue,
		__xlTextLen(sValue),
		NULL
	);
}

static int xl_rand_int32(void)
{
	return (int)xrtRand32();
}

static int xl_rand_range(int iMin, int iMax)
{
	return xrtRandRange(iMin, iMax);
}

static const char* xl_rand_text(int iLen)
{
	return xrtRandStr(NULL, 0u, (size_t)((iLen > 0) ? iLen : 0));
}

static const char* xl_host_name(void)
{
	return xlrt_host_name();
}

static const char* xl_host_ip(void)
{
	return xlrt_host_ip();
}

static const char* xl_env_get(const char* sName)
{
	return xlrt_env_get(sName);
}

static int xl_env_exists(const char* sName)
{
	return xlrt_env_exists(sName);
}

static const char* xl_env_cwd(void)
{
	return xlrt_env_cwd();
}

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

	if ( pBuf == NULL ) {
		return 0;
	}
	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return 1;
	}

	iNewCap = (pBuf->iCap == 0u) ? 256u : pBuf->iCap;
	while ( iNewCap < (pBuf->iLen + iNeed + 1) ) {
		iNewCap *= 2u;
	}

	sNewData = (char*)realloc(pBuf->sData, iNewCap);
	if ( sNewData == NULL ) {
		return 0;
	}

	pBuf->sData = sNewData;
	pBuf->iCap = iNewCap;
	return 1;
}

static int __xlDirListAppend(XL_DirListBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( (pBuf == NULL) || (sText == NULL) ) {
		return 0;
	}

	iLen = strlen(sText);
	if ( !__xlDirListGrow(pBuf, iLen + 1u) ) {
		return 0;
	}

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
	pBuf = (XL_DirListBuffer*)Param;
	if ( (pBuf == NULL) || (sPath == NULL) ) {
		return 0;
	}
	if ( (bDir == 0) && pBuf->bWantFiles ) {
		if ( !__xlDirListAppend(pBuf, (const char*)sPath) ) {
			return 1;
		}
	}
	if ( (bDir == 1) && pBuf->bWantDirs ) {
		if ( !__xlDirListAppend(pBuf, (const char*)sPath) ) {
			return 1;
		}
	}
	(void)iSize;
	return 0;
}

static const char* __xlDirCollect(const char* sPath, int bRecursive, int bWantFiles, int bWantDirs)
{
	XL_DirListBuffer buf;

	if ( sPath == NULL ) {
		return "";
	}

	memset(&buf, 0, sizeof(buf));
	buf.bWantFiles = bWantFiles;
	buf.bWantDirs = bWantDirs;
	xrtDirScan((str)sPath, bRecursive ? TRUE : FALSE, __xlDirListScanProc, &buf);
	if ( buf.sData == NULL ) {
		return "";
	}
	return buf.sData;
}

static const char* xl_dir_list(const char* sPath)
{
	return __xlDirCollect(sPath, 0, 1, 1);
}

static const char* xl_dir_files(const char* sPath)
{
	return __xlDirCollect(sPath, 0, 1, 0);
}

static const char* xl_dir_dirs(const char* sPath)
{
	return __xlDirCollect(sPath, 0, 0, 1);
}

static const char* xl_dir_tree(const char* sPath)
{
	return __xlDirCollect(sPath, 1, 1, 1);
}

static const char* xl_fmt_int_text(int value, const char* sPattern)
{
	return xrtIntFormat((int64)value, (str)sPattern);
}

static const char* xl_fmt_float_text(double value, const char* sPattern)
{
	return xrtNumFormat(value, (str)sPattern);
}

static void xl_process_init_config(xprocessconfig* pConfig, const char* sCommand, size_t iMaxCaptureBytes)
{
	xrtProcessConfigInit(pConfig);
	pConfig->iTargetKind = XPROC_TARGET_SHELL;
	pConfig->sCommand = (str)sCommand;
	pConfig->bHideWindow = true;
	pConfig->bMergeStderr = true;
	pConfig->iMaxCaptureBytes = iMaxCaptureBytes;
}

static void xl_process_init_config_in(xprocessconfig* pConfig, const char* sCommand, const char* sWorkDir, size_t iMaxCaptureBytes)
{
	xl_process_init_config(pConfig, sCommand, iMaxCaptureBytes);
	pConfig->sWorkDir = (str)sWorkDir;
}

static int xl_process_exec(const char* sCommand)
{
	xprocessconfig config;
	xprocessresult result;
	int iExitCode;
	bool bOk;

	xl_process_init_config(&config, sCommand, 4096u);
	bOk = xrtExecCapture(&config, &result, 0u);
	iExitCode = result.iExitCode;
	xrtProcessResultUnit(&result);
	if ( !bOk && (iExitCode == 0) ) {
		return -1;
	}
	return iExitCode;
}

static const char* xl_process_capture(const char* sCommand)
{
	xprocessconfig config;
	xprocessresult result;
	const char* sOut;
	const char* sCopy;
	bool bOk;

	xl_process_init_config(&config, sCommand, 65536u);
	bOk = xrtExecCapture(&config, &result, 0u);
	if ( !bOk ) {
		xrtProcessResultUnit(&result);
		return "";
	}

	sOut = (result.pStdout != NULL) ? (const char*)result.pStdout : "";
	sCopy = xrtCopyStr((str)sOut, result.iStdoutSize);
	xrtProcessResultUnit(&result);
	return (sCopy != NULL) ? sCopy : "";
}

static int xl_process_exec_in(const char* sCommand, const char* sWorkDir)
{
	xprocessconfig config;
	xprocessresult result;
	int iExitCode;
	bool bOk;

	xl_process_init_config_in(&config, sCommand, sWorkDir, 4096u);
	bOk = xrtExecCapture(&config, &result, 0u);
	iExitCode = result.iExitCode;
	xrtProcessResultUnit(&result);
	if ( !bOk && (iExitCode == 0) ) {
		return -1;
	}
	return iExitCode;
}

static const char* xl_process_capture_in(const char* sCommand, const char* sWorkDir)
{
	xprocessconfig config;
	xprocessresult result;
	const char* sOut;
	const char* sCopy;
	bool bOk;

	xl_process_init_config_in(&config, sCommand, sWorkDir, 65536u);
	bOk = xrtExecCapture(&config, &result, 0u);
	if ( !bOk ) {
		xrtProcessResultUnit(&result);
		return "";
	}

	sOut = (result.pStdout != NULL) ? (const char*)result.pStdout : "";
	sCopy = xrtCopyStr((str)sOut, result.iStdoutSize);
	xrtProcessResultUnit(&result);
	return (sCopy != NULL) ? sCopy : "";
}

#endif
