#include "xrt_zip_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>



typedef struct {
	xbuffer Data;
	uint64 Position;
	zip_error_t Error;
} xrtzipentrysource;



static zip_int64_t __xrtZipEntrySourceCallback(void* pState, void* pData,
	zip_uint64_t iLength, zip_source_cmd_t iCommand)
{
	xrtzipentrysource* pSource = (xrtzipentrysource*)pState;
	switch ( iCommand ) {
	case ZIP_SOURCE_OPEN:
		pSource->Position = 0u;
		return 0;
	case ZIP_SOURCE_READ: {
		uint64 iRemain = pSource->Position < pSource->Data->Length ?
			pSource->Data->Length - pSource->Position : 0u;
		uint64 iRead = iLength < iRemain ? iLength : iRemain;
		if ( iRead != 0u ) {
			memcpy(pData, pSource->Data->Buffer + pSource->Position, (size_t)iRead);
			pSource->Position += iRead;
		}
		return (zip_int64_t)iRead;
	}
	case ZIP_SOURCE_CLOSE:
		return 0;
	case ZIP_SOURCE_STAT: {
		zip_stat_t* pStat;
		if ( iLength < sizeof(zip_stat_t) ) {
			zip_error_set(&pSource->Error, ZIP_ER_INVAL, 0);
			return -1;
		}
		pStat = (zip_stat_t*)pData;
		zip_stat_init(pStat);
		pStat->size = pSource->Data->Length;
		pStat->mtime = time(NULL);
		pStat->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
		return sizeof(zip_stat_t);
	}
	case ZIP_SOURCE_ERROR:
		return zip_error_to_data(&pSource->Error, pData, iLength);
	case ZIP_SOURCE_FREE:
		xrtBufferDestroy(pSource->Data);
		zip_error_fini(&pSource->Error);
		xrtFree(pSource);
		return 0;
	case ZIP_SOURCE_SEEK: {
		zip_int64_t iOffset = zip_source_seek_compute_offset(pSource->Position,
			pSource->Data->Length, pData, iLength, &pSource->Error);
		if ( iOffset < 0 ) { return -1; }
		pSource->Position = (uint64)iOffset;
		return 0;
	}
	case ZIP_SOURCE_TELL:
		return (zip_int64_t)pSource->Position;
	case ZIP_SOURCE_SUPPORTS:
		return ZIP_SOURCE_SUPPORTS_SEEKABLE;
	default:
		zip_error_set(&pSource->Error, ZIP_ER_OPNOTSUPP, 0);
		return -1;
	}
}

static zip_source_t* __xrtZipEntrySourceCreate(xrtziparchive* pArchive,
	const void* pData, size_t iSize)
{
	xrtzipentrysource* pEntry;
	zip_source_t* pSource;
	pEntry = (xrtzipentrysource*)xrtMalloc(sizeof(*pEntry));
	if ( pEntry == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot allocate ZIP entry source");
		return NULL;
	}
	memset(pEntry, 0, sizeof(*pEntry));
	zip_error_init(&pEntry->Error);
	pEntry->Data = xrtBufferCreate(iSize > 65536u ? (uint32)iSize : 65536u);
	if ( pEntry->Data == NULL ||
		(iSize != 0u && !xrtBufferAppend(pEntry->Data, (ptr)pData, (uint32)iSize, XBUF_BINARY)) ) {
		xrtBufferDestroy(pEntry->Data);
		zip_error_fini(&pEntry->Error);
		xrtFree(pEntry);
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot copy ZIP entry data");
		return NULL;
	}
	pSource = zip_source_function(pArchive->Zip, __xrtZipEntrySourceCallback, pEntry);
	if ( pSource == NULL ) {
		xrtBufferDestroy(pEntry->Data);
		zip_error_fini(&pEntry->Error);
		xrtFree(pEntry);
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot create ZIP entry source");
	}
	return pSource;
}



void __xrtZipSetError(xrtziparchive* pArchive, int iCode, int iBackend, int iSystem,
	uint64 iEntry, const char* sFormat, ...)
{
	va_list pArgs;
	if ( pArchive == NULL ) { return; }
	pArchive->Error.iCode = iCode;
	pArchive->Error.iBackendCode = iBackend;
	pArchive->Error.iSystemCode = iSystem;
	pArchive->Error.iEntryIndex = iEntry;
	va_start(pArgs, sFormat);
	vsnprintf(pArchive->Error.sMessage, sizeof(pArchive->Error.sMessage), sFormat, pArgs);
	va_end(pArgs);
	pArchive->Error.sMessage[sizeof(pArchive->Error.sMessage) - 1u] = 0;
}

void __xrtZipSetBackendError(xrtziparchive* pArchive, const zip_error_t* pError,
	uint64 iEntry, const char* sAction)
{
	int iZip = pError != NULL ? zip_error_code_zip(pError) : ZIP_ER_INTERNAL;
	int iSystem = pError != NULL ? zip_error_code_system(pError) : 0;
	int iCode = XRT_ZIP_ERROR_BACKEND;
	const char* sDetail = pError != NULL ? zip_error_strerror((zip_error_t*)pError) : "unknown libzip error";
	if ( iZip == ZIP_ER_MEMORY ) { iCode = XRT_ZIP_ERROR_MEMORY; }
	else if ( iZip == ZIP_ER_NOENT ) { iCode = XRT_ZIP_ERROR_NOT_FOUND; }
	else if ( iZip == ZIP_ER_EXISTS ) { iCode = XRT_ZIP_ERROR_EXISTS; }
	else if ( iZip == ZIP_ER_NOZIP || iZip == ZIP_ER_INCONS || iZip == ZIP_ER_CRC ||
		iZip == ZIP_ER_TRUNCATED_ZIP || iZip == ZIP_ER_COMPRESSED_DATA ) { iCode = XRT_ZIP_ERROR_FORMAT; }
	else if ( iZip == ZIP_ER_READ || iZip == ZIP_ER_WRITE || iZip == ZIP_ER_OPEN ||
		iZip == ZIP_ER_CLOSE || iZip == ZIP_ER_SEEK || iZip == ZIP_ER_TELL ) { iCode = XRT_ZIP_ERROR_IO; }
	else if ( iZip == ZIP_ER_INVAL ) { iCode = XRT_ZIP_ERROR_ARGUMENT; }
	else if ( iZip == ZIP_ER_RDONLY || iZip == ZIP_ER_ZIPCLOSED ) { iCode = XRT_ZIP_ERROR_STATE; }
	__xrtZipSetError(pArchive, iCode, iZip, iSystem, iEntry, "%s: %s", sAction, sDetail);
}

bool __xrtZipCheckOpen(xrtziparchive* pArchive, bool bWrite)
{
	if ( pArchive == NULL || pArchive->Zip == NULL || pArchive->Committed ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_STATE, ZIP_ER_ZIPCLOSED, 0, UINT64_MAX,
			"ZIP archive is closed");
		return FALSE;
	}
	if ( bWrite && pArchive->ReadOnly ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_STATE, ZIP_ER_RDONLY, 0, UINT64_MAX,
			"ZIP archive is read-only");
		return FALSE;
	}
	return TRUE;
}



XXAPI void xrtZipOptionsInit(xrtzipoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_ZIP_OPTIONS_VERSION;
	pOptions->iDefaultMethod = XRT_ZIP_METHOD_DEFLATE;
	pOptions->iDefaultLevel = 6;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XXAPI void xrtZipErrorInit(xrtziperror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iEntryIndex = UINT64_MAX;
}

XXAPI void xrtZipEntryInit(xrtzipentry* pEntry)
{
	if ( pEntry == NULL ) { return; }
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->iSize = sizeof(*pEntry);
	pEntry->iVersion = XRT_ZIP_ENTRY_VERSION;
}

static bool __xrtZipOptionsCopy(xrtziparchive* pArchive, const xrtzipoptions* pOptions)
{
	xrtZipOptionsInit(&pArchive->Options);
	if ( pOptions != NULL ) {
		if ( pOptions->iSize < sizeof(*pOptions) || pOptions->iVersion != XRT_ZIP_OPTIONS_VERSION ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
				"invalid ZIP options version");
			return FALSE;
		}
		pArchive->Options = *pOptions;
	}
	if ( pArchive->Options.iDefaultMethod != XRT_ZIP_METHOD_STORE &&
		pArchive->Options.iDefaultMethod != XRT_ZIP_METHOD_DEFLATE ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP compression method must be store or deflate");
		return FALSE;
	}
	if ( pArchive->Options.iDefaultLevel < 0 || pArchive->Options.iDefaultLevel > 9 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP compression level must be 0..9");
		return FALSE;
	}
	return TRUE;
}

static xrtziparchive* __xrtZipArchiveAllocate(const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = (xrtziparchive*)xrtMalloc(sizeof(*pArchive));
	if ( pArchive == NULL ) { return NULL; }
	memset(pArchive, 0, sizeof(*pArchive));
	xrtZipErrorInit(&pArchive->Error);
	__xrtZipOptionsCopy(pArchive, pOptions);
	return pArchive;
}

static bool __xrtZipArchiveOpenBytes(xrtziparchive* pArchive, const void* pData,
	size_t iSize, int iFlags, bool bReadOnly)
{
	zip_error_t tError;
	zip_source_t* pSource = NULL;
	zip_int64_t iCount;
	if ( pArchive->Error.iCode != XRT_ZIP_OK ) { return FALSE; }
	if ( iSize > pArchive->Options.tLimits.iMaxInputBytes &&
		pArchive->Options.tLimits.iMaxInputBytes != 0u ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP input exceeds the configured limit");
		return FALSE;
	}
	zip_error_init(&tError);
	pArchive->Memory = __xrtZipMemorySourceCreate(pData, iSize,
		bReadOnly ? 0u : pArchive->Options.tLimits.iMaxOutputBytes, &tError, &pSource);
	if ( pArchive->Memory == NULL ) {
		__xrtZipSetBackendError(pArchive, &tError, UINT64_MAX, "cannot create ZIP source");
		zip_error_fini(&tError);
		return FALSE;
	}
	pArchive->Zip = zip_open_from_source(pSource, iFlags, &tError);
	if ( pArchive->Zip == NULL ) {
		__xrtZipSetBackendError(pArchive, &tError, UINT64_MAX, "cannot open ZIP archive");
		zip_source_free(pSource);
		zip_error_fini(&tError);
		return FALSE;
	}
	zip_error_fini(&tError);
	pArchive->ReadOnly = bReadOnly;
	iCount = zip_get_num_entries(pArchive->Zip, 0);
	if ( iCount < 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot read ZIP entry count");
		return FALSE;
	}
	if ( pArchive->Options.tLimits.iMaxEntries != 0u &&
		(uint64)iCount > pArchive->Options.tLimits.iMaxEntries ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP archive contains too many entries");
		return FALSE;
	}
	return TRUE;
}

XXAPI xrtziparchive* xrtZipArchiveCreate(const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = __xrtZipArchiveAllocate(pOptions);
	if ( pArchive != NULL ) {
		__xrtZipArchiveOpenBytes(pArchive, NULL, 0u, ZIP_CREATE | ZIP_TRUNCATE, FALSE);
	}
	return pArchive;
}

XXAPI xrtziparchive* xrtZipArchiveOpenMemory(const void* pData, size_t iSize, bool bWritable,
	const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = __xrtZipArchiveAllocate(pOptions);
	if ( pArchive != NULL ) {
		if ( pData == NULL && iSize != 0u ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
				"invalid ZIP memory input");
		}
		else {
			__xrtZipArchiveOpenBytes(pArchive, pData, iSize,
				bWritable ? ZIP_CHECKCONS : ZIP_RDONLY | ZIP_CHECKCONS, !bWritable);
		}
	}
	return pArchive;
}

XXAPI xrtziparchive* xrtZipArchiveOpenFile(str sPath, int iMode, const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = __xrtZipArchiveAllocate(pOptions);
	xbuffer pInput = NULL;
	bool bExists;
	bool bNeedRead;
	int iFlags;
	if ( pArchive == NULL ) { return NULL; }
	if ( sPath == NULL || sPath[0] == 0 || iMode < XRT_ZIP_MODE_READ || iMode > XRT_ZIP_MODE_EXCLUSIVE ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"invalid ZIP file mode or path");
		return pArchive;
	}
	pArchive->Path = xrtCopyStr(sPath, 0);
	if ( pArchive->Path == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot copy ZIP path");
		return pArchive;
	}
	bExists = xrtFileExists(sPath);
	if ( iMode == XRT_ZIP_MODE_READ && !bExists ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP file does not exist: %s", sPath);
		return pArchive;
	}
	if ( iMode == XRT_ZIP_MODE_EXCLUSIVE && bExists ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_EXISTS, ZIP_ER_EXISTS, 0, UINT64_MAX,
			"ZIP file already exists: %s", sPath);
		return pArchive;
	}
	bNeedRead = bExists && iMode != XRT_ZIP_MODE_WRITE && iMode != XRT_ZIP_MODE_EXCLUSIVE;
	if ( bNeedRead ) {
		xreader pReader = xrtReaderOpen(sPath);
		if ( pReader != NULL ) {
			pInput = xrtReaderReadAll(pReader, pArchive->Options.tLimits.iMaxInputBytes);
			xrtReaderDestroy(pReader);
		}
		if ( pInput == NULL ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_READ, 0, UINT64_MAX,
				"cannot read ZIP file: %s", sPath);
			return pArchive;
		}
	}
	if ( iMode == XRT_ZIP_MODE_READ ) { iFlags = ZIP_RDONLY | ZIP_CHECKCONS; }
	else if ( bNeedRead ) { iFlags = ZIP_CHECKCONS; }
	else { iFlags = ZIP_CREATE | ZIP_TRUNCATE; }
	__xrtZipArchiveOpenBytes(pArchive,
		pInput != NULL ? pInput->Buffer : NULL, pInput != NULL ? pInput->Length : 0u,
		iFlags, iMode == XRT_ZIP_MODE_READ);
	xrtBufferDestroy(pInput);
	return pArchive;
}

XXAPI const xrtziperror* xrtZipArchiveError(const xrtziparchive* pArchive)
{
	return pArchive != NULL ? &pArchive->Error : NULL;
}

XXAPI bool xrtZipArchiveIsOpen(const xrtziparchive* pArchive)
{
	return pArchive != NULL && pArchive->Zip != NULL && !pArchive->Committed;
}

XXAPI bool xrtZipArchiveIsMemory(const xrtziparchive* pArchive)
{
	return pArchive != NULL && pArchive->Path == NULL;
}

XXAPI uint64 xrtZipArchiveCount(xrtziparchive* pArchive)
{
	zip_int64_t iCount;
	if ( !__xrtZipCheckOpen(pArchive, FALSE) ) { return 0u; }
	iCount = zip_get_num_entries(pArchive->Zip, 0);
	if ( iCount < 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot read ZIP entry count");
		return 0u;
	}
	return (uint64)iCount;
}

XXAPI int64 xrtZipArchiveLocate(xrtziparchive* pArchive, str sName)
{
	zip_int64_t iIndex;
	if ( !__xrtZipCheckOpen(pArchive, FALSE) || sName == NULL ) { return -1; }
	iIndex = zip_name_locate(pArchive->Zip, (const char*)sName, ZIP_FL_ENC_UTF_8);
	return iIndex;
}

XXAPI bool xrtZipArchiveEntryAt(xrtziparchive* pArchive, uint64 iIndex, xrtzipentry* pEntry)
{
	zip_stat_t tStat;
	zip_uint8_t iOpsys = 0u;
	zip_uint32_t iAttributes = 0u;
	zip_uint32_t iCommentLength = 0u;
	const char* sComment;
	if ( !__xrtZipCheckOpen(pArchive, FALSE) || pEntry == NULL ||
		iIndex > ZIP_UINT64_MAX ) { return FALSE; }
	xrtZipEntryInit(pEntry);
	if ( zip_stat_index(pArchive->Zip, (zip_uint64_t)iIndex, ZIP_FL_ENC_UTF_8, &tStat) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), iIndex,
			"cannot read ZIP entry metadata");
		return FALSE;
	}
	pEntry->iIndex = iIndex;
	pEntry->sName = tStat.name;
	pEntry->iUncompressedSize = tStat.size;
	pEntry->iCompressedSize = tStat.comp_size;
	pEntry->iMTime = (int64)tStat.mtime;
	pEntry->iCRC32 = tStat.crc;
	pEntry->iCompressionMethod = tStat.comp_method;
	pEntry->iEncryptionMethod = tStat.encryption_method;
	pEntry->bDirectory = tStat.name != NULL && tStat.name[0] != 0 &&
		tStat.name[strlen(tStat.name) - 1u] == '/';
	if ( zip_file_get_external_attributes(pArchive->Zip, (zip_uint64_t)iIndex, 0,
		&iOpsys, &iAttributes) == 0 && iOpsys == ZIP_OPSYS_UNIX ) {
		pEntry->iUnixMode = iAttributes >> 16;
	}
	sComment = zip_file_get_comment(pArchive->Zip, (zip_uint64_t)iIndex,
		&iCommentLength, ZIP_FL_ENC_UTF_8);
	pEntry->sComment = sComment != NULL ? sComment : "";
	return TRUE;
}

static bool __xrtZipEntryOutputAllowed(xrtziparchive* pArchive, const xrtzipentry* pEntry)
{
	uint64 iAllowed;
	if ( pEntry->iUncompressedSize > UINT32_MAX ||
		(pArchive->Options.tLimits.iMaxItemBytes != 0u &&
		pEntry->iUncompressedSize > pArchive->Options.tLimits.iMaxItemBytes) ||
		(pArchive->Options.tLimits.iMaxOutputBytes != 0u &&
		pEntry->iUncompressedSize > pArchive->Options.tLimits.iMaxOutputBytes) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0,
			pEntry->iIndex, "ZIP entry exceeds the configured output limit");
		return FALSE;
	}
	if ( pArchive->Options.tLimits.iMaxCompressionRatio != 0u &&
		pEntry->iCompressedSize != 0u ) {
		iAllowed = pEntry->iCompressedSize > UINT64_MAX /
			pArchive->Options.tLimits.iMaxCompressionRatio ? UINT64_MAX :
			pEntry->iCompressedSize * pArchive->Options.tLimits.iMaxCompressionRatio;
		if ( pEntry->iUncompressedSize > iAllowed ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0,
				pEntry->iIndex, "ZIP entry expansion ratio exceeds the configured limit");
			return FALSE;
		}
	}
	return TRUE;
}

XXAPI xbuffer xrtZipArchiveReadAt(xrtziparchive* pArchive, uint64 iIndex)
{
	xrtzipentry tEntry;
	zip_file_t* pFile;
	xbuffer pOutput;
	uint8 sChunk[65536];
	uint64 iTotal = 0u;
	if ( !xrtZipArchiveEntryAt(pArchive, iIndex, &tEntry) ||
		!__xrtZipEntryOutputAllowed(pArchive, &tEntry) ) { return NULL; }
	if ( tEntry.bDirectory ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, iIndex,
			"cannot read a ZIP directory as file data");
		return NULL;
	}
	pFile = zip_fopen_index(pArchive->Zip, (zip_uint64_t)iIndex, 0);
	if ( pFile == NULL ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), iIndex,
			"cannot open ZIP entry");
		return NULL;
	}
	pOutput = xrtBufferCreate(tEntry.iUncompressedSize > 65536u ?
		(uint32)tEntry.iUncompressedSize : 65536u);
	if ( pOutput == NULL ) {
		zip_fclose(pFile);
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, iIndex,
			"cannot allocate ZIP entry output");
		return NULL;
	}
	for ( ;; ) {
		zip_int64_t iRead = zip_fread(pFile, sChunk, sizeof(sChunk));
		if ( iRead < 0 ) {
			__xrtZipSetBackendError(pArchive, zip_file_get_error(pFile), iIndex,
				"cannot decompress ZIP entry");
			xrtBufferDestroy(pOutput);
			zip_fclose(pFile);
			return NULL;
		}
		if ( iRead == 0 ) { break; }
		if ( (uint64)iRead > tEntry.iUncompressedSize - iTotal ||
			!xrtBufferAppend(pOutput, sChunk, (uint32)iRead, XBUF_BINARY) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0,
				iIndex, "ZIP entry produced more data than declared");
			xrtBufferDestroy(pOutput);
			zip_fclose(pFile);
			return NULL;
		}
		iTotal += (uint64)iRead;
	}
	if ( zip_fclose(pFile) != 0 || iTotal != tEntry.iUncompressedSize ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_FORMAT, ZIP_ER_DATA_LENGTH, 0,
			iIndex, "ZIP entry length or CRC validation failed");
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}

XXAPI xbuffer xrtZipArchiveRead(xrtziparchive* pArchive, str sName)
{
	int64 iIndex = xrtZipArchiveLocate(pArchive, sName);
	if ( iIndex < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP entry does not exist: %s", sName != NULL ? (const char*)sName : "");
		return NULL;
	}
	return xrtZipArchiveReadAt(pArchive, (uint64)iIndex);
}

static bool __xrtZipMethodValid(xrtziparchive* pArchive, int* pMethod, int* pLevel)
{
	if ( *pMethod == XRT_ZIP_METHOD_DEFAULT ) { *pMethod = pArchive->Options.iDefaultMethod; }
	if ( *pLevel < 0 ) { *pLevel = pArchive->Options.iDefaultLevel; }
	if ( (*pMethod != XRT_ZIP_METHOD_STORE && *pMethod != XRT_ZIP_METHOD_DEFLATE) ||
		*pLevel < 0 || *pLevel > 9 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP method must be store/deflate and level must be 0..9");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveAdd(xrtziparchive* pArchive, str sName, const void* pData, size_t iSize,
	bool bOverwrite, int iMethod, int iLevel)
{
	zip_source_t* pSource;
	zip_int64_t iIndex;
	uint64 iCount;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) || sName == NULL || sName[0] == 0 ||
		(pData == NULL && iSize != 0u) || iSize > UINT32_MAX ||
		!xrtIsUTF8(sName, strlen((const char*)sName)) ||
		!__xrtZipEntryPathSafe((const char*)sName, FALSE, pArchive, UINT64_MAX) ||
		!__xrtZipMethodValid(pArchive, &iMethod, &iLevel) ) { return FALSE; }
	if ( (pArchive->Options.tLimits.iMaxItemBytes != 0u && iSize > pArchive->Options.tLimits.iMaxItemBytes) ||
		(pArchive->Options.tLimits.iMaxInputBytes != 0u && iSize > pArchive->Options.tLimits.iMaxInputBytes) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP entry input exceeds the configured limit");
		return FALSE;
	}
	iCount = xrtZipArchiveCount(pArchive);
	if ( pArchive->Options.tLimits.iMaxEntries != 0u &&
		iCount >= pArchive->Options.tLimits.iMaxEntries && xrtZipArchiveLocate(pArchive, sName) < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP archive entry limit reached");
		return FALSE;
	}
	pSource = __xrtZipEntrySourceCreate(pArchive, pData, iSize);
	if ( pSource == NULL ) { return FALSE; }
	iIndex = zip_file_add(pArchive->Zip, (const char*)sName, pSource,
		ZIP_FL_ENC_UTF_8 | (bOverwrite ? ZIP_FL_OVERWRITE : 0));
	if ( iIndex < 0 ) {
		zip_source_free(pSource);
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot add ZIP entry");
		return FALSE;
	}
	if ( zip_set_file_compression(pArchive->Zip, (zip_uint64_t)iIndex,
		iMethod, (zip_uint32_t)iLevel) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot configure ZIP compression");
		zip_unchange(pArchive->Zip, (zip_uint64_t)iIndex);
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveAddFile(xrtziparchive* pArchive, str sName, str sSourcePath,
	bool bOverwrite, int iMethod, int iLevel)
{
	xreader pReader;
	xbuffer pData;
	bool bRet;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) || sSourcePath == NULL || !xrtFileExists(sSourcePath) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP source file does not exist: %s", sSourcePath != NULL ? (const char*)sSourcePath : "");
		return FALSE;
	}
	pReader = xrtReaderOpen(sSourcePath);
	pData = pReader != NULL ? xrtReaderReadAll(pReader, pArchive->Options.tLimits.iMaxItemBytes) : NULL;
	xrtReaderDestroy(pReader);
	if ( pData == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_READ, 0, UINT64_MAX,
			"cannot read ZIP source file: %s", sSourcePath);
		return FALSE;
	}
	bRet = xrtZipArchiveAdd(pArchive, sName, pData->Buffer, pData->Length,
		bOverwrite, iMethod, iLevel);
	xrtBufferDestroy(pData);
	return bRet;
}

XXAPI bool xrtZipArchiveAddDirectory(xrtziparchive* pArchive, str sName)
{
	str sDirectory;
	size_t iLength;
	zip_int64_t iIndex;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) || sName == NULL || sName[0] == 0 ) { return FALSE; }
	iLength = strlen((const char*)sName);
	sDirectory = sName[iLength - 1u] == '/' ? xrtCopyStr(sName, iLength) : xrtFormat("%s/", sName);
	if ( sDirectory == NULL || !__xrtZipEntryPathSafe((const char*)sDirectory, TRUE, pArchive, UINT64_MAX) ) {
		xrtFree(sDirectory);
		return FALSE;
	}
	iIndex = zip_dir_add(pArchive->Zip, (const char*)sDirectory, ZIP_FL_ENC_UTF_8);
	xrtFree(sDirectory);
	if ( iIndex < 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot add ZIP directory");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveDelete(xrtziparchive* pArchive, str sName)
{
	int64 iIndex;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) ) { return FALSE; }
	iIndex = xrtZipArchiveLocate(pArchive, sName);
	if ( iIndex < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP entry does not exist: %s", sName != NULL ? (const char*)sName : "");
		return FALSE;
	}
	if ( zip_delete(pArchive->Zip, (zip_uint64_t)iIndex) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot delete ZIP entry");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveRename(xrtziparchive* pArchive, str sOldName, str sNewName)
{
	int64 iIndex;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) || sNewName == NULL ||
		!xrtIsUTF8(sNewName, strlen((const char*)sNewName)) ||
		!__xrtZipEntryPathSafe((const char*)sNewName, FALSE, pArchive, UINT64_MAX) ) { return FALSE; }
	iIndex = xrtZipArchiveLocate(pArchive, sOldName);
	if ( iIndex < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP entry does not exist: %s", sOldName != NULL ? (const char*)sOldName : "");
		return FALSE;
	}
	if ( zip_file_rename(pArchive->Zip, (zip_uint64_t)iIndex, (const char*)sNewName, ZIP_FL_ENC_UTF_8) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot rename ZIP entry");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveSetComment(xrtziparchive* pArchive, str sComment)
{
	size_t iLength = sComment != NULL ? strlen((const char*)sComment) : 0u;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) || iLength > UINT16_MAX ||
		(sComment != NULL && !xrtIsUTF8(sComment, iLength)) ) { return FALSE; }
	if ( zip_set_archive_comment(pArchive->Zip, sComment != NULL ? (const char*)sComment : "",
		(zip_uint16_t)iLength) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot set ZIP archive comment");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveSetEntryComment(xrtziparchive* pArchive, str sName, str sComment)
{
	int64 iIndex;
	size_t iLength = sComment != NULL ? strlen((const char*)sComment) : 0u;
	if ( !__xrtZipCheckOpen(pArchive, TRUE) || iLength > UINT16_MAX ||
		(sComment != NULL && !xrtIsUTF8(sComment, iLength)) ) { return FALSE; }
	iIndex = xrtZipArchiveLocate(pArchive, sName);
	if ( iIndex < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP entry does not exist: %s", sName != NULL ? (const char*)sName : "");
		return FALSE;
	}
	if ( zip_file_set_comment(pArchive->Zip, (zip_uint64_t)iIndex,
		sComment != NULL ? (const char*)sComment : "", (zip_uint16_t)iLength, ZIP_FL_ENC_UTF_8) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot set ZIP entry comment");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZipArchiveClose(xrtziparchive* pArchive)
{
	if ( pArchive == NULL ) { return FALSE; }
	if ( !pArchive->Committed ) {
		if ( pArchive->Zip == NULL ) { return FALSE; }
		if ( zip_close(pArchive->Zip) != 0 ) {
			__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
				"cannot commit ZIP archive");
			return FALSE;
		}
		pArchive->Zip = NULL;
		pArchive->Committed = TRUE;
	}
	if ( pArchive->Path != NULL && !pArchive->ReadOnly && !pArchive->Saved ) {
		if ( pArchive->Memory == NULL || pArchive->Memory->Base == NULL ||
			!xrtFileWriteAllAtomic(pArchive->Path, (str)pArchive->Memory->Base->Buffer,
				pArchive->Memory->Base->Length, XRT_CP_BINARY) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, UINT64_MAX,
				"cannot atomically save ZIP file: %s", pArchive->Path);
			return FALSE;
		}
		pArchive->Saved = TRUE;
	}
	return TRUE;
}

XXAPI xbuffer xrtZipArchiveTakeBuffer(xrtziparchive* pArchive)
{
	if ( pArchive == NULL || pArchive->Path != NULL || !pArchive->Committed ) { return NULL; }
	return __xrtZipMemorySourceTake(pArchive->Memory);
}

XXAPI void xrtZipArchiveDestroy(xrtziparchive* pArchive)
{
	if ( pArchive == NULL ) { return; }
	if ( pArchive->Zip != NULL ) { zip_discard(pArchive->Zip); }
	__xrtZipMemorySourceDestroy(pArchive->Memory);
	xrtFree(pArchive->Path);
	xrtFree(pArchive);
}
