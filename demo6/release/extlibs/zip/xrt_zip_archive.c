#include "xrt_zip_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>



typedef struct {
	xbuffer* Data;
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
		uint64 iRemain = pSource->Position < pSource->Data->Size ?
			pSource->Data->Size - pSource->Position : 0u;
		uint64 iRead = iLength < iRemain ? iLength : iRemain;
		if ( iRead != 0u ) {
			memcpy(pData, pSource->Data->Data + pSource->Position, (size_t)iRead);
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
		pStat->size = pSource->Data->Size;
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
			pSource->Data->Size, pData, iLength, &pSource->Error);
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
	pEntry->Data = __xrtZipBufferCreate(iSize > 65536u ? iSize : 65536u);
	if ( pEntry->Data == NULL || (iSize != 0u &&
		!xrtBufferAppend(pEntry->Data,
			(xbytesview){ (const unsigned char*)pData, iSize })) ) {
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
	if ( iSystem == EFBIG ) { iCode = XRT_ZIP_ERROR_LIMIT; }
	else if ( iZip == ZIP_ER_MEMORY ) { iCode = XRT_ZIP_ERROR_MEMORY; }
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

/* 调用方 Writer 的内存故障保留原分类，其他失败作为 I/O。 */
void __xrtZipSetOutputError(xrtziparchive* pArchive, uint64 iEntry,
	int iBackend, const char* sMessage, cstr sPath)
{
	const xerror* pCause = xrtGetError();
	bool bMemory = pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY;
	if ( sPath != NULL ) {
		__xrtZipSetError(pArchive,
			bMemory ? XRT_ZIP_ERROR_MEMORY : XRT_ZIP_ERROR_IO,
			bMemory ? ZIP_ER_MEMORY : iBackend, 0, iEntry,
			"%s: %s", sMessage, sPath);
	} else {
		__xrtZipSetError(pArchive,
			bMemory ? XRT_ZIP_ERROR_MEMORY : XRT_ZIP_ERROR_IO,
			bMemory ? ZIP_ER_MEMORY : iBackend, 0, iEntry, "%s", sMessage);
	}
}

bool __xrtZipCheckOpen(xrtziparchive* pArchive, bool bWrite)
{
	if ( pArchive == NULL || pArchive->Zip == NULL || pArchive->Committed ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_STATE, ZIP_ER_ZIPCLOSED, 0, UINT64_MAX,
			"ZIP archive is closed");
		return false;
	}
	if ( bWrite && pArchive->ReadOnly ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_STATE, ZIP_ER_RDONLY, 0, UINT64_MAX,
			"ZIP archive is read-only");
		return false;
	}
	return true;
}



XRT_API void xrtZipOptionsInit(xrtzipoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_ZIP_OPTIONS_VERSION;
	pOptions->iDefaultMethod = XRT_ZIP_METHOD_DEFLATE;
	pOptions->iDefaultLevel = 6;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->tLimits.iMaxNodes = 0u;
	pOptions->tLimits.iMaxDepth = 0u;
}

XRT_API void xrtZipErrorInit(xrtziperror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iEntryIndex = UINT64_MAX;
}

XRT_API void xrtZipEntryInit(xrtzipentry* pEntry)
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
		if ( pOptions->iSize != sizeof(*pOptions) ||
			pOptions->iVersion != XRT_ZIP_OPTIONS_VERSION ||
			pOptions->iReserved != 0u ||
			pOptions->tLimits.iSize != sizeof(pOptions->tLimits) ||
			pOptions->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
			pOptions->tLimits.iMaxNodes != 0u ||
			pOptions->tLimits.iMaxDepth != 0u ||
			pOptions->tLimits.iFlags != 0u ||
			pOptions->tLimits.iReserved != 0u ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
				"invalid ZIP options");
			return false;
		}
		pArchive->Options = *pOptions;
	}
	if ( pArchive->Options.iDefaultMethod != XRT_ZIP_METHOD_STORE &&
		pArchive->Options.iDefaultMethod != XRT_ZIP_METHOD_DEFLATE ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP compression method must be store or deflate");
		return false;
	}
	if ( pArchive->Options.iDefaultLevel < 0 || pArchive->Options.iDefaultLevel > 9 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP compression level must be 0..9");
		return false;
	}
	return true;
}

/* 验证已打开归档的条目数量上限。 */
static bool __xrtZipArchiveCheckCount(xrtziparchive* pArchive)
{
	zip_int64_t iCount = zip_get_num_entries(pArchive->Zip, 0);

	if ( iCount < 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot read ZIP entry count");
		return false;
	}
	if ( (pArchive->Options.tLimits.iMaxEntries != 0u) &&
		((uint64)iCount > pArchive->Options.tLimits.iMaxEntries) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP archive contains too many entries");
		return false;
	}
	return true;
}





/* 验证磁盘归档输入，并取得可靠的普通文件大小。 */
static bool __xrtZipArchiveCheckFile(xrtziparchive* pArchive, cstr sPath)
{
	xfileinfo Info;

	if ( !xrtPathStat(sPath, true, &Info) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_READ, 0, UINT64_MAX,
			"cannot inspect ZIP file: %s", sPath);
		return false;
	}
	if ( Info.Type != XFILE_TYPE_FILE ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP path is not a regular file: %s", sPath);
		return false;
	}
	if ( (pArchive->Options.tLimits.iMaxInputBytes != 0u) &&
		(Info.Size > pArchive->Options.tLimits.iMaxInputBytes) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP input exceeds the configured limit");
		return false;
	}
	return true;
}





/* 通过 libzip 打开路径，并把后端错误保存到归档对象。 */
static bool __xrtZipArchiveOpenPath(xrtziparchive* pArchive, cstr sPath,
	int iFlags, bool bReadOnly)
{
	zip_error_t Error;
	int iCode = ZIP_ER_OK;

	pArchive->Zip = zip_open(sPath, iFlags, &iCode);
	if ( pArchive->Zip == NULL ) {
		zip_error_init_with_code(&Error, iCode);
		__xrtZipSetBackendError(pArchive, &Error, UINT64_MAX, "cannot open ZIP archive");
		zip_error_fini(&Error);
		return false;
	}
	pArchive->ReadOnly = bReadOnly;
	return __xrtZipArchiveCheckCount(pArchive);
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
	if ( pArchive->Error.iCode != XRT_ZIP_OK ) { return false; }
	if ( iSize > pArchive->Options.tLimits.iMaxInputBytes &&
		pArchive->Options.tLimits.iMaxInputBytes != 0u ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP input exceeds the configured limit");
		return false;
	}
	zip_error_init(&tError);
	pArchive->Memory = __xrtZipMemorySourceCreate(pData, iSize,
		bReadOnly ? 0u : pArchive->Options.tLimits.iMaxOutputBytes, &tError, &pSource);
	if ( pArchive->Memory == NULL ) {
		__xrtZipSetBackendError(pArchive, &tError, UINT64_MAX, "cannot create ZIP source");
		zip_error_fini(&tError);
		return false;
	}
	pArchive->Zip = zip_open_from_source(pSource, iFlags, &tError);
	if ( pArchive->Zip == NULL ) {
		__xrtZipSetBackendError(pArchive, &tError, UINT64_MAX, "cannot open ZIP archive");
		zip_source_free(pSource);
		zip_error_fini(&tError);
		return false;
	}
	zip_error_fini(&tError);
	pArchive->ReadOnly = bReadOnly;
	return __xrtZipArchiveCheckCount(pArchive);
}

XRT_API xrtziparchive* xrtZipArchiveCreate(const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = __xrtZipArchiveAllocate(pOptions);
	if ( pArchive != NULL ) {
		__xrtZipArchiveOpenBytes(pArchive, NULL, 0u, ZIP_CREATE | ZIP_TRUNCATE, false);
	}
	return pArchive;
}

XRT_API xrtziparchive* xrtZipArchiveOpenMemory(const void* pData, size_t iSize, bool bWritable,
	const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = __xrtZipArchiveAllocate(pOptions);

	if ( pArchive == NULL ) { return NULL; }
	if ( pArchive->Error.iCode != XRT_ZIP_OK ) { return pArchive; }
	if ( (pData == NULL) && (iSize != 0u) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"invalid ZIP memory input");
	} else {
		__xrtZipArchiveOpenBytes(pArchive, pData, iSize,
			bWritable ? ZIP_CHECKCONS : ZIP_RDONLY | ZIP_CHECKCONS, !bWritable);
	}
	return pArchive;
}

XRT_API xrtziparchive* xrtZipArchiveOpenFile(cstr sPath, int iMode,
	const xrtzipoptions* pOptions)
{
	xrtziparchive* pArchive = __xrtZipArchiveAllocate(pOptions);
	str sDirectory;
	xfile File;
	bool bExists;
	int iFlags;

	if ( pArchive == NULL ) { return NULL; }
	if ( pArchive->Error.iCode != XRT_ZIP_OK ) { return pArchive; }
	if ( (sPath == NULL) || (sPath[0] == 0) ||
		(iMode < XRT_ZIP_MODE_READ) || (iMode > XRT_ZIP_MODE_EXCLUSIVE) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"invalid ZIP file mode or path");
		return pArchive;
	}
	pArchive->Path = xrtStrDup(sPath);
	if ( pArchive->Path == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot copy ZIP path");
		return pArchive;
	}
	bExists = xrtFileExists(sPath);
	if ( (iMode == XRT_ZIP_MODE_READ) && !bExists ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP file does not exist: %s", sPath);
		return pArchive;
	}
	if ( (iMode == XRT_ZIP_MODE_EXCLUSIVE) && bExists ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_EXISTS, ZIP_ER_EXISTS, 0, UINT64_MAX,
			"ZIP file already exists: %s", sPath);
		return pArchive;
	}
	if ( bExists && ((iMode == XRT_ZIP_MODE_READ) || (iMode == XRT_ZIP_MODE_APPEND)) &&
		!__xrtZipArchiveCheckFile(pArchive, sPath) ) {
		return pArchive;
	}
	if ( iMode == XRT_ZIP_MODE_READ ) {
		__xrtZipArchiveOpenPath(pArchive, sPath, ZIP_RDONLY | ZIP_CHECKCONS, true);
		return pArchive;
	}

	/* 磁盘写入始终在目标同目录完成，提交时再原子替换。 */
	sDirectory = xrtPathParent(sPath);
	if ( sDirectory == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot build ZIP transaction directory");
		return pArchive;
	}
	File = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-zip-", ".tmp", &pArchive->TempPath);
	xrtFree(sDirectory);
	if ( File == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_OPEN, 0, UINT64_MAX,
			"cannot create ZIP transaction file: %s", sPath);
		return pArchive;
	}
	if ( !xrtClose(File) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_CLOSE, 0, UINT64_MAX,
			"cannot close ZIP transaction file: %s", pArchive->TempPath);
		return pArchive;
	}
	if ( bExists && (iMode == XRT_ZIP_MODE_APPEND) ) {
		if ( !xrtFileCopy(sPath, pArchive->TempPath, true) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_READ, 0, UINT64_MAX,
				"cannot stage ZIP file for update: %s", sPath);
			return pArchive;
		}
		iFlags = ZIP_CHECKCONS;
	} else {
		iFlags = ZIP_CREATE | ZIP_TRUNCATE;
	}
	pArchive->ReplaceTarget = iMode != XRT_ZIP_MODE_EXCLUSIVE;
	__xrtZipArchiveOpenPath(pArchive, pArchive->TempPath, iFlags, false);
	return pArchive;
}

XRT_API const xrtziperror* xrtZipArchiveError(const xrtziparchive* pArchive)
{
	return pArchive != NULL ? &pArchive->Error : NULL;
}

XRT_API bool xrtZipArchiveIsOpen(const xrtziparchive* pArchive)
{
	return pArchive != NULL && pArchive->Zip != NULL && !pArchive->Committed;
}

XRT_API bool xrtZipArchiveIsMemory(const xrtziparchive* pArchive)
{
	return pArchive != NULL && pArchive->Path == NULL;
}

XRT_API uint64 xrtZipArchiveCount(xrtziparchive* pArchive)
{
	zip_int64_t iCount;
	if ( !__xrtZipCheckOpen(pArchive, false) ) { return 0u; }
	iCount = zip_get_num_entries(pArchive->Zip, 0);
	if ( iCount < 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot read ZIP entry count");
		return 0u;
	}
	return (uint64)iCount;
}





/* 验证用于查询或修改的 ZIP 条目名。 */
static bool __xrtZipEntryNameValid(xrtziparchive* pArchive, cstr sName)
{
	if ( (sName == NULL) || (sName[0] == 0) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP entry name is required");
		return false;
	}
	if ( !xrtUtf8Valid((xstrview){ sName, strlen(sName) }, NULL) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP entry name is not valid UTF-8");
		return false;
	}
	return true;
}





XRT_API int64 xrtZipArchiveLocate(xrtziparchive* pArchive, cstr sName)
{
	zip_int64_t iIndex;

	if ( !__xrtZipCheckOpen(pArchive, false) ) { return -1; }
	if ( !__xrtZipEntryNameValid(pArchive, sName) ) { return -1; }
	iIndex = zip_name_locate(pArchive->Zip, (const char*)sName, ZIP_FL_ENC_UTF_8);
	if ( iIndex < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP entry does not exist: %s", sName);
	}
	return iIndex;
}

XRT_API bool xrtZipArchiveEntryAt(xrtziparchive* pArchive, uint64 iIndex, xrtzipentry* pEntry)
{
	zip_stat_t tStat;
	zip_uint8_t iOpsys = 0u;
	zip_uint32_t iAttributes = 0u;
	zip_uint32_t iCommentLength = 0u;
	const char* sComment;
	if ( !__xrtZipCheckOpen(pArchive, false) ) { return false; }
	if ( pEntry == NULL || iIndex > ZIP_UINT64_MAX ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, iIndex,
			"invalid ZIP entry metadata request");
		return false;
	}
	xrtZipEntryInit(pEntry);
	if ( zip_stat_index(pArchive->Zip, (zip_uint64_t)iIndex, ZIP_FL_ENC_UTF_8, &tStat) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), iIndex,
			"cannot read ZIP entry metadata");
		return false;
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
	return true;
}

static bool __xrtZipEntryOutputAllowed(xrtziparchive* pArchive, const xrtzipentry* pEntry)
{
	uint64 iAllowed;
	if ( (pArchive->Options.tLimits.iMaxItemBytes != 0u &&
		pEntry->iUncompressedSize > pArchive->Options.tLimits.iMaxItemBytes) ||
		(pArchive->Options.tLimits.iMaxOutputBytes != 0u &&
		pEntry->iUncompressedSize > pArchive->Options.tLimits.iMaxOutputBytes) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0,
			pEntry->iIndex, "ZIP entry exceeds the configured output limit");
		return false;
	}
	if ( pArchive->Options.tLimits.iMaxCompressionRatio != 0u &&
		pEntry->iCompressedSize != 0u ) {
		iAllowed = pEntry->iCompressedSize > UINT64_MAX /
			pArchive->Options.tLimits.iMaxCompressionRatio ? UINT64_MAX :
			pEntry->iCompressedSize * pArchive->Options.tLimits.iMaxCompressionRatio;
		if ( pEntry->iUncompressedSize > iAllowed ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0,
				pEntry->iIndex, "ZIP entry expansion ratio exceeds the configured limit");
			return false;
		}
	}
	return true;
}




/* 把一个普通文件条目流式写入调用方 Writer。 */
XRT_API bool xrtZipArchiveReadAtTo(xrtziparchive* pArchive, uint64 iIndex,
	xwriter* pWriter)
{
	xrtzipentry tEntry;
	zip_file_t* pFile;
	uint8 sChunk[65536];
	uint64 iTotal = 0u;

	if ( !__xrtZipCheckOpen(pArchive, false) ) { return false; }
	if ( pWriter == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, iIndex,
			"ZIP entry output Writer is null");
		return false;
	}
	if ( !xrtZipArchiveEntryAt(pArchive, iIndex, &tEntry) ||
		!__xrtZipEntryOutputAllowed(pArchive, &tEntry) ) { return false; }
	if ( tEntry.bDirectory ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, iIndex,
			"cannot read a ZIP directory as file data");
		return false;
	}
	pFile = zip_fopen_index(pArchive->Zip, (zip_uint64_t)iIndex, 0);
	if ( pFile == NULL ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), iIndex,
			"cannot open ZIP entry");
		return false;
	}
	for ( ;; ) {
		zip_int64_t iRead = zip_fread(pFile, sChunk, sizeof(sChunk));

		if ( iRead < 0 ) {
			__xrtZipSetBackendError(pArchive, zip_file_get_error(pFile), iIndex,
				"cannot decompress ZIP entry");
			zip_fclose(pFile);
			return false;
		}
		if ( iRead == 0 ) { break; }
		if ( (uint64)iRead > (tEntry.iUncompressedSize - iTotal) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0,
				iIndex, "ZIP entry produced more data than declared");
			zip_fclose(pFile);
			return false;
		}
		if ( !xrtWriterWriteFull(pWriter, sChunk, (size_t)iRead, NULL) ) {
			__xrtZipSetOutputError(pArchive, iIndex, ZIP_ER_WRITE,
				"cannot write ZIP entry output", NULL);
			zip_fclose(pFile);
			return false;
		}
		iTotal += (uint64)iRead;
	}
	if ( (zip_fclose(pFile) != 0) || (iTotal != tEntry.iUncompressedSize) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_FORMAT, ZIP_ER_DATA_LENGTH, 0,
			iIndex, "ZIP entry length or CRC validation failed");
		return false;
	}
	return true;
}




/* 按名称把条目流式写入调用方 Writer。 */
XRT_API bool xrtZipArchiveReadTo(xrtziparchive* pArchive, cstr sName,
	xwriter* pWriter)
{
	int64 iIndex = xrtZipArchiveLocate(pArchive, sName);

	if ( iIndex < 0 ) { return false; }
	return xrtZipArchiveReadAtTo(pArchive, (uint64)iIndex, pWriter);
}





/* 读取一个条目到新 Buffer；大条目按需增长，不按声明大小一次预留。 */
XRT_API xbuffer* xrtZipArchiveReadAt(xrtziparchive* pArchive, uint64 iIndex)
{
	xrtzipentry Entry;
	xbuffer* pOutput;
	xwriter* pWriter;
	size_t iCapacity;
	bool bRead;
	bool bClose;

	if ( !xrtZipArchiveEntryAt(pArchive, iIndex, &Entry) ) { return NULL; }
	iCapacity = Entry.iUncompressedSize < 65536u ?
		(size_t)Entry.iUncompressedSize : 65536u;
	pOutput = __xrtZipBufferCreate(iCapacity);
	if ( pOutput == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, iIndex,
			"cannot allocate ZIP entry output");
		return NULL;
	}
	pWriter = xrtWriterFromBuffer(pOutput);
	if ( pWriter == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, iIndex,
			"cannot create ZIP entry output Writer");
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	bRead = xrtZipArchiveReadAtTo(pArchive, iIndex, pWriter);
	bClose = xrtWriterDestroy(pWriter);
	if ( !bRead || !bClose ) {
		if ( bRead ) {
			__xrtZipSetOutputError(pArchive, iIndex, ZIP_ER_CLOSE,
				"cannot close ZIP entry output Writer", NULL);
		}
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}





/* 按名称读取一个条目到新 Buffer。 */
XRT_API xbuffer* xrtZipArchiveRead(xrtziparchive* pArchive, cstr sName)
{
	int64 iIndex = xrtZipArchiveLocate(pArchive, sName);

	if ( iIndex < 0 ) { return NULL; }
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
		return false;
	}
	return true;
}




/* 验证新条目的名称、大小、压缩参数和条目数量。 */
static bool __xrtZipEntryInputValid(xrtziparchive* pArchive, cstr sName,
	uint64 iSize, int* pMethod, int* pLevel)
{
	uint64 iCount;

	if ( !__xrtZipEntryNameValid(pArchive, sName) ) { return false; }
	if ( !__xrtZipEntryPathSafe((const char*)sName, false,
		pArchive, UINT64_MAX) ) { return false; }
	if ( !__xrtZipMethodValid(pArchive, pMethod, pLevel) ) { return false; }
	if ( ((pArchive->Options.tLimits.iMaxItemBytes != 0u) &&
		(iSize > pArchive->Options.tLimits.iMaxItemBytes)) ||
		((pArchive->Options.tLimits.iMaxInputBytes != 0u) &&
		(iSize > pArchive->Options.tLimits.iMaxInputBytes)) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP entry input exceeds the configured limit");
		return false;
	}
	iCount = xrtZipArchiveCount(pArchive);
	if ( pArchive->Options.tLimits.iMaxEntries != 0u &&
		iCount >= pArchive->Options.tLimits.iMaxEntries && xrtZipArchiveLocate(pArchive, sName) < 0 ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP archive entry limit reached");
		return false;
	}
	return true;
}





/* 把一个已经验证的 libzip Source 接入归档，成功后归档接管 Source。 */
static bool __xrtZipArchiveAddSource(xrtziparchive* pArchive, cstr sName,
	zip_source_t* pSource, bool bOverwrite, int iMethod, int iLevel)
{
	zip_int64_t iIndex;

	iIndex = zip_file_add(pArchive->Zip, (const char*)sName, pSource,
		ZIP_FL_ENC_UTF_8 | (bOverwrite ? ZIP_FL_OVERWRITE : 0));
	if ( iIndex < 0 ) {
		zip_source_free(pSource);
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot add ZIP entry");
		return false;
	}
	if ( zip_set_file_compression(pArchive->Zip, (zip_uint64_t)iIndex,
		iMethod, (zip_uint32_t)iLevel) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot configure ZIP compression");
		zip_unchange(pArchive->Zip, (zip_uint64_t)iIndex);
		return false;
	}
	return true;
}






/* 复制调用方字节并添加普通文件条目。 */
XRT_API bool xrtZipArchiveAdd(xrtziparchive* pArchive, cstr sName,
	const void* pData, size_t iSize, bool bOverwrite, int iMethod, int iLevel)
{
	zip_source_t* pSource;

	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	if ( (pData == NULL) && (iSize != 0u) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"invalid ZIP entry input");
		return false;
	}
	if ( !__xrtZipEntryInputValid(pArchive, sName, (uint64)iSize,
		&iMethod, &iLevel) ) {
		return false;
	}
	pSource = __xrtZipEntrySourceCreate(pArchive, pData, iSize);
	if ( pSource == NULL ) { return false; }
	return __xrtZipArchiveAddSource(pArchive, sName, pSource,
		bOverwrite, iMethod, iLevel);
}





/* 使用 libzip 文件源流式添加普通文件，避免把源文件整体装入内存。 */
XRT_API bool xrtZipArchiveAddFile(xrtziparchive* pArchive, cstr sName, cstr sSourcePath,
	bool bOverwrite, int iMethod, int iLevel)
{
	xfileinfo Info;
	zip_source_t* pSource;

	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	if ( (sSourcePath == NULL) || !xrtPathStat(sSourcePath, true, &Info) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_NOT_FOUND, ZIP_ER_NOENT, 0, UINT64_MAX,
			"ZIP source file does not exist: %s", sSourcePath != NULL ? (const char*)sSourcePath : "");
		return false;
	}
	if ( Info.Type != XFILE_TYPE_FILE ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP source path is not a regular file: %s", sSourcePath);
		return false;
	}
	if ( !__xrtZipEntryInputValid(pArchive, sName, Info.Size,
		&iMethod, &iLevel) ) {
		return false;
	}
	pSource = zip_source_file(pArchive->Zip, sSourcePath, 0u, ZIP_LENGTH_TO_END);
	if ( pSource == NULL ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot create ZIP file source");
		return false;
	}
	return __xrtZipArchiveAddSource(pArchive, sName, pSource,
		bOverwrite, iMethod, iLevel);
}

XRT_API bool xrtZipArchiveAddDirectory(xrtziparchive* pArchive, cstr sName)
{
	str sDirectory;
	size_t iLength;
	uint64 iCount;
	zip_int64_t iIndex;

	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	if ( !__xrtZipEntryNameValid(pArchive, sName) ) { return false; }
	iCount = xrtZipArchiveCount(pArchive);
	if ( (pArchive->Options.tLimits.iMaxEntries != 0u) &&
		(iCount >= pArchive->Options.tLimits.iMaxEntries) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP archive entry limit reached");
		return false;
	}
	iLength = strlen((const char*)sName);
	sDirectory = sName[iLength - 1u] == '/' ?
		xrtStrDupN(sName, iLength) : xrtFormat("%s/", sName);
	if ( sDirectory == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot copy ZIP directory name");
		return false;
	}
	if ( !__xrtZipEntryPathSafe((const char*)sDirectory,
		true, pArchive, UINT64_MAX) ) {
		xrtFree(sDirectory);
		return false;
	}
	iIndex = zip_dir_add(pArchive->Zip, (const char*)sDirectory, ZIP_FL_ENC_UTF_8);
	xrtFree(sDirectory);
	if ( iIndex < 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot add ZIP directory");
		return false;
	}
	return true;
}

XRT_API bool xrtZipArchiveDelete(xrtziparchive* pArchive, cstr sName)
{
	int64 iIndex;
	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	iIndex = xrtZipArchiveLocate(pArchive, sName);
	if ( iIndex < 0 ) { return false; }
	if ( zip_delete(pArchive->Zip, (zip_uint64_t)iIndex) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot delete ZIP entry");
		return false;
	}
	return true;
}

XRT_API bool xrtZipArchiveRename(xrtziparchive* pArchive, cstr sOldName, cstr sNewName)
{
	int64 iIndex;

	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	if ( !__xrtZipEntryNameValid(pArchive, sNewName) ) { return false; }
	if ( !__xrtZipEntryPathSafe((const char*)sNewName,
		false, pArchive, UINT64_MAX) ) { return false; }
	iIndex = xrtZipArchiveLocate(pArchive, sOldName);
	if ( iIndex < 0 ) { return false; }
	if ( zip_file_rename(pArchive->Zip, (zip_uint64_t)iIndex,
		(const char*)sNewName, ZIP_FL_ENC_UTF_8) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot rename ZIP entry");
		return false;
	}
	return true;
}

XRT_API bool xrtZipArchiveSetComment(xrtziparchive* pArchive, cstr sComment)
{
	size_t iLength = sComment != NULL ? strlen((const char*)sComment) : 0u;

	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	if ( (iLength > UINT16_MAX) || ((sComment != NULL) &&
		!xrtUtf8Valid((xstrview){ sComment, iLength }, NULL)) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP archive comment is too long or not valid UTF-8");
		return false;
	}
	if ( zip_set_archive_comment(pArchive->Zip, sComment != NULL ? (const char*)sComment : "",
		(zip_uint16_t)iLength) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
			"cannot set ZIP archive comment");
		return false;
	}
	return true;
}

XRT_API bool xrtZipArchiveSetEntryComment(xrtziparchive* pArchive, cstr sName, cstr sComment)
{
	int64 iIndex;
	size_t iLength = sComment != NULL ? strlen((const char*)sComment) : 0u;

	if ( !__xrtZipCheckOpen(pArchive, true) ) { return false; }
	if ( (iLength > UINT16_MAX) || ((sComment != NULL) &&
		!xrtUtf8Valid((xstrview){ sComment, iLength }, NULL)) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP entry comment is too long or not valid UTF-8");
		return false;
	}
	iIndex = xrtZipArchiveLocate(pArchive, sName);
	if ( iIndex < 0 ) { return false; }
	if ( zip_file_set_comment(pArchive->Zip, (zip_uint64_t)iIndex,
		sComment != NULL ? (const char*)sComment : "", (zip_uint16_t)iLength, ZIP_FL_ENC_UTF_8) != 0 ) {
		__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), (uint64)iIndex,
			"cannot set ZIP entry comment");
		return false;
	}
	return true;
}

XRT_API bool xrtZipArchiveClose(xrtziparchive* pArchive)
{
	xfileinfo Info;

	if ( pArchive == NULL ) { return false; }
	if ( !pArchive->Committed ) {
		if ( pArchive->Zip == NULL ) { return false; }
		if ( zip_close(pArchive->Zip) != 0 ) {
			__xrtZipSetBackendError(pArchive, zip_get_error(pArchive->Zip), UINT64_MAX,
				"cannot commit ZIP archive");
			return false;
		}
		pArchive->Zip = NULL;
		pArchive->Committed = true;
	}
	if ( (pArchive->Path != NULL) && !pArchive->ReadOnly && !pArchive->Saved ) {
		if ( (pArchive->TempPath == NULL) ||
			!xrtPathStat(pArchive->TempPath, true, &Info) ||
			(Info.Type != XFILE_TYPE_FILE) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, UINT64_MAX,
				"cannot inspect committed ZIP transaction: %s", pArchive->Path);
			return false;
		}
		if ( (pArchive->Options.tLimits.iMaxOutputBytes != 0u) &&
			(Info.Size > pArchive->Options.tLimits.iMaxOutputBytes) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_WRITE, 0, UINT64_MAX,
				"committed ZIP archive exceeds the configured output limit");
			return false;
		}
		if ( !xrtPathRename(pArchive->TempPath, pArchive->Path,
			pArchive->ReplaceTarget) ) {
			__xrtZipSetError(pArchive,
				(!pArchive->ReplaceTarget && xrtPathExists(pArchive->Path)) ?
					XRT_ZIP_ERROR_EXISTS : XRT_ZIP_ERROR_IO,
				!pArchive->ReplaceTarget ? ZIP_ER_EXISTS : ZIP_ER_WRITE,
				0, UINT64_MAX, "cannot atomically save ZIP file: %s", pArchive->Path);
			return false;
		}
		pArchive->Saved = true;
		xrtFree(pArchive->TempPath);
		pArchive->TempPath = NULL;
	}
	return true;
}

XRT_API xbuffer* xrtZipArchiveTakeBuffer(xrtziparchive* pArchive)
{
	if ( pArchive == NULL ) { return NULL; }
	if ( (pArchive->Path != NULL) || !pArchive->Committed ||
		(pArchive->Memory == NULL) || (pArchive->Memory->Base == NULL) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_STATE, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP Buffer is only available after closing a memory archive");
		return NULL;
	}
	return __xrtZipMemorySourceTake(pArchive->Memory);
}

XRT_API void xrtZipArchiveDestroy(xrtziparchive* pArchive)
{
	if ( pArchive == NULL ) { return; }
	if ( pArchive->Zip != NULL ) { zip_discard(pArchive->Zip); }
	__xrtZipMemorySourceDestroy(pArchive->Memory);
	if ( (pArchive->TempPath != NULL) && xrtPathExists(pArchive->TempPath) ) {
		xrtFileDelete(pArchive->TempPath);
	}
	xrtFree(pArchive->Path);
	xrtFree(pArchive->TempPath);
	xrtFree(pArchive);
}
