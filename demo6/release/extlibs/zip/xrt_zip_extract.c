#include "xrt_zip_internal.h"

#include <string.h>



#define XRT_ZIP_MODE_MASK 0170000u
#define XRT_ZIP_MODE_FILE 0100000u
#define XRT_ZIP_MODE_DIR 0040000u



/* 验证 ZIP 条目名可安全映射为目标目录中的相对路径。 */
bool __xrtZipEntryPathSafe(const char* sName, bool bDirectory,
	xrtziparchive* pArchive, uint64 iEntry)
{
	if ( !xrtPathIsSafeEntry(xrtStrView(sName), bDirectory) ||
		(bDirectory && (sName != NULL) && (sName[0] != 0) &&
		(sName[strlen(sName) - 1u] != '/')) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH, ZIP_ER_INVAL, 0, iEntry,
			"unsafe ZIP entry path: %s", sName != NULL ? sName : "");
		return false;
	}
	return true;
}





/* 安全解压全部条目；普通文件通过同目录临时文件流式提交。 */
XRT_API bool xrtZipArchiveExtractAll(xrtziparchive* pArchive, cstr sDestination,
	bool bOverwrite)
{
	uint64 iCount;
	uint64 iTotal = 0u;
	uint64 i;

	if ( !__xrtZipCheckOpen(pArchive, false) ) {
		return false;
	}
	if ( (sDestination == NULL) || (sDestination[0] == 0) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP extraction destination is required");
		return false;
	}
	if ( !xrtDirExists(sDestination) && !xrtDirCreateAll(sDestination) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, UINT64_MAX,
			"cannot create ZIP extraction directory: %s", sDestination);
		return false;
	}
	iCount = xrtZipArchiveCount(pArchive);
	for ( i = 0u; i < iCount; ++i ) {
		xrtzipentry Entry;
		str sOutput;
		uint32 iType;

		if ( !xrtZipArchiveEntryAt(pArchive, i, &Entry) ||
			!__xrtZipEntryPathSafe(Entry.sName, Entry.bDirectory, pArchive, i) ) {
			return false;
		}
		iType = Entry.iUnixMode & XRT_ZIP_MODE_MASK;
		if ( (iType != 0u) && (iType != XRT_ZIP_MODE_FILE) &&
			(iType != XRT_ZIP_MODE_DIR) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH, ZIP_ER_OPNOTSUPP, 0, i,
				"ZIP links and special files are disabled: %s", Entry.sName);
			return false;
		}
		if ( Entry.iUncompressedSize > (UINT64_MAX - iTotal) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0, i,
				"ZIP extracted size overflow");
			return false;
		}
		iTotal += Entry.iUncompressedSize;
		if ( (pArchive->Options.tLimits.iMaxOutputBytes != 0u) &&
			(iTotal > pArchive->Options.tLimits.iMaxOutputBytes) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_LIMIT, ZIP_ER_DATA_LENGTH, 0, i,
				"ZIP extraction exceeds the configured output limit");
			return false;
		}

		sOutput = xrtPathJoin(sDestination, Entry.sName);
		if ( sOutput == NULL ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, i,
				"cannot build ZIP extraction path");
			return false;
		}
		if ( Entry.bDirectory || (iType == XRT_ZIP_MODE_DIR) ) {
			if ( !xrtDirExists(sOutput) && !xrtDirCreateAll(sOutput) ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, i,
					"cannot create ZIP directory: %s", sOutput);
				xrtFree(sOutput);
				return false;
			}
		} else {
			str sParent = xrtPathParent(sOutput);
			str sTemporary = NULL;
			xfile File;
			xwriter* pWriter;
			bool bRead;
			bool bFlush;
			bool bClose;

			if ( (sParent == NULL) ||
				(!xrtDirExists(sParent) && !xrtDirCreateAll(sParent)) ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, i,
					"cannot create ZIP entry parent directory: %s", sOutput);
				xrtFree(sParent);
				xrtFree(sOutput);
				return false;
			}
			if ( xrtPathExists(sOutput) && !bOverwrite ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_EXISTS, ZIP_ER_EXISTS, 0, i,
					"ZIP extraction target already exists: %s", sOutput);
				xrtFree(sParent);
				xrtFree(sOutput);
				return false;
			}

			File = xrtFileTemp(sParent, ".xrt-zip-entry-", ".tmp", &sTemporary);
			xrtFree(sParent);
			pWriter = File != NULL ? xrtWriterTakeFile(&File) : NULL;
			if ( pWriter == NULL ) {
				if ( File != NULL ) { (void)xrtClose(File); }
				if ( sTemporary != NULL ) { (void)xrtFileDelete(sTemporary); }
				xrtFree(sTemporary);
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_OPEN, 0, i,
					"cannot create ZIP entry transaction: %s", sOutput);
				xrtFree(sOutput);
				return false;
			}

			bRead = xrtZipArchiveReadAtTo(pArchive, i, pWriter);
			bFlush = bRead && xrtWriterFlush(pWriter);
			bClose = xrtWriterDestroy(pWriter);
			if ( !bRead || !bFlush || !bClose ) {
				if ( bRead ) {
					__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, i,
						"cannot write ZIP entry: %s", sOutput);
				}
				(void)xrtFileDelete(sTemporary);
				xrtFree(sTemporary);
				xrtFree(sOutput);
				return false;
			}
			if ( !xrtPathRename(sTemporary, sOutput, bOverwrite) ) {
				__xrtZipSetError(pArchive,
					(!bOverwrite && xrtPathExists(sOutput)) ?
						XRT_ZIP_ERROR_EXISTS : XRT_ZIP_ERROR_IO,
					!bOverwrite ? ZIP_ER_EXISTS : ZIP_ER_WRITE,
					0, i, "cannot commit ZIP entry: %s", sOutput);
				(void)xrtFileDelete(sTemporary);
				xrtFree(sTemporary);
				xrtFree(sOutput);
				return false;
			}
			xrtFree(sTemporary);
		}
		xrtFree(sOutput);
	}
	return true;
}
