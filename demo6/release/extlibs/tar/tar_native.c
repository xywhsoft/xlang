#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#ifndef XRT_MODULE_FILE_TREE
#define XRT_MODULE_FILE_TREE
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif

#include "xruntime.h"
#include "include/xrt_tar.h"

#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XTAR_EXPORT __declspec(dllexport)
#else
	#define XTAR_EXPORT
#endif



typedef struct {
	xrttarreader* Reader;
	xbuffer* OwnedInput;
	xrttarentry Entry;
	bool HasEntry;
} xtar_reader_handle;



typedef struct {
	xvalue* Callback;
	xerror* InvokeError;
	xerrkind InvokeKind;
	cstr InvokeMessage;
	bool InvokeFailed;
} xtar_callback_context;

/* 把 TAR 状态映射为 XRT 通用错误类别。 */
static xerrkind xtar_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_TAR_ERROR_ARGUMENT: return XERR_ARGUMENT;
		case XRT_TAR_ERROR_MEMORY: return XERR_MEMORY;
		case XRT_TAR_ERROR_FORMAT: return XERR_PROTOCOL;
		case XRT_TAR_ERROR_LIMIT: return XERR_RANGE;
		case XRT_TAR_ERROR_UNSAFE_PATH: return XERR_PERMISSION;
		case XRT_TAR_ERROR_EXISTS: return XERR_EXISTS;
		case XRT_TAR_ERROR_STATE: return XERR_STATE;
		case XRT_TAR_ERROR_UNSUPPORTED: return XERR_UNSUPPORTED;
		case XRT_TAR_ERROR_CANCELLED: return XERR_CANCELLED;
		case XRT_TAR_ERROR_CALLBACK: return XERR_STATE;
		default: return XERR_IO;
	}
}



/* 发布 Reader 的结构化错误。 */
static void xtar_publish_reader_error(const xrttarreader* pReader)
{
	const xrttarerror* pError = xrtTarReaderError(pReader);
	int iCode = pError != NULL ? pError->iCode : XRT_TAR_ERROR_IO;
	cstr sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "TAR reader operation failed";

	xrtSetErrorInfo(xtar_error_kind(iCode),
		"xlang.extlib.tar", iCode, sMessage);
}



/* 发布 Writer 的结构化错误。 */
static void xtar_publish_writer_error(const xrttarwriter* pWriter)
{
	const xrttarerror* pError = xrtTarWriterError(pWriter);
	int iCode = pError != NULL ? pError->iCode : XRT_TAR_ERROR_IO;
	cstr sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "TAR writer operation failed";

	xrtSetErrorInfo(xtar_error_kind(iCode),
		"xlang.extlib.tar", iCode, sMessage);
}



/* 填充 FFI 暴露的资源和提取策略。 */
static void xtar_options(xrttaroptions* pOptions,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, bool bAllowSymlinks, bool bAllowHardlinks,
	bool bAllowSpecialFiles, bool bPreservePermissions)
{
	xrtTarOptionsInit(pOptions);
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pOptions->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxEntries != 0u ) { pOptions->tLimits.iMaxEntries = iMaxEntries; }
	pOptions->bAllowSymlinks = bAllowSymlinks;
	pOptions->bAllowHardlinks = bAllowHardlinks;
	pOptions->bAllowSpecialFiles = bAllowSpecialFiles;
	pOptions->bPreservePermissions = bPreservePermissions;
}





/* 返回当前线程最后一个错误的稳定消息。 */
XTAR_EXPORT const char* xtar_last_error(void)
{
	const xerror* pError = xrtGetError();

	while ( (pError != NULL) && (xrtErrorCause(pError) != NULL) ) {
		pError = xrtErrorCause(pError);
	}
	return pError != NULL ? xrtErrorMessage(pError) : "TAR operation failed";
}



/* c_ulonglong 在 LP64 上不是 XRT uint64；导出签名须与生成的 C 原型一致。 */
/* 在硬上限内把完整文件接管为 XRT Buffer。 */
XTAR_EXPORT void* xtar_read_file(cstr sPath, unsigned long long iLimit)
{
	bytes pData;
	size_t iSize = 0u;
	xbuffer* pBuffer;
	xrtresourcelimits Limits;

	xrtClearError();
	if ( iLimit == 0u ) {
		xrtResourceLimitsInit(&Limits);
		iLimit = Limits.iMaxInputBytes;
	}
	if ( iLimit > SIZE_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.tar",
			XRT_TAR_ERROR_LIMIT, "TAR file input exceeds the platform size limit");
		return NULL;
	}
	pData = xrtFileReadAllLimit(sPath, (size_t)iLimit, &iSize);
	if ( pData == NULL ) { return NULL; }
	pBuffer = xrtBufferCreateTake(&pData, iSize, iSize);
	if ( pBuffer == NULL ) { xrtFree(pData); }
	return pBuffer;
}




/* 仅依据可验证的文件头识别压缩容器。 */
XTAR_EXPORT int xtar_file_compression(cstr sPath)
{
	unsigned char arrHeader[6] = { 0 };
	xreader* pReader;
	size_t iRead = 0u;
	bool bRead;
	bool bClose;

	xrtClearError();
	pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) { return -1; }
	bRead = xrtReaderRead(pReader, arrHeader, sizeof(arrHeader), &iRead);
	bClose = xrtReaderDestroy(pReader);
	if ( !bRead || !bClose ) { return -1; }
	if ( (iRead >= 2u) && (arrHeader[0] == 0x1Fu) &&
		(arrHeader[1] == 0x8Bu) ) {
		return 1;
	}
	if ( (iRead >= 3u) && (arrHeader[0] == 'B') &&
		(arrHeader[1] == 'Z') && (arrHeader[2] == 'h') ) {
		return 2;
	}
	if ( (iRead >= 6u) && (arrHeader[0] == 0xFDu) &&
		(arrHeader[1] == '7') && (arrHeader[2] == 'z') &&
		(arrHeader[3] == 'X') && (arrHeader[4] == 'Z') &&
		(arrHeader[5] == 0x00u) ) {
		return 4;
	}
	return 0;
}



/* 原子写入完整文件。 */
XTAR_EXPORT bool xtar_write_file(cstr sPath, void* pData, unsigned long long iSize)
{
	xrtClearError();
	if ( iSize > SIZE_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.tar",
			XRT_TAR_ERROR_LIMIT, "TAR file output exceeds the platform size limit");
		return false;
	}
	return xrtFileWriteAtomic(sPath,
		(xbytesview){ (const uint8*)pData, (size_t)iSize });
}





/* 校验 Reader 并包装当前条目状态。 */
static void* xtar_validate_reader(xrttarreader* pReader, xbuffer* pOwnedInput)
{
	xtar_reader_handle* pHandle;
	const xrttarerror* pError = xrtTarReaderError(pReader);

	if ( pReader == NULL || (pError != NULL && pError->iCode != XRT_TAR_OK) ) {
		xtar_publish_reader_error(pReader);
		xrtTarReaderDestroy(pReader);
		xrtBufferDestroy(pOwnedInput);
		return NULL;
	}
	pHandle = (xtar_reader_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtTarReaderDestroy(pReader);
		xrtBufferDestroy(pOwnedInput);
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.tar",
			XRT_TAR_ERROR_MEMORY, "cannot allocate TAR reader handle");
		return NULL;
	}
	pHandle->Reader = pReader;
	pHandle->OwnedInput = pOwnedInput;
	xrtTarEntryInit(&pHandle->Entry);
	return pHandle;
}



/* 从内存创建 Reader 绑定。 */
XTAR_EXPORT void* xtar_reader_open_memory(void* pData, unsigned long long iSize,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	bool bAllowSymlinks, bool bAllowHardlinks, bool bAllowSpecialFiles,
	bool bPreservePermissions)
{
	xrttaroptions Options;
	xbuffer* pOwnedInput;

	xrtClearError();
	xtar_options(&Options, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, bPreservePermissions);
	if ( (iSize > SIZE_MAX) ||
		(Options.tLimits.iMaxInputBytes != 0u &&
		 iSize > Options.tLimits.iMaxInputBytes) ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.tar",
			XRT_TAR_ERROR_LIMIT, "TAR input exceeds the configured limit");
		return NULL;
	}
	if ( pData == NULL && iSize != 0u ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.tar",
			XRT_TAR_ERROR_ARGUMENT, "TAR input data is null");
		return NULL;
	}
	/* XRT 内存 Reader 借用视图；语言桥持有副本到 Reader 销毁。 */
	pOwnedInput = xrtBufferCreate();
	if ( pOwnedInput == NULL ) { return NULL; }
	if ( iSize != 0u && !xrtBufferAppend(pOwnedInput,
		(xbytesview){ (const uint8*)pData, (size_t)iSize }) ) {
		xrtBufferDestroy(pOwnedInput);
		return NULL;
	}
	return xtar_validate_reader(
		xrtTarReaderOpenMemory(pOwnedInput->Data, iSize, &Options), pOwnedInput);
}



/* 从文件创建 Reader 绑定。 */
XTAR_EXPORT void* xtar_reader_open_file(cstr sPath,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	bool bAllowSymlinks, bool bAllowHardlinks, bool bAllowSpecialFiles,
	bool bPreservePermissions)
{
	xrttaroptions Options;

	xrtClearError();
	xtar_options(&Options, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, bPreservePermissions);
	return xtar_validate_reader(xrtTarReaderOpenFile(sPath, &Options), NULL);
}



/* 销毁 Reader 绑定。 */
XTAR_EXPORT void xtar_reader_destroy(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;

	if ( pHandle == NULL ) { return; }
	xrtTarReaderDestroy(pHandle->Reader);
	xrtBufferDestroy(pHandle->OwnedInput);
	xrtFree(pHandle);
}



/* 移动到下一个条目。 */
XTAR_EXPORT bool xtar_reader_next(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;

	if ( pHandle == NULL ) { return false; }
	pHandle->HasEntry = false;
	if ( xrtTarReaderNext(pHandle->Reader, &pHandle->Entry) ) {
		pHandle->HasEntry = true;
		return true;
	}
	if ( !xrtTarReaderEOF(pHandle->Reader) ) {
		xtar_publish_reader_error(pHandle->Reader);
	}
	return false;
}



/* 查询 Reader 是否到达归档末尾。 */
XTAR_EXPORT bool xtar_reader_eof(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && xrtTarReaderEOF(pHandle->Reader);
}



/* 复制当前条目的字符串元数据。 */
static void* xtar_entry_string(xtar_reader_handle* pHandle, cstr sValue)
{
	str sResult;

	if ( pHandle == NULL || !pHandle->HasEntry ) {
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.tar",
			XRT_TAR_ERROR_STATE, "TAR reader has no current entry");
		return NULL;
	}
	sResult = xrtStrDup(sValue != NULL ? sValue : "");
	if ( sResult == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.tar",
			XRT_TAR_ERROR_MEMORY, "cannot copy TAR entry metadata");
	}
	return sResult;
}



XTAR_EXPORT void* xtar_reader_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle,
		pHandle != NULL ? pHandle->Entry.sName : NULL);
}



XTAR_EXPORT void* xtar_reader_link_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle,
		pHandle != NULL ? pHandle->Entry.sLinkName : NULL);
}



XTAR_EXPORT void* xtar_reader_user_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle,
		pHandle != NULL ? pHandle->Entry.sUserName : NULL);
}



XTAR_EXPORT void* xtar_reader_group_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle,
		pHandle != NULL ? pHandle->Entry.sGroupName : NULL);
}



XTAR_EXPORT int64 xtar_reader_size(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ?
		(int64)pHandle->Entry.iSizeBytes : -1;
}



XTAR_EXPORT int64 xtar_reader_mtime(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? pHandle->Entry.iMTime : 0;
}



XTAR_EXPORT int64 xtar_reader_uid(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? (int64)pHandle->Entry.iUID : 0;
}



XTAR_EXPORT int64 xtar_reader_gid(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? (int64)pHandle->Entry.iGID : 0;
}



XTAR_EXPORT int xtar_reader_mode(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? (int)pHandle->Entry.iMode : 0;
}



XTAR_EXPORT int xtar_reader_device_major(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ?
		(int)pHandle->Entry.iDeviceMajor : 0;
}



XTAR_EXPORT int xtar_reader_device_minor(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ?
		(int)pHandle->Entry.iDeviceMinor : 0;
}



XTAR_EXPORT int xtar_reader_type(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ?
		pHandle->Entry.iType : XRT_TAR_ENTRY_OTHER;
}





/* 读取当前普通文件条目并清除绑定的当前条目状态。 */
XTAR_EXPORT void* xtar_reader_read(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	xbuffer* pResult;

	if ( pHandle == NULL ) { return NULL; }
	pResult = xrtTarReaderRead(pHandle->Reader);
	if ( pResult == NULL ) {
		xtar_publish_reader_error(pHandle->Reader);
		return NULL;
	}
	pHandle->HasEntry = false;
	return pResult;
}



/* 跳过当前条目并清除绑定状态。 */
XTAR_EXPORT bool xtar_reader_skip(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;

	if ( pHandle == NULL ) { return false; }
	if ( !xrtTarReaderSkip(pHandle->Reader) ) {
		xtar_publish_reader_error(pHandle->Reader);
		return false;
	}
	pHandle->HasEntry = false;
	return true;
}



/* 使用核心安全提取路径解压全部条目。 */
XTAR_EXPORT bool xtar_reader_extract_all(void* pValue,
	cstr sDestination, bool bOverwrite)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;

	if ( pHandle == NULL ) { return false; }
	if ( !xrtTarReaderExtractAll(pHandle->Reader,
		sDestination, bOverwrite) ) {
		xtar_publish_reader_error(pHandle->Reader);
		return false;
	}
	pHandle->HasEntry = false;
	return true;
}





/* 把 XLang 字符串数组借用为核心精确名称白名单。 */
XTAR_EXPORT bool xtar_reader_extract_names(void* pValue,
	cstr sDestination, bool bOverwrite, xvalue* pNames)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	xstrview* pViews = NULL;
	size_t iCount;
	size_t i;
	bool bResult;

	if ( pHandle == NULL ) { return false; }
	if ( pNames == NULL || xrtValueType(pNames) != XVALUE_ARRAY ) {
		xrtSetErrorInfo(XERR_TYPE, "xlang.extlib.tar",
			XRT_TAR_ERROR_ARGUMENT,
			"TAR extraction names must be an array<string>");
		return false;
	}
	iCount = xrtValueCount(pNames);
	if ( iCount != 0u ) {
		pViews = (xstrview*)xrtCalloc(iCount, sizeof(*pViews));
		if ( pViews == NULL ) { return false; }
	}
	for ( i = 0u; i < iCount; ++i ) {
		xvalue* pName = xrtValueArrayGet(pNames, i);

		if ( !xrtValueGetString(pName, &pViews[i]) ) {
			xrtFree(pViews);
			xrtSetErrorInfo(XERR_TYPE, "xlang.extlib.tar",
				XRT_TAR_ERROR_ARGUMENT,
				"TAR extraction names must contain strings");
			return false;
		}
		if ( pViews[i].Size != 0u &&
			memchr(pViews[i].Data, 0, pViews[i].Size) != NULL ) {
			xrtFree(pViews);
			xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.tar",
				XRT_TAR_ERROR_ARGUMENT,
				"TAR extraction name contains an embedded NUL");
			return false;
		}
	}
	bResult = xrtTarReaderExtractNames(pHandle->Reader, sDestination,
		bOverwrite, pViews, iCount);
	pHandle->HasEntry = false;
	xrtFree(pViews);
	if ( !bResult ) { xtar_publish_reader_error(pHandle->Reader); }
	return bResult;
}




/* 持久保存进度回调失败，避免后续清理覆盖原始错误。 */
static void xtar_callback_fail(xtar_callback_context* pContext,
	xerrkind Kind, cstr sMessage)
{
	pContext->InvokeFailed = true;
	pContext->InvokeKind = Kind;
	pContext->InvokeMessage = sMessage;
	pContext->InvokeError = xrtTakeError();
}





/* 返回稳定的条目类型名称，供 XLang 选择回调直接使用。 */
static cstr xtar_entry_kind(int iType)
{
	switch ( iType ) {
		case XRT_TAR_ENTRY_FILE: return "file";
		case XRT_TAR_ENTRY_DIRECTORY: return "directory";
		case XRT_TAR_ENTRY_SYMLINK: return "symlink";
		case XRT_TAR_ENTRY_HARDLINK: return "hardlink";
		case XRT_TAR_ENTRY_FIFO: return "fifo";
		case XRT_TAR_ENTRY_CHARACTER: return "character";
		case XRT_TAR_ENTRY_BLOCK: return "block";
		default: return "other";
	}
}



/* 把一个 TAR 条目转为 XLang callable(name, kind, size)。 */
static xrttarselect xtar_select(const xrttarentry* pEntry, ptr pUserData)
{
	xtar_callback_context* pContext = (xtar_callback_context*)pUserData;
	xrtcallframe Frame;
	xrtcallresult Result;
	xvalue* arrArguments[3] = { NULL, NULL, NULL };
	xvalue* pReturn;
	bool bExtract = false;

	arrArguments[0] = xrtValueString(xrtStrView(
		pEntry->sName != NULL ? pEntry->sName : ""));
	arrArguments[1] = xrtValueString(xrtStrView(
		xtar_entry_kind(pEntry->iType)));
	arrArguments[2] = xrtValueInt((int64)pEntry->iSizeBytes);
	if ( (arrArguments[0] == NULL) ||
		(arrArguments[1] == NULL) || (arrArguments[2] == NULL) ) {
		xtar_callback_fail(pContext, XERR_MEMORY,
			"cannot allocate TAR selector arguments");
		goto cleanup;
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 3u;
	Frame.Arguments = arrArguments;
	xrtCallResultInit(&Result);
	if ( !xrtValueInvoke(pContext->Callback, &Frame, &Result) ) {
		xtar_callback_fail(pContext, XERR_STATE,
			"cannot invoke TAR selector callback");
	} else if ( xrtCallResultCount(&Result) == 0u ) {
		xtar_callback_fail(pContext, XERR_TYPE,
			"TAR selector callback must return bool");
	} else {
		pReturn = xrtCallResultGet(&Result, 0u);
		if ( !xrtValueGetBool(pReturn, &bExtract) ) {
			xtar_callback_fail(pContext, XERR_TYPE,
				"TAR selector callback must return bool");
		}
	}
	xrtCallResultUnit(&Result);

cleanup:
	xrtValueRelease(arrArguments[0]);
	xrtValueRelease(arrArguments[1]);
	xrtValueRelease(arrArguments[2]);
	if ( pContext->InvokeFailed ) { return XRT_TAR_SELECT_ERROR; }
	return bExtract ? XRT_TAR_SELECT_EXTRACT : XRT_TAR_SELECT_SKIP;
}



/* 使用核心选择器提取，并原样恢复 XLang 回调抛出的错误。 */
XTAR_EXPORT bool xtar_reader_extract_where(void* pValue,
	cstr sDestination, bool bOverwrite, xvalue* pCallback)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	xtar_callback_context Context;
	bool bResult;

	if ( pHandle == NULL || pCallback == NULL ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.tar",
			XRT_TAR_ERROR_ARGUMENT, "TAR selector callback is required");
		return false;
	}
	memset(&Context, 0, sizeof(Context));
	Context.Callback = pCallback;
	bResult = xrtTarReaderExtractWhere(pHandle->Reader, sDestination,
		bOverwrite, xtar_select, &Context, NULL, NULL);
	pHandle->HasEntry = false;
	if ( Context.InvokeFailed ) {
		if ( Context.InvokeError != NULL ) {
			xrtSetError(Context.InvokeError);
			xrtErrorFree(Context.InvokeError);
		} else {
			xrtSetErrorInfo(Context.InvokeKind, "xlang.extlib.tar",
				XRT_TAR_ERROR_CALLBACK, Context.InvokeMessage);
		}
		return false;
	}
	if ( !bResult ) { xtar_publish_reader_error(pHandle->Reader); }
	return bResult;
}




/* 把一个 TAR 进度事件转为 XLang callable(name, index, bytes)。 */
static bool xtar_progress(const xrttarentry* pEntry,
	const xrtprogress* pProgress, ptr pUserData)
{
	xtar_callback_context* pContext = (xtar_callback_context*)pUserData;
	xrtcallframe Frame;
	xrtcallresult Result;
	xvalue* arrArguments[3] = { NULL, NULL, NULL };
	xvalue* pReturn;
	bool bContinue = false;

	if ( pEntry == NULL ) { return true; }
	arrArguments[0] = xrtValueString(xrtStrView(
		pEntry->sName != NULL ? pEntry->sName : ""));
	arrArguments[1] = xrtValueInt((int64)(pEntry->iIndex + 1u));
	arrArguments[2] = xrtValueInt((int64)pProgress->iOutputBytes);
	if ( (arrArguments[0] == NULL) ||
		(arrArguments[1] == NULL) || (arrArguments[2] == NULL) ) {
		xtar_callback_fail(pContext, XERR_MEMORY,
			"cannot allocate TAR progress arguments");
		goto cleanup;
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 3u;
	Frame.Arguments = arrArguments;
	xrtCallResultInit(&Result);
	if ( !xrtValueInvoke(pContext->Callback, &Frame, &Result) ) {
		xtar_callback_fail(pContext, XERR_STATE,
			"cannot invoke TAR progress callback");
	} else if ( xrtCallResultCount(&Result) == 0u ) {
		xtar_callback_fail(pContext, XERR_TYPE,
			"TAR progress callback must return bool");
	} else {
		pReturn = xrtCallResultGet(&Result, 0u);
		if ( !xrtValueGetBool(pReturn, &bContinue) ) {
			xtar_callback_fail(pContext, XERR_TYPE,
				"TAR progress callback must return bool");
			bContinue = false;
		}
	}
	xrtCallResultUnit(&Result);

cleanup:
	xrtValueRelease(arrArguments[0]);
	xrtValueRelease(arrArguments[1]);
	xrtValueRelease(arrArguments[2]);
	return bContinue;
}



/* 使用同一核心提取器转发进度；用户取消不是异常。 */
XTAR_EXPORT bool xtar_reader_extract_progress(void* pValue,
	cstr sDestination, bool bOverwrite, xvalue* pCallback)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	xtar_callback_context Context;
	const xrttarerror* pError;
	bool bResult;

	if ( pHandle == NULL ) { return false; }
	memset(&Context, 0, sizeof(Context));
	Context.Callback = pCallback;
	bResult = xrtTarReaderExtract(pHandle->Reader, sDestination,
		bOverwrite, pCallback != NULL ? xtar_progress : NULL, &Context);
	pHandle->HasEntry = false;
	if ( Context.InvokeFailed ) {
		if ( Context.InvokeError != NULL ) {
			xrtSetError(Context.InvokeError);
			xrtErrorFree(Context.InvokeError);
		} else {
			xrtSetErrorInfo(Context.InvokeKind, "xlang.extlib.tar",
				XRT_TAR_ERROR_STATE, Context.InvokeMessage);
		}
		return false;
	}
	if ( bResult ) { return true; }
	pError = xrtTarReaderError(pHandle->Reader);
	if ( pError != NULL && pError->iCode == XRT_TAR_ERROR_CANCELLED ) {
		xrtClearError();
		return true;
	}
	xtar_publish_reader_error(pHandle->Reader);
	return false;
}



/* 把当前普通文件条目事务写入指定路径。 */
XTAR_EXPORT bool xtar_reader_extract_to(void* pValue, cstr sDestination)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	str sDirectory = NULL;
	str sTemporary = NULL;
	xfile File = NULL;
	xwriter* pWriter = NULL;
	bool bResult = false;

	if ( pHandle == NULL || !pHandle->HasEntry ) {
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.tar",
			XRT_TAR_ERROR_STATE, "TAR reader has no current entry");
		return false;
	}
	if ( pHandle->Entry.iType != XRT_TAR_ENTRY_FILE ) {
		xrtSetErrorInfo(XERR_TYPE, "xlang.extlib.tar",
			XRT_TAR_ERROR_STATE, "current TAR entry is not a regular file");
		return false;
	}
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	if ( sDirectory[0] != 0 && strcmp(sDirectory, ".") != 0 &&
		!xrtDirExists(sDirectory) && !xrtDirCreateAll(sDirectory) ) {
		goto cleanup;
	}
	File = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-tar-", ".tmp", &sTemporary);
	if ( File == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&File);
	if ( pWriter == NULL ) { goto cleanup; }
	bResult = xrtTarReaderReadTo(pHandle->Reader, pWriter) &&
		xrtWriterFlush(pWriter);
	if ( !xrtWriterDestroy(pWriter) ) { bResult = false; }
	pWriter = NULL;
	if ( bResult ) {
		bResult = xrtPathRename(sTemporary, sDestination, true);
	}

cleanup:
	if ( pWriter != NULL ) { (void)xrtWriterDestroy(pWriter); }
	if ( File != NULL ) { (void)xrtClose(File); }
	if ( !bResult && sTemporary != NULL && xrtPathExists(sTemporary) ) {
		(void)xrtFileDelete(sTemporary);
	}
	if ( !bResult && xrtGetError() == NULL ) {
		xtar_publish_reader_error(pHandle->Reader);
	}
	if ( bResult ) { pHandle->HasEntry = false; }
	xrtFree(sDirectory);
	xrtFree(sTemporary);
	return bResult;
}





/* 校验 Writer 构造结果。 */
static void* xtar_validate_writer(xrttarwriter* pWriter)
{
	const xrttarerror* pError = xrtTarWriterError(pWriter);

	if ( pWriter == NULL || (pError != NULL && pError->iCode != XRT_TAR_OK) ) {
		xtar_publish_writer_error(pWriter);
		xrtTarWriterDestroy(pWriter);
		return NULL;
	}
	return pWriter;
}



/* 创建内存 Writer 绑定。 */
XTAR_EXPORT void* xtar_writer_create_memory(unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, bool bAllowSymlinks,
	bool bAllowHardlinks, bool bAllowSpecialFiles)
{
	xrttaroptions Options;

	xrtClearError();
	xtar_options(&Options, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, false);
	return xtar_validate_writer(xrtTarWriterCreateMemory(&Options));
}



/* 创建事务文件 Writer 绑定。 */
XTAR_EXPORT void* xtar_writer_open_file(cstr sPath,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	bool bAllowSymlinks, bool bAllowHardlinks, bool bAllowSpecialFiles)
{
	xrttaroptions Options;

	xrtClearError();
	xtar_options(&Options, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, false);
	return xtar_validate_writer(xrtTarWriterOpenFile(sPath, &Options));
}




/* 流式复制已有条目并创建绑定同一路径的事务追加 Writer。 */
XTAR_EXPORT void* xtar_writer_open_append(cstr sPath,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	bool bAllowSymlinks, bool bAllowHardlinks, bool bAllowSpecialFiles,
	bool bAppendMode)
{
	xrttaroptions Options;

	(void)bAppendMode;
	xrtClearError();
	xtar_options(&Options, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, false);
	return xtar_validate_writer(xrtTarWriterOpenAppend(sPath, &Options));
}



/* 销毁 Writer 绑定。 */
XTAR_EXPORT void xtar_writer_destroy(void* pValue)
{
	xrtTarWriterDestroy((xrttarwriter*)pValue);
}



/* 统一发布 Writer 操作结果。 */
static bool xtar_writer_result(xrttarwriter* pWriter, bool bResult)
{
	if ( !bResult ) { xtar_publish_writer_error(pWriter); }
	return bResult;
}



/* 添加内存普通文件条目。 */
XTAR_EXPORT bool xtar_writer_add(void* pValue, cstr sName,
	void* pData, unsigned long long iSize, int iMode, long long iMTime,
	unsigned long long iUID, unsigned long long iGID,
	cstr sUserName, cstr sGroupName)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	xrttarentry Entry;

	xrtTarEntryInit(&Entry);
	Entry.sName = sName;
	Entry.iType = XRT_TAR_ENTRY_FILE;
	Entry.iMode = (uint32)iMode;
	Entry.iMTime = iMTime;
	Entry.iUID = iUID;
	Entry.iGID = iGID;
	Entry.sUserName = sUserName;
	Entry.sGroupName = sGroupName;
	return xtar_writer_result(pWriter,
		xrtTarWriterAdd(pWriter, &Entry, pData, iSize));
}



/* 添加 UTF-8 文本条目。 */
XTAR_EXPORT bool xtar_writer_add_str(void* pValue, cstr sName,
	cstr sText, unsigned long long iSize, int iMode, long long iMTime,
	unsigned long long iUID, unsigned long long iGID,
	cstr sUserName, cstr sGroupName)
{
	/* XLang point ABI 使用 void*；底层仅借用并读取文本字节。 */
	return xtar_writer_add(pValue, sName, (void*)sText, iSize,
		iMode, iMTime, iUID, iGID, sUserName, sGroupName);
}



/* 流式添加本地文件。 */
XTAR_EXPORT bool xtar_writer_add_file(void* pValue,
	cstr sName, cstr sPath, int iMode, long long iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter,
		xrtTarWriterAddFile(pWriter, sName, sPath, (uint32)iMode, iMTime));
}



/* 添加目录条目。 */
XTAR_EXPORT bool xtar_writer_add_directory(void* pValue,
	cstr sName, int iMode, long long iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter,
		xrtTarWriterAddDirectory(pWriter, sName, (uint32)iMode, iMTime));
}



/* 添加符号链接或硬链接条目。 */
XTAR_EXPORT bool xtar_writer_add_link(void* pValue,
	cstr sName, cstr sTarget, bool bHardLink, int iMode, long long iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;

	if ( bHardLink ) {
		xrttarentry Entry;

		xrtTarEntryInit(&Entry);
		Entry.sName = sName;
		Entry.sLinkName = sTarget;
		Entry.iType = XRT_TAR_ENTRY_HARDLINK;
		Entry.iMode = (uint32)iMode;
		Entry.iMTime = iMTime;
		return xtar_writer_result(pWriter,
			xrtTarWriterAdd(pWriter, &Entry, NULL, 0u));
	}
	return xtar_writer_result(pWriter,
		xrtTarWriterAddSymlink(pWriter, sName, sTarget,
			(uint32)iMode, iMTime));
}



/* 添加 FIFO 元数据条目。 */
XTAR_EXPORT bool xtar_writer_add_fifo(void* pValue,
	cstr sName, int iMode, long long iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	xrttarentry Entry;

	xrtTarEntryInit(&Entry);
	Entry.sName = sName;
	Entry.iType = XRT_TAR_ENTRY_FIFO;
	Entry.iMode = (uint32)iMode;
	Entry.iMTime = iMTime;
	return xtar_writer_result(pWriter,
		xrtTarWriterAdd(pWriter, &Entry, NULL, 0u));
}



/* 使用核心目录树遍历添加本地目录。 */
XTAR_EXPORT bool xtar_writer_add_dir(void* pValue,
	cstr sArchiveRoot, cstr sLocalPath, bool bRecursive)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter,
		xrtTarWriterAddTree(pWriter, sArchiveRoot, sLocalPath, bRecursive));
}



/* 完成并提交 Writer。 */
XTAR_EXPORT bool xtar_writer_close(void* pValue)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter, xrtTarWriterClose(pWriter));
}



/* 移交内存归档结果。 */
XTAR_EXPORT void* xtar_writer_take_buffer(void* pValue)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	xbuffer* pResult = xrtTarWriterTakeBuffer(pWriter);

	if ( pResult == NULL ) { xtar_publish_writer_error(pWriter); }
	return pResult;
}
