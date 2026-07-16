#include "include/xrt_zip.h"

#if defined(_WIN32) || defined(_WIN64)
#define XZIP_EXPORT __declspec(dllexport)
#else
#define XZIP_EXPORT
#endif



static void xzip_publish_error(const xrtziparchive* pArchive)
{
	const xrtziperror* pError = xrtZipArchiveError(pArchive);
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "ZIP operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
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



XZIP_EXPORT void* xzip_archive_create(int iMethod, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint32 iMaxRatio)
{
	xrtzipoptions tOptions;
	xrtClearError();
	xzip_options(&tOptions, iMethod, iLevel, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	return xzip_validate_archive(xrtZipArchiveCreate(&tOptions));
}

XZIP_EXPORT void* xzip_archive_open_memory(const void* pData, size_t iSize, bool bWritable,
	int iMethod, int iLevel, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint32 iMaxRatio)
{
	xrtzipoptions tOptions;
	xrtClearError();
	xzip_options(&tOptions, iMethod, iLevel, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	return xzip_validate_archive(xrtZipArchiveOpenMemory(pData, iSize, bWritable, &tOptions));
}

XZIP_EXPORT void* xzip_archive_open_file(str sPath, int iMode, int iMethod, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint32 iMaxRatio)
{
	xrtzipoptions tOptions;
	xrtClearError();
	xzip_options(&tOptions, iMethod, iLevel, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries, iMaxRatio);
	return xzip_validate_archive(xrtZipArchiveOpenFile(sPath, iMode, &tOptions));
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

XZIP_EXPORT int64 xzip_archive_locate(void* pHandle, str sName)
{
	return xrtZipArchiveLocate((xrtziparchive*)pHandle, sName);
}

static bool xzip_entry(void* pHandle, uint64 iIndex, xrtzipentry* pEntry)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveEntryAt(pArchive, iIndex, pEntry) ) {
		xzip_publish_error(pArchive);
		return FALSE;
	}
	return TRUE;
}

XZIP_EXPORT void* xzip_archive_name_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	if ( !xzip_entry(pHandle, iIndex, &tEntry) ) { return NULL; }
	return xrtCopyStr((str)tEntry.sName, 0u);
}

XZIP_EXPORT int64 xzip_archive_size_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? (int64)tEntry.iUncompressedSize : -1;
}

XZIP_EXPORT int64 xzip_archive_compressed_size_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? (int64)tEntry.iCompressedSize : -1;
}

XZIP_EXPORT int64 xzip_archive_mtime_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.iMTime : -1;
}

XZIP_EXPORT uint32 xzip_archive_crc_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.iCRC32 : 0u;
}

XZIP_EXPORT int xzip_archive_method_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.iCompressionMethod : -1;
}

XZIP_EXPORT bool xzip_archive_directory_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	return xzip_entry(pHandle, iIndex, &tEntry) ? tEntry.bDirectory : FALSE;
}

XZIP_EXPORT void* xzip_archive_comment_at(void* pHandle, uint64 iIndex)
{
	xrtzipentry tEntry;
	if ( !xzip_entry(pHandle, iIndex, &tEntry) ) { return NULL; }
	return xrtCopyStr((str)tEntry.sComment, 0u);
}

XZIP_EXPORT void* xzip_archive_read_at(void* pHandle, uint64 iIndex)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	xbuffer pData = xrtZipArchiveReadAt(pArchive, iIndex);
	if ( pData == NULL ) { xzip_publish_error(pArchive); }
	return pData;
}

XZIP_EXPORT bool xzip_archive_add(void* pHandle, str sName, const void* pData, size_t iSize,
	bool bOverwrite, int iMethod, int iLevel)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveAdd(pArchive, sName, pData, iSize, bOverwrite, iMethod, iLevel) ) {
		xzip_publish_error(pArchive);
		return FALSE;
	}
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_add_file(void* pHandle, str sName, str sPath,
	bool bOverwrite, int iMethod, int iLevel)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveAddFile(pArchive, sName, sPath, bOverwrite, iMethod, iLevel) ) {
		xzip_publish_error(pArchive);
		return FALSE;
	}
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_add_directory(void* pHandle, str sName)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveAddDirectory(pArchive, sName) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_delete(void* pHandle, str sName)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveDelete(pArchive, sName) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_rename(void* pHandle, str sOldName, str sNewName)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveRename(pArchive, sOldName, sNewName) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_set_comment(void* pHandle, str sComment)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveSetComment(pArchive, sComment) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_set_entry_comment(void* pHandle, str sName, str sComment)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveSetEntryComment(pArchive, sName, sComment) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_extract_all(void* pHandle, str sDestination, bool bOverwrite)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveExtractAll(pArchive, sDestination, bOverwrite) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT bool xzip_archive_close(void* pHandle)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	if ( !xrtZipArchiveClose(pArchive) ) { xzip_publish_error(pArchive); return FALSE; }
	return TRUE;
}

XZIP_EXPORT void* xzip_archive_take_buffer(void* pHandle)
{
	xrtziparchive* pArchive = (xrtziparchive*)pHandle;
	xbuffer pData = xrtZipArchiveTakeBuffer(pArchive);
	if ( pData == NULL ) {
		xrtSetError(xrtFormat("ZIP archive has no memory result"), TRUE);
	}
	return pData;
}
