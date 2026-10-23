#define XRT_MODULE_STRING
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif

#include "include/xrt_zip.h"
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XZIP_EXPORT __declspec(dllexport)
#else
#define XZIP_EXPORT
#endif



/* 把 ZIP 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xzip_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_ZIP_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_ZIP_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_ZIP_ERROR_FORMAT:
			return XERR_PROTOCOL;
		case XRT_ZIP_ERROR_NOT_FOUND:
			return XERR_NOT_FOUND;
		case XRT_ZIP_ERROR_EXISTS:
			return XERR_EXISTS;
		case XRT_ZIP_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_ZIP_ERROR_UNSAFE_PATH:
			return XERR_PERMISSION;
		case XRT_ZIP_ERROR_STATE:
			return XERR_STATE;
		default:
			return XERR_IO;
	}
}





/* 发布可由 XLang 和 C 调用方统一读取的结构化错误。 */
static void xzip_publish_error(const xrtziparchive* pArchive)
{
	const xrtziperror* pError = xrtZipArchiveError(pArchive);
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "ZIP operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_ZIP_ERROR_BACKEND;

	xrtSetErrorInfo(xzip_error_kind(iCode), "xlang.extlib.zip", iCode, sMessage);
}

static void xzip_options(xrtzipoptions* pOptions, int iMethod, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint32 iMaxRatio)
{
	xrtZipOptionsInit(pOptions);
	pOptions->iDefaultMethod = iMethod;
	pOptions->iDefaultLevel = iLevel;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pOptions->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxEntries != 0u ) { pOptions->tLimits.iMaxEntries = iMaxEntries; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static void* xzip_validate_archive(xrtziparchive* pArchive)
{
	if ( pArchive == NULL || !xrtZipArchiveIsOpen(pArchive) ) {
		xzip_publish_error(pArchive);
		xrtZipArchiveDestroy(pArchive);
		return NULL;
	}
	return pArchive;
}



XZIP_EXPORT const char* xzip_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XZIP_EXPORT void* xzip_archive_create(int iMethod, int iLevel,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned int iMaxRatio)
{
	xrtzipoptions tOptions;
	xrtClearError();
	xzip_options(&tOptions, iMethod, iLevel, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	return xzip_validate_archive(xrtZipArchiveCreate(&tOptions));
}

XZIP_EXPORT void* xzip_archive_open_memory(void* pData, size_t iSize, bool bWritable,
	int iMethod, int iLevel, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned int iMaxRatio)
{
	xrtzipoptions tOptions;
	xrtClearError();
	xzip_options(&tOptions, iMethod, iLevel, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	return xzip_validate_archive(xrtZipArchiveOpenMemory(pData, iSize, bWritable, &tOptions));
}

XZIP_EXPORT void* xzip_archive_open_file(const char* sPath, int iMode,
	int iMethod, int iLevel, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned int iMaxRatio)
{
	xrtzipoptions tOptions;
	xrtClearError();
	xzip_options(&tOptions, iMethod, iLevel, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	return xzip_validate_archive(xrtZipArchiveOpenFile((str)sPath, iMode, &tOptions));
}

/* XLang 字符串保留显式长度；公开构造器不能只读取 NUL 前缀。 */
XZIP_EXPORT void* xzip_archive_open_file_value(xvalue* pPath, int iMode,
	int iMethod, int iLevel, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned int iMaxRatio)
{
	xstrview Path;
	str sPath;
	void* pResult;
	xrtClearError();
	if (pPath == NULL || xrtValueType(pPath) != XVALUE_STRING ||
		!xrtValueGetString(pPath, &Path)) {
		xrtSetErrorInfo(XERR_TYPE, "xlang.extlib.zip", XRT_ZIP_ERROR_ARGUMENT,
			"ZIP path must be a string");
		return NULL;
	}
	if (Path.Size != 0u && (Path.Data == NULL || memchr(Path.Data, 0, Path.Size) != NULL)) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.zip", XRT_ZIP_ERROR_ARGUMENT,
			"ZIP path contains an embedded NUL byte");
		return NULL;
	}
	sPath = xrtStrDupView(Path);
	if (sPath == NULL) { return NULL; }
	pResult = xzip_archive_open_file(sPath, iMode, iMethod, iLevel,
		iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	xrtFree(sPath);
	return pResult;
}

XZIP_EXPORT void xzip_archive_destroy(void* pHandle)
{
	xrtZipArchiveDestroy((xrtziparchive*)pHandle);
}

XZIP_EXPORT bool xzip_archive_is_memory(void* pHandle)
{
	return xrtZipArchiveIsMemory((xrtziparchive*)pHandle);
}

XZIP_EXPORT bool xzip_archive_is_open(void* pHandle)
{
	return xrtZipArchiveIsOpen((xrtziparchive*)pHandle);
}

XZIP_EXPORT int64 xzip_archive_count(void* pHandle)
{
	return (int64)xrtZipArchiveCount((xrtziparchive*)pHandle);
}

XZIP_EXPORT int64 xzip_archive_locate(void* pHandle, const char* sName)
{
	return xrtZipArchiveLocate((xrtziparchive*)pHandle, (str)sName);
}

static bool xzip_entry(void* pHandle, int64_t iIndex, xrtzipentry* pEntry)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( iIndex < 0 ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.zip", XRT_ZIP_ERROR_ARGUMENT,
			"ZIP entry index is negative");
		return false;
	}
	if ( !xrtZipArchiveEntryAt(pArchive, (uint64)iIndex, pEntry) ) {
		xzip_publish_error(pArchive);
		return false;
	}
	return true;
}

XZIP_EXPORT void* xzip_archive_name_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	str sResult;

	if ( !xzip_entry(pHandle, iIndex, &tEntry) ) { return NULL; }
	sResult = xrtStrDup(tEntry.sName);
	if ( sResult == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.zip",
			XRT_ZIP_ERROR_MEMORY, "cannot copy ZIP entry name");
	}
	return sResult;
}

XZIP_EXPORT int64 xzip_archive_size_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? (int64)tEntry.iUncompressedSize : -1;
}

XZIP_EXPORT int64 xzip_archive_compressed_size_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? (int64)tEntry.iCompressedSize : -1;
}

XZIP_EXPORT int64 xzip_archive_mtime_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.iMTime : -1;
}

XZIP_EXPORT int64_t xzip_archive_crc_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.iCRC32 : 0u;
}

XZIP_EXPORT int64_t xzip_archive_method_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.iCompressionMethod : -1;
}

XZIP_EXPORT bool xzip_archive_directory_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.bDirectory : false;
}

XZIP_EXPORT void* xzip_archive_comment_at(void* pHandle, int64_t iIndex)
{
	xrtzipentry tEntry;
	str sResult;

	if ( !xzip_entry(pHandle, iIndex, &tEntry) ) { return NULL; }
	sResult = xrtStrDup(tEntry.sComment);
	if ( sResult == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.zip",
			XRT_ZIP_ERROR_MEMORY, "cannot copy ZIP entry comment");
	}
	return sResult;
}

XZIP_EXPORT void* xzip_archive_read_at(void* pHandle, int64_t iIndex)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	xbuffer* pData;
	if ( iIndex < 0 ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.zip", XRT_ZIP_ERROR_ARGUMENT,
			"ZIP entry index is negative");
		return NULL;
	}
	pData = xrtZipArchiveReadAt(pArchive, (uint64)iIndex);
	if ( pData == NULL ) { xzip_publish_error(pArchive); }
	return pData;
}

XZIP_EXPORT bool xzip_archive_add(void* pHandle, const char* sName, void* pData, size_t iSize,
	bool bOverwrite, int iMethod, int iLevel)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveAdd(pArchive, (str)sName, pData, iSize, bOverwrite, iMethod, iLevel) ) {
		xzip_publish_error(pArchive);
		return false;
	}
	return true;
}

XZIP_EXPORT bool xzip_archive_add_file(void* pHandle, const char* sName, const char* sPath,
	bool bOverwrite, int iMethod, int iLevel)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveAddFile(pArchive, (str)sName, (str)sPath, bOverwrite, iMethod, iLevel) ) {
		xzip_publish_error(pArchive);
		return false;
	}
	return true;
}

XZIP_EXPORT bool xzip_archive_add_directory(void* pHandle, const char* sName)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveAddDirectory(pArchive, (str)sName) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT bool xzip_archive_delete(void* pHandle, const char* sName)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveDelete(pArchive, (str)sName) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT bool xzip_archive_rename(void* pHandle, const char* sOldName, const char* sNewName)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveRename(pArchive, (str)sOldName, (str)sNewName) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT bool xzip_archive_set_comment(void* pHandle, const char* sComment)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveSetComment(pArchive, (str)sComment) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT bool xzip_archive_set_entry_comment(void* pHandle, const char* sName, const char* sComment)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveSetEntryComment(pArchive, (str)sName, (str)sComment) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT bool xzip_archive_extract_all(void* pHandle, const char* sDestination, bool bOverwrite)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveExtractAll(pArchive, (str)sDestination, bOverwrite) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT bool xzip_archive_close(void* pHandle)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveClose(pArchive) ) { xzip_publish_error(pArchive); return false; }
	return true;
}

XZIP_EXPORT void* xzip_archive_take_buffer(void* pHandle)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	xbuffer* pData = xrtZipArchiveTakeBuffer(pArchive);
	if ( pData == NULL ) {
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.zip",
			XRT_ZIP_ERROR_STATE, "ZIP archive has no memory result");
	}
	return pData;
}

XZIP_EXPORT bool xzip_archive_add_text(void* handle, const char* name,
    xstrview text, bool overwrite, int method, int level)
{
    return xzip_archive_add(handle, name, (void*)text.Data, text.Size,
        overwrite, method, level);
}
