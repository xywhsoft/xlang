#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING

#include "xrt.h"
#include "../third_party/libzip/lib/zipint.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>



typedef struct {
	str Path;
	str Temporary;
	xfile ReadFile;
	xfile WriteFile;
	uint64 Start;
	uint64 Size;
	uint64 Position;
	zip_int64_t Length;
	zip_error_t Error;
} xrtzipsourcefile;



/* 把 XRT 文件错误映射为 libzip 文件 Source 错误。 */
static void __xrtZipFileError(xrtzipsourcefile* pSource, int iZip, int iFallback)
{
	const xerror* pError = xrtGetError();
	int iSystem = pError != NULL ? xrtErrorSystemCode(pError) : 0;
	xerrkind Kind = pError != NULL ? xrtErrorKind(pError) : XERR_NONE;

	if ( Kind == XERR_NOT_FOUND ) { iZip = ZIP_ER_NOENT; }
	else if ( Kind == XERR_EXISTS ) { iZip = ZIP_ER_EXISTS; }
	else if ( Kind == XERR_MEMORY ) { iZip = ZIP_ER_MEMORY; }
	else if ( Kind == XERR_PERMISSION ) { iZip = ZIP_ER_OPEN; }
	zip_error_set(&pSource->Error, iZip, iSystem != 0 ? iSystem : iFallback);
}





/* 关闭读句柄；Source CLOSE 重复调用时保持幂等。 */
static bool __xrtZipFileCloseRead(xrtzipsourcefile* pSource)
{
	if ( pSource->ReadFile == NULL ) { return true; }
	if ( !xrtClose(pSource->ReadFile) ) {
		pSource->ReadFile = NULL;
		__xrtZipFileError(pSource, ZIP_ER_CLOSE, EIO);
		return false;
	}
	pSource->ReadFile = NULL;
	return true;
}





/* 清理尚未提交的写事务。 */
static void __xrtZipFileRollback(xrtzipsourcefile* pSource)
{
	if ( pSource->WriteFile != NULL ) {
		(void)xrtClose(pSource->WriteFile);
		pSource->WriteFile = NULL;
	}
	if ( pSource->Temporary != NULL ) {
		if ( xrtFileExists(pSource->Temporary) ) {
			(void)xrtFileDelete(pSource->Temporary);
		}
		xrtFree(pSource->Temporary);
		pSource->Temporary = NULL;
	}
}





/* 查询源文件区间，并验证偏移和固定长度。 */
static bool __xrtZipFileStat(xrtzipsourcefile* pSource, zip_stat_t* pStat)
{
	xfileinfo Info;
	uint64 iRemain;

	if ( !xrtPathStat(pSource->Path, true, &Info) ||
		(Info.Type != XFILE_TYPE_FILE) ) {
		__xrtZipFileError(pSource, ZIP_ER_READ, ENOENT);
		return false;
	}
	if ( pSource->Start > Info.Size ) {
		zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
		return false;
	}
	iRemain = Info.Size - pSource->Start;
	if ( pSource->Length > 0 ) {
		if ( (uint64)pSource->Length > iRemain ) {
			zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
			return false;
		}
		pSource->Size = (uint64)pSource->Length;
	} else {
		pSource->Size = iRemain;
	}
	if ( pStat != NULL ) {
		zip_stat_init(pStat);
		pStat->size = pSource->Size;
		pStat->mtime = (time_t)(Info.Modified / 1000000);
		pStat->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
	}
	return true;
}





/* 打开并定位到源区间起点。 */
static bool __xrtZipFileOpenRead(xrtzipsourcefile* pSource)
{
	uint64 iPosition;

	if ( !__xrtZipFileStat(pSource, NULL) ) { return false; }
	pSource->ReadFile = xrtOpen(pSource->Path, XFILE_READ);
	if ( pSource->ReadFile == NULL ) {
		__xrtZipFileError(pSource, ZIP_ER_OPEN, EIO);
		return false;
	}
	if ( !xrtSeek(pSource->ReadFile, (int64)pSource->Start,
		XSEEK_START, &iPosition) ) {
		__xrtZipFileError(pSource, ZIP_ER_SEEK, EIO);
		__xrtZipFileCloseRead(pSource);
		return false;
	}
	pSource->Position = 0u;
	return true;
}





/* 创建与源文件同目录的独占写事务。 */
static bool __xrtZipFileBeginWrite(xrtzipsourcefile* pSource)
{
	str sDirectory;

	if ( pSource->WriteFile != NULL ) {
		zip_error_set(&pSource->Error, ZIP_ER_INUSE, EBUSY);
		return false;
	}
	sDirectory = xrtPathParent(pSource->Path);
	if ( sDirectory == NULL ) {
		__xrtZipFileError(pSource, ZIP_ER_MEMORY, ENOMEM);
		return false;
	}
	pSource->WriteFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-libzip-", ".tmp", &pSource->Temporary);
	xrtFree(sDirectory);
	if ( pSource->WriteFile == NULL ) {
		__xrtZipFileError(pSource, ZIP_ER_TMPOPEN, EIO);
		return false;
	}
	return true;
}





/* 提交写事务并原子替换 Source 路径。 */
static bool __xrtZipFileCommitWrite(xrtzipsourcefile* pSource)
{
	if ( (pSource->WriteFile == NULL) || (pSource->Temporary == NULL) ) {
		zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
		return false;
	}
	if ( !xrtFlush(pSource->WriteFile) ) {
		__xrtZipFileError(pSource, ZIP_ER_WRITE, EIO);
		return false;
	}
	if ( !xrtClose(pSource->WriteFile) ) {
		pSource->WriteFile = NULL;
		__xrtZipFileError(pSource, ZIP_ER_CLOSE, EIO);
		return false;
	}
	pSource->WriteFile = NULL;
	if ( !xrtPathRename(pSource->Temporary, pSource->Path, true) ) {
		__xrtZipFileError(pSource, ZIP_ER_RENAME, EIO);
		return false;
	}
	xrtFree(pSource->Temporary);
	pSource->Temporary = NULL;
	return true;
}





/* 执行 libzip 文件 Source 状态机。 */
static zip_int64_t __xrtZipFileCallback(void* pState, void* pData,
	zip_uint64_t iLength, zip_source_cmd_t iCommand)
{
	xrtzipsourcefile* pSource = (xrtzipsourcefile*)pState;

	switch ( iCommand ) {
		case ZIP_SOURCE_ACCEPT_EMPTY:
			return 1;

		case ZIP_SOURCE_OPEN:
			return __xrtZipFileOpenRead(pSource) ? 0 : -1;

		case ZIP_SOURCE_READ: {
			uint64 iRemain;
			size_t iRequest;
			size_t iRead = 0u;

			if ( (pSource->ReadFile == NULL) || (iLength > SIZE_MAX) ) {
				zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
				return -1;
			}
			iRemain = pSource->Position < pSource->Size ?
				pSource->Size - pSource->Position : 0u;
			iRequest = (size_t)(iLength < iRemain ? iLength : iRemain);
			if ( !xrtRead(pSource->ReadFile, pData, iRequest, &iRead) ) {
				__xrtZipFileError(pSource, ZIP_ER_READ, EIO);
				return -1;
			}
			pSource->Position += iRead;
			return (zip_int64_t)iRead;
		}

		case ZIP_SOURCE_CLOSE:
			return __xrtZipFileCloseRead(pSource) ? 0 : -1;

		case ZIP_SOURCE_STAT:
			if ( iLength < sizeof(zip_stat_t) ) {
				zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
				return -1;
			}
			return __xrtZipFileStat(pSource, (zip_stat_t*)pData) ?
				(zip_int64_t)sizeof(zip_stat_t) : -1;

		case ZIP_SOURCE_ERROR:
			return zip_error_to_data(&pSource->Error, pData, iLength);

		case ZIP_SOURCE_FREE:
			(void)__xrtZipFileCloseRead(pSource);
			__xrtZipFileRollback(pSource);
			xrtFree(pSource->Path);
			zip_error_fini(&pSource->Error);
			xrtFree(pSource);
			return 0;

		case ZIP_SOURCE_SEEK: {
			zip_int64_t iOffset;
			uint64 iPosition;

			if ( pSource->ReadFile == NULL ) {
				zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
				return -1;
			}
			iOffset = zip_source_seek_compute_offset(pSource->Position,
				pSource->Size, pData, iLength, &pSource->Error);
			if ( iOffset < 0 ) { return -1; }
			if ( !xrtSeek(pSource->ReadFile,
				(int64)(pSource->Start + (uint64)iOffset), XSEEK_START, &iPosition) ) {
				__xrtZipFileError(pSource, ZIP_ER_SEEK, EIO);
				return -1;
			}
			pSource->Position = (uint64)iOffset;
			return 0;
		}

		case ZIP_SOURCE_TELL:
			return pSource->Position <= ZIP_INT64_MAX ?
				(zip_int64_t)pSource->Position : -1;

		case ZIP_SOURCE_BEGIN_WRITE:
			return __xrtZipFileBeginWrite(pSource) ? 0 : -1;

		case ZIP_SOURCE_COMMIT_WRITE:
			return __xrtZipFileCommitWrite(pSource) ? 0 : -1;

		case ZIP_SOURCE_ROLLBACK_WRITE:
			__xrtZipFileRollback(pSource);
			return 0;

		case ZIP_SOURCE_WRITE: {
			size_t iWritten = 0u;

			if ( (pSource->WriteFile == NULL) || (iLength > SIZE_MAX) ) {
				zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
				return -1;
			}
			if ( !xrtWriteFull(pSource->WriteFile, pData,
				(size_t)iLength, &iWritten) ) {
				__xrtZipFileError(pSource, ZIP_ER_WRITE, EIO);
				return -1;
			}
			return (zip_int64_t)iWritten;
		}

		case ZIP_SOURCE_SEEK_WRITE: {
			struct zip_source_args_seek* pSeek;
			xseek Origin;
			uint64 iPosition;

			if ( (pSource->WriteFile == NULL) || (iLength < sizeof(*pSeek)) ) {
				zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
				return -1;
			}
			pSeek = (struct zip_source_args_seek*)pData;
			if ( pSeek->whence == SEEK_SET ) { Origin = XSEEK_START; }
			else if ( pSeek->whence == SEEK_CUR ) { Origin = XSEEK_CURRENT; }
			else if ( pSeek->whence == SEEK_END ) { Origin = XSEEK_END; }
			else {
				zip_error_set(&pSource->Error, ZIP_ER_INVAL, EINVAL);
				return -1;
			}
			if ( !xrtSeek(pSource->WriteFile, pSeek->offset, Origin, &iPosition) ) {
				__xrtZipFileError(pSource, ZIP_ER_SEEK, EIO);
				return -1;
			}
			return 0;
		}

		case ZIP_SOURCE_TELL_WRITE: {
			uint64 iPosition;

			if ( (pSource->WriteFile == NULL) ||
				!xrtTell(pSource->WriteFile, &iPosition) ||
				(iPosition > ZIP_INT64_MAX) ) {
				__xrtZipFileError(pSource, ZIP_ER_TELL, EOVERFLOW);
				return -1;
			}
			return (zip_int64_t)iPosition;
		}

		case ZIP_SOURCE_REMOVE:
			if ( !xrtFileExists(pSource->Path) ) { return 0; }
			if ( !xrtFileDelete(pSource->Path) ) {
				__xrtZipFileError(pSource, ZIP_ER_REMOVE, EIO);
				return -1;
			}
			return 0;

		case ZIP_SOURCE_SUPPORTS:
			return ZIP_SOURCE_SUPPORTS_WRITABLE |
				ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ACCEPT_EMPTY);

		default:
			zip_error_set(&pSource->Error, ZIP_ER_OPNOTSUPP, EINVAL);
			return -1;
	}
}





/* 创建保留 UTF-8 路径的 XRT 文件 Source。 */
ZIP_EXTERN zip_source_t* zip_source_file_create(const char* sName,
	zip_uint64_t iStart, zip_int64_t iLength, zip_error_t* pError)
{
	xrtzipsourcefile* pSource;
	zip_source_t* pZipSource;

	if ( (sName == NULL) || (sName[0] == 0) || (iLength < -1) ||
		(iStart > INT64_MAX) ) {
		zip_error_set(pError, ZIP_ER_INVAL, EINVAL);
		return NULL;
	}
	pSource = (xrtzipsourcefile*)xrtMalloc(sizeof(*pSource));
	if ( pSource == NULL ) {
		zip_error_set(pError, ZIP_ER_MEMORY, ENOMEM);
		return NULL;
	}
	memset(pSource, 0, sizeof(*pSource));
	zip_error_init(&pSource->Error);
	pSource->Path = xrtStrDup(sName);
	pSource->Start = iStart;
	pSource->Length = iLength;
	if ( pSource->Path == NULL ) {
		zip_error_fini(&pSource->Error);
		xrtFree(pSource);
		zip_error_set(pError, ZIP_ER_MEMORY, ENOMEM);
		return NULL;
	}
	pZipSource = zip_source_function_create(__xrtZipFileCallback, pSource, pError);
	if ( pZipSource == NULL ) {
		xrtFree(pSource->Path);
		zip_error_fini(&pSource->Error);
		xrtFree(pSource);
	}
	return pZipSource;
}





/* 创建归属于指定归档错误域的文件 Source。 */
ZIP_EXTERN zip_source_t* zip_source_file(zip_t* pArchive, const char* sName,
	zip_uint64_t iStart, zip_int64_t iLength)
{
	if ( pArchive == NULL ) { return NULL; }
	return zip_source_file_create(sName, iStart, iLength, &pArchive->error);
}
