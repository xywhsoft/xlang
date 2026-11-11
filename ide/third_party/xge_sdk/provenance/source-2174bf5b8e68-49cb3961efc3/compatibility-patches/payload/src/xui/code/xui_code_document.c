#include "xui_config.h"
#if XUI_ENABLE_CODE_EDIT
#include "xui_internal.h"
#include "xui_code_document_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct xui_code_line_t {
	int iStart;
	int iEnd;
	int iTextEnd;
} xui_code_line_t;

typedef struct xui_code_edit_action_t {
	int iStart;
	char* sDeleted;
	int iDeletedLength;
	char* sInserted;
	int iInsertedLength;
} xui_code_edit_action_t;

typedef struct xui_code_edit_group_t {
	xui_code_edit_action_t* pActions;
	int iActionCount;
	int iActionCapacity;
	uint64_t iBeforeState;
	uint64_t iAfterState;
} xui_code_edit_group_t;

#define XUI_CODE_DOCUMENT_MAGIC UINT32_C(0x5843444f)
typedef struct xui_code_document_call_t xui_code_document_call_t;
struct xui_code_document_t {
    uint32_t iMagic;
    uint64_t iCreatorThread;
    size_t iActiveCalls;
    bool bWriting,bDestroyPending,bDestroying;
    xui_lifetime_link_t* pLifetimeLinks;
    xui_widget pInternalOwner;
    xui_context pObserverContext;
    xui_lifetime_link_t tObserverContextLink;
    xui_internal_code_document_prepare_proc onPrepareEdit;
    xui_code_document_call_t* pActiveCall;
	char* sBuffer;
	int iLength;
	int iBufferCapacity;
	int iGapStart;
	int iGapEnd;
	char* sSnapshot;
	int iSnapshotCapacity;
	int bSnapshotValid;
	xui_code_line_t* pLines;
	int iLineCount;
	int iLineCapacity;
	int iPendingLineShiftStart;
	int iPendingLineShiftDelta;
	xui_code_edit_group_t* pUndo;
	int iUndoCount;
	int iUndoCapacity;
	xui_code_edit_group_t* pRedo;
	int iRedoCount;
	int iRedoCapacity;
	xui_code_edit_group_t tEditGroup;
	int iEditDepth;
	int bHasLastEditRange;
	int bChangePending;
	int bApplyingHistory;
	int iLastEditStart;
	int iLastEditEnd;
	uint32_t iVersion;
	uint32_t iChangeVersion;
	uint64_t iState;
	uint64_t iSavedState;
	uint64_t iNextState;
	int bDirty;
	int bForcedDirty;
	xui_code_document_change_proc onChange;
	void* pChangeUser;
	xui_internal_code_document_edit_proc onEdit;
	void* pEditUser;
	void* pLexerCCache;
	void (*onLexerCCacheDestroy)(void* pCache);
	char sError[160];
};

struct xui_code_document_call_t {
    xui_code_document document;
    xui_context context;
    xui_call_scope_t scope;
    xui_internal_code_document_prepare_proc prepare;
    xui_internal_code_document_edit_proc edited;
    void* user;
};
static int __xuiCodeDocumentDestroyNow(xui_code_document);
static int __xuiCodeDocumentCallBegin(xui_code_document document,xui_code_document_call_t* call)
{
    if(!document || document->iMagic!=XUI_CODE_DOCUMENT_MAGIC || document->bDestroying || document->bDestroyPending)return XUI_ERROR_INVALID_STATE;
    if(document->iCreatorThread!=xrtThreadCurrentId() || document->bWriting)return XUI_ERROR_BUSY;
    if(document->pInternalOwner && !xuiInternalWidgetIsValid(document->pInternalOwner))return XUI_ERROR_INVALID_STATE;
    call->context=document->pObserverContext ? document->pObserverContext:(document->pInternalOwner ? document->pInternalOwner->pContext:NULL);
    if(call->context){int status=xuiCallScopeBegin(call->context,&call->scope);if(status!=XUI_OK)return status;}
    ++document->iActiveCalls;document->bWriting=true;document->pActiveCall=call;
    call->document=document;call->prepare=document->onPrepareEdit;call->edited=document->onEdit;call->user=document->pEditUser;
    return XUI_OK;
}
static int __xuiCodeDocumentCallEnd(xui_code_document_call_t* call,int status)
{
    if(!call->document)return status;
    xui_code_document document=call->document;
    if(status==XUI_OK && (document->bDestroyPending ||
        (document->pInternalOwner && !xuiInternalWidgetIsValid(document->pInternalOwner)) ||
        (call->context && (call->context->bDestroyPending || call->context->bDestroying))))status=XUI_ERROR_INVALID_STATE;
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,status);
    /* The Document pin outlives Context tail cleanup: closing CodeEdit there
     * may consume its unique Document owner, but cannot free this call. */
    receipt.status=xuiInternalCallScopeEndOutcome(&call->scope,receipt.status,&receipt.error);
    document->pActiveCall=NULL;document->bWriting=false;--document->iActiveCalls;
    if(document->bDestroyPending && !document->iActiveCalls)xuiInternalCleanupAdd(&receipt,__xuiCodeDocumentDestroyNow(document));
    call->document=NULL;return xuiInternalCleanupEnd(NULL,&receipt);
}
static int __xuiCodeDocumentPrepareEdit(xui_code_document document,int start,int end,
    const char* text,int bytes,bool reset)
{
    xui_code_document_call_t* call=document->pActiveCall;
    if(!call)return XUI_ERROR_INVALID_STATE;
    if(call->prepare){int status=call->prepare(document,start,end,text,bytes,reset,call->user);if(status!=XUI_OK)return status;}
    if(document->bDestroyPending || (document->pInternalOwner && !xuiInternalWidgetIsValid(document->pInternalOwner)) ||
        (call->context && (call->context->bDestroyPending || call->context->bDestroying)))return XUI_ERROR_INVALID_STATE;
    return XUI_OK;
}
static void __xuiCodeDocumentEdited(xui_code_document document,int start,int end,int newEnd,
    int firstLine,int lastLine,int newLastLine,bool reset)
{
    xui_code_document_call_t* call=document->pActiveCall;
    if(call && call->edited && !document->bDestroyPending &&
        (!document->pInternalOwner || xuiInternalWidgetIsValid(document->pInternalOwner)) && (!call->context || (!call->context->bDestroyPending && !call->context->bDestroying)))
        call->edited(document,start,end,newEnd,firstLine,lastLine,newLastLine,reset,call->user);
}
XUI_API bool xuiCodeDocumentOwnershipBusy(xui_code_document document)
{return !document || document->iMagic!=XUI_CODE_DOCUMENT_MAGIC || document->iActiveCalls || document->bDestroying;}
XUI_API int xuiCodeDocumentLifetimeWatch(xui_code_document document,xui_lifetime_link_t* link,xui_lifetime_invalidate_proc invalidated,void* user)
{
    if(!document || document->iMagic!=XUI_CODE_DOCUMENT_MAGIC || document->bDestroyPending || !link || !invalidated)return XUI_ERROR_INVALID_ARGUMENT;
    if(document->iCreatorThread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    if(link->pNext || link->ppPrevious || link->onInvalidate || link->pUser)return XUI_ERROR_INVALID_STATE;
    link->pNext=document->pLifetimeLinks;link->ppPrevious=&document->pLifetimeLinks;link->onInvalidate=invalidated;link->pUser=user;
    if(link->pNext)link->pNext->ppPrevious=&link->pNext;
    document->pLifetimeLinks=link;return XUI_OK;
}
static int __xuiCodeDocumentImplSetTextLength(xui_code_document pDocument, const char* sText, int iLength);
static int __xuiCodeDocumentImplLoadTextFile(xui_code_document pDocument, const char* sPath, int iCharset);
static int __xuiCodeDocumentImplReplace(xui_code_document pDocument, int iStart, int iEnd, const char* sText);
static int __xuiCodeDocumentImplBeginEdit(xui_code_document pDocument);
static int __xuiCodeDocumentImplEndEdit(xui_code_document pDocument);
static int __xuiCodeDocumentImplUndo(xui_code_document pDocument);
static int __xuiCodeDocumentImplRedo(xui_code_document pDocument);
static int __xuiCodeDocumentRebuildLines(xui_code_document pDocument);
static int __xuiCodeDocumentReserveLines(xui_code_document pDocument, int iCapacity);
static unsigned char __xuiCodeDocumentByteAt(xui_code_document pDocument, int iOffset);

static int __xuiCodeDocumentUnicodeRead(void* pUser, int iOffset, unsigned char* pByte)
{
	xui_code_document pDocument;

	pDocument = (xui_code_document)pUser;
	if ( pDocument == NULL || pByte == NULL ||
	     iOffset < 0 || iOffset >= pDocument->iLength ) return 0;
	*pByte = __xuiCodeDocumentByteAt(pDocument, iOffset);
	return 1;
}

static void __xuiCodeDocumentMaterializeLineShift(xui_code_document pDocument)
{
	xui_code_line_t* pLine;
	int i;

	if ( pDocument == NULL || pDocument->iPendingLineShiftDelta == 0 ) return;
	for ( i = pDocument->iPendingLineShiftStart; i < pDocument->iLineCount; i++ ) {
		pLine = &pDocument->pLines[i];
		pLine->iStart += pDocument->iPendingLineShiftDelta;
		pLine->iEnd += pDocument->iPendingLineShiftDelta;
		pLine->iTextEnd += pDocument->iPendingLineShiftDelta;
	}
	pDocument->iPendingLineShiftStart = 0;
	pDocument->iPendingLineShiftDelta = 0;
}

static int __xuiCodeDocumentLineField(xui_code_document pDocument,
	int iLine, int iField)
{
	int iValue;

	if ( pDocument == NULL || iLine < 0 || iLine >= pDocument->iLineCount ) return 0;
	if ( iField == 0 ) iValue = pDocument->pLines[iLine].iStart;
	else if ( iField == 1 ) iValue = pDocument->pLines[iLine].iEnd;
	else iValue = pDocument->pLines[iLine].iTextEnd;
	if ( pDocument->iPendingLineShiftDelta != 0 &&
	     iLine >= pDocument->iPendingLineShiftStart ) {
		iValue += pDocument->iPendingLineShiftDelta;
	}
	return iValue;
}

static int __xuiCodeDocumentGapSize(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return 0;
	return pDocument->iGapEnd - pDocument->iGapStart;
}

static unsigned char __xuiCodeDocumentByteAt(xui_code_document pDocument, int iOffset)
{
	int iPhysical;

	if ( pDocument == NULL || iOffset < 0 || iOffset >= pDocument->iLength ) return 0;
	iPhysical = iOffset;
	if ( iOffset >= pDocument->iGapStart ) iPhysical += __xuiCodeDocumentGapSize(pDocument);
	return (unsigned char)pDocument->sBuffer[iPhysical];
}

static int __xuiCodeDocumentCopyRangeRaw(xui_code_document pDocument,
	int iStart, int iEnd, char* sOutput)
{
	int iBeforeEnd;
	int iBeforeLength;
	int iAfterLength;

	if ( pDocument == NULL || sOutput == NULL ||
	     iStart < 0 || iEnd < iStart || iEnd > pDocument->iLength ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iBeforeEnd = (iEnd < pDocument->iGapStart) ? iEnd : pDocument->iGapStart;
	iBeforeLength = iBeforeEnd - iStart;
	if ( iBeforeLength < 0 ) iBeforeLength = 0;
	if ( iBeforeLength > 0 ) {
		memcpy(sOutput, pDocument->sBuffer + iStart, (size_t)iBeforeLength);
	}
	iAfterLength = iEnd - (iStart + iBeforeLength);
	if ( iAfterLength > 0 ) {
		int iAfterStart = iStart + iBeforeLength;
		memcpy(sOutput + iBeforeLength,
			pDocument->sBuffer + iAfterStart + __xuiCodeDocumentGapSize(pDocument),
			(size_t)iAfterLength);
	}
	return XUI_OK;
}

static int __xuiCodeDocumentRangeEquals(xui_code_document pDocument,
	int iStart, int iEnd, const char* sText, int iTextLength)
{
	int i;

	if ( pDocument == NULL || sText == NULL || iStart < 0 || iEnd < iStart ||
	     iEnd > pDocument->iLength || iTextLength != iEnd - iStart ) return 0;
	for ( i = 0; i < iTextLength; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, iStart + i) !=
		     (unsigned char)sText[i] ) return 0;
	}
	return 1;
}

static void __xuiCodeDocumentSetError(xui_code_document pDocument, const char* sError)
{
	if ( pDocument == NULL ) return;
	if ( sError == NULL ) sError = "";
	strncpy(pDocument->sError, sError, sizeof(pDocument->sError) - 1u);
	pDocument->sError[sizeof(pDocument->sError) - 1u] = '\0';
}

static void __xuiCodeDocumentUpdateDirty(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return;
	pDocument->bDirty = pDocument->bForcedDirty ||
		(pDocument->iState != pDocument->iSavedState);
}

static int __xuiCodeDocumentUtf8Next(xui_code_document pDocument, int iOffset)
{
	if ( pDocument == NULL ) return 0;
	return xuiInternalTextGraphemeNextRead(__xuiCodeDocumentUnicodeRead,
		pDocument, pDocument->iLength, iOffset);
}

static int __xuiCodeDocumentClampOffset(xui_code_document pDocument, int iOffset)
{
	if ( pDocument == NULL ) return 0;
	return xuiInternalTextGraphemeClampRead(__xuiCodeDocumentUnicodeRead,
		pDocument, pDocument->iLength, iOffset);
}

static int __xuiCodeDocumentReserveSnapshot(xui_code_document pDocument, int iCapacity)
{
	char* sNew;
	int iDoubledCapacity;

	if ( pDocument == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pDocument->iSnapshotCapacity ) return XUI_OK;
	iDoubledCapacity = (pDocument->iSnapshotCapacity <= INT_MAX / 2) ?
		pDocument->iSnapshotCapacity * 2 : iCapacity;
	if ( iCapacity < iDoubledCapacity ) {
		iCapacity = iDoubledCapacity;
	}
	if ( iCapacity < 32 ) iCapacity = 32;
	sNew = (char*)xrtRealloc(pDocument->sSnapshot, (size_t)iCapacity);
	if ( sNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->sSnapshot = sNew;
	pDocument->iSnapshotCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentEnsureSnapshot(xui_code_document pDocument)
{
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->bSnapshotValid ) return XUI_OK;
	iRet = __xuiCodeDocumentReserveSnapshot(pDocument, pDocument->iLength + 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCodeDocumentCopyRangeRaw(pDocument, 0, pDocument->iLength, pDocument->sSnapshot);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->sSnapshot[pDocument->iLength] = '\0';
	pDocument->bSnapshotValid = 1;
	return XUI_OK;
}

static int __xuiCodeDocumentMoveGap(xui_code_document pDocument, int iOffset)
{
	int iMove;

	if ( pDocument == NULL || iOffset < 0 || iOffset > pDocument->iLength ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iOffset < pDocument->iGapStart ) {
		iMove = pDocument->iGapStart - iOffset;
		memmove(pDocument->sBuffer + pDocument->iGapEnd - iMove,
			pDocument->sBuffer + iOffset, (size_t)iMove);
		pDocument->iGapStart = iOffset;
		pDocument->iGapEnd -= iMove;
	} else if ( iOffset > pDocument->iGapStart ) {
		iMove = iOffset - pDocument->iGapStart;
		memmove(pDocument->sBuffer + pDocument->iGapStart,
			pDocument->sBuffer + pDocument->iGapEnd, (size_t)iMove);
		pDocument->iGapStart += iMove;
		pDocument->iGapEnd += iMove;
	}
	return XUI_OK;
}

static int __xuiCodeDocumentEnsureGap(xui_code_document pDocument, int iNeed)
{
	char* sNew;
	int iGapSize;
	int iSuffixLength;
	int iGrowth;
	int iNewCapacity;
	int iNewGapEnd;

	if ( pDocument == NULL || iNeed < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iGapSize = __xuiCodeDocumentGapSize(pDocument);
	if ( iGapSize >= iNeed ) return XUI_OK;
	iGrowth = pDocument->iLength / 8;
	if ( iGrowth < 4096 ) iGrowth = 4096;
	if ( iGrowth > 1024 * 1024 ) iGrowth = 1024 * 1024;
	if ( iGrowth < iNeed - iGapSize ) iGrowth = iNeed - iGapSize;
	if ( iGapSize > INT_MAX - pDocument->iLength ||
	     iGrowth > INT_MAX - pDocument->iLength - iGapSize ) {
		__xuiCodeDocumentSetError(pDocument, "document too large");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iNewCapacity = pDocument->iLength + iGapSize + iGrowth;
	iSuffixLength = pDocument->iBufferCapacity - pDocument->iGapEnd;
	sNew = (char*)xrtRealloc(pDocument->sBuffer, (size_t)iNewCapacity);
	if ( sNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->sBuffer = sNew;
	iNewGapEnd = iNewCapacity - iSuffixLength;
	if ( iSuffixLength > 0 && iNewGapEnd != pDocument->iGapEnd ) {
		memmove(pDocument->sBuffer + iNewGapEnd,
			pDocument->sBuffer + pDocument->iGapEnd,
			(size_t)iSuffixLength);
	}
	pDocument->iGapEnd = iNewGapEnd;
	pDocument->iBufferCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentSetOwnedText(xui_code_document pDocument, char* sText, int iLength)
{
	char* sNewBuffer;
	char* sOldBuffer;
	xui_code_line_t* pLine;
	int iRet;
	int iGapReserve;
	int iNewCapacity;
	int iLineCount;
	int iLine;
	int iStart;
	int i;

	if ( pDocument == NULL || sText == NULL || iLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iGapReserve = 4096;
	if ( iLength > 0 && iLength / 64 > iGapReserve ) iGapReserve = iLength / 64;
	if ( iGapReserve > 1024 * 1024 ) iGapReserve = 1024 * 1024;
	if ( iLength > INT_MAX - iGapReserve ) {
		__xuiCodeDocumentSetError(pDocument, "document too large");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iLineCount = 1;
	for ( i = 0; i < iLength; i++ ) {
		if ( sText[i] == '\n' ) {
			if ( iLineCount == INT_MAX ) return XUI_ERROR_OUT_OF_MEMORY;
			iLineCount++;
		}
	}
	iRet = __xuiCodeDocumentReserveLines(pDocument, iLineCount);
	if ( iRet != XUI_OK ) return iRet;
	iNewCapacity = iLength + iGapReserve;
	sNewBuffer = (char*)xrtRealloc(sText, (size_t)iNewCapacity);
	if ( sNewBuffer == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}

	/* All fallible work is complete. Install text and its derived line index together. */
	sOldBuffer = pDocument->sBuffer;
	pDocument->sBuffer = sNewBuffer;
	pDocument->iLength = iLength;
	pDocument->iBufferCapacity = iNewCapacity;
	pDocument->iGapStart = iLength;
	pDocument->iGapEnd = iNewCapacity;
	pDocument->bSnapshotValid = 0;
	pDocument->iPendingLineShiftStart = 0;
	pDocument->iPendingLineShiftDelta = 0;
	pDocument->iLineCount = 0;
	iLine = 0;
	iStart = 0;
	for ( i = 0; i < iLength; i++ ) {
		if ( sNewBuffer[i] != '\n' ) continue;
		pLine = &pDocument->pLines[iLine++];
		pLine->iStart = iStart;
		pLine->iEnd = i + 1;
		pLine->iTextEnd = i;
		iStart = i + 1;
	}
	pLine = &pDocument->pLines[iLine++];
	pLine->iStart = iStart;
	pLine->iEnd = iLength;
	pLine->iTextEnd = iLength;
	pDocument->iLineCount = iLine;
	xrtFree(sOldBuffer);
	return XUI_OK;
}

static int __xuiCodeDocumentReserveLines(xui_code_document pDocument, int iCapacity)
{
	xui_code_line_t* pNew;
	int iDoubledCapacity;

	if ( pDocument == NULL || iCapacity < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= pDocument->iLineCapacity ) return XUI_OK;
	iDoubledCapacity = (pDocument->iLineCapacity <= INT_MAX / 2) ?
		pDocument->iLineCapacity * 2 : iCapacity;
	if ( iCapacity < iDoubledCapacity ) {
		iCapacity = iDoubledCapacity;
	}
	if ( iCapacity < 16 ) iCapacity = 16;
	if ( (size_t)iCapacity > SIZE_MAX / sizeof(*pNew) ) {
		__xuiCodeDocumentSetError(pDocument, "document too large");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pNew = (xui_code_line_t*)xrtRealloc(pDocument->pLines, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->pLines = pNew;
	pDocument->iLineCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentAppendLine(xui_code_document pDocument, int iStart, int iEnd, int iTextEnd)
{
	int iRet;

	iRet = __xuiCodeDocumentReserveLines(pDocument, pDocument->iLineCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->pLines[pDocument->iLineCount].iStart = iStart;
	pDocument->pLines[pDocument->iLineCount].iEnd = iEnd;
	pDocument->pLines[pDocument->iLineCount].iTextEnd = iTextEnd;
	pDocument->iLineCount++;
	return XUI_OK;
}

static int __xuiCodeDocumentRebuildLines(xui_code_document pDocument)
{
	xui_code_line_t* pLine;
	int i;
	int iStart;
	int iRet;
	int iLineCount;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->iPendingLineShiftStart = 0;
	pDocument->iPendingLineShiftDelta = 0;
	iLineCount = 1;
	for ( i = 0; i < pDocument->iLength; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) {
			if ( iLineCount == INT_MAX ) return XUI_ERROR_OUT_OF_MEMORY;
			iLineCount++;
		}
	}
	iRet = __xuiCodeDocumentReserveLines(pDocument, iLineCount);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->iLineCount = 0;
	iStart = 0;
	for ( i = 0; i < pDocument->iLength; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) {
			pLine = &pDocument->pLines[pDocument->iLineCount++];
			pLine->iStart = iStart;
			pLine->iEnd = i + 1;
			pLine->iTextEnd = i;
			iStart = i + 1;
		}
	}
	pLine = &pDocument->pLines[pDocument->iLineCount++];
	pLine->iStart = iStart;
	pLine->iEnd = pDocument->iLength;
	pLine->iTextEnd = pDocument->iLength;
	return XUI_OK;
}

static int __xuiCodeDocumentFindLine(xui_code_document pDocument, int iOffset)
{
	int iLow;
	int iHigh;
	int iMid;

	if ( pDocument == NULL || pDocument->iLineCount <= 0 ) return 0;
	if ( iOffset <= 0 ) return 0;
	if ( iOffset >= pDocument->iLength ) return pDocument->iLineCount - 1;
	iLow = 0;
	iHigh = pDocument->iLineCount - 1;
	while ( iLow <= iHigh ) {
		iMid = iLow + (iHigh - iLow) / 2;
		if ( iOffset < __xuiCodeDocumentLineField(pDocument, iMid, 0) ) {
			iHigh = iMid - 1;
		} else if ( iOffset >= __xuiCodeDocumentLineField(pDocument, iMid, 1) &&
		            iMid + 1 < pDocument->iLineCount ) {
			iLow = iMid + 1;
		} else {
			return iMid;
		}
	}
	if ( iLow < 0 ) return 0;
	if ( iLow >= pDocument->iLineCount ) return pDocument->iLineCount - 1;
	return iLow;
}

static int __xuiCodeDocumentUpdateLinesForReplace(xui_code_document pDocument,
	int iStartLine, int iEndLine, int iOldSegmentEnd, int iDelta,
	int bInsertedLineBreak)
{
	xui_code_line_t* pLine;
	int iNewSegmentEnd;
	int iSuffixStart;
	int iSuffixCount;
	int iAffectedCount;
	int iOldAffectedCount;
	int iNewLineCount;
	int iWrite;
	int iStart;
	int i;
	int iRet;

	if ( pDocument == NULL || iStartLine < 0 || iEndLine < iStartLine ||
	     iEndLine >= pDocument->iLineCount ) return XUI_ERROR_INVALID_ARGUMENT;
	iNewSegmentEnd = iOldSegmentEnd + iDelta;
	if ( iNewSegmentEnd < __xuiCodeDocumentLineField(pDocument, iStartLine, 0) ) {
		iNewSegmentEnd = __xuiCodeDocumentLineField(pDocument, iStartLine, 0);
	}
	if ( iNewSegmentEnd > pDocument->iLength ) iNewSegmentEnd = pDocument->iLength;
	iSuffixStart = iEndLine + 1;
	iSuffixCount = pDocument->iLineCount - iSuffixStart;
	iOldAffectedCount = iEndLine - iStartLine + 1;
	if ( iStartLine == iEndLine && !bInsertedLineBreak &&
	     (pDocument->iPendingLineShiftDelta == 0 ||
	      pDocument->iPendingLineShiftStart == iSuffixStart) ) {
		iStart = __xuiCodeDocumentLineField(pDocument, iStartLine, 0);
		pLine = &pDocument->pLines[iStartLine];
		pLine->iStart = iStart;
		pLine->iEnd = iNewSegmentEnd;
		pLine->iTextEnd = (iNewSegmentEnd > iStart &&
			__xuiCodeDocumentByteAt(pDocument, iNewSegmentEnd - 1) == '\n') ?
			iNewSegmentEnd - 1 : iNewSegmentEnd;
		if ( iSuffixCount > 0 ) {
			pDocument->iPendingLineShiftStart = iSuffixStart;
			pDocument->iPendingLineShiftDelta += iDelta;
		}
		return XUI_OK;
	}
	iAffectedCount = 0;
	for ( i = __xuiCodeDocumentLineField(pDocument, iStartLine, 0); i < iNewSegmentEnd; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) iAffectedCount++;
	}
	if ( iSuffixCount == 0 ||
	     iNewSegmentEnd <= __xuiCodeDocumentLineField(pDocument, iStartLine, 0) ||
	     __xuiCodeDocumentByteAt(pDocument, iNewSegmentEnd - 1) != '\n' ) {
		iAffectedCount++;
	}
	if ( iStartLine == iEndLine && iAffectedCount == iOldAffectedCount &&
	     (pDocument->iPendingLineShiftDelta == 0 ||
	      pDocument->iPendingLineShiftStart == iSuffixStart) ) {
		iStart = __xuiCodeDocumentLineField(pDocument, iStartLine, 0);
		pLine = &pDocument->pLines[iStartLine];
		pLine->iStart = iStart;
		pLine->iEnd = iNewSegmentEnd;
		pLine->iTextEnd = (iNewSegmentEnd > iStart &&
			__xuiCodeDocumentByteAt(pDocument, iNewSegmentEnd - 1) == '\n') ?
			iNewSegmentEnd - 1 : iNewSegmentEnd;
		if ( iSuffixCount > 0 ) {
			pDocument->iPendingLineShiftStart = iSuffixStart;
			pDocument->iPendingLineShiftDelta += iDelta;
		}
		return XUI_OK;
	}
	__xuiCodeDocumentMaterializeLineShift(pDocument);
	iNewLineCount = iStartLine + iAffectedCount + iSuffixCount;
	if ( iNewLineCount <= 0 ) iNewLineCount = 1;
	iRet = __xuiCodeDocumentReserveLines(pDocument, iNewLineCount);
	if ( iRet != XUI_OK ) return iRet;
	if ( iSuffixCount > 0 ) {
		memmove(&pDocument->pLines[iStartLine + iAffectedCount],
			&pDocument->pLines[iSuffixStart],
			sizeof(*pDocument->pLines) * (size_t)iSuffixCount);
		for ( i = 0; i < iSuffixCount; i++ ) {
			pLine = &pDocument->pLines[iStartLine + iAffectedCount + i];
			pLine->iStart += iDelta;
			pLine->iEnd += iDelta;
			pLine->iTextEnd += iDelta;
		}
	}
	iWrite = iStartLine;
	iStart = pDocument->pLines[iStartLine].iStart;
	for ( i = iStart; i < iNewSegmentEnd; i++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, i) == '\n' ) {
			pLine = &pDocument->pLines[iWrite++];
			pLine->iStart = iStart;
			pLine->iEnd = i + 1;
			pLine->iTextEnd = i;
			iStart = i + 1;
		}
	}
	if ( iWrite < iStartLine + iAffectedCount ) {
		pLine = &pDocument->pLines[iWrite++];
		pLine->iStart = iStart;
		pLine->iEnd = iNewSegmentEnd;
		pLine->iTextEnd = iNewSegmentEnd;
	}
	pDocument->iLineCount = iNewLineCount;
	return XUI_OK;
}

static int __xuiCodeDocumentReplaceRaw(xui_code_document pDocument,
	int iStart, int iEnd, const char* sText, int iTextLength)
{
	int iOldLength;
	int iNewLength;
	int iDeletedLength;
	int iDelta;
	int iDeletedLineBreaks;
	int iInsertedLineBreaks;
	int iNewLineCount;
	int iRequiredGap;
	int iStartLine;
	int iEndLine;
	int iNewEndLine;
	int iOldSegmentEnd;
	int bInsertedLineBreak;
	int iRet;

	if ( pDocument == NULL || sText == NULL || iTextLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iStart < 0 || iEnd < iStart || iEnd > pDocument->iLength ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldLength = pDocument->iLength;
	iDeletedLength = iEnd - iStart;
	if ( iTextLength > INT_MAX - (iOldLength - iDeletedLength) ) {
		__xuiCodeDocumentSetError(pDocument, "document too large");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iNewLength = iOldLength - iDeletedLength + iTextLength;
	iDeletedLineBreaks = 0;
	for ( iRet = iStart; iRet < iEnd; iRet++ ) {
		if ( __xuiCodeDocumentByteAt(pDocument, iRet) == '\n' ) iDeletedLineBreaks++;
	}
	iInsertedLineBreaks = 0;
	for ( iRet = 0; iRet < iTextLength; iRet++ ) {
		if ( sText[iRet] == '\n' ) iInsertedLineBreaks++;
	}
	iNewLineCount = pDocument->iLineCount - iDeletedLineBreaks + iInsertedLineBreaks;
	if ( iNewLineCount <= 0 ) iNewLineCount = 1;
	iRet = __xuiCodeDocumentReserveLines(pDocument, iNewLineCount);
	if ( iRet != XUI_OK ) return iRet;
	iRequiredGap = iTextLength - iDeletedLength;
	if ( iRequiredGap < 0 ) iRequiredGap = 0;
	iRet = __xuiCodeDocumentEnsureGap(pDocument, iRequiredGap);
	if ( iRet != XUI_OK ) return iRet;
    if(!pDocument->bApplyingHistory){
        iRet=__xuiCodeDocumentPrepareEdit(pDocument,iStart,iEnd,sText,iTextLength,false);if(iRet!=XUI_OK)return iRet;
    }
	iStartLine = __xuiCodeDocumentFindLine(pDocument, iStart);
	iEndLine = __xuiCodeDocumentFindLine(pDocument, iEnd);
	iOldSegmentEnd = __xuiCodeDocumentLineField(pDocument, iEndLine, 1);
	bInsertedLineBreak = (iInsertedLineBreaks > 0);
	iRet = __xuiCodeDocumentMoveGap(pDocument, iStart);
	if ( iRet != XUI_OK ) return iRet;
	pDocument->iGapEnd += iDeletedLength;
	if ( iTextLength > 0 ) {
		memcpy(pDocument->sBuffer + pDocument->iGapStart, sText, (size_t)iTextLength);
		pDocument->iGapStart += iTextLength;
	}
	pDocument->iLength = iNewLength;
	pDocument->bSnapshotValid = 0;
	iDelta = iTextLength - (iEnd - iStart);
	iRet = __xuiCodeDocumentUpdateLinesForReplace(pDocument, iStartLine,
		iEndLine, iOldSegmentEnd, iDelta, bInsertedLineBreak);
	if ( iRet != XUI_OK ) {
		/* The text is already valid; recover the derived index deterministically. */
		iRet = __xuiCodeDocumentRebuildLines(pDocument);
	}
    if(iRet==XUI_OK){
        iNewEndLine=__xuiCodeDocumentFindLine(pDocument,iStart+iTextLength);
        __xuiCodeDocumentEdited(pDocument,iStart,iEnd,iStart+iTextLength,iStartLine,iEndLine,iNewEndLine,false);
    }
	return iRet;
}

static char* __xuiCodeDocumentNormalizeText(const char* sText, int iInputLength, int* pLength)
{
	char* sOut;
	int i;
	int j;

	if ( sText == NULL ) sText = "";
	if ( iInputLength < 0 ) iInputLength = (int)strlen(sText);
	sOut = (char*)xrtMalloc((size_t)iInputLength + 1u);
	if ( sOut == NULL ) return NULL;
	j = 0;
	for ( i = 0; i < iInputLength; i++ ) {
		if ( sText[i] == '\r' ) {
			if ( (i + 1 < iInputLength) && (sText[i + 1] == '\n') ) i++;
			sOut[j++] = '\n';
		} else {
			sOut[j++] = sText[i];
		}
	}
	sOut[j] = '\0';
	if ( pLength != NULL ) *pLength = j;
	return sOut;
}

static int __xuiCodeDocumentNormalizeTextInPlace(char* sText, int iInputLength, int* pLength)
{
	int i;
	int j;

	if ( sText == NULL || iInputLength < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	j = 0;
	for ( i = 0; i < iInputLength; i++ ) {
		if ( sText[i] == '\r' ) {
			if ( (i + 1 < iInputLength) && (sText[i + 1] == '\n') ) i++;
			sText[j++] = '\n';
		} else {
			sText[j++] = sText[i];
		}
	}
	sText[j] = '\0';
	if ( pLength != NULL ) *pLength = j;
	return XUI_OK;
}

static int __xuiCodeDocumentValidateUtf8(xui_code_document pDocument, const char* sText, int iLength)
{
	if ( sText == NULL ) return XUI_OK;
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	if ( !xrtUtf8Valid(xuiXrtText(sText, (size_t)iLength), NULL) ) {
		__xuiCodeDocumentSetError(pDocument, "invalid UTF-8 input");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return XUI_OK;
}

static void __xuiCodeDocumentActionClear(xui_code_edit_action_t* pAction)
{
	if ( pAction == NULL ) return;
	xrtFree(pAction->sDeleted);
	xrtFree(pAction->sInserted);
	memset(pAction, 0, sizeof(*pAction));
}

static void __xuiCodeDocumentGroupClear(xui_code_edit_group_t* pGroup)
{
	int i;

	if ( pGroup == NULL ) return;
	for ( i = 0; i < pGroup->iActionCount; i++ ) {
		__xuiCodeDocumentActionClear(&pGroup->pActions[i]);
	}
	xrtFree(pGroup->pActions);
	memset(pGroup, 0, sizeof(*pGroup));
}

static void __xuiCodeDocumentGroupArrayClear(xui_code_edit_group_t* pGroups, int iCount)
{
	int i;

	if ( pGroups == NULL ) return;
	for ( i = 0; i < iCount; i++ ) {
		__xuiCodeDocumentGroupClear(&pGroups[i]);
	}
}

static int __xuiCodeDocumentGroupArrayReserve(xui_code_document pDocument, xui_code_edit_group_t** ppGroups, int* pCapacity, int iNeed)
{
	xui_code_edit_group_t* pNew;
	int iCapacity;

	if ( pDocument == NULL || ppGroups == NULL || pCapacity == NULL || iNeed < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iNeed <= *pCapacity ) return XUI_OK;
	iCapacity = (*pCapacity > 0) ? *pCapacity : 8;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > INT_MAX / 2 ) return XUI_ERROR_OUT_OF_MEMORY;
		iCapacity *= 2;
	}
	pNew = (xui_code_edit_group_t*)xrtRealloc(*ppGroups, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pNew + *pCapacity, 0, sizeof(*pNew) * (size_t)(iCapacity - *pCapacity));
	*ppGroups = pNew;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentGroupReserveActions(xui_code_document pDocument, xui_code_edit_group_t* pGroup, int iNeed)
{
	xui_code_edit_action_t* pNew;
	int iCapacity;

	if ( pDocument == NULL || pGroup == NULL || iNeed < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iNeed <= pGroup->iActionCapacity ) return XUI_OK;
	iCapacity = (pGroup->iActionCapacity > 0) ? pGroup->iActionCapacity : 4;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > INT_MAX / 2 ) return XUI_ERROR_OUT_OF_MEMORY;
		iCapacity *= 2;
	}
	pNew = (xui_code_edit_action_t*)xrtRealloc(pGroup->pActions, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pNew + pGroup->iActionCapacity, 0, sizeof(*pNew) * (size_t)(iCapacity - pGroup->iActionCapacity));
	pGroup->pActions = pNew;
	pGroup->iActionCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeDocumentActionInit(xui_code_document pDocument, xui_code_edit_action_t* pAction,
	int iStart, const char* sDeleted, int iDeletedLength, const char* sInserted, int iInsertedLength)
{
	if ( pDocument == NULL || pAction == NULL || iStart < 0 || iDeletedLength < 0 || iInsertedLength < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pAction, 0, sizeof(*pAction));
	if ( iDeletedLength > 0 ) {
		pAction->sDeleted = (char*)xrtMalloc((size_t)iDeletedLength + 1u);
		if ( pAction->sDeleted == NULL ) goto oom;
		if ( sDeleted != NULL ) {
			memcpy(pAction->sDeleted, sDeleted, (size_t)iDeletedLength);
		} else if ( __xuiCodeDocumentCopyRangeRaw(pDocument, iStart,
			iStart + iDeletedLength, pAction->sDeleted) != XUI_OK ) {
			__xuiCodeDocumentActionClear(pAction);
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		pAction->sDeleted[iDeletedLength] = '\0';
	}
	if ( iInsertedLength > 0 ) {
		pAction->sInserted = (char*)xrtMalloc((size_t)iInsertedLength + 1u);
		if ( pAction->sInserted == NULL ) goto oom;
		memcpy(pAction->sInserted, sInserted, (size_t)iInsertedLength);
		pAction->sInserted[iInsertedLength] = '\0';
	}
	pAction->iStart = iStart;
	pAction->iDeletedLength = iDeletedLength;
	pAction->iInsertedLength = iInsertedLength;
	return XUI_OK;

oom:
	__xuiCodeDocumentActionClear(pAction);
	__xuiCodeDocumentSetError(pDocument, "out of memory");
	return XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiCodeDocumentTryMergeAction(xui_code_edit_group_t* pGroup,
	const xui_code_edit_action_t* pAction)
{
	xui_code_edit_action_t* pPrevious;
	char* sMerged;
	int iMergedLength;

	if ( pGroup == NULL || pAction == NULL || pGroup->iActionCount != 1 ) return 0;
	pPrevious = &pGroup->pActions[0];
	if ( pPrevious->iDeletedLength == 0 && pAction->iDeletedLength == 0 &&
	     pPrevious->iInsertedLength > 0 && pAction->iInsertedLength > 0 &&
	     pPrevious->iStart + pPrevious->iInsertedLength == pAction->iStart ) {
		if ( pPrevious->iInsertedLength > INT_MAX - pAction->iInsertedLength ) return 0;
		iMergedLength = pPrevious->iInsertedLength + pAction->iInsertedLength;
		sMerged = (char*)xrtRealloc(pPrevious->sInserted, (size_t)iMergedLength + 1u);
		if ( sMerged == NULL ) return 0;
		memcpy(sMerged + pPrevious->iInsertedLength, pAction->sInserted,
			(size_t)pAction->iInsertedLength);
		sMerged[iMergedLength] = '\0';
		pPrevious->sInserted = sMerged;
		pPrevious->iInsertedLength = iMergedLength;
		return 1;
	}
	if ( pPrevious->iInsertedLength == 0 && pAction->iInsertedLength == 0 &&
	     pPrevious->iDeletedLength > 0 && pAction->iDeletedLength > 0 ) {
		if ( pPrevious->iDeletedLength > INT_MAX - pAction->iDeletedLength ) return 0;
		iMergedLength = pPrevious->iDeletedLength + pAction->iDeletedLength;
		sMerged = (char*)xrtMalloc((size_t)iMergedLength + 1u);
		if ( sMerged == NULL ) return 0;
		if ( pAction->iStart + pAction->iDeletedLength == pPrevious->iStart ) {
			memcpy(sMerged, pAction->sDeleted, (size_t)pAction->iDeletedLength);
			memcpy(sMerged + pAction->iDeletedLength, pPrevious->sDeleted,
				(size_t)pPrevious->iDeletedLength);
			pPrevious->iStart = pAction->iStart;
		} else if ( pAction->iStart == pPrevious->iStart ) {
			memcpy(sMerged, pPrevious->sDeleted, (size_t)pPrevious->iDeletedLength);
			memcpy(sMerged + pPrevious->iDeletedLength, pAction->sDeleted,
				(size_t)pAction->iDeletedLength);
		} else {
			xrtFree(sMerged);
			return 0;
		}
		sMerged[iMergedLength] = '\0';
		xrtFree(pPrevious->sDeleted);
		pPrevious->sDeleted = sMerged;
		pPrevious->iDeletedLength = iMergedLength;
		return 1;
	}
	return 0;
}

static void __xuiCodeDocumentClearRedo(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return;
	__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
	pDocument->iRedoCount = 0;
}

static int __xuiCodeDocumentTextLineBreaks(const char* sText, int iLength)
{
	int iCount;
	int i;

	if ( sText == NULL || iLength <= 0 ) return 0;
	iCount = 0;
	for ( i = 0; i < iLength; i++ ) {
		if ( sText[i] == '\n' ) iCount++;
	}
	return iCount;
}

static int __xuiCodeDocumentPrepareHistory(xui_code_document pDocument,
	const xui_code_edit_group_t* pGroup, int bUndo)
{
	const xui_code_edit_action_t* pAction;
	const char* sDeleted;
	const char* sInserted;
	int iDeletedLength;
	int iInsertedLength;
	int iLength;
	int iLineCount;
	int iMaximumLength;
	int iMaximumLineCount;
	int i;
	int iRet;

	if ( pDocument == NULL || pGroup == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = pDocument->iLength;
	iLineCount = pDocument->iLineCount;
	iMaximumLength = iLength;
	iMaximumLineCount = iLineCount;
	for ( i = 0; i < pGroup->iActionCount; i++ ) {
		int iActionIndex = bUndo ? pGroup->iActionCount - 1 - i : i;
		pAction = &pGroup->pActions[iActionIndex];
		if ( bUndo ) {
			sDeleted = pAction->sInserted;
			iDeletedLength = pAction->iInsertedLength;
			sInserted = pAction->sDeleted;
			iInsertedLength = pAction->iDeletedLength;
		} else {
			sDeleted = pAction->sDeleted;
			iDeletedLength = pAction->iDeletedLength;
			sInserted = pAction->sInserted;
			iInsertedLength = pAction->iInsertedLength;
		}
		if ( iDeletedLength < 0 || iDeletedLength > iLength ||
		     iInsertedLength < 0 || iInsertedLength > INT_MAX - (iLength - iDeletedLength) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		iLength = iLength - iDeletedLength + iInsertedLength;
		iLineCount = iLineCount -
			__xuiCodeDocumentTextLineBreaks(sDeleted, iDeletedLength) +
			__xuiCodeDocumentTextLineBreaks(sInserted, iInsertedLength);
		if ( iLineCount <= 0 ) iLineCount = 1;
		if ( iLength > iMaximumLength ) iMaximumLength = iLength;
		if ( iLineCount > iMaximumLineCount ) iMaximumLineCount = iLineCount;
	}
	iRet = __xuiCodeDocumentReserveLines(pDocument, iMaximumLineCount);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiCodeDocumentEnsureGap(pDocument, iMaximumLength - pDocument->iLength);
}

static void __xuiCodeDocumentSetLastEditRange(xui_code_document pDocument, int iStart, int iEnd)
{
	if ( pDocument == NULL ) return;
	if ( iStart < 0 ) iStart = 0;
	if ( iEnd < iStart ) iEnd = iStart;
	if ( iEnd > pDocument->iLength ) iEnd = pDocument->iLength;
	if ( !pDocument->bHasLastEditRange || pDocument->iEditDepth <= 0 ) {
		pDocument->iLastEditStart = iStart;
		pDocument->iLastEditEnd = iEnd;
		pDocument->bHasLastEditRange = 1;
		return;
	}
	if ( iStart < pDocument->iLastEditStart ) pDocument->iLastEditStart = iStart;
	if ( iEnd > pDocument->iLastEditEnd ) pDocument->iLastEditEnd = iEnd;
}

static void __xuiCodeDocumentNotifyChanged(xui_code_document pDocument)
{
	xui_code_range_t tRange;

	if ( pDocument == NULL ) return;
	pDocument->bChangePending = 1;
	if ( pDocument->iEditDepth > 0 ) return;
	if ( pDocument->onChange == NULL ) {
		pDocument->bChangePending = 0;
		return;
	}
	tRange.iStart = pDocument->bHasLastEditRange ? pDocument->iLastEditStart : 0;
	tRange.iEnd = pDocument->bHasLastEditRange ? pDocument->iLastEditEnd : pDocument->iLength;
	pDocument->bChangePending = 0;
	pDocument->onChange(pDocument, tRange, pDocument->iVersion, pDocument->pChangeUser);
}

XUI_API int xuiCodeDocumentCreate(xui_code_document* ppDocument)
{
	xui_code_document pDocument;
	int iRet;

	if ( ppDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppDocument = NULL;
	pDocument = (xui_code_document)xrtMalloc(sizeof(*pDocument));
	if ( pDocument == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pDocument, 0, sizeof(*pDocument));
    pDocument->iMagic=XUI_CODE_DOCUMENT_MAGIC;pDocument->iCreatorThread=xrtThreadCurrentId();
	pDocument->sBuffer = (char*)xrtMalloc(4096u);
	if ( pDocument->sBuffer == NULL ) {
		xrtFree(pDocument);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pDocument->iBufferCapacity = 4096;
	pDocument->iGapStart = 0;
	pDocument->iGapEnd = 4096;
	iRet = __xuiCodeDocumentRebuildLines(pDocument);
	if ( iRet != XUI_OK ) {
		xuiCodeDocumentDestroy(pDocument);
		return iRet;
	}
	*ppDocument = pDocument;
	return XUI_OK;
}

static int __xuiCodeDocumentDestroyNow(xui_code_document pDocument)
{
    pDocument->bDestroying=true;
    xuiLifetimeUnwatch(&pDocument->tObserverContextLink);
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,XUI_OK);
    xuiInternalLifetimeInvalidateAll(&pDocument->pLifetimeLinks);
    xuiInternalCleanupAdd(&receipt,XUI_OK);
	if ( pDocument->pLexerCCache != NULL &&
	     pDocument->onLexerCCacheDestroy != NULL ) {
		pDocument->onLexerCCacheDestroy(pDocument->pLexerCCache);
	}
	xrtFree(pDocument->sBuffer);
	xrtFree(pDocument->sSnapshot);
	xrtFree(pDocument->pLines);
	__xuiCodeDocumentGroupArrayClear(pDocument->pUndo, pDocument->iUndoCount);
	__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
	__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
	xrtFree(pDocument->pUndo);
	xrtFree(pDocument->pRedo);
    pDocument->iMagic=0;xrtFree(pDocument);
    xuiInternalCleanupAdd(&receipt,XUI_OK);return xuiInternalCleanupEnd(NULL,&receipt);
}
static int __xuiCodeDocumentDestroyOwnerChecked(xui_code_document document,xui_widget internalOwner,bool* consumed)
{
    if(!consumed)return XUI_ERROR_INVALID_ARGUMENT;
    *consumed=false;
    if(!document || document->iMagic!=XUI_CODE_DOCUMENT_MAGIC)return XUI_ERROR_INVALID_ARGUMENT;
    if(document->iCreatorThread!=xrtThreadCurrentId() || document->bDestroying)return XUI_ERROR_BUSY;
    if(document->bDestroyPending || document->pInternalOwner!=internalOwner)return XUI_ERROR_INVALID_STATE;
    document->bDestroyPending=true;*consumed=true;
    return document->iActiveCalls ? XUI_OK:__xuiCodeDocumentDestroyNow(document);
}
XUI_API int xuiCodeDocumentDestroyOwnerChecked(xui_code_document document,bool* consumed)
{return __xuiCodeDocumentDestroyOwnerChecked(document,NULL,consumed);}
int xuiInternalCodeDocumentClaimOwner(xui_code_document document,xui_widget owner)
{
    if(!document || document->iMagic!=XUI_CODE_DOCUMENT_MAGIC || !xuiInternalWidgetIsValid(owner))return XUI_ERROR_INVALID_ARGUMENT;
    if(document->iCreatorThread!=xrtThreadCurrentId() || document->iActiveCalls)return XUI_ERROR_BUSY;
    if(document->pInternalOwner || document->bDestroyPending)return XUI_ERROR_INVALID_STATE;
    document->pInternalOwner=owner;return XUI_OK;
}
int xuiInternalCodeDocumentDestroyOwned(xui_widget owner,xui_code_document document)
{bool consumed=false;return __xuiCodeDocumentDestroyOwnerChecked(document,document && document->pInternalOwner ? owner:NULL,&consumed);}
XUI_API int xuiCodeDocumentDestroyChecked(xui_code_document document)
{bool consumed=false;return xuiCodeDocumentDestroyOwnerChecked(document,&consumed);}
XUI_API void xuiCodeDocumentDestroy(xui_code_document document)
{if(document)(void)xuiCodeDocumentDestroyChecked(document);}


XUI_API int xuiCodeDocumentSetText(xui_code_document pDocument, const char* sText)
{
	return xuiCodeDocumentSetTextLength(pDocument,sText,-1);
}

static int __xuiCodeDocumentImplSetTextLength(xui_code_document pDocument, const char* sText, int iLength)
{
	char* sNormalized;
	int iOldLength;
	int iOldEndLine;
	int iNormalizedLength;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL ) {
		if ( iLength > 0 ) return XUI_ERROR_INVALID_ARGUMENT;
		sText = "";
		iLength = 0;
	}
	if ( iLength < 0 ) iLength = (int)strlen(sText);
	iRet = __xuiCodeDocumentValidateUtf8(pDocument, sText, iLength);
	if ( iRet != XUI_OK ) return iRet;
	sNormalized = __xuiCodeDocumentNormalizeText(sText, iLength, &iNormalizedLength);
	if ( sNormalized == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iOldLength = pDocument->iLength;
	iOldEndLine = pDocument->iLineCount - 1;
    iRet=__xuiCodeDocumentPrepareEdit(pDocument,0,pDocument->iLength,sNormalized,iNormalizedLength,true);
    if(iRet==XUI_OK)iRet=__xuiCodeDocumentSetOwnedText(pDocument,sNormalized,iNormalizedLength);
	if ( iRet == XUI_OK ) {
		pDocument->iVersion++;
		pDocument->iChangeVersion++;
		pDocument->bDirty = 0;
		pDocument->bForcedDirty = 0;
		pDocument->iState = ++pDocument->iNextState;
		pDocument->iSavedState = pDocument->iState;
		__xuiCodeDocumentSetLastEditRange(pDocument, 0, iNormalizedLength);
		__xuiCodeDocumentGroupArrayClear(pDocument->pUndo, pDocument->iUndoCount);
		__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
		__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
		pDocument->iUndoCount = 0;
		pDocument->iRedoCount = 0;
		__xuiCodeDocumentEdited(pDocument,0,iOldLength,iNormalizedLength,0,iOldEndLine,pDocument->iLineCount-1,true);
		__xuiCodeDocumentNotifyChanged(pDocument);
		sNormalized = NULL;
	}
	xrtFree(sNormalized);
	return iRet;
}

XUI_API const char* xuiCodeDocumentGetText(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return "";
	if ( __xuiCodeDocumentEnsureSnapshot(pDocument) != XUI_OK ) return "";
	return (pDocument->sSnapshot != NULL) ? pDocument->sSnapshot : "";
}

XUI_API int xuiCodeDocumentGetByte(xui_code_document pDocument, int iOffset, char* pByte)
{
	if ( pDocument == NULL || pByte == NULL ||
	     iOffset < 0 || iOffset >= pDocument->iLength ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pByte = (char)__xuiCodeDocumentByteAt(pDocument, iOffset);
	return XUI_OK;
}

XUI_API int xuiCodeDocumentCopyRange(xui_code_document pDocument,
	int iStart, int iEnd, char* sOutput, int iCapacity, int* pLength)
{
	int iLength;
	int iRet;

	if ( pDocument == NULL || iStart < 0 || iEnd < iStart ||
	     iEnd > pDocument->iLength || iCapacity < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLength = iEnd - iStart;
	if ( pLength != NULL ) *pLength = iLength;
	if ( sOutput == NULL ) return (iCapacity == 0) ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
	if ( iCapacity <= iLength ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = __xuiCodeDocumentCopyRangeRaw(pDocument, iStart, iEnd, sOutput);
	if ( iRet != XUI_OK ) return iRet;
	sOutput[iLength] = '\0';
	return XUI_OK;
}

static int __xuiCodeDocumentImplLoadTextFile(xui_code_document pDocument, const char* sPath, int iCharset)
{
	char* sText;
	size_t iSize;
	int iLength;
	int iOldLength;
	int iOldEndLine;
	int iRet;

	if ( (pDocument == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iSize = 0;
	sText = xrtFileReadText(sPath, (xencoding)iCharset, XUTF_STRICT, &iSize);
	if ( sText == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "file read failed");
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	if ( iSize > (size_t)INT_MAX ) {
		__xuiCodeDocumentSetError(pDocument, "file too large");
		xrtFree(sText);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iLength = (int)iSize;
	iRet = __xuiCodeDocumentValidateUtf8(pDocument, sText, iLength);
	if ( iRet == XUI_OK ) iRet = __xuiCodeDocumentNormalizeTextInPlace(sText, iLength, &iLength);
	if ( iRet == XUI_OK ) {
		iOldLength = pDocument->iLength;
		iOldEndLine = pDocument->iLineCount - 1;
	    iRet=__xuiCodeDocumentPrepareEdit(pDocument,0,pDocument->iLength,sText,iLength,true);
    if(iRet==XUI_OK)iRet=__xuiCodeDocumentSetOwnedText(pDocument,sText,iLength);
		if ( iRet == XUI_OK ) {
			pDocument->iVersion++;
			pDocument->iChangeVersion++;
			pDocument->bDirty = 0;
			pDocument->bForcedDirty = 0;
			pDocument->iState = ++pDocument->iNextState;
			pDocument->iSavedState = pDocument->iState;
			__xuiCodeDocumentSetLastEditRange(pDocument, 0, iLength);
			__xuiCodeDocumentGroupArrayClear(pDocument->pUndo, pDocument->iUndoCount);
			__xuiCodeDocumentGroupArrayClear(pDocument->pRedo, pDocument->iRedoCount);
			__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
			pDocument->iUndoCount = 0;
			pDocument->iRedoCount = 0;
			__xuiCodeDocumentEdited(pDocument,0,iOldLength,iLength,0,iOldEndLine,pDocument->iLineCount-1,true);
			__xuiCodeDocumentNotifyChanged(pDocument);
			sText = NULL;
		}
	}
	xrtFree(sText);
	return iRet;
}

XUI_API int xuiCodeDocumentSaveTextFile(xui_code_document pDocument, const char* sPath, int iCharset)
{
	int iRet;

	if ( (pDocument == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiCodeDocumentEnsureSnapshot(pDocument);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xrtFileWriteText(sPath,
		xuiXrtText(pDocument->sSnapshot, (size_t)pDocument->iLength),
		(xencoding)iCharset, XUTF_STRICT, false);
	if ( iRet == 0 ) {
		__xuiCodeDocumentSetError(pDocument, "file write failed");
		return XUI_ERROR_RESOURCE_FAILED;
	}
	pDocument->bForcedDirty = 0;
	pDocument->iSavedState = pDocument->iState;
	__xuiCodeDocumentUpdateDirty(pDocument);
	return XUI_OK;
}

XUI_API int xuiCodeDocumentGetLength(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return 0;
	return pDocument->iLength;
}

XUI_API int xuiCodeDocumentGetLineCount(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return 0;
	return pDocument->iLineCount;
}

void* xuiInternalCodeDocumentGetLexerCCache(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->pLexerCCache : NULL;
}

int xuiInternalCodeDocumentSetLexerCCache(xui_code_document pDocument,
	void* pCache, void (*onDestroy)(void* pCache))
{
	if ( pDocument == NULL || (pCache != NULL && onDestroy == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pDocument->pLexerCCache != pCache &&
	     pDocument->pLexerCCache != NULL &&
	     pDocument->onLexerCCacheDestroy != NULL ) {
		pDocument->onLexerCCacheDestroy(pDocument->pLexerCCache);
	}
	pDocument->pLexerCCache = pCache;
	pDocument->onLexerCCacheDestroy = onDestroy;
	return XUI_OK;
}

static void __xuiCodeDocumentObserverContextGone(void* raw)
{
    xui_code_document document=raw;
    document->pObserverContext=NULL;document->onPrepareEdit=NULL;document->onEdit=NULL;document->pEditUser=NULL;
}
int xuiInternalCodeDocumentSetEditObserverChecked(xui_code_document document,xui_context context,
    xui_internal_code_document_prepare_proc prepare,xui_internal_code_document_edit_proc edited,void* user)
{
    if(!document || document->iMagic!=XUI_CODE_DOCUMENT_MAGIC)return XUI_ERROR_INVALID_ARGUMENT;
    if(document->iCreatorThread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    if(document->bWriting && (prepare || edited))return XUI_ERROR_BUSY;
    if(document->bDestroyPending && (prepare || edited))return XUI_ERROR_INVALID_STATE;
    xui_call_scope_t scope={0};
    if(context){int status=xuiCallScopeBegin(context,&scope);if(status!=XUI_OK)return status;}
    xuiLifetimeUnwatch(&document->tObserverContextLink);
    int status=context ? xuiContextLifetimeWatch(context,&document->tObserverContextLink,__xuiCodeDocumentObserverContextGone,document):XUI_OK;
    if(status==XUI_OK){document->onPrepareEdit=prepare;document->onEdit=edited;document->pEditUser=user;document->pObserverContext=context;}
    else {document->onPrepareEdit=NULL;document->onEdit=NULL;document->pEditUser=NULL;document->pObserverContext=NULL;}
    xerror* first=xrtTakeError();status=xuiInternalCallScopeEndOutcome(&scope,status,&first);xrtSetErrorTake(first);return status;
}
int xuiInternalCodeDocumentSetEditObserver(xui_code_document pDocument,
	xui_internal_code_document_edit_proc onEdit, void* pUser)
{
    return xuiInternalCodeDocumentSetEditObserverChecked(pDocument,NULL,NULL,onEdit,onEdit ? pUser:NULL);
}

int xuiInternalCodeDocumentClampOffset(xui_code_document pDocument,
	int iOffset, int bForward, int* pClamped)
{
	int iValue;

	if ( pDocument == NULL || pClamped == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iOffset < 0 ) iOffset = 0;
	if ( iOffset > pDocument->iLength ) iOffset = pDocument->iLength;
	iValue = __xuiCodeDocumentClampOffset(pDocument, iOffset);
	if ( bForward && iValue < iOffset ) {
		iValue = __xuiCodeDocumentUtf8Next(pDocument, iValue);
	}
	if ( iValue < 0 || iValue > pDocument->iLength ) {
		return XUI_ERROR_INVALID_STATE;
	}
	*pClamped = iValue;
	return XUI_OK;
}

XUI_API int xuiCodeDocumentGetLineRange(xui_code_document pDocument, int iLine, int* pStart, int* pEnd)
{
	if ( (pDocument == NULL) || (iLine < 0) || (iLine >= pDocument->iLineCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pStart != NULL ) *pStart = __xuiCodeDocumentLineField(pDocument, iLine, 0);
	if ( pEnd != NULL ) *pEnd = __xuiCodeDocumentLineField(pDocument, iLine, 2);
	return XUI_OK;
}

XUI_API int xuiCodeDocumentOffsetToLineColumn(xui_code_document pDocument, int iOffset, int* pLine, int* pColumn)
{
	int iLow;
	int iHigh;
	int iMid;
	int iLine;
	int iPos;
	int iColumn;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iOffset = __xuiCodeDocumentClampOffset(pDocument, iOffset);
	iLow = 0;
	iHigh = pDocument->iLineCount - 1;
	iLine = 0;
	while ( iLow <= iHigh ) {
		iMid = (iLow + iHigh) / 2;
		if ( iOffset < __xuiCodeDocumentLineField(pDocument, iMid, 0) ) {
			iHigh = iMid - 1;
		} else if ( (iOffset >= __xuiCodeDocumentLineField(pDocument, iMid, 1)) &&
		            (iMid + 1 < pDocument->iLineCount) ) {
			iLow = iMid + 1;
		} else {
			iLine = iMid;
			break;
		}
	}
	if ( iLow > iHigh ) iLine = (iLow < pDocument->iLineCount) ? iLow : (pDocument->iLineCount - 1);
	if ( pLine != NULL ) *pLine = iLine;
	if ( pColumn != NULL ) {
		iColumn = 0;
		iPos = __xuiCodeDocumentLineField(pDocument, iLine, 0);
		while ( iPos < iOffset &&
		        iPos < __xuiCodeDocumentLineField(pDocument, iLine, 2) ) {
			iPos = __xuiCodeDocumentUtf8Next(pDocument, iPos);
			iColumn++;
		}
		*pColumn = iColumn;
	}
	return XUI_OK;
}

XUI_API int xuiCodeDocumentLineColumnToOffset(xui_code_document pDocument, int iLine, int iColumn, int* pOffset)
{
	int iOffset;
	int i;

	if ( (pDocument == NULL) || (iLine < 0) || (iLine >= pDocument->iLineCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iColumn < 0 ) iColumn = 0;
	iOffset = __xuiCodeDocumentLineField(pDocument, iLine, 0);
	for ( i = 0; i < iColumn &&
	     iOffset < __xuiCodeDocumentLineField(pDocument, iLine, 2); i++ ) {
		iOffset = __xuiCodeDocumentUtf8Next(pDocument, iOffset);
	}
	iOffset = __xuiCodeDocumentClampOffset(pDocument, iOffset);
	if ( pOffset != NULL ) *pOffset = iOffset;
	return XUI_OK;
}

static int __xuiCodeDocumentImplReplace(xui_code_document pDocument, int iStart, int iEnd, const char* sText)
{
	xui_code_edit_action_t tAction;
	xui_code_edit_group_t* pGroup=NULL;
	char* sNormalized;
	int iInsertLength;
	int iDeletedLength;
	int bStandalone;
	int bMerged;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeDocumentValidateUtf8(pDocument, sText, -1);
	if ( iRet != XUI_OK ) return iRet;
	iStart = __xuiCodeDocumentClampOffset(pDocument, iStart);
	iEnd = __xuiCodeDocumentClampOffset(pDocument, iEnd);
	if ( iEnd < iStart ) {
		int iTemp = iStart;
		iStart = iEnd;
		iEnd = iTemp;
	}
	sNormalized = __xuiCodeDocumentNormalizeText(sText, -1, &iInsertLength);
	if ( sNormalized == NULL ) {
		__xuiCodeDocumentSetError(pDocument, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	iDeletedLength = iEnd - iStart;
	if ( iDeletedLength == 0 && iInsertLength == 0 ) {
		xrtFree(sNormalized);
		return XUI_OK;
	}
	if ( __xuiCodeDocumentRangeEquals(pDocument, iStart, iEnd,
	     sNormalized, iInsertLength) ) {
		xrtFree(sNormalized);
		return XUI_OK;
	}
	memset(&tAction, 0, sizeof(tAction));
	iRet = __xuiCodeDocumentActionInit(pDocument, &tAction, iStart,
		NULL, iDeletedLength, sNormalized, iInsertLength);
	if ( iRet != XUI_OK ) {
		xrtFree(sNormalized);
		return iRet;
	}
	bStandalone = (pDocument->iEditDepth == 0);
	bMerged = 0;
	if ( bStandalone ) {
		iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pUndo,
			&pDocument->iUndoCapacity, pDocument->iUndoCount + 1);
		if ( iRet != XUI_OK ) goto cleanup;
		pGroup = &pDocument->pUndo[pDocument->iUndoCount];
		__xuiCodeDocumentGroupClear(pGroup);
		pGroup->iBeforeState = pDocument->iState;
	} else {
		pGroup = &pDocument->tEditGroup;
	}
	iRet = __xuiCodeDocumentGroupReserveActions(pDocument, pGroup, pGroup->iActionCount + 1);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiCodeDocumentReplaceRaw(pDocument, iStart, iEnd, sNormalized, iInsertLength);
	if ( iRet != XUI_OK ) goto cleanup;
	if ( bStandalone && pDocument->iRedoCount == 0 && pDocument->iUndoCount > 0 ) {
		xui_code_edit_group_t* pPrevious = &pDocument->pUndo[pDocument->iUndoCount - 1];
		if ( pPrevious->iAfterState != pDocument->iSavedState &&
		     __xuiCodeDocumentTryMergeAction(pPrevious, &tAction) ) {
			pGroup = pPrevious;
			bMerged = 1;
		}
	}
	if ( !bMerged ) {
		pGroup->pActions[pGroup->iActionCount++] = tAction;
		memset(&tAction, 0, sizeof(tAction));
	}
	__xuiCodeDocumentClearRedo(pDocument);
	pDocument->iVersion++;
	pDocument->iChangeVersion++;
	pDocument->iState = ++pDocument->iNextState;
	pGroup->iAfterState = pDocument->iState;
	__xuiCodeDocumentUpdateDirty(pDocument);
	if ( bStandalone && !bMerged ) pDocument->iUndoCount++;
	__xuiCodeDocumentSetLastEditRange(pDocument, iStart, iStart + iInsertLength);
	__xuiCodeDocumentNotifyChanged(pDocument);

cleanup:
    /* A standalone history slot is not counted until commit. Pre-edit may
     * reject or close its owner after the slot's action capacity was reserved;
     * it must not escape the normal counted-group destructor walk. */
    if(iRet!=XUI_OK && bStandalone && pGroup)__xuiCodeDocumentGroupClear(pGroup);
	__xuiCodeDocumentActionClear(&tAction);
	xrtFree(sNormalized);
	return iRet;
}

XUI_API int xuiCodeDocumentInsert(xui_code_document pDocument, int iOffset, const char* sText)
{
	return xuiCodeDocumentReplace(pDocument,iOffset,iOffset,sText);
}

XUI_API int xuiCodeDocumentDelete(xui_code_document pDocument, int iStart, int iEnd)
{
	return xuiCodeDocumentReplace(pDocument,iStart,iEnd,"");
}

static int __xuiCodeDocumentImplBeginEdit(xui_code_document pDocument)
{
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	if ( pDocument->iEditDepth == 0 ) {
		iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pUndo,
			&pDocument->iUndoCapacity, pDocument->iUndoCount + 1);
		if ( iRet != XUI_OK ) return iRet;
		__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
		pDocument->tEditGroup.iBeforeState = pDocument->iState;
		pDocument->bHasLastEditRange = 0;
	}
	pDocument->iEditDepth++;
	return XUI_OK;
}

static int __xuiCodeDocumentImplEndEdit(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iEditDepth <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->iEditDepth--;
	if ( pDocument->iEditDepth == 0 ) {
		if ( pDocument->tEditGroup.iActionCount > 0 ) {
			pDocument->pUndo[pDocument->iUndoCount++] = pDocument->tEditGroup;
			memset(&pDocument->tEditGroup, 0, sizeof(pDocument->tEditGroup));
		} else {
			__xuiCodeDocumentGroupClear(&pDocument->tEditGroup);
		}
		if ( pDocument->bChangePending ) __xuiCodeDocumentNotifyChanged(pDocument);
	}
	return XUI_OK;
}

XUI_API int xuiCodeDocumentSetChangeCallback(xui_code_document pDocument, xui_code_document_change_proc onChange, void* pUser)
{
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->onChange = onChange;
	pDocument->pChangeUser = pUser;
	return XUI_OK;
}

static int __xuiCodeDocumentImplUndo(xui_code_document pDocument)
{
	xui_code_edit_group_t* pGroup;
	xui_code_edit_action_t* pAction;
	int i;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iEditDepth > 0 || pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	if ( pDocument->iUndoCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pRedo,
		&pDocument->iRedoCapacity, pDocument->iRedoCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pGroup = &pDocument->pUndo[pDocument->iUndoCount - 1];
	iRet = __xuiCodeDocumentPrepareHistory(pDocument, pGroup, 1);
	if ( iRet != XUI_OK ) return iRet;
    iRet=__xuiCodeDocumentPrepareEdit(pDocument,0,pDocument->iLength,NULL,0,true);if(iRet!=XUI_OK)return iRet;
	pDocument->bApplyingHistory = 1;
	iRet = XUI_OK;
	for ( i = pGroup->iActionCount - 1; i >= 0; i-- ) {
		pAction = &pGroup->pActions[i];
		iRet = __xuiCodeDocumentReplaceRaw(pDocument,
			pAction->iStart,
			pAction->iStart + pAction->iInsertedLength,
			(pAction->sDeleted != NULL) ? pAction->sDeleted : "",
			pAction->iDeletedLength);
		if ( iRet != XUI_OK ) break;
	}
	pDocument->bApplyingHistory = 0;
	if ( iRet == XUI_OK ) {
		pDocument->iVersion++;
		pDocument->iChangeVersion++;
		pDocument->iState = pGroup->iBeforeState;
		__xuiCodeDocumentUpdateDirty(pDocument);
		__xuiCodeDocumentSetLastEditRange(pDocument, 0, pDocument->iLength);
		pDocument->pRedo[pDocument->iRedoCount++] = *pGroup;
		memset(pGroup, 0, sizeof(*pGroup));
		pDocument->iUndoCount--;
		__xuiCodeDocumentNotifyChanged(pDocument);
	}
	return iRet;
}

static int __xuiCodeDocumentImplRedo(xui_code_document pDocument)
{
	xui_code_edit_group_t* pGroup;
	xui_code_edit_action_t* pAction;
	int i;
	int iRet;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDocument->iEditDepth > 0 || pDocument->bApplyingHistory ) return XUI_ERROR_UNSUPPORTED;
	if ( pDocument->iRedoCount <= 0 ) return XUI_ERROR_UNSUPPORTED;
	iRet = __xuiCodeDocumentGroupArrayReserve(pDocument, &pDocument->pUndo,
		&pDocument->iUndoCapacity, pDocument->iUndoCount + 1);
	if ( iRet != XUI_OK ) return iRet;
	pGroup = &pDocument->pRedo[pDocument->iRedoCount - 1];
	iRet = __xuiCodeDocumentPrepareHistory(pDocument, pGroup, 0);
	if ( iRet != XUI_OK ) return iRet;
    iRet=__xuiCodeDocumentPrepareEdit(pDocument,0,pDocument->iLength,NULL,0,true);if(iRet!=XUI_OK)return iRet;
	pDocument->bApplyingHistory = 1;
	iRet = XUI_OK;
	for ( i = 0; i < pGroup->iActionCount; i++ ) {
		pAction = &pGroup->pActions[i];
		iRet = __xuiCodeDocumentReplaceRaw(pDocument,
			pAction->iStart,
			pAction->iStart + pAction->iDeletedLength,
			(pAction->sInserted != NULL) ? pAction->sInserted : "",
			pAction->iInsertedLength);
		if ( iRet != XUI_OK ) break;
	}
	pDocument->bApplyingHistory = 0;
	if ( iRet == XUI_OK ) {
		pDocument->iVersion++;
		pDocument->iChangeVersion++;
		pDocument->iState = pGroup->iAfterState;
		__xuiCodeDocumentUpdateDirty(pDocument);
		__xuiCodeDocumentSetLastEditRange(pDocument, 0, pDocument->iLength);
		pDocument->pUndo[pDocument->iUndoCount++] = *pGroup;
		memset(pGroup, 0, sizeof(*pGroup));
		pDocument->iRedoCount--;
		__xuiCodeDocumentNotifyChanged(pDocument);
	}
	return iRet;
}

XUI_API int xuiCodeDocumentCanUndo(xui_code_document pDocument)
{
	return (pDocument != NULL) && (pDocument->iUndoCount > 0);
}

XUI_API int xuiCodeDocumentCanRedo(xui_code_document pDocument)
{
	return (pDocument != NULL) && (pDocument->iRedoCount > 0);
}

XUI_API uint32_t xuiCodeDocumentGetVersion(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->iVersion : 0u;
}

XUI_API uint32_t xuiCodeDocumentGetChangeVersion(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->iChangeVersion : 0u;
}

XUI_API int xuiCodeDocumentGetLastEditRange(xui_code_document pDocument, xui_code_range_t* pRange)
{
	if ( (pDocument == NULL) || (pRange == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !pDocument->bHasLastEditRange ) return XUI_ERROR_UNSUPPORTED;
	pRange->iStart = pDocument->iLastEditStart;
	pRange->iEnd = pDocument->iLastEditEnd;
	return XUI_OK;
}

XUI_API int xuiCodeDocumentGetDirty(xui_code_document pDocument)
{
	return (pDocument != NULL) ? pDocument->bDirty : 0;
}

XUI_API int xuiCodeDocumentSetDirty(xui_code_document pDocument, int bDirty)
{
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pDocument->bForcedDirty = bDirty ? 1 : 0;
	if ( !pDocument->bForcedDirty ) {
		pDocument->iSavedState = pDocument->iState;
	}
	__xuiCodeDocumentUpdateDirty(pDocument);
	return XUI_OK;
}

XUI_API const char* xuiCodeDocumentGetLastError(xui_code_document pDocument)
{
	if ( pDocument == NULL ) return "invalid document";
	return pDocument->sError;
}

XUI_API int xuiCodeDocumentSetTextLength(xui_code_document pDocument, const char* sText, int iLength)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplSetTextLength(pDocument,sText,iLength);
    return __xuiCodeDocumentCallEnd(&call,status);
}

XUI_API int xuiCodeDocumentLoadTextFile(xui_code_document pDocument, const char* sPath, int iCharset)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplLoadTextFile(pDocument,sPath,iCharset);
    return __xuiCodeDocumentCallEnd(&call,status);
}

XUI_API int xuiCodeDocumentReplace(xui_code_document pDocument, int iStart, int iEnd, const char* sText)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplReplace(pDocument,iStart,iEnd,sText);
    return __xuiCodeDocumentCallEnd(&call,status);
}

XUI_API int xuiCodeDocumentBeginEdit(xui_code_document pDocument)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplBeginEdit(pDocument);
    return __xuiCodeDocumentCallEnd(&call,status);
}

XUI_API int xuiCodeDocumentEndEdit(xui_code_document pDocument)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplEndEdit(pDocument);
    return __xuiCodeDocumentCallEnd(&call,status);
}

XUI_API int xuiCodeDocumentUndo(xui_code_document pDocument)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplUndo(pDocument);
    return __xuiCodeDocumentCallEnd(&call,status);
}

XUI_API int xuiCodeDocumentRedo(xui_code_document pDocument)
{
    xui_code_document_call_t call={0};int status=__xuiCodeDocumentCallBegin(pDocument,&call);
    if(status==XUI_OK)status=__xuiCodeDocumentImplRedo(pDocument);
    return __xuiCodeDocumentCallEnd(&call,status);
}

/* Additive XUI document primitive. Included in the Document implementation,
 * after its existing helpers; the IDE never inspects private document storage. */
typedef struct xui_document_batch_row_t {
    xui_code_document document;
    xui_lifetime_link_t lifetime;
    xui_code_document_call_t call;
    uint32_t version,changeVersion;
    uint64_t state,nextState;
    int beforeLength,beforeLines,afterLength,afterLines,undoCount,changed;
    char *buffer,*snapshot;
    xui_code_line_t* lines;
    xui_code_edit_group_t *undo,group;
} xui_document_batch_row_t;
struct xui_code_document_batch_t {
    uint64_t thread;
    size_t count,used,limit;
    bool busy,attempted;
    xui_document_batch_row_t* rows;
};
static void __xuiDocumentBatchGone(void* user)
{ ((xui_document_batch_row_t*)user)->document=NULL; }
static int __xuiDocumentBatchAllocate(xui_code_document_batch batch,size_t bytes,void** out)
{
    *out=NULL;
    if(bytes>batch->limit-batch->used)return XUI_ERROR_OUT_OF_MEMORY;
    void* memory=xrtMalloc(bytes);
    if(!memory)return XUI_ERROR_OUT_OF_MEMORY;
    memset(memory,0,bytes);batch->used+=bytes;*out=memory;return XUI_OK;
}
static void __xuiDocumentBatchFree(xui_code_document_batch batch)
{
    if(!batch)return;
    for(size_t i=0;i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        xuiLifetimeUnwatch(&row->lifetime);
        xrtFree(row->buffer);xrtFree(row->snapshot);xrtFree(row->lines);
        xrtFree(row->undo);__xuiCodeDocumentGroupClear(&row->group);
    }
    xrtFree(batch->rows);xrtFree(batch);
}
static int __xuiDocumentBatchFinishCalls(xui_code_document_batch batch,int status)
{
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,status);
    for(size_t i=batch->count;i;--i)
        xuiInternalCleanupAdd(&receipt,__xuiCodeDocumentCallEnd(&batch->rows[i-1].call,XUI_OK));
    return xuiInternalCleanupEnd(NULL,&receipt);
}
static bool __xuiDocumentBatchLive(const xui_document_batch_row_t* row)
{
    xui_code_document d=row->document;
    return d && !d->bDestroyPending && !d->bDestroying &&
        (!d->pInternalOwner || xuiInternalWidgetIsValid(d->pInternalOwner)) &&
        (!row->call.context || (!row->call.context->bDestroyPending &&
            !row->call.context->bDestroying && !row->call.context->bCloseRequested));
}
static bool __xuiDocumentBatchCurrent(const xui_document_batch_row_t* row)
{
    xui_code_document d=row->document;
    return __xuiDocumentBatchLive(row) && d->iVersion==row->version &&
        d->iChangeVersion==row->changeVersion && d->iState==row->state &&
        d->iNextState==row->nextState && d->iLength==row->beforeLength &&
        d->iUndoCount==row->undoCount && !d->iEditDepth && !d->bApplyingHistory;
}
XUI_API int xuiCodeDocumentBatchPrepare(const xui_code_document_batch_item_t* items,
    size_t count,size_t budget,xui_code_document_batch* output)
{
    if(!output)return XUI_ERROR_INVALID_ARGUMENT;
    *output=NULL;
    if((count && !items) || count>512 || budget>256u*1024u*1024u)return XUI_ERROR_INVALID_ARGUMENT;
    size_t limit=budget?budget:128u*1024u*1024u;
    if(sizeof(struct xui_code_document_batch_t)>limit)return XUI_ERROR_OUT_OF_MEMORY;
    xui_code_document_batch batch=xrtMalloc(sizeof(*batch));
    if(!batch)return XUI_ERROR_OUT_OF_MEMORY;
    memset(batch,0,sizeof(*batch));batch->thread=xrtThreadCurrentId();batch->limit=limit;
    batch->used=sizeof(*batch);batch->busy=true;
    int status=__xuiDocumentBatchAllocate(batch,(count?count:1)*sizeof(*batch->rows),(void**)&batch->rows);
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    batch->count=count;
    for(size_t i=0;status==XUI_OK && i<count;++i){
        xui_document_batch_row_t* row=batch->rows+i;row->document=items[i].pDocument;
        for(size_t j=0;j<i;++j)if(row->document==batch->rows[j].document)status=XUI_ERROR_INVALID_ARGUMENT;
        if(status==XUI_OK)status=__xuiCodeDocumentCallBegin(row->document,&row->call);
    }
    size_t beforeBytes=0,afterBytes=0;
    for(size_t i=0;status==XUI_OK && i<count;++i){
        xui_document_batch_row_t* row=batch->rows+i;xui_code_document d=row->document;
        const xui_code_document_batch_item_t* item=items+i;
        if(item->Before.Size>8u*1024u*1024u || item->After.Size>8u*1024u*1024u ||
            item->Before.Size>64u*1024u*1024u-beforeBytes || item->After.Size>64u*1024u*1024u-afterBytes ||
            (item->Before.Size && !item->Before.Data) || (item->After.Size && !item->After.Data)){
            status=XUI_ERROR_INVALID_ARGUMENT;break;
        }
        beforeBytes+=item->Before.Size;afterBytes+=item->After.Size;
        const char *before=item->Before.Data?item->Before.Data:"",*after=item->After.Data?item->After.Data:"";
        if(d->iEditDepth || d->bApplyingHistory || d->iNextState==UINT64_MAX ||
            d->iUndoCount<0 || d->iUndoCount==INT_MAX || d->iChangeVersion!=item->iChangeVersion ||
            d->iLength!=(int)item->Before.Size ||
            !__xuiCodeDocumentRangeEquals(d,0,d->iLength,before,(int)item->Before.Size)){
            status=XUI_ERROR_INVALID_STATE;break;
        }
        if(memchr(before,0,item->Before.Size) || memchr(after,0,item->After.Size) ||
            memchr(before,'\r',item->Before.Size) || memchr(after,'\r',item->After.Size)){
            status=XUI_ERROR_INVALID_ARGUMENT;break;
        }
        status=__xuiCodeDocumentValidateUtf8(d,after,(int)item->After.Size);
        if(status!=XUI_OK)break;
        row->version=d->iVersion;row->changeVersion=d->iChangeVersion;row->state=d->iState;row->nextState=d->iNextState;
        row->beforeLength=d->iLength;row->beforeLines=d->iLineCount;row->afterLength=(int)item->After.Size;row->undoCount=d->iUndoCount;
        row->changed=item->Before.Size!=item->After.Size || memcmp(before,after,item->Before.Size);
        status=xuiCodeDocumentLifetimeWatch(d,&row->lifetime,__xuiDocumentBatchGone,row);
        if(status!=XUI_OK || !row->changed)continue;
        row->afterLines=__xuiCodeDocumentTextLineBreaks(after,row->afterLength)+1;
        status=__xuiDocumentBatchAllocate(batch,item->After.Size+4096,(void**)&row->buffer);
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,item->After.Size+1,(void**)&row->snapshot);
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,(size_t)row->afterLines*sizeof(*row->lines),(void**)&row->lines);
        if(status==XUI_OK && (size_t)row->undoCount+1>SIZE_MAX/sizeof(*row->undo))status=XUI_ERROR_OUT_OF_MEMORY;
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,((size_t)row->undoCount+1)*sizeof(*row->undo),(void**)&row->undo);
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,sizeof(*row->group.pActions),(void**)&row->group.pActions);
        size_t strings=(item->Before.Size?item->Before.Size+1:0)+(item->After.Size?item->After.Size+1:0);
        if(status==XUI_OK && strings>batch->limit-batch->used)status=XUI_ERROR_OUT_OF_MEMORY;
        if(status!=XUI_OK)break;
        batch->used+=strings;row->group.iActionCount=row->group.iActionCapacity=1;
        status=__xuiCodeDocumentActionInit(d,row->group.pActions,0,before,row->beforeLength,after,row->afterLength);
        if(status!=XUI_OK)break;
        memcpy(row->buffer,after,item->After.Size);memcpy(row->snapshot,after,item->After.Size);
        int line=0,start=0;
        for(int j=0;j<row->afterLength;++j)if(after[j]=='\n'){
            row->lines[line++]=(xui_code_line_t){start,j+1,j};start=j+1;
        }
        row->lines[line]=(xui_code_line_t){start,row->afterLength,row->afterLength};
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;
    for(size_t i=0;status==XUI_OK && i<count;++i)if(!batch->rows[i].document)status=XUI_ERROR_INVALID_STATE;
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    *output=batch;return XUI_OK;
}
XUI_API int xuiCodeDocumentBatchCommit(xui_code_document_batch batch,bool* committed)
{
    if(!committed)return XUI_ERROR_INVALID_ARGUMENT;
    *committed=false;
    if(!batch)return XUI_ERROR_INVALID_ARGUMENT;
    if(batch->busy || batch->thread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    if(batch->attempted)return XUI_ERROR_INVALID_STATE;
    batch->busy=true;batch->attempted=true;int status=XUI_OK;
    for(size_t i=0;status==XUI_OK && i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        status=__xuiCodeDocumentCallBegin(row->document,&row->call);
        if(status==XUI_OK && !__xuiDocumentBatchCurrent(row))status=XUI_ERROR_INVALID_STATE;
    }
    for(size_t i=0;status==XUI_OK && i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        if(row->changed)status=__xuiCodeDocumentPrepareEdit(row->document,0,row->beforeLength,
            row->group.pActions->sInserted?row->group.pActions->sInserted:"",row->afterLength,true);
    }
    /* Every callback has finished before the final group fence. No allocation,
     * fallible operation or user callback occurs in the publication loop. */
    for(size_t i=0;status==XUI_OK && i<batch->count;++i)
        if(!__xuiDocumentBatchCurrent(batch->rows+i))status=XUI_ERROR_INVALID_STATE;
    if(status==XUI_OK){
        for(size_t i=0;i<batch->count;++i){
            xui_document_batch_row_t* row=batch->rows+i;xui_code_document d=row->document;
            if(!row->changed)continue;
            if(row->undoCount)memcpy(row->undo,d->pUndo,(size_t)row->undoCount*sizeof(*row->undo));
            row->group.iBeforeState=d->iState;row->group.iAfterState=d->iNextState+1;
            row->undo[row->undoCount]=row->group;memset(&row->group,0,sizeof(row->group));
            xrtFree(d->pUndo);d->pUndo=row->undo;row->undo=NULL;d->iUndoCount=row->undoCount+1;d->iUndoCapacity=d->iUndoCount;
            __xuiCodeDocumentClearRedo(d);
            xrtFree(d->sBuffer);xrtFree(d->sSnapshot);xrtFree(d->pLines);
            d->sBuffer=row->buffer;row->buffer=NULL;d->iLength=row->afterLength;d->iBufferCapacity=row->afterLength+4096;
            d->iGapStart=row->afterLength;d->iGapEnd=d->iBufferCapacity;
            d->sSnapshot=row->snapshot;row->snapshot=NULL;d->iSnapshotCapacity=row->afterLength+1;d->bSnapshotValid=1;
            d->pLines=row->lines;row->lines=NULL;d->iLineCount=d->iLineCapacity=row->afterLines;
            d->iPendingLineShiftStart=d->iPendingLineShiftDelta=0;
            ++d->iVersion;++d->iChangeVersion;d->iState=++d->iNextState;
            __xuiCodeDocumentUpdateDirty(d);__xuiCodeDocumentSetLastEditRange(d,0,d->iLength);
        }
        *committed=true;
        for(size_t i=0;i<batch->count;++i){
            xui_document_batch_row_t* row=batch->rows+i;if(!row->changed || !__xuiDocumentBatchLive(row))continue;
            __xuiCodeDocumentEdited(row->document,0,row->beforeLength,row->afterLength,0,row->beforeLines-1,row->afterLines-1,false);
            if(__xuiDocumentBatchLive(row))__xuiCodeDocumentNotifyChanged(row->document);
        }
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;return status;
}
XUI_API int xuiCodeDocumentBatchDestroy(xui_code_document_batch batch)
{
    if(!batch)return XUI_OK;
    if(batch->busy || batch->thread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    __xuiDocumentBatchFree(batch);return XUI_OK;
}

#endif
