#include "xrt_tar_internal.h"

#include <string.h>



typedef struct xrttarpendinglink {
	str sOutput;
	str sTarget;
	bool bDone;
	struct xrttarpendinglink* pNext;
} xrttarpendinglink;

typedef struct xrttarpendingmode {
	str sPath;
	uint32 iMode;
	struct xrttarpendingmode* pNext;
} xrttarpendingmode;



/* 在锚定根内处理已有末级对象，检查和删除都不跟随链接。 */
static bool __xrtTarRootPrepareTarget(xrttarreader* pReader,
	xroot Root, cstr sName, bool bOverwrite)
{
	xfileinfo Info;
	const xerror* pError;

	if ( !xrtRootStat(Root, sName, false, &Info) ) {
		pError = xrtGetError();
		if ( (pError != NULL) &&
			(xrtErrorKind(pError) == XERR_NOT_FOUND) ) {
			xrtClearError();
			return true;
		}
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot inspect TAR extraction target: %s", sName);
		return false;
	}
	if ( !bOverwrite ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_EXISTS,
			"TAR extraction target already exists: %s", sName);
		return false;
	}
	if ( Info.Type == XFILE_TYPE_DIRECTORY ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_EXISTS,
			"TAR extraction will not replace a directory: %s", sName);
		return false;
	}
	if ( !xrtRootRemove(Root, sName) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot replace TAR extraction target: %s", sName);
		return false;
	}
	return true;
}






/* 在锚定根内逐级创建目录，已有对象必须解析为目录且不能逃出根。 */
static bool __xrtTarRootEnsureDirectory(xrttarreader* pReader,
	xroot Root, cstr sPath)
{
	str sCopy = xrtStrDup(sPath);
	char* p;
	bool bResult = false;

	if ( sCopy == NULL ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot copy TAR directory path");
		return false;
	}
	while ( sCopy[0] != 0 && sCopy[strlen(sCopy) - 1u] == '/' ) {
		sCopy[strlen(sCopy) - 1u] = 0;
	}
	if ( sCopy[0] == 0 ) {
		bResult = true;
		goto cleanup;
	}
	for ( p = sCopy; ; ++p ) {
		char iSaved;
		xfileinfo Info;
		const xerror* pError;

		if ( (*p != '/') && (*p != 0) ) { continue; }
		iSaved = *p;
		*p = 0;
		if ( !xrtRootStat(Root, sCopy, true, &Info) ) {
			pError = xrtGetError();
			if ( (pError == NULL) || (xrtErrorKind(pError) != XERR_NOT_FOUND) ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot inspect TAR directory: %s", sCopy);
				goto cleanup;
			}
			xrtClearError();
			if ( !xrtRootDirCreate(Root, sCopy, 0700u) ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot create TAR directory: %s", sCopy);
				goto cleanup;
			}
		} else if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_EXISTS,
				"TAR directory target is not a directory: %s", sCopy);
			goto cleanup;
		}
		*p = iSaved;
		if ( iSaved == 0 ) { break; }
	}
	bResult = true;

cleanup:
	xrtFree(sCopy);
	return bResult;
}





/* 在锚定根内创建当前条目的全部父目录。 */
static bool __xrtTarRootEnsureParent(xrttarreader* pReader,
	xroot Root, cstr sName)
{
	str sParent = xrtPathParent(sName);
	bool bResult;

	if ( sParent == NULL ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot build TAR entry parent path");
		return false;
	}
	bResult = (sParent[0] == 0) || (strcmp(sParent, ".") == 0) ||
		__xrtTarRootEnsureDirectory(pReader, Root, sParent);
	xrtFree(sParent);
	return bResult;
}





/* 直接通过根句柄创建普通文件，末级符号链接永远不会被覆盖或跟随。 */
static bool __xrtTarExtractFile(xrttarreader* pReader, xroot Root,
	cstr sName, bool bOverwrite)
{
	xfileoptions Options;
	xfile File;
	xwriter* pWriter;
	bool bRead;
	bool bFlush;
	bool bClose;

	if ( !__xrtTarRootEnsureParent(pReader, Root, sName) ) { return false; }
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_NOFOLLOW |
		(bOverwrite ? XFILE_TRUNCATE : XFILE_EXCLUSIVE);
	Options.Mode = pReader->Options.bPreservePermissions ?
		(pReader->Entry.iMode & pReader->Options.iFileModeMask) : 0600u;
	File = xrtRootFileOpen(Root, sName, &Options);
	if ( File == NULL ) {
		const xerror* pError = xrtGetError();

		__xrtTarSetReaderError(pReader,
			(pError != NULL && xrtErrorKind(pError) == XERR_EXISTS) ?
				XRT_TAR_ERROR_EXISTS : XRT_TAR_ERROR_IO,
			"cannot create TAR extraction file: %s", sName);
		return false;
	}
	pWriter = xrtWriterTakeFile(&File);
	if ( pWriter == NULL ) {
		(void)xrtClose(File);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR extraction writer");
		return false;
	}
	bRead = xrtTarReaderReadTo(pReader, pWriter);
	bFlush = bRead && xrtWriterFlush(pWriter);
	bClose = xrtWriterDestroy(pWriter);
	if ( !bRead ) { return false; }
	if ( !bFlush || !bClose ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_IO,
			"cannot finish TAR extraction file: %s", sName);
		return false;
	}
	return true;
}



static bool __xrtTarQueueMode(xrttarreader* pReader, xrttarpendingmode** pHead,
	cstr sPath, uint32 iMode)
{
	xrttarpendingmode* pItem;
	if ( !pReader->Options.bPreservePermissions ) { return true; }
	pItem = (xrttarpendingmode*)xrtCalloc(1u, sizeof(*pItem));
	if ( pItem != NULL ) { pItem->sPath = xrtStrDup(sPath); }
	if ( pItem == NULL || pItem->sPath == NULL ) {
		xrtFree(pItem);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot retain TAR permission metadata");
		return false;
	}
	pItem->iMode = iMode & pReader->Options.iFileModeMask;
	pItem->pNext = *pHead;
	*pHead = pItem;
	return true;
}

static bool __xrtTarApplyModes(xrttarreader* pReader,
	xroot Root, xrttarpendingmode* pHead)
{
	xrttarpendingmode* pItem;

	for ( pItem = pHead; pItem != NULL; pItem = pItem->pNext ) {
		if ( !xrtRootSetMode(Root, pItem->sPath,
			true, pItem->iMode) ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
				"cannot restore TAR entry permissions: %s", pItem->sPath);
			return false;
		}
	}
	return true;
}

static void __xrtTarFreeModes(xrttarpendingmode* pHead)
{
	while ( pHead != NULL ) {
		xrttarpendingmode* pNext = pHead->pNext;
		xrtFree(pHead->sPath);
		xrtFree(pHead);
		pHead = pNext;
	}
}



static bool __xrtTarQueueHardlink(xrttarreader* pReader,
	xrttarpendinglink** pHead, cstr sOutput, cstr sTarget)
{
	xrttarpendinglink* pItem;
	if ( !pReader->Options.bAllowHardlinks ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR hard links are disabled: %s", pReader->Entry.sName);
		return false;
	}
	if ( sTarget == NULL || !xrtPathIsSafeEntry(xrtStrView(sTarget), false) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSAFE_PATH,
			"unsafe TAR hard-link target: %s", sTarget != NULL ? sTarget : "");
		return false;
	}
	pItem = (xrttarpendinglink*)xrtCalloc(1u, sizeof(*pItem));
	if ( pItem != NULL ) {
		pItem->sOutput = xrtStrDup(sOutput);
		pItem->sTarget = xrtStrDup(sTarget);
	}
	if ( pItem == NULL || pItem->sOutput == NULL || pItem->sTarget == NULL ) {
		if ( pItem != NULL ) {
			xrtFree(pItem->sOutput);
			xrtFree(pItem->sTarget);
		}
		xrtFree(pItem);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot retain TAR hard-link metadata");
		return false;
	}
	if ( strcmp((const char*)pItem->sOutput, (const char*)pItem->sTarget) == 0 ) {
		xrtFree(pItem->sOutput);
		xrtFree(pItem->sTarget);
		xrtFree(pItem);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
			"TAR hard link cannot target itself: %s", sTarget);
		return false;
	}
	pItem->pNext = *pHead;
	*pHead = pItem;
	return true;
}

static bool __xrtTarCreateHardlinks(xrttarreader* pReader,
	xroot Root, xrttarpendinglink* pHead, bool bOverwrite)
{
	bool bRemaining;
	bool bProgress;
	do {
		xrttarpendinglink* pItem;
		bRemaining = false;
		bProgress = false;
		for ( pItem = pHead; pItem != NULL; pItem = pItem->pNext ) {
			xfileinfo Info;
			const xerror* pError;

			if ( pItem->bDone ) { continue; }
			bRemaining = true;
			if ( !xrtRootStat(Root, pItem->sTarget, true, &Info) ) {
				pError = xrtGetError();
				if ( (pError != NULL) &&
					(xrtErrorKind(pError) == XERR_NOT_FOUND) ) {
					xrtClearError();
					continue;
				}
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot inspect TAR hard-link target: %s", pItem->sTarget);
				return false;
			}
			if ( Info.Type != XFILE_TYPE_FILE ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
					"TAR hard-link target is not a regular file: %s", pItem->sTarget);
				return false;
			}
			if ( !__xrtTarRootEnsureParent(pReader, Root, pItem->sOutput) ||
				!__xrtTarRootPrepareTarget(pReader, Root,
					pItem->sOutput, bOverwrite) ||
				!xrtRootLinkHard(Root, pItem->sTarget, pItem->sOutput) ) {
				if ( pReader->Error.iCode == XRT_TAR_OK ) {
					__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
						"cannot create TAR hard link: %s", pItem->sOutput);
				}
				return false;
			}
			pItem->bDone = true;
			bProgress = true;
		}
	} while ( bRemaining && bProgress );
	if ( bRemaining ) {
		xrttarpendinglink* pItem;
		for ( pItem = pHead; pItem != NULL; pItem = pItem->pNext ) {
			if ( !pItem->bDone ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
					"unresolved TAR hard-link target: %s", pItem->sTarget);
				break;
			}
		}
		return false;
	}
	return true;
}

static void __xrtTarFreeHardlinks(xrttarpendinglink* pHead)
{
	while ( pHead != NULL ) {
		xrttarpendinglink* pNext = pHead->pNext;
		xrtFree(pHead->sOutput);
		xrtFree(pHead->sTarget);
		xrtFree(pHead);
		pHead = pNext;
	}
}



static bool __xrtTarExtractSymlink(xrttarreader* pReader,
	xroot Root, const xrttarentry* pEntry, bool bOverwrite)
{
	if ( !pReader->Options.bAllowSymlinks ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR symbolic links are disabled: %s", pEntry->sName);
		return false;
	}
	if ( pEntry->sLinkName == NULL ||
		!xrtPathIsSafeEntry(xrtStrView(pEntry->sLinkName), false) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSAFE_PATH,
			"unsafe TAR symbolic-link target: %s",
			pEntry->sLinkName != NULL ? pEntry->sLinkName : "");
		return false;
	}
	if ( !__xrtTarRootEnsureParent(pReader, Root, pEntry->sName) ||
		!__xrtTarRootPrepareTarget(pReader, Root,
			pEntry->sName, bOverwrite) ||
		!xrtRootLinkCreate(Root, pEntry->sLinkName,
			pEntry->sName, false) ) {
		if ( pReader->Error.iCode == XRT_TAR_OK ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
				"cannot create TAR symbolic link: %s", pEntry->sName);
		}
		return false;
	}
	return true;
}

static bool __xrtTarExtractFifo(xrttarreader* pReader,
	xroot Root, const xrttarentry* pEntry, bool bOverwrite)
{
	if ( !pReader->Options.bAllowSpecialFiles ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR special files are disabled: %s", pEntry->sName);
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)Root;
		(void)bOverwrite;
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR FIFO extraction is not supported on Windows: %s", pEntry->sName);
		return false;
	#else
		if ( !__xrtTarRootEnsureParent(pReader, Root, pEntry->sName) ||
			!__xrtTarRootPrepareTarget(pReader, Root,
				pEntry->sName, bOverwrite) ||
			!xrtRootFifoCreate(Root, pEntry->sName,
				pReader->Options.bPreservePermissions ?
				(pEntry->iMode & pReader->Options.iFileModeMask) : 0600u) ) {
			if ( pReader->Error.iCode == XRT_TAR_OK ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot create TAR FIFO: %s", pEntry->sName);
			}
			return false;
		}
		return true;
	#endif
}



/* 在条目边界报告提取进度；结束事件中的条目为空。 */
static bool __xrtTarExtractProgress(xrttarreader* pReader,
	const xrttarentry* pEntry, uint64 iTotalInput, bool bTotalKnown,
	bool bFinal, xrttarprogressproc pProgress, ptr pUserData)
{
	xrtprogress Progress;

	if ( pProgress == NULL ) { return true; }
	memset(&Progress, 0, sizeof(Progress));
	Progress.iSize = sizeof(Progress);
	Progress.iVersion = XRT_PROGRESS_VERSION;
	Progress.iFlags = (bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) |
		(bFinal ? XRT_PROGRESS_FINAL : 0u);
	Progress.iInputBytes = pReader->Offset;
	Progress.iTotalInputBytes = bTotalKnown ? iTotalInput : 0u;
	Progress.iOutputBytes = pReader->TotalOutput;
	return pProgress(pEntry, &Progress, pUserData);
}





/* 按选择器提取条目；跳过的条目不参与路径检查，也不会产生文件系统副作用。 */
XRT_API bool xrtTarReaderExtractWhere(xrttarreader* pReader,
	cstr sDestination, bool bOverwrite,
	xrttarselectproc pSelect, ptr pSelectData,
	xrttarprogressproc pProgress, ptr pProgressData)
{
	xrttarentry tEntry;
	xrttarpendinglink* pLinks = NULL;
	xrttarpendingmode* pModes = NULL;
	xroot Root = NULL;
	uint64 iTotalInput = 0u;
	bool bTotalKnown;
	bool bHaveCurrent;
	bool bResult = false;

	if ( pReader == NULL || pReader->Failed ) { return false; }
	if ( (sDestination == NULL) || (sDestination[0] == 0) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
			"TAR extraction destination is required");
		return false;
	}
	if ( !xrtDirExists(sDestination) && !xrtDirCreateAll(sDestination) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot create TAR extraction directory: %s", sDestination);
		return false;
	}
	Root = xrtRootOpen(sDestination);
	if ( Root == NULL ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot open TAR extraction directory: %s", sDestination);
		return false;
	}
	bTotalKnown = xrtReaderSize(pReader->Reader, &iTotalInput);
	if ( !bTotalKnown ) { xrtClearError(); }
	bHaveCurrent = pReader->HasCurrent;
	for ( ;; ) {
		bool bDirectory;
		xrttarselect Select;
		if ( bHaveCurrent ) {
			tEntry = pReader->Entry;
			bHaveCurrent = false;
		}
		else if ( !xrtTarReaderNext(pReader, &tEntry) ) {
			if ( !xrtTarReaderEOF(pReader) ) { goto cleanup; }
			break;
		}
		Select = pSelect != NULL ?
			pSelect(&tEntry, pSelectData) : XRT_TAR_SELECT_EXTRACT;
		if ( Select == XRT_TAR_SELECT_ERROR ) {
			const xerror* pError = xrtGetError();

			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_CALLBACK,
				"TAR selector failed for %s: %s",
				tEntry.sName != NULL ? tEntry.sName : "",
				pError != NULL ? xrtErrorMessage(pError) :
					"selector returned an error");
			goto cleanup;
		}
		if ( Select != XRT_TAR_SELECT_SKIP &&
			Select != XRT_TAR_SELECT_EXTRACT ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
				"TAR selector returned an invalid result for: %s",
				tEntry.sName != NULL ? tEntry.sName : "");
			goto cleanup;
		}
		if ( Select == XRT_TAR_SELECT_SKIP ) {
			if ( !xrtTarReaderSkip(pReader) ) { goto cleanup; }
			if ( !__xrtTarExtractProgress(pReader, &tEntry, iTotalInput,
				bTotalKnown, false, pProgress, pProgressData) ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_CANCELLED,
					"TAR extraction was cancelled");
				goto cleanup;
			}
			continue;
		}
		bDirectory = tEntry.iType == XRT_TAR_ENTRY_DIRECTORY;
		if ( !__xrtTarEntryPathSafe(tEntry.sName, bDirectory,
			&pReader->Error, tEntry.iIndex) ) {
			__xrtTarFailReader(pReader, XRT_TAR_ERROR_UNSAFE_PATH,
				"unsafe TAR entry path: %s",
				tEntry.sName != NULL ? tEntry.sName : "");
			goto cleanup;
		}
		if ( tEntry.iType == XRT_TAR_ENTRY_DIRECTORY ) {
			if ( !__xrtTarRootEnsureDirectory(pReader, Root, tEntry.sName) ||
				!xrtTarReaderSkip(pReader) ||
				!__xrtTarQueueMode(pReader, &pModes,
					tEntry.sName, tEntry.iMode) ) {
				if ( pReader->Error.iCode == XRT_TAR_OK ) {
					__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
						"cannot create TAR directory: %s", tEntry.sName);
				}
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_FILE ) {
			if ( !__xrtTarExtractFile(pReader, Root, tEntry.sName, bOverwrite) ||
				!__xrtTarQueueMode(pReader, &pModes,
					tEntry.sName, tEntry.iMode) ) {
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_SYMLINK ) {
			if ( !__xrtTarExtractSymlink(pReader, Root,
				&tEntry, bOverwrite) ||
				!xrtTarReaderSkip(pReader) ) {
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_HARDLINK ) {
			if ( !__xrtTarQueueHardlink(pReader, &pLinks, tEntry.sName,
				tEntry.sLinkName) || !xrtTarReaderSkip(pReader) ) {
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_FIFO ) {
			if ( !__xrtTarExtractFifo(pReader, Root,
				&tEntry, bOverwrite) ||
				!xrtTarReaderSkip(pReader) ||
				!__xrtTarQueueMode(pReader, &pModes,
					tEntry.sName, tEntry.iMode) ) {
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_CHARACTER ||
			tEntry.iType == XRT_TAR_ENTRY_BLOCK ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
				"TAR device-node extraction is intentionally unsupported: %s", tEntry.sName);
			goto cleanup;
		}
		else {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
				"unsupported TAR entry type: %s", tEntry.sName);
			goto cleanup;
		}
		if ( !__xrtTarExtractProgress(pReader, &tEntry, iTotalInput,
			bTotalKnown, false, pProgress, pProgressData) ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_CANCELLED,
				"TAR extraction was cancelled");
			goto cleanup;
		}
	}
	bResult = __xrtTarCreateHardlinks(pReader, Root,
		pLinks, bOverwrite) &&
		__xrtTarApplyModes(pReader, Root, pModes);
	if ( bResult ) {
		(void)__xrtTarExtractProgress(pReader, NULL, iTotalInput,
			bTotalKnown, true, pProgress, pProgressData);
	}

cleanup:
	__xrtTarFreeHardlinks(pLinks);
	__xrtTarFreeModes(pModes);
	if ( (Root != NULL) && !xrtRootClose(Root) && bResult ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot close TAR extraction root");
		bResult = false;
	}
	return bResult;
}





typedef struct {
	const xstrview* pNames;
	size_t iCount;
} xrttarnameselector;



/* 精确匹配归档中的完整条目名，目录尾斜线也属于名称的一部分。 */
static xrttarselect __xrtTarSelectName(const xrttarentry* pEntry,
	ptr pUserData)
{
	xrttarnameselector* pSelector = (xrttarnameselector*)pUserData;
	size_t iNameSize;
	size_t i;

	if ( pEntry == NULL || pEntry->sName == NULL ) {
		return XRT_TAR_SELECT_SKIP;
	}
	iNameSize = strlen(pEntry->sName);
	for ( i = 0u; i < pSelector->iCount; ++i ) {
		if ( pSelector->pNames[i].Size == iNameSize &&
			(iNameSize == 0u || memcmp(pSelector->pNames[i].Data,
				pEntry->sName, iNameSize) == 0) ) {
			return XRT_TAR_SELECT_EXTRACT;
		}
	}
	return XRT_TAR_SELECT_SKIP;
}




/* 提取全部条目并可选报告进度。 */
XRT_API bool xrtTarReaderExtract(xrttarreader* pReader, cstr sDestination,
	bool bOverwrite, xrttarprogressproc pProgress, ptr pUserData)
{
	return xrtTarReaderExtractWhere(pReader, sDestination, bOverwrite,
		NULL, NULL, pProgress, pUserData);
}




/* 按精确名称白名单提取；空白名单会完整消费 Reader，但不落盘。 */
XRT_API bool xrtTarReaderExtractNames(xrttarreader* pReader,
	cstr sDestination, bool bOverwrite,
	const xstrview* pNames, size_t iNameCount)
{
	xrttarnameselector Selector;
	size_t i;

	if ( pReader == NULL || pReader->Failed ) { return false; }
	if ( pNames == NULL && iNameCount != 0u ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
			"TAR extraction names are required");
		return false;
	}
	for ( i = 0u; i < iNameCount; ++i ) {
		if ( pNames[i].Data == NULL && pNames[i].Size != 0u ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
				"TAR extraction name contains invalid storage");
			return false;
		}
	}
	Selector.pNames = pNames;
	Selector.iCount = iNameCount;
	return xrtTarReaderExtractWhere(pReader, sDestination, bOverwrite,
		__xrtTarSelectName, &Selector, NULL, NULL);
}




/* 无进度回调的便捷提取入口。 */
XRT_API bool xrtTarReaderExtractAll(xrttarreader* pReader,
	cstr sDestination, bool bOverwrite)
{
	return xrtTarReaderExtract(pReader, sDestination,
		bOverwrite, NULL, NULL);
}
