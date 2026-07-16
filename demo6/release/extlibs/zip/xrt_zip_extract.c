#include "xrt_zip_internal.h"

#include <string.h>



#define XRT_ZIP_MODE_MASK 0170000u
#define XRT_ZIP_MODE_FILE 0100000u
#define XRT_ZIP_MODE_DIR 0040000u



bool __xrtZipEntryPathSafe(const char* sName, bool bDirectory, xrtziparchive* pArchive,
	uint64 iEntry)
{
	if ( !xrtPathIsSafeArchiveEntry((str)sName, bDirectory) ||
		(bDirectory && sName != NULL && sName[0] != 0 && sName[strlen(sName) - 1u] != '/') ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH, ZIP_ER_INVAL, 0, iEntry,
			"unsafe ZIP entry path: %s", sName != NULL ? sName : "");
		return FALSE;
	}
	return TRUE;
}



XXAPI bool xrtZipArchiveExtractAll(xrtziparchive* pArchive, str sDestination, bool bOverwrite)
{
	uint64 iCount;
	uint64 iTotal = 0u;
	uint64 i;
	if ( !__xrtZipCheckOpen(pArchive, FALSE) || sDestination == NULL || sDestination[0] == 0 ) {
		return FALSE;
	}
	if ( !xrtDirExists(sDestination) && !xrtDirCreateAll(sDestination) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, UINT64_MAX,
			"cannot create ZIP extraction directory: %s", sDestination);
		return FALSE;
	}
	iCount = xrtZipArchiveCount(pArchive);
	for ( i = 0u; i < iCount; ++i ) {
		xrtzipentry tEntry;
		str sJoined;
		str sOutput;
		uint32 iType;
		if ( !xrtZipArchiveEntryAt(pArchive, i, &tEntry) ||
			!__xrtZipEntryPathSafe(tEntry.sName, tEntry.bDirectory, pArchive, i) ) { return FALSE; }
		iType = tEntry.iUnixMode & XRT_ZIP_MODE_MASK;
		if ( iType != 0u && iType != XRT_ZIP_MODE_FILE && iType != XRT_ZIP_MODE_DIR ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH, ZIP_ER_OPNOTSUPP, 0, i,
				"ZIP links and special files are disabled: %s", tEntry.sName);
			return FALSE;
		}
		if ( tEntry.iUncompressedSize > UINT64_MAX - iTotal ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0, i,
				"ZIP extracted size overflow");
			return FALSE;
		}
		iTotal += tEntry.iUncompressedSize;
		if ( pArchive->Options.tLimits.iMaxOutputBytes != 0u &&
			iTotal > pArchive->Options.tLimits.iMaxOutputBytes ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0, i,
				"ZIP extraction exceeds the configured output limit");
			return FALSE;
		}
		sJoined = xrtPathJoin(2u, sDestination, tEntry.sName);
		sOutput = sJoined != NULL ? xrtCopyStr(sJoined, 0u) : NULL;
		if ( sOutput == NULL ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, i,
				"cannot build ZIP extraction path");
			return FALSE;
		}
		if ( tEntry.bDirectory || iType == XRT_ZIP_MODE_DIR ) {
			if ( !xrtDirExists(sOutput) && !xrtDirCreateAll(sOutput) ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, i,
					"cannot create ZIP directory: %s", sOutput);
				xrtFree(sOutput);
				return FALSE;
			}
		}
		else {
			str sParentTemp = xrtPathGetDir(sOutput, 0u);
			str sParent = sParentTemp != NULL ? xrtCopyStr(sParentTemp, 0u) : NULL;
			xbuffer pData;
			if ( sParent == NULL || (!xrtDirExists(sParent) && !xrtDirCreateAll(sParent)) ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, i,
					"cannot create ZIP entry parent directory: %s", sOutput);
				xrtFree(sParent);
				xrtFree(sOutput);
				return FALSE;
			}
			xrtFree(sParent);
			if ( (xrtFileExists(sOutput) || xrtDirExists(sOutput)) && !bOverwrite ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_EXISTS, ZIP_ER_EXISTS, 0, i,
					"ZIP extraction target already exists: %s", sOutput);
				xrtFree(sOutput);
				return FALSE;
			}
			pData = xrtZipArchiveReadAt(pArchive, i);
			if ( pData == NULL || !xrtFileWriteAllAtomic(sOutput,
				pData != NULL ? (str)pData->Buffer : NULL,
				pData != NULL ? pData->Length : 0u, XRT_CP_BINARY) ) {
				if ( pArchive->Error.iCode == XRT_ZIP_OK ) {
					__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, i,
						"cannot write ZIP entry: %s", sOutput);
				}
				xrtBufferDestroy(pData);
				xrtFree(sOutput);
				return FALSE;
			}
			xrtBufferDestroy(pData);
		}
		xrtFree(sOutput);
	}
	return TRUE;
}
