#include "xrt_zip_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>



/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
xbuffer* __xrtZipBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}



static bool __xrtZipSourceBeginWrite(xrtzipmemorysource* pMemory)
{
	if ( pMemory->Write != NULL ) {
		zip_error_set(&pMemory->Error, ZIP_ER_INUSE, 0);
		return false;
	}
	pMemory->Write = __xrtZipBufferCreate(65536u);
	if ( pMemory->Write == NULL ) {
		zip_error_set(&pMemory->Error, ZIP_ER_MEMORY, 0);
		return false;
	}
	pMemory->Writer = xrtWriterFromBuffer(pMemory->Write);
	if ( pMemory->Writer == NULL ) {
		xrtBufferDestroy(pMemory->Write);
		pMemory->Write = NULL;
		zip_error_set(&pMemory->Error, ZIP_ER_MEMORY, 0);
		return false;
	}
	return true;
}

static void __xrtZipSourceRollback(xrtzipmemorysource* pMemory)
{
	if ( pMemory == NULL ) { return; }
	xrtWriterDestroy(pMemory->Writer);
	pMemory->Writer = NULL;
	xrtBufferDestroy(pMemory->Write);
	pMemory->Write = NULL;
}

static zip_int64_t __xrtZipSourceCallback(void* pState, void* pData,
	zip_uint64_t iLength, zip_source_cmd_t iCommand)
{
	xrtzipmemorysource* pMemory = (xrtzipmemorysource*)pState;
	switch ( iCommand ) {
	case ZIP_SOURCE_ACCEPT_EMPTY:
		return 1;

	case ZIP_SOURCE_OPEN:
		pMemory->ReadPosition = 0u;
		return 0;

	case ZIP_SOURCE_READ: {
		uint64 iRemain;
		uint64 iRead;
		if ( iLength > ZIP_INT64_MAX ) {
			zip_error_set(&pMemory->Error, ZIP_ER_INVAL, 0);
			return -1;
		}
		iRemain = pMemory->Base != NULL && pMemory->ReadPosition < pMemory->Base->Size ?
			pMemory->Base->Size - pMemory->ReadPosition : 0u;
		iRead = iLength < iRemain ? iLength : iRemain;
		if ( iRead != 0u ) {
			memcpy(pData, pMemory->Base->Data + pMemory->ReadPosition, (size_t)iRead);
			pMemory->ReadPosition += iRead;
		}
		return (zip_int64_t)iRead;
	}

	case ZIP_SOURCE_CLOSE:
		return 0;

	case ZIP_SOURCE_STAT: {
		zip_stat_t* pStat;
		if ( iLength < sizeof(zip_stat_t) ) {
			zip_error_set(&pMemory->Error, ZIP_ER_INVAL, 0);
			return -1;
		}
		pStat = (zip_stat_t*)pData;
		zip_stat_init(pStat);
		pStat->mtime = time(NULL);
		pStat->size = pMemory->Base != NULL ? pMemory->Base->Size : 0u;
		pStat->comp_size = pStat->size;
		pStat->comp_method = ZIP_CM_STORE;
		pStat->encryption_method = ZIP_EM_NONE;
		pStat->valid = ZIP_STAT_MTIME | ZIP_STAT_SIZE | ZIP_STAT_COMP_SIZE |
			ZIP_STAT_COMP_METHOD | ZIP_STAT_ENCRYPTION_METHOD;
		return sizeof(zip_stat_t);
	}

	case ZIP_SOURCE_ERROR:
		return zip_error_to_data(&pMemory->Error, pData, iLength);

	case ZIP_SOURCE_FREE:
		return 0;

	case ZIP_SOURCE_SEEK: {
		zip_int64_t iOffset = zip_source_seek_compute_offset(pMemory->ReadPosition,
			pMemory->Base != NULL ? pMemory->Base->Size : 0u, pData, iLength, &pMemory->Error);
		if ( iOffset < 0 ) { return -1; }
		pMemory->ReadPosition = (uint64)iOffset;
		return 0;
	}

	case ZIP_SOURCE_TELL:
		return pMemory->ReadPosition <= ZIP_INT64_MAX ? (zip_int64_t)pMemory->ReadPosition : -1;

	case ZIP_SOURCE_BEGIN_WRITE:
		return __xrtZipSourceBeginWrite(pMemory) ? 0 : -1;

	case ZIP_SOURCE_COMMIT_WRITE:
		if ( pMemory->Write == NULL ) {
			zip_error_set(&pMemory->Error, ZIP_ER_INVAL, 0);
			return -1;
		}
		if ( !xrtWriterDestroy(pMemory->Writer) ) {
			pMemory->Writer = NULL;
			zip_error_set(&pMemory->Error, ZIP_ER_WRITE, EIO);
			return -1;
		}
		pMemory->Writer = NULL;
		xrtBufferDestroy(pMemory->Base);
		pMemory->Base = pMemory->Write;
		pMemory->Write = NULL;
		return 0;

	case ZIP_SOURCE_ROLLBACK_WRITE:
		__xrtZipSourceRollback(pMemory);
		return 0;

	case ZIP_SOURCE_WRITE: {
		uint64 iPosition = 0u;
		if ( pMemory->Writer == NULL || iLength > SIZE_MAX ||
			!xrtWriterTell(pMemory->Writer, &iPosition) ||
			iLength > UINT64_MAX - iPosition ||
			(pMemory->MaxBytes != 0u && iPosition + iLength > pMemory->MaxBytes) ) {
			zip_error_set(&pMemory->Error, ZIP_ER_WRITE, EFBIG);
			return -1;
		}
		if ( !xrtWriterWriteFull(pMemory->Writer, pData, (size_t)iLength, NULL) ) {
			/* 内存源扩容的 OOM 必须穿过 libzip，不能降级为普通写入错误。 */
			const xerror* pCause = xrtGetError();
			bool bMemory = pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY;
			zip_error_set(&pMemory->Error, bMemory ? ZIP_ER_MEMORY : ZIP_ER_WRITE,
				bMemory ? 0 : EIO);
			return -1;
		}
		return (zip_int64_t)iLength;
	}

	case ZIP_SOURCE_SEEK_WRITE: {
		struct zip_source_args_seek* pSeek;
		uint64 iPosition;
		xseek Origin;
		if ( pMemory->Writer == NULL || iLength < sizeof(*pSeek) ) {
			zip_error_set(&pMemory->Error, ZIP_ER_INVAL, 0);
			return -1;
		}
		pSeek = (struct zip_source_args_seek*)pData;
		if ( pSeek->whence == SEEK_SET ) { Origin = XSEEK_START; }
		else if ( pSeek->whence == SEEK_CUR ) { Origin = XSEEK_CURRENT; }
		else if ( pSeek->whence == SEEK_END ) { Origin = XSEEK_END; }
		else {
			zip_error_set(&pMemory->Error, ZIP_ER_INVAL, EINVAL);
			return -1;
		}
		if ( !xrtWriterSeek(pMemory->Writer, pSeek->offset, Origin, &iPosition) ) {
			zip_error_set(&pMemory->Error, ZIP_ER_SEEK, EINVAL);
			return -1;
		}
		return 0;
	}

	case ZIP_SOURCE_TELL_WRITE: {
		uint64 iPosition = 0u;
		if ( pMemory->Writer == NULL || !xrtWriterTell(pMemory->Writer, &iPosition) ||
			iPosition > ZIP_INT64_MAX ) {
			zip_error_set(&pMemory->Error, ZIP_ER_TELL, EFBIG);
			return -1;
		}
		return (zip_int64_t)iPosition;
	}

	case ZIP_SOURCE_REMOVE:
		if ( pMemory->Base == NULL ) { pMemory->Base = xrtBufferCreate(); }
		else { xrtBufferClear(pMemory->Base); }
		if ( pMemory->Base == NULL ) {
			zip_error_set(&pMemory->Error, ZIP_ER_MEMORY, 0);
			return -1;
		}
		return 0;

	case ZIP_SOURCE_SUPPORTS:
		return ZIP_SOURCE_SUPPORTS_WRITABLE |
			ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ACCEPT_EMPTY);

	default:
		zip_error_set(&pMemory->Error, ZIP_ER_OPNOTSUPP, 0);
		return -1;
	}
}



xrtzipmemorysource* __xrtZipMemorySourceCreate(const void* pData, size_t iSize, uint64 iMaxBytes,
	zip_error_t* pError, zip_source_t** pSource)
{
	xrtzipmemorysource* pMemory;
	zip_source_t* pZipSource;
	if ( pSource != NULL ) { *pSource = NULL; }
	if ( pSource == NULL || (pData == NULL && iSize != 0u) ||
		(iMaxBytes != 0u && iSize > iMaxBytes) ) {
		zip_error_set(pError, ZIP_ER_INVAL, 0);
		return NULL;
	}
	pMemory = (xrtzipmemorysource*)xrtMalloc(sizeof(*pMemory));
	if ( pMemory == NULL ) {
		zip_error_set(pError, ZIP_ER_MEMORY, 0);
		return NULL;
	}
	memset(pMemory, 0, sizeof(*pMemory));
	zip_error_init(&pMemory->Error);
	pMemory->MaxBytes = iMaxBytes;
	pMemory->Base = __xrtZipBufferCreate(iSize > 65536u ? iSize : 65536u);
	if ( pMemory->Base == NULL || (iSize != 0u &&
		!xrtBufferAppend(pMemory->Base,
			(xbytesview){ (const unsigned char*)pData, iSize })) ) {
		__xrtZipMemorySourceDestroy(pMemory);
		zip_error_set(pError, ZIP_ER_MEMORY, 0);
		return NULL;
	}
	pZipSource = zip_source_function_create(__xrtZipSourceCallback, pMemory, pError);
	if ( pZipSource == NULL ) {
		__xrtZipMemorySourceDestroy(pMemory);
		return NULL;
	}
	*pSource = pZipSource;
	return pMemory;
}

void __xrtZipMemorySourceDestroy(xrtzipmemorysource* pMemory)
{
	if ( pMemory == NULL ) { return; }
	__xrtZipSourceRollback(pMemory);
	xrtBufferDestroy(pMemory->Base);
	zip_error_fini(&pMemory->Error);
	xrtFree(pMemory);
}

xbuffer* __xrtZipMemorySourceTake(xrtzipmemorysource* pMemory)
{
	xbuffer* pResult;
	if ( pMemory == NULL || pMemory->Write != NULL ) { return NULL; }
	pResult = pMemory->Base;
	pMemory->Base = NULL;
	return pResult;
}
