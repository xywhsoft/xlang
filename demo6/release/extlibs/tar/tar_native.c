#include "include/xrt_tar.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XTAR_EXPORT __declspec(dllexport)
#else
#define XTAR_EXPORT
#endif

typedef struct {
	xrttarreader* Reader;
	xrttarentry Entry;
	bool HasEntry;
} xtar_reader_handle;



static void xtar_publish_reader_error(const xrttarreader* pReader)
{
	const xrttarerror* pError = xrtTarReaderError(pReader);
	xrtSetError(xrtFormat("%s", pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "TAR reader operation failed"), TRUE);
}

static void xtar_publish_writer_error(const xrttarwriter* pWriter)
{
	const xrttarerror* pError = xrtTarWriterError(pWriter);
	xrtSetError(xrtFormat("%s", pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "TAR writer operation failed"), TRUE);
}

static void xtar_options(xrttaroptions* pOptions, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, bool bAllowSymlinks, bool bAllowHardlinks,
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

static void* xtar_validate_reader(xrttarreader* pReader)
{
	xtar_reader_handle* pHandle;
	const xrttarerror* pError = xrtTarReaderError(pReader);
	if ( pReader == NULL || (pError != NULL && pError->iCode != XRT_TAR_OK) ) {
		xtar_publish_reader_error(pReader);
		xrtTarReaderDestroy(pReader);
		return NULL;
	}
	pHandle = (xtar_reader_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtTarReaderDestroy(pReader);
		xrtSetError(xrtFormat("cannot allocate TAR reader handle"), TRUE);
		return NULL;
	}
	pHandle->Reader = pReader;
	xrtTarEntryInit(&pHandle->Entry);
	return pHandle;
}

XTAR_EXPORT void* xtar_reader_open_memory(const void* pData, uint64 iSize,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	bool bAllowSymlinks, bool bAllowHardlinks, bool bAllowSpecialFiles,
	bool bPreservePermissions)
{
	xrttaroptions tOptions;
	xrtClearError();
	xtar_options(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, bPreservePermissions);
	return xtar_validate_reader(xrtTarReaderOpenMemory(pData, iSize, &tOptions));
}

XTAR_EXPORT void* xtar_reader_open_file(str sPath,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	bool bAllowSymlinks, bool bAllowHardlinks, bool bAllowSpecialFiles,
	bool bPreservePermissions)
{
	xrttaroptions tOptions;
	xrtClearError();
	xtar_options(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, bPreservePermissions);
	return xtar_validate_reader(xrtTarReaderOpenFile(sPath, &tOptions));
}

XTAR_EXPORT void xtar_reader_destroy(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	if ( pHandle == NULL ) { return; }
	xrtTarReaderDestroy(pHandle->Reader);
	xrtFree(pHandle);
}

XTAR_EXPORT bool xtar_reader_next(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	if ( pHandle == NULL ) { return FALSE; }
	pHandle->HasEntry = xrtTarReaderNext(pHandle->Reader, &pHandle->Entry);
	if ( !pHandle->HasEntry && !xrtTarReaderEOF(pHandle->Reader) ) {
		xtar_publish_reader_error(pHandle->Reader);
	}
	return pHandle->HasEntry;
}

XTAR_EXPORT bool xtar_reader_eof(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && xrtTarReaderEOF(pHandle->Reader);
}

static void* xtar_entry_string(xtar_reader_handle* pHandle, const char* sValue)
{
	if ( pHandle == NULL || !pHandle->HasEntry ) {
		xrtSetError(xrtFormat("TAR reader has no current entry"), TRUE);
		return NULL;
	}
	return xrtCopyStr((str)(sValue != NULL ? sValue : ""), 0u);
}

XTAR_EXPORT void* xtar_reader_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle, pHandle != NULL ? pHandle->Entry.sName : NULL);
}

XTAR_EXPORT void* xtar_reader_link_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle, pHandle != NULL ? pHandle->Entry.sLinkName : NULL);
}

XTAR_EXPORT void* xtar_reader_user_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle, pHandle != NULL ? pHandle->Entry.sUserName : NULL);
}

XTAR_EXPORT void* xtar_reader_group_name(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return xtar_entry_string(pHandle, pHandle != NULL ? pHandle->Entry.sGroupName : NULL);
}

XTAR_EXPORT int64 xtar_reader_size(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? (int64)pHandle->Entry.iSizeBytes : -1;
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
	return pHandle != NULL && pHandle->HasEntry ? (int)pHandle->Entry.iDeviceMajor : 0;
}

XTAR_EXPORT int xtar_reader_device_minor(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? (int)pHandle->Entry.iDeviceMinor : 0;
}

XTAR_EXPORT int xtar_reader_type(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasEntry ? pHandle->Entry.iType : XRT_TAR_ENTRY_OTHER;
}

XTAR_EXPORT void* xtar_reader_read(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	xbuffer pResult = pHandle != NULL ? xrtTarReaderRead(pHandle->Reader) : NULL;
	if ( pResult == NULL && pHandle != NULL ) { xtar_publish_reader_error(pHandle->Reader); }
	return pResult;
}

XTAR_EXPORT bool xtar_reader_skip(void* pValue)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	if ( pHandle == NULL || !xrtTarReaderSkip(pHandle->Reader) ) {
		if ( pHandle != NULL ) { xtar_publish_reader_error(pHandle->Reader); }
		return FALSE;
	}
	return TRUE;
}

XTAR_EXPORT bool xtar_reader_extract_all(void* pValue, str sDestination, bool bOverwrite)
{
	xtar_reader_handle* pHandle = (xtar_reader_handle*)pValue;
	if ( pHandle == NULL || !xrtTarReaderExtractAll(pHandle->Reader, sDestination, bOverwrite) ) {
		if ( pHandle != NULL ) { xtar_publish_reader_error(pHandle->Reader); }
		return FALSE;
	}
	return TRUE;
}



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

XTAR_EXPORT void* xtar_writer_create_memory(uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, bool bAllowSymlinks, bool bAllowHardlinks,
	bool bAllowSpecialFiles)
{
	xrttaroptions tOptions;
	xrtClearError();
	xtar_options(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, FALSE);
	return xtar_validate_writer(xrtTarWriterCreateMemory(&tOptions));
}

XTAR_EXPORT void* xtar_writer_open_file(str sPath, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, bool bAllowSymlinks, bool bAllowHardlinks,
	bool bAllowSpecialFiles)
{
	xrttaroptions tOptions;
	xrtClearError();
	xtar_options(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		bAllowSymlinks, bAllowHardlinks, bAllowSpecialFiles, FALSE);
	return xtar_validate_writer(xrtTarWriterOpenFile(sPath, &tOptions));
}

XTAR_EXPORT void xtar_writer_destroy(void* pValue)
{
	xrtTarWriterDestroy((xrttarwriter*)pValue);
}

static bool xtar_writer_result(xrttarwriter* pWriter, bool bResult)
{
	if ( !bResult ) { xtar_publish_writer_error(pWriter); }
	return bResult;
}

XTAR_EXPORT bool xtar_writer_add(void* pValue, str sName, const void* pData, uint64 iSize,
	int iMode, int64 iMTime, uint64 iUID, uint64 iGID, str sUserName, str sGroupName)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	xrttarentry tEntry;
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sName;
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iMode = (uint32)iMode;
	tEntry.iMTime = iMTime;
	tEntry.iUID = iUID;
	tEntry.iGID = iGID;
	tEntry.sUserName = (const char*)sUserName;
	tEntry.sGroupName = (const char*)sGroupName;
	return xtar_writer_result(pWriter, xrtTarWriterAdd(pWriter, &tEntry, pData, iSize));
}

XTAR_EXPORT bool xtar_writer_add_file(void* pValue, str sName, str sPath, int iMode, int64 iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter,
		xrtTarWriterAddFile(pWriter, sName, sPath, (uint32)iMode, iMTime));
}

XTAR_EXPORT bool xtar_writer_add_directory(void* pValue, str sName, int iMode, int64 iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter,
		xrtTarWriterAddDirectory(pWriter, sName, (uint32)iMode, iMTime));
}

XTAR_EXPORT bool xtar_writer_add_link(void* pValue, str sName, str sTarget,
	bool bHardLink, int iMode, int64 iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	if ( bHardLink ) {
		xrttarentry tEntry;
		xrtTarEntryInit(&tEntry);
		tEntry.sName = (const char*)sName;
		tEntry.sLinkName = (const char*)sTarget;
		tEntry.iType = XRT_TAR_ENTRY_HARDLINK;
		tEntry.iMode = (uint32)iMode;
		tEntry.iMTime = iMTime;
		return xtar_writer_result(pWriter, xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u));
	}
	return xtar_writer_result(pWriter,
		xrtTarWriterAddSymlink(pWriter, sName, sTarget, (uint32)iMode, iMTime));
}

XTAR_EXPORT bool xtar_writer_add_fifo(void* pValue, str sName, int iMode, int64 iMTime)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	xrttarentry tEntry;
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sName;
	tEntry.iType = XRT_TAR_ENTRY_FIFO;
	tEntry.iMode = (uint32)iMode;
	tEntry.iMTime = iMTime;
	return xtar_writer_result(pWriter, xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u));
}

XTAR_EXPORT bool xtar_writer_close(void* pValue)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	return xtar_writer_result(pWriter, xrtTarWriterClose(pWriter));
}

XTAR_EXPORT void* xtar_writer_take_buffer(void* pValue)
{
	xrttarwriter* pWriter = (xrttarwriter*)pValue;
	xbuffer pResult = xrtTarWriterTakeBuffer(pWriter);
	if ( pResult == NULL ) { xrtSetError(xrtFormat("TAR writer has no memory result"), TRUE); }
	return pResult;
}
