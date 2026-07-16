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



static str __xrtTarOutputPath(str sDestination, const char* sName)
{
	str sJoined = xrtPathJoin(2u, sDestination, (str)sName);
	return sJoined != NULL ? xrtCopyStr(sJoined, 0u) : NULL;
}

static bool __xrtTarEnsureParent(xrttarreader* pReader, str sPath)
{
	str sParentValue = xrtPathGetDir(sPath, 0u);
	str sParent = sParentValue != NULL ? xrtCopyStr(sParentValue, 0u) : NULL;
	bool bResult = sParent != NULL &&
		(xrtDirExists(sParent) || xrtDirCreateAll(sParent));
	xrtFree(sParent);
	if ( !bResult ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot create TAR entry parent directory: %s", sPath);
	}
	return bResult;
}

static bool __xrtTarPrepareTarget(xrttarreader* pReader, str sPath, bool bOverwrite)
{
	if ( !xrtPathExists(sPath) ) { return TRUE; }
	if ( !bOverwrite ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_EXISTS,
			"TAR extraction target already exists: %s", sPath);
		return FALSE;
	}
	if ( xrtDirExists(sPath) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_EXISTS,
			"TAR extraction will not replace a directory: %s", sPath);
		return FALSE;
	}
	if ( !xrtFileDelete(sPath) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot replace TAR extraction target: %s", sPath);
		return FALSE;
	}
	return TRUE;
}

static bool __xrtTarExtractFile(xrttarreader* pReader, str sOutput, bool bOverwrite)
{
	str sTemp;
	xwriter pWriter;
	bool bResult;
	if ( !__xrtTarEnsureParent(pReader, sOutput) ) { return FALSE; }
	if ( xrtPathExists(sOutput) && !bOverwrite ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_EXISTS,
			"TAR extraction target already exists: %s", sOutput);
		return FALSE;
	}
	sTemp = xrtPathRandom(sOutput, 0u, (str)".xrt-tar-tmp", 0u, 8u);
	if ( sTemp == NULL || sTemp[0] == 0 ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot create TAR extraction temporary path");
		xrtFree(sTemp);
		return FALSE;
	}
	pWriter = xrtWriterOpen(sTemp, TRUE);
	if ( pWriter == NULL ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot create TAR extraction temporary file: %s", sTemp);
		xrtFree(sTemp);
		return FALSE;
	}
	bResult = xrtTarReaderCopy(pReader, pWriter) && xrtWriterFlush(pWriter);
	xrtWriterDestroy(pWriter);
	if ( bResult ) {
		bResult = xrtFileRename(sTemp, sOutput, bOverwrite);
		if ( !bResult ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
				"cannot commit TAR extraction target: %s", sOutput);
		}
	}
	if ( !bResult && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



static bool __xrtTarQueueMode(xrttarreader* pReader, xrttarpendingmode** pHead,
	str sPath, uint32 iMode)
{
	xrttarpendingmode* pItem;
	if ( !pReader->Options.bPreservePermissions ) { return TRUE; }
	pItem = (xrttarpendingmode*)xrtCalloc(1u, sizeof(*pItem));
	if ( pItem != NULL ) { pItem->sPath = xrtCopyStr(sPath, 0u); }
	if ( pItem == NULL || pItem->sPath == NULL ) {
		xrtFree(pItem);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot retain TAR permission metadata");
		return FALSE;
	}
	pItem->iMode = iMode & pReader->Options.iFileModeMask;
	pItem->pNext = *pHead;
	*pHead = pItem;
	return TRUE;
}

static bool __xrtTarApplyModes(xrttarreader* pReader, xrttarpendingmode* pHead)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)pReader;
		(void)pHead;
		return TRUE;
	#else
		xrttarpendingmode* pItem;
		for ( pItem = pHead; pItem != NULL; pItem = pItem->pNext ) {
			if ( !xrtFileSetAttr(pItem->sPath, (int)pItem->iMode) ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot restore TAR entry permissions: %s", pItem->sPath);
				return FALSE;
			}
		}
		return TRUE;
	#endif
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



static bool __xrtTarQueueHardlink(xrttarreader* pReader, xrttarpendinglink** pHead,
	str sDestination, str sOutput, const char* sTarget)
{
	xrttarpendinglink* pItem;
	str sTargetPath;
	if ( !pReader->Options.bAllowHardlinks ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR hard links are disabled: %s", pReader->Entry.sName);
		return FALSE;
	}
	if ( sTarget == NULL || !xrtPathIsSafeArchiveEntry((str)sTarget, FALSE) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSAFE_PATH,
			"unsafe TAR hard-link target: %s", sTarget != NULL ? sTarget : "");
		return FALSE;
	}
	sTargetPath = __xrtTarOutputPath(sDestination, sTarget);
	pItem = (xrttarpendinglink*)xrtCalloc(1u, sizeof(*pItem));
	if ( pItem != NULL ) {
		pItem->sOutput = xrtCopyStr(sOutput, 0u);
		pItem->sTarget = sTargetPath;
	}
	if ( pItem == NULL || pItem->sOutput == NULL || pItem->sTarget == NULL ) {
		xrtFree(sTargetPath);
		if ( pItem != NULL ) { xrtFree(pItem->sOutput); }
		xrtFree(pItem);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot retain TAR hard-link metadata");
		return FALSE;
	}
	if ( strcmp((const char*)pItem->sOutput, (const char*)pItem->sTarget) == 0 ) {
		xrtFree(pItem->sOutput);
		xrtFree(pItem->sTarget);
		xrtFree(pItem);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
			"TAR hard link cannot target itself: %s", sTarget);
		return FALSE;
	}
	pItem->pNext = *pHead;
	*pHead = pItem;
	return TRUE;
}

static bool __xrtTarCreateHardlinks(xrttarreader* pReader, xrttarpendinglink* pHead,
	bool bOverwrite)
{
	bool bRemaining;
	bool bProgress;
	do {
		xrttarpendinglink* pItem;
		bRemaining = FALSE;
		bProgress = FALSE;
		for ( pItem = pHead; pItem != NULL; pItem = pItem->pNext ) {
			if ( pItem->bDone ) { continue; }
			bRemaining = TRUE;
			if ( !xrtFileExists(pItem->sTarget) ) { continue; }
			if ( !__xrtTarEnsureParent(pReader, pItem->sOutput) ||
				!__xrtTarPrepareTarget(pReader, pItem->sOutput, bOverwrite) ||
				!xrtFileCreateHardLink(pItem->sOutput, pItem->sTarget) ) {
				if ( pReader->Error.iCode == XRT_TAR_OK ) {
					__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
						"cannot create TAR hard link: %s", pItem->sOutput);
				}
				return FALSE;
			}
			pItem->bDone = TRUE;
			bProgress = TRUE;
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
		return FALSE;
	}
	return TRUE;
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



static bool __xrtTarExtractSymlink(xrttarreader* pReader, const xrttarentry* pEntry,
	str sOutput, bool bOverwrite)
{
	if ( !pReader->Options.bAllowSymlinks ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR symbolic links are disabled: %s", pEntry->sName);
		return FALSE;
	}
	if ( pEntry->sLinkName == NULL ||
		!xrtPathIsSafeArchiveEntry((str)pEntry->sLinkName, FALSE) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSAFE_PATH,
			"unsafe TAR symbolic-link target: %s",
			pEntry->sLinkName != NULL ? pEntry->sLinkName : "");
		return FALSE;
	}
	if ( !__xrtTarEnsureParent(pReader, sOutput) ||
		!__xrtTarPrepareTarget(pReader, sOutput, bOverwrite) ||
		!xrtFileCreateSymbolicLink(sOutput, (str)pEntry->sLinkName, FALSE) ) {
		if ( pReader->Error.iCode == XRT_TAR_OK ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
				"cannot create TAR symbolic link: %s", pEntry->sName);
		}
		return FALSE;
	}
	return TRUE;
}

static bool __xrtTarExtractFifo(xrttarreader* pReader, const xrttarentry* pEntry,
	str sOutput, bool bOverwrite)
{
	if ( !pReader->Options.bAllowSpecialFiles ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR special files are disabled: %s", pEntry->sName);
		return FALSE;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)sOutput;
		(void)bOverwrite;
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR FIFO extraction is not supported on Windows: %s", pEntry->sName);
		return FALSE;
	#else
		if ( !__xrtTarEnsureParent(pReader, sOutput) ||
			!__xrtTarPrepareTarget(pReader, sOutput, bOverwrite) ||
			!xrtFileCreateFifo(sOutput, pReader->Options.bPreservePermissions ?
				(int)(pEntry->iMode & pReader->Options.iFileModeMask) : 0600) ) {
			if ( pReader->Error.iCode == XRT_TAR_OK ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot create TAR FIFO: %s", pEntry->sName);
			}
			return FALSE;
		}
		return TRUE;
	#endif
}



XXAPI bool xrtTarReaderExtractAll(xrttarreader* pReader, str sDestination, bool bOverwrite)
{
	xrttarentry tEntry;
	xrttarpendinglink* pLinks = NULL;
	xrttarpendingmode* pModes = NULL;
	bool bHaveCurrent;
	bool bResult = FALSE;
	if ( pReader == NULL || pReader->Failed || sDestination == NULL || sDestination[0] == 0 ) {
		return FALSE;
	}
	if ( !xrtDirExists(sDestination) && !xrtDirCreateAll(sDestination) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
			"cannot create TAR extraction directory: %s", sDestination);
		return FALSE;
	}
	bHaveCurrent = pReader->HasCurrent;
	for ( ;; ) {
		str sOutput;
		bool bDirectory;
		if ( bHaveCurrent ) {
			tEntry = pReader->Entry;
			bHaveCurrent = FALSE;
		}
		else if ( !xrtTarReaderNext(pReader, &tEntry) ) {
			if ( !xrtTarReaderEOF(pReader) ) { goto cleanup; }
			break;
		}
		bDirectory = tEntry.iType == XRT_TAR_ENTRY_DIRECTORY;
		if ( !__xrtTarEntryPathSafe(tEntry.sName, bDirectory,
			&pReader->Error, tEntry.iIndex) ) {
			pReader->Failed = TRUE;
			xrtSetError(xrtCopyStr((str)pReader->Error.sMessage, 0u), TRUE);
			goto cleanup;
		}
		sOutput = __xrtTarOutputPath(sDestination, tEntry.sName);
		if ( sOutput == NULL ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
				"cannot build TAR extraction path");
			goto cleanup;
		}
		if ( tEntry.iType == XRT_TAR_ENTRY_DIRECTORY ) {
			if ( (!xrtDirExists(sOutput) && !xrtDirCreateAll(sOutput)) ||
				!xrtTarReaderSkip(pReader) ||
				!__xrtTarQueueMode(pReader, &pModes, sOutput, tEntry.iMode) ) {
				if ( pReader->Error.iCode == XRT_TAR_OK ) {
					__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
						"cannot create TAR directory: %s", sOutput);
				}
				xrtFree(sOutput);
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_FILE ) {
			if ( !__xrtTarExtractFile(pReader, sOutput, bOverwrite) ||
				!__xrtTarQueueMode(pReader, &pModes, sOutput, tEntry.iMode) ) {
				xrtFree(sOutput);
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_SYMLINK ) {
			if ( !__xrtTarExtractSymlink(pReader, &tEntry, sOutput, bOverwrite) ||
				!xrtTarReaderSkip(pReader) ) {
				xrtFree(sOutput);
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_HARDLINK ) {
			if ( !__xrtTarQueueHardlink(pReader, &pLinks, sDestination, sOutput,
				tEntry.sLinkName) || !xrtTarReaderSkip(pReader) ) {
				xrtFree(sOutput);
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_FIFO ) {
			if ( !__xrtTarExtractFifo(pReader, &tEntry, sOutput, bOverwrite) ||
				!xrtTarReaderSkip(pReader) ||
				!__xrtTarQueueMode(pReader, &pModes, sOutput, tEntry.iMode) ) {
				xrtFree(sOutput);
				goto cleanup;
			}
		}
		else if ( tEntry.iType == XRT_TAR_ENTRY_CHARACTER ||
			tEntry.iType == XRT_TAR_ENTRY_BLOCK ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
				"TAR device-node extraction is intentionally unsupported: %s", tEntry.sName);
			xrtFree(sOutput);
			goto cleanup;
		}
		else {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_UNSUPPORTED,
				"unsupported TAR entry type: %s", tEntry.sName);
			xrtFree(sOutput);
			goto cleanup;
		}
		xrtFree(sOutput);
	}
	bResult = __xrtTarCreateHardlinks(pReader, pLinks, bOverwrite) &&
		__xrtTarApplyModes(pReader, pModes);

cleanup:
	__xrtTarFreeHardlinks(pLinks);
	__xrtTarFreeModes(pModes);
	return bResult;
}
