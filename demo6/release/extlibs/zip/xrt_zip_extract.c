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



/* 逐级检查目标路径，拒绝已有链接；缺失的目录只创建当前一级。 */
static bool __xrtZipPrepareOutput(cstr sOutput, cstr sDestination,
	bool bDirectory, xrtziparchive* pArchive, uint64 iEntry)
{
	size_t iRootLength = strlen(sDestination);
	size_t iLength = strlen(sOutput);
	str sCopy;
	bool bResult = true;
	size_t i;

	while ( (iRootLength > 1u) &&
		((sDestination[iRootLength - 1u] == '/') ||
		(sDestination[iRootLength - 1u] == '\\')) ) {
		--iRootLength;
	}
	while ( (iLength > iRootLength) &&
		((sOutput[iLength - 1u] == '/') || (sOutput[iLength - 1u] == '\\')) ) {
		--iLength;
	}
	sCopy = xrtStrDupN(sOutput, iLength);
	if ( sCopy == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, iEntry,
			"cannot allocate ZIP output path");
		return false;
	}
	for ( i = iRootLength + 1u; i <= iLength; ++i ) {
		bool bLast = i == iLength;
		char iSaved = sCopy[i];
		xfileinfo Info;

		if ( !bLast && (iSaved != '/') && (iSaved != '\\') ) {
			continue;
		}
		sCopy[i] = 0;
		if ( xrtPathStat(sCopy, false, &Info) ) {
			if ( (Info.Type == XFILE_TYPE_LINK) ||
				(Info.Type != ((bLast && !bDirectory) ?
					XFILE_TYPE_FILE : XFILE_TYPE_DIRECTORY)) ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH, ZIP_ER_INVAL,
					0, iEntry, "ZIP output path is a link or wrong type: %s", sCopy);
				bResult = false;
				break;
			}
		} else if ( (xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) ) {
			xrtClearError();
			if ( (!bLast || bDirectory) && !xrtDirCreate(sCopy) ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0,
					iEntry, "cannot create ZIP output directory: %s", sCopy);
				bResult = false;
				break;
			}
		} else {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_READ, 0, iEntry,
				"cannot inspect ZIP output path: %s", sCopy);
			bResult = false;
			break;
		}
		sCopy[i] = iSaved;
	}
	xrtFree(sCopy);
	return bResult;
}





/* 去掉末尾分隔符与点段，避免 lstat("link/") 跟随末级链接。 */
static str __xrtZipDestinationForStat(cstr sDestination)
{
	size_t iLength = strlen(sDestination);
	size_t iMinimum = (iLength >= 3u && sDestination[1] == ':' &&
		(sDestination[2] == '/' || sDestination[2] == '\\')) ? 3u : 1u;

	while ( iLength > iMinimum ) {
		if ( sDestination[iLength - 1u] == '/' ||
			sDestination[iLength - 1u] == '\\' ) {
			--iLength;
		} else if ( iLength >= iMinimum + 2u &&
			sDestination[iLength - 1u] == '.' &&
			(sDestination[iLength - 2u] == '/' ||
			sDestination[iLength - 2u] == '\\') ) {
			iLength -= 2u;
		} else {
			break;
		}
	}
	return xrtStrDupN(sDestination, iLength);
}



/* 创建目标前检查所有已有父目录，不能只检查目标的末级。 */
static bool __xrtZipDestinationParentsSafe(cstr sDestination,
	xrtziparchive* pArchive)
{
	str sParent = xrtPathParent(sDestination);

	if ( sParent == NULL ) {
		__xrtZipSetOutputError(pArchive, UINT64_MAX, ZIP_ER_OPEN,
			"cannot inspect ZIP extraction parent", sDestination);
		return false;
	}
	while ( sParent[0] != 0 ) {
		xfileinfo Info;
		str sNext;
		bool bRoot;

		if ( xrtPathStat(sParent, false, &Info) ) {
			if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
				__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH,
					ZIP_ER_INVAL, 0, UINT64_MAX,
					"ZIP extraction destination is a link or wrong type: %s",
					sParent);
				xrtFree(sParent);
				return false;
			}
		} else if ( (xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) ) {
			xrtClearError();
		} else {
			__xrtZipSetOutputError(pArchive, UINT64_MAX, ZIP_ER_READ,
				"cannot inspect ZIP extraction parent", sParent);
			xrtFree(sParent);
			return false;
		}
		sNext = xrtPathParent(sParent);
		if ( sNext == NULL ) {
			__xrtZipSetOutputError(pArchive, UINT64_MAX, ZIP_ER_OPEN,
				"cannot inspect ZIP extraction parent", sParent);
			xrtFree(sParent);
			return false;
		}
		bRoot = strcmp(sParent, sNext) == 0;
		xrtFree(sParent);
		sParent = sNext;
		if ( bRoot ) break;
	}
	xrtFree(sParent);
	return true;
}



/* 流式提取事务的 Writer 阶段；调用后 Writer 始终关闭。 */
bool __xrtZipExtractEntryToWriter(xrtziparchive* pArchive, uint64 iEntry,
	xwriter* pWriter, cstr sOutput)
{
	bool bRead = xrtZipArchiveReadAtTo(pArchive, iEntry, pWriter);
	bool bFlush = bRead && xrtWriterFlush(pWriter);
	if ( bRead && !bFlush ) {
		__xrtZipSetOutputError(pArchive, iEntry, ZIP_ER_WRITE,
			"cannot flush ZIP entry output", sOutput);
	}
	bool bClose = xrtWriterDestroy(pWriter);
	if ( bRead && bFlush && !bClose ) {
		__xrtZipSetOutputError(pArchive, iEntry, ZIP_ER_CLOSE,
			"cannot close ZIP entry output", sOutput);
	}
	return bRead && bFlush && bClose;
}



/* 安全解压全部条目；普通文件通过同目录临时文件流式提交。 */
XRT_API bool xrtZipArchiveExtractAll(xrtziparchive* pArchive, cstr sDestination,
	bool bOverwrite)
{
	uint64 iCount;
	uint64 iTotal = 0u;
	uint64 i;
	xfileinfo DestinationInfo;
	str sStatDestination;

	if ( !__xrtZipCheckOpen(pArchive, false) ) {
		return false;
	}
	if ( (sDestination == NULL) || (sDestination[0] == 0) ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_ARGUMENT, ZIP_ER_INVAL, 0, UINT64_MAX,
			"ZIP extraction destination is required");
		return false;
	}
	sStatDestination = __xrtZipDestinationForStat(sDestination);
	if ( sStatDestination == NULL ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_MEMORY, ZIP_ER_MEMORY, 0, UINT64_MAX,
			"cannot allocate ZIP extraction directory path");
		return false;
	}
	if ( !__xrtZipDestinationParentsSafe(sStatDestination, pArchive) ) {
		xrtFree(sStatDestination);
		return false;
	}
	/* 根目录也不能是链接；后续逐级检查只覆盖它下面的分量。 */
	if ( !xrtPathStat(sStatDestination, false, &DestinationInfo) ) {
		if ( (xrtGetError() == NULL) ||
			(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_READ, 0, UINT64_MAX,
				"cannot inspect ZIP extraction directory: %s", sDestination);
			xrtFree(sStatDestination);
			return false;
		}
		xrtClearError();
		if ( !xrtDirCreateAll(sDestination) ||
			!xrtPathStat(sStatDestination, false, &DestinationInfo) ) {
			__xrtZipSetError(pArchive, XRT_ZIP_ERROR_IO, ZIP_ER_WRITE, 0, UINT64_MAX,
				"cannot create ZIP extraction directory: %s", sDestination);
			xrtFree(sStatDestination);
			return false;
		}
	}
	xrtFree(sStatDestination);
	if ( DestinationInfo.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtZipSetError(pArchive, XRT_ZIP_ERROR_UNSAFE_PATH, ZIP_ER_INVAL,
			0, UINT64_MAX, "ZIP extraction destination is a link or wrong type: %s",
			sDestination);
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
		if ( !__xrtZipPrepareOutput(sOutput, sDestination,
			Entry.bDirectory || (iType == XRT_ZIP_MODE_DIR), pArchive, i) ) {
			xrtFree(sOutput);
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

			if ( !__xrtZipExtractEntryToWriter(pArchive, i, pWriter, sOutput) ) {
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
