#include "xrt_zip_internal.h"

#include <errno.h>
#include <string.h>
#include <time.h>



static bool __xrtZipSourceBeginWrite(xrtzipmemorysource* pMemory)
{
	if ( pMemory->Write != NULL ) {
		zip_error_set(&pMemory->Error, ZIP_ER_INUSE, 0);
		return FALSE;
	}
	pMemory->Write = xrtBufferCreate(65536u);
	if ( pMemory->Write == NULL ) {
		zip_error_set(&pMemory->Error, ZIP_ER_MEMORY, 0);
		return FALSE;
	}
	pMemory->Writer = xrtWriterFromBuffer(pMemory->Write, XRT_STREAM_BORROW);
	if ( pMemory->Writer == NULL ) {
		xrtBufferDestroy(pMemory->Write);
		pMemory->Write = NULL;
		zip_error_set(&pMemory->Error, ZIP_ER_MEMORY, 0);
		return FALSE;
	}
	return TRUE;
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
		iRemain = pMemory->Base != NULL && pMemory->ReadPosition < pMemory->Base->Length ?
			pMemory->Base->Length - pMemory->ReadPosition : 0u;
		iRead = iLength < iRemain ? iLength : iRemain;
		if ( iRead != 0u ) {
			memcpy(pData, pMemory->Base->Buffer + pMemory->ReadPosition, (size_t)iRead);
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
		pStat->size = pMemory->Base != NULL ? pMemory->Base->Length : 0u;
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
		pMemory->SourceFreed = TRUE;
		return 0;

	case ZIP_SOURCE_SEEK: {
		zip_int64_t iOffset = zip_source_seek_compute_offset(pMemory->ReadPosition,
			pMemory->Base != NULL ? pMemory->Base->Length : 0u, pData, iLength, &pMemory->Error);
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
		xrtWriterDestroy(pMemory->Writer);
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
		if ( pMemory->Writer == NULL || !xrtWriterTell(pMemory->Writer, &iPosition) ||
			iLength > UINT64_MAX - iPosition ||
			(pMemory->MaxBytes != 0u && iPosition + iLength > pMemory->MaxBytes) ||
			iPosition + iLength > UINT32_MAX ) {
			zip_error_set(&pMemory->Error, ZIP_ER_WRITE, EFBIG);
			return -1;
		}
		if ( !xrtWriterWriteAll(pMemory->Writer, pData, iLength) ) {
			zip_error_set(&pMemory->Error, ZIP_ER_WRITE, ENOMEM);
			return -1;
		}
		return (zip_int64_t)iLength;
	}

	case ZIP_SOURCE_SEEK_WRITE: {
		struct zip_source_args_seek* pSeek;
		uint64 iPosition;
		if ( pMemory->Writer == NULL || iLength < sizeof(*pSeek) ) {
			zip_error_set(&pMemory->Error, ZIP_ER_INVAL, 0);
			return -1;
		}
		pSeek = (struct zip_source_args_seek*)pData;
		if ( !xrtWriterSeek(pMemory->Writer, pSeek->offset, pSeek->whence, &iPosition) ) {
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
		xrtBufferDestroy(pMemory->Base);
		pMemory->Base = xrtBufferCreate(1u);
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
	if ( pSource == NULL || (pData == NULL && iSize != 0u) || iSize > UINT32_MAX ||
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
	pMemory->Base = xrtBufferCreate(iSize > 65536u ? (uint32)iSize : 65536u);
	if ( pMemory->Base == NULL ||
		(iSize != 0u && !xrtBufferAppend(pMemory->Base, (ptr)pData, (uint32)iSize, XBUF_BINARY)) ) {
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

xbuffer __xrtZipMemorySourceTake(xrtzipmemorysource* pMemory)
{
	xbuffer pResult;
	if ( pMemory == NULL || pMemory->Write != NULL ) { return NULL; }
	pResult = pMemory->Base;
	pMemory->Base = NULL;
	return pResult;
}
