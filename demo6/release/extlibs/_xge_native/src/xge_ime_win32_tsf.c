/*
 * Windows text input backend.
 *
 * TSF is the authority for keystroke ownership in COMPOSITION/FULL modes.
 * XUI owns the editable document. The TSF text store keeps a UTF-16 shadow
 * snapshot plus the active composition range so native text services can
 * query surrounding text without mutating widgets behind XUI's event route.
 */
#include <imm.h>
#include <msctf.h>
#include <oleauto.h>
#include <textstor.h>
#include "xge_ime_win32_compat.h"

#define XGE_TSF_VIEW_COOKIE 1u
#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION ((HRESULT)0x80040200L)
#endif
#ifndef CONNECT_E_ADVISELIMIT
#define CONNECT_E_ADVISELIMIT ((HRESULT)0x80040201L)
#endif

typedef struct xge_tsf_text_store_t {
	ITextStoreACP tStore;
	ITfContextOwnerCompositionSink tCompositionSink;
	LONG iRefCount;
	ITextStoreACPSink* pSink;
	DWORD iSinkMask;
	DWORD iLockType;
	DWORD iPendingLock;
	WCHAR* sText;
	LONG iTextLength;
	LONG iTextCapacity;
	LONG iSelectionStart;
	LONG iSelectionEnd;
	int iDocumentOffset;
	LONG iCompositionStart;
	LONG iCompositionEnd;
	WCHAR* sCompositionOriginal;
	LONG iCompositionOriginalLength;
	LONG iCompositionOriginalCapacity;
	LONG iLastChangeStart;
	LONG iLastChangeEnd;
	LONG iPendingCompositionStart;
	LONG iPendingCompositionEnd;
	int iReplacementStart;
	int iReplacementEnd;
	int bHasCompositionRange;
	int bHasReplacementRange;
	int bPendingCompositionEdit;
	int bComposing;
	int bTextChanged;
	int bEndPending;
} xge_tsf_text_store_t;

typedef struct xge_tsf_ui_element_sink_t {
	ITfUIElementSink tSink;
	LONG iRefCount;
} xge_tsf_ui_element_sink_t;

typedef struct xge_win32_ime_state_t {
	HWND hWnd;
	WNDPROC pOriginalWindowProc;
	int bEnabled;
	int iMode;
	int bComInitialized;
	int bTsfInitialized;
	int bHasCandidateRect;
	xge_rect_t tCandidateRect;
	RECT tCandidateClientRect;
	RECT tCandidateScreenRect;
	int bImmComposing;
	int bImmFallbackActive;
	int bImmFallbackCommitted;
	int bImmFallbackEnded;
	int bImmReplacementRange;
	int iImmReplacementStart;
	int iImmReplacementEnd;
	ITfThreadMgrEx* pThreadMgr;
	ITfDocumentMgr* pDocumentMgr;
	ITfContext* pContext;
	ITfKeystrokeMgr* pKeystrokeMgr;
	ITfUIElementMgr* pUiElementMgr;
	ITfSource* pUiElementSource;
	DWORD iUiElementSinkCookie;
	xge_tsf_ui_element_sink_t tUiElementSink;
	DWORD iCandidateElementId;
	ITfCandidateListUIElementBehavior* pCandidateBehavior;
	char** pCandidateText;
	int iCandidateCapacity;
	int iCandidateCount;
	int iCandidateSelection;
	int iCandidatePageStart;
	int iCandidatePageSize;
	int bCandidateVisible;
	int bCandidateCanSelect;
	int bCandidatePresenterReady;
	TfClientId iClientId;
	TfEditCookie iEditCookie;
	xge_ime_text_client_t tTextClient;
	int bHasTextClient;
	xge_tsf_text_store_t tTextStore;
} xge_win32_ime_state_t;

static xge_win32_ime_state_t g_xgeWin32Ime;

static void __xgeImeWin32ApplyImmCandidateRect(void);
static int __xgeTsfCompositionNeedsImmFallback(const xge_tsf_text_store_t* pStore);
static int __xgeTsfWideToUtf8(const WCHAR* sWide, int iWideLength,
	char** psText, int* pTextLength);

static int __xgeImeFillQueryCharPosition(IMECHARPOSITION* pPosition,
	const RECT* pCaretScreenRect, const RECT* pDocumentScreenRect)
{
	LONG iLineHeight;

	if ( pPosition == NULL || pCaretScreenRect == NULL ||
	     pDocumentScreenRect == NULL ||
	     pPosition->dwSize < sizeof(*pPosition) ) return 0;
	iLineHeight = pCaretScreenRect->bottom - pCaretScreenRect->top;
	if ( iLineHeight < 1 ) iLineHeight = 1;
	pPosition->pt.x = pCaretScreenRect->left;
	pPosition->pt.y = pCaretScreenRect->top;
	pPosition->cLineHeight = (UINT)iLineHeight;
	pPosition->rcDocument = *pDocumentScreenRect;
	return 1;
}

static void __xgeImeFillCandidateForm(const RECT* pCaretRect,
	CANDIDATEFORM* pCandidate)
{
	if ( pCaretRect == NULL || pCandidate == NULL ) return;
	memset(pCandidate, 0, sizeof(*pCandidate));
	pCandidate->dwIndex = 0;
	pCandidate->dwStyle = CFS_CANDIDATEPOS;
	pCandidate->ptCurrentPos.x = pCaretRect->left;
	pCandidate->ptCurrentPos.y = pCaretRect->bottom;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreQueryInterface(ITextStoreACP* pInterface, REFIID pIid, void** ppObject);
static ULONG STDMETHODCALLTYPE __xgeTsfStoreAddRef(ITextStoreACP* pInterface);
static ULONG STDMETHODCALLTYPE __xgeTsfStoreRelease(ITextStoreACP* pInterface);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreAdviseSink(ITextStoreACP* pInterface, REFIID pIid, IUnknown* pUnknown, DWORD iMask);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreUnadviseSink(ITextStoreACP* pInterface, IUnknown* pUnknown);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestLock(ITextStoreACP* pInterface, DWORD iFlags, HRESULT* pSessionResult);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetStatus(ITextStoreACP* pInterface, TS_STATUS* pStatus);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreQueryInsert(ITextStoreACP* pInterface, LONG iTestStart, LONG iTestEnd, ULONG iLength, LONG* pResultStart, LONG* pResultEnd);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetSelection(ITextStoreACP* pInterface, ULONG iIndex, ULONG iCount, TS_SELECTION_ACP* pSelection, ULONG* pFetched);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreSetSelection(ITextStoreACP* pInterface, ULONG iCount, const TS_SELECTION_ACP* pSelection);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetText(ITextStoreACP* pInterface, LONG iStart, LONG iEnd, WCHAR* sText, ULONG iTextCapacity, ULONG* pTextLength, TS_RUNINFO* pRunInfo, ULONG iRunCapacity, ULONG* pRunCount, LONG* pNext);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreSetText(ITextStoreACP* pInterface, DWORD iFlags, LONG iStart, LONG iEnd, const WCHAR* sText, ULONG iTextLength, TS_TEXTCHANGE* pChange);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetFormattedText(ITextStoreACP* pInterface, LONG iStart, LONG iEnd, IDataObject** ppDataObject);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetEmbedded(ITextStoreACP* pInterface, LONG iPosition, REFGUID pService, REFIID pIid, IUnknown** ppUnknown);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreQueryInsertEmbedded(ITextStoreACP* pInterface, const GUID* pService, const FORMATETC* pFormat, WINBOOL* pInsertable);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreInsertEmbedded(ITextStoreACP* pInterface, DWORD iFlags, LONG iStart, LONG iEnd, IDataObject* pDataObject, TS_TEXTCHANGE* pChange);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreInsertTextAtSelection(ITextStoreACP* pInterface, DWORD iFlags, const WCHAR* sText, ULONG iTextLength, LONG* pStart, LONG* pEnd, TS_TEXTCHANGE* pChange);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreInsertEmbeddedAtSelection(ITextStoreACP* pInterface, DWORD iFlags, IDataObject* pDataObject, LONG* pStart, LONG* pEnd, TS_TEXTCHANGE* pChange);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestSupportedAttrs(ITextStoreACP* pInterface, DWORD iFlags, ULONG iCount, const TS_ATTRID* pAttrs);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestAttrsAtPosition(ITextStoreACP* pInterface, LONG iPosition, ULONG iCount, const TS_ATTRID* pAttrs, DWORD iFlags);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestAttrsTransitioningAtPosition(ITextStoreACP* pInterface, LONG iPosition, ULONG iCount, const TS_ATTRID* pAttrs, DWORD iFlags);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreFindNextAttrTransition(ITextStoreACP* pInterface, LONG iStart, LONG iHalt, ULONG iCount, const TS_ATTRID* pAttrs, DWORD iFlags, LONG* pNext, WINBOOL* pFound, LONG* pFoundOffset);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRetrieveRequestedAttrs(ITextStoreACP* pInterface, ULONG iCount, TS_ATTRVAL* pValues, ULONG* pFetched);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetEndACP(ITextStoreACP* pInterface, LONG* pEnd);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetActiveView(ITextStoreACP* pInterface, TsViewCookie* pView);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetACPFromPoint(ITextStoreACP* pInterface, TsViewCookie iView, const POINT* pPoint, DWORD iFlags, LONG* pPosition);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetTextExt(ITextStoreACP* pInterface, TsViewCookie iView, LONG iStart, LONG iEnd, RECT* pRect, WINBOOL* pClipped);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetScreenExt(ITextStoreACP* pInterface, TsViewCookie iView, RECT* pRect);
static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetWnd(ITextStoreACP* pInterface, TsViewCookie iView, HWND* pWnd);

static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionQueryInterface(ITfContextOwnerCompositionSink* pInterface, REFIID pIid, void** ppObject);
static ULONG STDMETHODCALLTYPE __xgeTsfCompositionAddRef(ITfContextOwnerCompositionSink* pInterface);
static ULONG STDMETHODCALLTYPE __xgeTsfCompositionRelease(ITfContextOwnerCompositionSink* pInterface);
static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionStart(ITfContextOwnerCompositionSink* pInterface, ITfCompositionView* pComposition, WINBOOL* pOk);
static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionUpdate(ITfContextOwnerCompositionSink* pInterface, ITfCompositionView* pComposition, ITfRange* pNewRange);
static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionEnd(ITfContextOwnerCompositionSink* pInterface, ITfCompositionView* pComposition);

static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkQueryInterface(ITfUIElementSink* pInterface, REFIID pIid, void** ppObject);
static ULONG STDMETHODCALLTYPE __xgeTsfUiSinkAddRef(ITfUIElementSink* pInterface);
static ULONG STDMETHODCALLTYPE __xgeTsfUiSinkRelease(ITfUIElementSink* pInterface);
static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkBegin(ITfUIElementSink* pInterface, DWORD iElementId, WINBOOL* pShow);
static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkUpdate(ITfUIElementSink* pInterface, DWORD iElementId);
static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkEnd(ITfUIElementSink* pInterface, DWORD iElementId);

static ITextStoreACPVtbl g_xgeTsfStoreVtbl = {
	__xgeTsfStoreQueryInterface,
	__xgeTsfStoreAddRef,
	__xgeTsfStoreRelease,
	__xgeTsfStoreAdviseSink,
	__xgeTsfStoreUnadviseSink,
	__xgeTsfStoreRequestLock,
	__xgeTsfStoreGetStatus,
	__xgeTsfStoreQueryInsert,
	__xgeTsfStoreGetSelection,
	__xgeTsfStoreSetSelection,
	__xgeTsfStoreGetText,
	__xgeTsfStoreSetText,
	__xgeTsfStoreGetFormattedText,
	__xgeTsfStoreGetEmbedded,
	__xgeTsfStoreQueryInsertEmbedded,
	__xgeTsfStoreInsertEmbedded,
	__xgeTsfStoreInsertTextAtSelection,
	__xgeTsfStoreInsertEmbeddedAtSelection,
	__xgeTsfStoreRequestSupportedAttrs,
	__xgeTsfStoreRequestAttrsAtPosition,
	__xgeTsfStoreRequestAttrsTransitioningAtPosition,
	__xgeTsfStoreFindNextAttrTransition,
	__xgeTsfStoreRetrieveRequestedAttrs,
	__xgeTsfStoreGetEndACP,
	__xgeTsfStoreGetActiveView,
	__xgeTsfStoreGetACPFromPoint,
	__xgeTsfStoreGetTextExt,
	__xgeTsfStoreGetScreenExt,
	__xgeTsfStoreGetWnd
};

static ITfContextOwnerCompositionSinkVtbl g_xgeTsfCompositionVtbl = {
	__xgeTsfCompositionQueryInterface,
	__xgeTsfCompositionAddRef,
	__xgeTsfCompositionRelease,
	__xgeTsfCompositionStart,
	__xgeTsfCompositionUpdate,
	__xgeTsfCompositionEnd
};

static ITfUIElementSinkVtbl g_xgeTsfUiElementSinkVtbl = {
	__xgeTsfUiSinkQueryInterface,
	__xgeTsfUiSinkAddRef,
	__xgeTsfUiSinkRelease,
	__xgeTsfUiSinkBegin,
	__xgeTsfUiSinkUpdate,
	__xgeTsfUiSinkEnd
};

static xge_tsf_text_store_t* __xgeTsfStoreFromInterface(ITextStoreACP* pInterface)
{
	return (xge_tsf_text_store_t*)((char*)pInterface - offsetof(xge_tsf_text_store_t, tStore));
}

static xge_tsf_text_store_t* __xgeTsfStoreFromComposition(ITfContextOwnerCompositionSink* pInterface)
{
	return (xge_tsf_text_store_t*)((char*)pInterface - offsetof(xge_tsf_text_store_t, tCompositionSink));
}

static int __xgeTsfStoreHasReadLock(const xge_tsf_text_store_t* pStore)
{
	return pStore != NULL && (pStore->iLockType & TS_LF_READ) != 0;
}

static int __xgeTsfStoreHasWriteLock(const xge_tsf_text_store_t* pStore)
{
	return pStore != NULL && (pStore->iLockType & TS_LF_READWRITE) == TS_LF_READWRITE;
}

static int __xgeTsfStoreReserve(xge_tsf_text_store_t* pStore, LONG iCapacity)
{
	WCHAR* sNew;
	LONG iNewCapacity;

	if ( iCapacity <= pStore->iTextCapacity ) return 1;
	iNewCapacity = (pStore->iTextCapacity > 0) ? pStore->iTextCapacity * 2 : 32;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	sNew = (WCHAR*)xrtRealloc(pStore->sText, sizeof(WCHAR) * (size_t)iNewCapacity);
	if ( sNew == NULL ) return 0;
	pStore->sText = sNew;
	pStore->iTextCapacity = iNewCapacity;
	return 1;
}

static int __xgeTsfStoreReserveCompositionOriginal(xge_tsf_text_store_t* pStore, LONG iCapacity)
{
	WCHAR* sNew;
	LONG iNewCapacity;

	if ( iCapacity <= pStore->iCompositionOriginalCapacity ) return 1;
	iNewCapacity = (pStore->iCompositionOriginalCapacity > 0) ? pStore->iCompositionOriginalCapacity : 16;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	sNew = (WCHAR*)xrtRealloc(pStore->sCompositionOriginal,
		 sizeof(WCHAR) * (size_t)iNewCapacity);
	if ( sNew == NULL ) return 0;
	pStore->sCompositionOriginal = sNew;
	pStore->iCompositionOriginalCapacity = iNewCapacity;
	return 1;
}

static LONG __xgeTsfClampLong(LONG iValue, LONG iMinimum, LONG iMaximum)
{
	if ( iValue < iMinimum ) return iMinimum;
	if ( iValue > iMaximum ) return iMaximum;
	return iValue;
}

static int __xgeTsfStoreDocumentUtf8Offset(const xge_tsf_text_store_t* pStore, LONG iWideOffset)
{
	iWideOffset = __xgeTsfClampLong(iWideOffset, 0, pStore->iTextLength);
	return pStore->iDocumentOffset + WideCharToMultiByte(CP_UTF8, 0,
		pStore->sText, iWideOffset, NULL, 0, NULL, NULL);
}

static int __xgeTsfStoreCaptureOriginal(xge_tsf_text_store_t* pStore, LONG iStart, LONG iEnd)
{
	LONG iLength;

	iStart = __xgeTsfClampLong(iStart, 0, pStore->iTextLength);
	iEnd = __xgeTsfClampLong(iEnd, iStart, pStore->iTextLength);
	iLength = iEnd - iStart;
	if ( !__xgeTsfStoreReserveCompositionOriginal(pStore, iLength + 1) ) return 0;
	if ( iLength > 0 ) {
		memcpy(pStore->sCompositionOriginal, pStore->sText + iStart,
			sizeof(WCHAR) * (size_t)iLength);
	}
	pStore->sCompositionOriginal[iLength] = L'\0';
	pStore->iCompositionOriginalLength = iLength;
	return 1;
}

static void __xgeTsfStoreSetReplacementRange(xge_tsf_text_store_t* pStore,
	LONG iStart, LONG iEnd)
{
	pStore->iReplacementStart = __xgeTsfStoreDocumentUtf8Offset(pStore, iStart);
	pStore->iReplacementEnd = __xgeTsfStoreDocumentUtf8Offset(pStore, iEnd);
	pStore->bHasReplacementRange = 1;
}

static void __xgeTsfStoreTrackCompositionEdit(xge_tsf_text_store_t* pStore,
	LONG iStart, LONG iEnd, LONG iNewEnd)
{
	LONG iDelta;
	LONG iOldCompositionEnd;

	if ( pStore == NULL || !pStore->bComposing ) return;
	iDelta = iNewEnd - iEnd;
	if ( !pStore->bHasCompositionRange ) {
		pStore->iCompositionStart = iStart;
		pStore->iCompositionEnd = iNewEnd;
		pStore->bHasCompositionRange = 1;
		return;
	}
	iOldCompositionEnd = pStore->iCompositionEnd;
	if ( iEnd < pStore->iCompositionStart ) {
		pStore->iCompositionStart += iDelta;
		pStore->iCompositionEnd += iDelta;
	} else if ( iStart <= iOldCompositionEnd ) {
		if ( iStart < pStore->iCompositionStart ) pStore->iCompositionStart = iStart;
		pStore->iCompositionEnd = iOldCompositionEnd + iDelta;
		if ( pStore->iCompositionEnd < iNewEnd ) pStore->iCompositionEnd = iNewEnd;
	}
	pStore->iCompositionStart = __xgeTsfClampLong(pStore->iCompositionStart, 0, pStore->iTextLength + iDelta);
	pStore->iCompositionEnd = __xgeTsfClampLong(pStore->iCompositionEnd, pStore->iCompositionStart, pStore->iTextLength + iDelta);
}

static HRESULT __xgeTsfStoreReplace(xge_tsf_text_store_t* pStore, LONG iStart, LONG iEnd, const WCHAR* sText, ULONG iTextLength, TS_TEXTCHANGE* pChange)
{
	LONG iNewLength;

	if ( !__xgeTsfStoreHasWriteLock(pStore) ) return TS_E_NOLOCK;
	if ( iStart < 0 || iEnd < iStart || iEnd > pStore->iTextLength ) return TS_E_INVALIDPOS;
	if ( sText == NULL && iTextLength > 0 ) return E_INVALIDARG;
	if ( !pStore->bComposing && !pStore->bPendingCompositionEdit ) {
		if ( !__xgeTsfStoreCaptureOriginal(pStore, iStart, iEnd) ) return E_OUTOFMEMORY;
		__xgeTsfStoreSetReplacementRange(pStore, iStart, iEnd);
		pStore->iPendingCompositionStart = iStart;
		pStore->iPendingCompositionEnd = iStart + (LONG)iTextLength;
		pStore->bPendingCompositionEdit = 1;
	}
	iNewLength = pStore->iTextLength - (iEnd - iStart) + (LONG)iTextLength;
	__xgeTsfStoreTrackCompositionEdit(pStore, iStart, iEnd, iStart + (LONG)iTextLength);
	if ( !__xgeTsfStoreReserve(pStore, iNewLength + 1) ) return E_OUTOFMEMORY;
	if ( iEnd < pStore->iTextLength ) {
		memmove(pStore->sText + iStart + (LONG)iTextLength,
		        pStore->sText + iEnd,
		        sizeof(WCHAR) * (size_t)(pStore->iTextLength - iEnd));
	}
	if ( iTextLength > 0 ) memcpy(pStore->sText + iStart, sText, sizeof(WCHAR) * (size_t)iTextLength);
	pStore->iTextLength = iNewLength;
	pStore->sText[iNewLength] = L'\0';
	pStore->iSelectionStart = iStart + (LONG)iTextLength;
	pStore->iSelectionEnd = pStore->iSelectionStart;
	pStore->bTextChanged = 1;
	pStore->iLastChangeStart = iStart;
	pStore->iLastChangeEnd = iStart + (LONG)iTextLength;
	if ( pChange != NULL ) {
		pChange->acpStart = iStart;
		pChange->acpOldEnd = iEnd;
		pChange->acpNewEnd = iStart + (LONG)iTextLength;
	}
	return S_OK;
}

static int __xgeTsfWideToUtf8(const WCHAR* sWide, int iWideLength, char** psText, int* pTextLength)
{
	char* sText;
	int iLength;

	if ( psText == NULL || pTextLength == NULL ) return 0;
	*psText = NULL;
	*pTextLength = 0;
	if ( sWide == NULL || iWideLength <= 0 ) return 1;
	iLength = WideCharToMultiByte(CP_UTF8, 0, sWide, iWideLength, NULL, 0, NULL, NULL);
	if ( iLength <= 0 ) return 0;
	sText = (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sText == NULL ) return 0;
	if ( WideCharToMultiByte(CP_UTF8, 0, sWide, iWideLength, sText, iLength, NULL, NULL) != iLength ) {
		xrtFree(sText);
		return 0;
	}
	sText[iLength] = '\0';
	*psText = sText;
	*pTextLength = iLength;
	return 1;
}

static int __xgeTsfUtf8ToWide(const char* sText, int iTextLength, WCHAR** psWide, int* pWideLength)
{
	WCHAR* sWide;
	int iLength;

	if ( psWide == NULL || pWideLength == NULL ) return 0;
	*psWide = NULL;
	*pWideLength = 0;
	if ( sText == NULL ) sText = "";
	if ( iTextLength < 0 ) iTextLength = (int)strlen(sText);
	if ( iTextLength == 0 ) {
		sWide = (WCHAR*)xrtMalloc(sizeof(WCHAR));
		if ( sWide == NULL ) return 0;
		sWide[0] = L'\0';
		*psWide = sWide;
		return 1;
	}
	iLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sText, iTextLength, NULL, 0);
	if ( iLength <= 0 ) return 0;
	sWide = (WCHAR*)xrtMalloc(sizeof(WCHAR) * ((size_t)iLength + 1u));
	if ( sWide == NULL ) return 0;
	if ( MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sText, iTextLength, sWide, iLength) != iLength ) {
		xrtFree(sWide);
		return 0;
	}
	sWide[iLength] = L'\0';
	*psWide = sWide;
	*pWideLength = iLength;
	return 1;
}

static LONG __xgeTsfUtf8ByteToWideOffset(const char* sText, int iTextLength, int iByteOffset)
{
	if ( sText == NULL || iTextLength <= 0 || iByteOffset <= 0 ) return 0;
	if ( iByteOffset > iTextLength ) iByteOffset = iTextLength;
	while ( iByteOffset > 0 && iByteOffset < iTextLength &&
	        (((unsigned char)sText[iByteOffset]) & 0xC0u) == 0x80u ) iByteOffset--;
	return (LONG)MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sText, iByteOffset, NULL, 0);
}

static int __xgeTsfStoreApplyClientSnapshot(xge_tsf_text_store_t* pStore,
	const xge_ime_text_snapshot_t* pSnapshot)
{
	WCHAR* sWide;
	TS_TEXTCHANGE tChange;
	LONG iOldTextLength;
	LONG iOldSelectionStart;
	LONG iOldSelectionEnd;
	LONG iNewSelectionStart;
	LONG iNewSelectionEnd;
	int iWideLength;
	int iTextLength;
	int iSelectionStart;
	int iSelectionEnd;
	int bTextChanged;

	if ( pStore == NULL || pSnapshot == NULL || pStore->bComposing || pStore->iLockType != 0 ) return 0;
	iTextLength = pSnapshot->iTextSize;
	if ( pSnapshot->sText == NULL ) iTextLength = 0;
	else if ( iTextLength < 0 ) iTextLength = (int)strlen(pSnapshot->sText);
	iSelectionStart = pSnapshot->iSelectionStart;
	iSelectionEnd = pSnapshot->iSelectionEnd;
	if ( iSelectionStart < 0 ) iSelectionStart = 0;
	if ( iSelectionEnd < iSelectionStart ) iSelectionEnd = iSelectionStart;
	if ( iSelectionStart > iTextLength ) iSelectionStart = iTextLength;
	if ( iSelectionEnd > iTextLength ) iSelectionEnd = iTextLength;
	sWide = NULL;
	iWideLength = 0;
	if ( !__xgeTsfUtf8ToWide(pSnapshot->sText, iTextLength, &sWide, &iWideLength) ) return 0;
	if ( !__xgeTsfStoreReserve(pStore, (LONG)iWideLength + 1) ) {
		xrtFree(sWide);
		return 0;
	}
	iOldTextLength = pStore->iTextLength;
	iOldSelectionStart = pStore->iSelectionStart;
	iOldSelectionEnd = pStore->iSelectionEnd;
	iNewSelectionStart = __xgeTsfUtf8ByteToWideOffset(pSnapshot->sText, iTextLength, iSelectionStart);
	iNewSelectionEnd = __xgeTsfUtf8ByteToWideOffset(pSnapshot->sText, iTextLength, iSelectionEnd);
	memset(&tChange, 0, sizeof(tChange));
	tChange.acpStart = 0;
	tChange.acpOldEnd = iOldTextLength;
	tChange.acpNewEnd = (LONG)iWideLength;
	bTextChanged = (iOldTextLength != (LONG)iWideLength ||
		(iWideLength > 0 && memcmp(pStore->sText, sWide,
		 sizeof(WCHAR) * (size_t)iWideLength) != 0));
	memcpy(pStore->sText, sWide, sizeof(WCHAR) * ((size_t)iWideLength + 1u));
	pStore->iTextLength = (LONG)iWideLength;
	pStore->iSelectionStart = iNewSelectionStart;
	pStore->iSelectionEnd = iNewSelectionEnd;
	pStore->iDocumentOffset = pSnapshot->iDocumentOffset;
	pStore->bTextChanged = 0;
	pStore->bEndPending = 0;
	pStore->bHasCompositionRange = 0;
	pStore->bHasReplacementRange = 0;
	pStore->bPendingCompositionEdit = 0;
	xrtFree(sWide);
	if ( pStore->pSink != NULL && bTextChanged &&
	     (pStore->iSinkMask & TS_AS_TEXT_CHANGE) != 0 ) {
		(void)pStore->pSink->lpVtbl->OnTextChange(pStore->pSink, 0, &tChange);
	}
	if ( pStore->pSink != NULL &&
	     (iOldSelectionStart != iNewSelectionStart || iOldSelectionEnd != iNewSelectionEnd) &&
	     (pStore->iSinkMask & TS_AS_SEL_CHANGE) != 0 ) {
		(void)pStore->pSink->lpVtbl->OnSelectionChange(pStore->pSink);
	}
	return 1;
}

static int __xgeTsfRefreshTextClient(void)
{
	xge_ime_text_snapshot_t tSnapshot;
	int iRet;

	if ( !g_xgeWin32Ime.bHasTextClient || g_xgeWin32Ime.tTextClient.onSnapshot == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	if ( g_xgeWin32Ime.tTextStore.bComposing ) return XGE_OK;
	memset(&tSnapshot, 0, sizeof(tSnapshot));
	tSnapshot.iSize = sizeof(tSnapshot);
	iRet = g_xgeWin32Ime.tTextClient.onSnapshot(g_xgeWin32Ime.tTextClient.pUser, &tSnapshot);
	if ( iRet != XGE_OK ) return iRet;
	if ( tSnapshot.iSize != 0u && tSnapshot.iSize < sizeof(tSnapshot) ) return XGE_ERROR_INVALID_ARGUMENT;
	return __xgeTsfStoreApplyClientSnapshot(&g_xgeWin32Ime.tTextStore, &tSnapshot) ? XGE_OK : XGE_ERROR_INVALID_ARGUMENT;
}

static int __xgeTsfUtf8Offset(const WCHAR* sWide, int iWideLength)
{
	if ( sWide == NULL || iWideLength <= 0 ) return 0;
	return WideCharToMultiByte(CP_UTF8, 0, sWide, iWideLength, NULL, 0, NULL, NULL);
}

static void __xgeTsfStoreEmitUpdate(xge_tsf_text_store_t* pStore)
{
	char* sText;
	int iTextLength;
	int iCursor;
	int iSelectionStart;
	int iSelectionEnd;
	if ( g_xgeWin32Ime.bImmFallbackActive && __xgeTsfCompositionNeedsImmFallback(pStore) ) return;

	sText = NULL;
	iTextLength = 0;
	LONG iStart = pStore->bHasCompositionRange ? pStore->iCompositionStart : 0;
	LONG iEnd = pStore->bHasCompositionRange ? pStore->iCompositionEnd : pStore->iTextLength;
	iStart = __xgeTsfClampLong(iStart, 0, pStore->iTextLength);
	iEnd = __xgeTsfClampLong(iEnd, iStart, pStore->iTextLength);
	if ( !__xgeTsfWideToUtf8(pStore->sText + iStart, (int)(iEnd - iStart), &sText, &iTextLength) ) return;
	iCursor = __xgeTsfUtf8Offset(pStore->sText + iStart, (int)(__xgeTsfClampLong(pStore->iSelectionEnd, iStart, iEnd) - iStart));
	iSelectionStart = __xgeTsfUtf8Offset(pStore->sText + iStart, (int)(__xgeTsfClampLong(pStore->iSelectionStart, iStart, iEnd) - iStart));
	iSelectionEnd = iCursor;
	(void)__xgeImeQueuePushRange(XGE_EVENT_IME_UPDATE, sText, iTextLength,
		iCursor, iSelectionStart, iSelectionEnd,
		pStore->bHasReplacementRange, pStore->iReplacementStart, pStore->iReplacementEnd);
	if ( sText != NULL ) xrtFree(sText);
}

static void __xgeTsfStoreEmitCommit(xge_tsf_text_store_t* pStore)
{
	char* sText;
	int iTextLength;

	LONG iStart = pStore->bHasCompositionRange ? pStore->iCompositionStart : pStore->iLastChangeStart;
	LONG iEnd = pStore->bHasCompositionRange ? pStore->iCompositionEnd : pStore->iLastChangeEnd;
	if ( g_xgeWin32Ime.bImmFallbackCommitted ) {
		g_xgeWin32Ime.bImmFallbackCommitted = 0;
		return;
	}
	iStart = __xgeTsfClampLong(iStart, 0, pStore->iTextLength);
	iEnd = __xgeTsfClampLong(iEnd, iStart, pStore->iTextLength);
	if ( pStore->bHasCompositionRange && pStore->sCompositionOriginal != NULL &&
	     pStore->iCompositionOriginalLength == iEnd - iStart &&
	     (iEnd == iStart || memcmp(pStore->sText + iStart,
	       pStore->sCompositionOriginal, sizeof(WCHAR) * (size_t)(iEnd - iStart)) == 0) ) return;
	if ( !pStore->bHasCompositionRange && iEnd <= iStart ) return;
	sText = NULL;
	iTextLength = 0;
	if ( __xgeTsfWideToUtf8(pStore->sText + iStart, (int)(iEnd - iStart), &sText, &iTextLength) ) {
		(void)__xgeImeQueuePushRange(XGE_EVENT_IME_COMMIT, sText, iTextLength,
			iTextLength, iTextLength, iTextLength,
			pStore->bHasReplacementRange, pStore->iReplacementStart, pStore->iReplacementEnd);
	}
	if ( sText != NULL ) xrtFree(sText);
}

static void __xgeTsfStoreClearComposition(xge_tsf_text_store_t* pStore)
{
	pStore->bTextChanged = 0;
	pStore->bHasCompositionRange = 0;
	pStore->bHasReplacementRange = 0;
	pStore->bPendingCompositionEdit = 0;
	pStore->iCompositionStart = 0;
	pStore->iCompositionEnd = 0;
	pStore->iCompositionOriginalLength = 0;
}

static void __xgeTsfStoreFlushEvents(xge_tsf_text_store_t* pStore)
{
	if ( pStore->bEndPending ) {
		if ( g_xgeWin32Ime.bImmFallbackEnded ) {
			g_xgeWin32Ime.bImmFallbackEnded = 0;
		} else if ( g_xgeWin32Ime.bImmComposing &&
		            (g_xgeWin32Ime.bImmFallbackActive ||
		             __xgeTsfCompositionNeedsImmFallback(pStore)) ) {
			/* The IMM result/end pair owns the visible fallback composition. */
			if ( pStore->bHasReplacementRange ) {
				g_xgeWin32Ime.bImmReplacementRange = 1;
				g_xgeWin32Ime.iImmReplacementStart = pStore->iReplacementStart;
				g_xgeWin32Ime.iImmReplacementEnd = pStore->iReplacementEnd;
			}
		} else {
			__xgeTsfStoreEmitCommit(pStore);
			(void)__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
		}
		pStore->bComposing = 0;
		pStore->bEndPending = 0;
		__xgeTsfStoreClearComposition(pStore);
	} else if ( pStore->bTextChanged ) {
		if ( pStore->bComposing ) {
			__xgeTsfStoreEmitUpdate(pStore);
			pStore->bTextChanged = 0;
		} else {
			__xgeTsfStoreEmitCommit(pStore);
			__xgeTsfStoreClearComposition(pStore);
		}
	}
	__xgeRenderRequestInternal();
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreQueryInterface(ITextStoreACP* pInterface, REFIID pIid, void** ppObject)
{
	xge_tsf_text_store_t* pStore;

	if ( ppObject == NULL ) return E_POINTER;
	*ppObject = NULL;
	if ( pInterface == NULL || pIid == NULL ) return E_INVALIDARG;
	pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( IsEqualIID(pIid, &IID_IUnknown) || IsEqualIID(pIid, &IID_ITextStoreACP) ) {
		*ppObject = &pStore->tStore;
	} else if ( IsEqualIID(pIid, &IID_ITfContextOwnerCompositionSink) ) {
		*ppObject = &pStore->tCompositionSink;
	} else {
		return E_NOINTERFACE;
	}
	InterlockedIncrement(&pStore->iRefCount);
	return S_OK;
}

static ULONG STDMETHODCALLTYPE __xgeTsfStoreAddRef(ITextStoreACP* pInterface)
{
	return (ULONG)InterlockedIncrement(&__xgeTsfStoreFromInterface(pInterface)->iRefCount);
}

static ULONG STDMETHODCALLTYPE __xgeTsfStoreRelease(ITextStoreACP* pInterface)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	LONG iCount = InterlockedDecrement(&pStore->iRefCount);
	if ( iCount < 1 ) {
		pStore->iRefCount = 1;
		return 1;
	}
	return (ULONG)iCount;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreAdviseSink(ITextStoreACP* pInterface, REFIID pIid, IUnknown* pUnknown, DWORD iMask)
{
	xge_tsf_text_store_t* pStore;
	ITextStoreACPSink* pSink;
	HRESULT iRet;

	if ( pUnknown == NULL || pIid == NULL ) return E_INVALIDARG;
	if ( !IsEqualIID(pIid, &IID_ITextStoreACPSink) ) return E_INVALIDARG;
	pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( pStore->pSink != NULL ) {
		pStore->iSinkMask = iMask;
		return CONNECT_E_ADVISELIMIT;
	}
	pSink = NULL;
	iRet = pUnknown->lpVtbl->QueryInterface(pUnknown, &IID_ITextStoreACPSink, (void**)&pSink);
	if ( FAILED(iRet) ) return iRet;
	pStore->pSink = pSink;
	pStore->iSinkMask = iMask;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreUnadviseSink(ITextStoreACP* pInterface, IUnknown* pUnknown)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	(void)pUnknown;
	if ( pStore->pSink == NULL ) return CONNECT_E_NOCONNECTION;
	pStore->pSink->lpVtbl->Release(pStore->pSink);
	pStore->pSink = NULL;
	pStore->iSinkMask = 0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestLock(ITextStoreACP* pInterface, DWORD iFlags, HRESULT* pSessionResult)
{
	xge_tsf_text_store_t* pStore;
	DWORD iNextLock;
	HRESULT iSession;

	if ( pSessionResult == NULL ) return E_INVALIDARG;
	pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( pStore->pSink == NULL ) return E_UNEXPECTED;
	if ( pStore->iLockType != 0 ) {
		if ( (iFlags & TS_LF_SYNC) != 0 ) {
			*pSessionResult = TS_E_SYNCHRONOUS;
			return S_OK;
		}
		iNextLock = ((iFlags & TS_LF_READWRITE) == TS_LF_READWRITE) ? TS_LF_READWRITE : TS_LF_READ;
		if ( (pStore->iPendingLock & TS_LF_READWRITE) != TS_LF_READWRITE ) pStore->iPendingLock = iNextLock;
		*pSessionResult = TS_S_ASYNC;
		return S_OK;
	}

	iNextLock = iFlags;
	do {
		pStore->iPendingLock = 0;
		pStore->iLockType = iNextLock;
		iSession = pStore->pSink->lpVtbl->OnLockGranted(pStore->pSink, iNextLock);
		pStore->iLockType = 0;
		__xgeTsfStoreFlushEvents(pStore);
		iNextLock = pStore->iPendingLock;
	} while ( iNextLock != 0 );
	*pSessionResult = iSession;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetStatus(ITextStoreACP* pInterface, TS_STATUS* pStatus)
{
	(void)pInterface;
	if ( pStatus == NULL ) return E_INVALIDARG;
	pStatus->dwDynamicFlags = 0;
	pStatus->dwStaticFlags = 0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreQueryInsert(ITextStoreACP* pInterface, LONG iTestStart, LONG iTestEnd, ULONG iLength, LONG* pResultStart, LONG* pResultEnd)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( pResultStart == NULL || pResultEnd == NULL ) return E_INVALIDARG;
	if ( iTestStart < 0 || iTestEnd < iTestStart || iTestEnd > pStore->iTextLength ) return TS_E_INVALIDPOS;
	*pResultStart = iTestStart;
	*pResultEnd = iTestStart + (LONG)iLength;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetSelection(ITextStoreACP* pInterface, ULONG iIndex, ULONG iCount, TS_SELECTION_ACP* pSelection, ULONG* pFetched)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( !__xgeTsfStoreHasReadLock(pStore) ) return TS_E_NOLOCK;
	if ( pSelection == NULL || pFetched == NULL || iCount == 0 ) return E_INVALIDARG;
	if ( iIndex != (ULONG)TS_DEFAULT_SELECTION && iIndex != 0 ) return TS_E_NOSELECTION;
	pSelection[0].acpStart = pStore->iSelectionStart;
	pSelection[0].acpEnd = pStore->iSelectionEnd;
	pSelection[0].style.ase = TS_AE_END;
	pSelection[0].style.fInterimChar = FALSE;
	*pFetched = 1;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreSetSelection(ITextStoreACP* pInterface, ULONG iCount, const TS_SELECTION_ACP* pSelection)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( !__xgeTsfStoreHasWriteLock(pStore) ) return TS_E_NOLOCK;
	if ( pSelection == NULL || iCount != 1 ) return E_INVALIDARG;
	if ( pSelection[0].acpStart < 0 || pSelection[0].acpEnd < pSelection[0].acpStart || pSelection[0].acpEnd > pStore->iTextLength ) return TS_E_INVALIDPOS;
	pStore->iSelectionStart = pSelection[0].acpStart;
	pStore->iSelectionEnd = pSelection[0].acpEnd;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetText(ITextStoreACP* pInterface, LONG iStart, LONG iEnd, WCHAR* sText, ULONG iTextCapacity, ULONG* pTextLength, TS_RUNINFO* pRunInfo, ULONG iRunCapacity, ULONG* pRunCount, LONG* pNext)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	ULONG iCopyLength;

	if ( !__xgeTsfStoreHasReadLock(pStore) ) return TS_E_NOLOCK;
	if ( pTextLength == NULL || pRunCount == NULL || pNext == NULL ) return E_INVALIDARG;
	if ( iStart < 0 || iStart > pStore->iTextLength ) return TS_E_INVALIDPOS;
	if ( iEnd == -1 ) iEnd = pStore->iTextLength;
	if ( iEnd < iStart || iEnd > pStore->iTextLength ) return TS_E_INVALIDPOS;
	iCopyLength = (ULONG)(iEnd - iStart);
	if ( iCopyLength > iTextCapacity ) iCopyLength = iTextCapacity;
	if ( iCopyLength > 0 && sText == NULL ) return E_INVALIDARG;
	if ( iCopyLength > 0 ) memcpy(sText, pStore->sText + iStart, sizeof(WCHAR) * (size_t)iCopyLength);
	*pTextLength = iCopyLength;
	*pNext = iStart + (LONG)iCopyLength;
	*pRunCount = 0;
	if ( iRunCapacity > 0 && pRunInfo != NULL && iCopyLength > 0 ) {
		pRunInfo[0].uCount = iCopyLength;
		pRunInfo[0].type = TS_RT_PLAIN;
		*pRunCount = 1;
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreSetText(ITextStoreACP* pInterface, DWORD iFlags, LONG iStart, LONG iEnd, const WCHAR* sText, ULONG iTextLength, TS_TEXTCHANGE* pChange)
{
	(void)iFlags;
	return __xgeTsfStoreReplace(__xgeTsfStoreFromInterface(pInterface), iStart, iEnd, sText, iTextLength, pChange);
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetFormattedText(ITextStoreACP* pInterface, LONG iStart, LONG iEnd, IDataObject** ppDataObject)
{
	(void)pInterface; (void)iStart; (void)iEnd;
	if ( ppDataObject != NULL ) *ppDataObject = NULL;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetEmbedded(ITextStoreACP* pInterface, LONG iPosition, REFGUID pService, REFIID pIid, IUnknown** ppUnknown)
{
	(void)pInterface; (void)iPosition; (void)pService; (void)pIid;
	if ( ppUnknown != NULL ) *ppUnknown = NULL;
	return TS_E_NOOBJECT;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreQueryInsertEmbedded(ITextStoreACP* pInterface, const GUID* pService, const FORMATETC* pFormat, WINBOOL* pInsertable)
{
	(void)pInterface; (void)pService; (void)pFormat;
	if ( pInsertable == NULL ) return E_INVALIDARG;
	*pInsertable = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreInsertEmbedded(ITextStoreACP* pInterface, DWORD iFlags, LONG iStart, LONG iEnd, IDataObject* pDataObject, TS_TEXTCHANGE* pChange)
{
	(void)pInterface; (void)iFlags; (void)iStart; (void)iEnd; (void)pDataObject; (void)pChange;
	return TS_E_FORMAT;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreInsertTextAtSelection(ITextStoreACP* pInterface, DWORD iFlags, const WCHAR* sText, ULONG iTextLength, LONG* pStart, LONG* pEnd, TS_TEXTCHANGE* pChange)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	LONG iStart = pStore->iSelectionStart;
	LONG iEnd = pStore->iSelectionEnd;
	HRESULT iRet;

	if ( !__xgeTsfStoreHasWriteLock(pStore) ) return TS_E_NOLOCK;
	if ( (iFlags & TS_IAS_QUERYONLY) != 0 ) {
		if ( pStart != NULL ) *pStart = iStart;
		if ( pEnd != NULL ) *pEnd = iStart + (LONG)iTextLength;
		return S_OK;
	}
	iRet = __xgeTsfStoreReplace(pStore, iStart, iEnd, sText, iTextLength, pChange);
	if ( SUCCEEDED(iRet) && (iFlags & TS_IAS_NOQUERY) == 0 ) {
		if ( pStart != NULL ) *pStart = iStart;
		if ( pEnd != NULL ) *pEnd = iStart + (LONG)iTextLength;
	}
	return iRet;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreInsertEmbeddedAtSelection(ITextStoreACP* pInterface, DWORD iFlags, IDataObject* pDataObject, LONG* pStart, LONG* pEnd, TS_TEXTCHANGE* pChange)
{
	(void)pInterface; (void)iFlags; (void)pDataObject; (void)pStart; (void)pEnd; (void)pChange;
	return TS_E_FORMAT;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestSupportedAttrs(ITextStoreACP* pInterface, DWORD iFlags, ULONG iCount, const TS_ATTRID* pAttrs)
{
	(void)pInterface; (void)iFlags; (void)iCount; (void)pAttrs;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestAttrsAtPosition(ITextStoreACP* pInterface, LONG iPosition, ULONG iCount, const TS_ATTRID* pAttrs, DWORD iFlags)
{
	(void)pInterface; (void)iPosition; (void)iCount; (void)pAttrs; (void)iFlags;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRequestAttrsTransitioningAtPosition(ITextStoreACP* pInterface, LONG iPosition, ULONG iCount, const TS_ATTRID* pAttrs, DWORD iFlags)
{
	(void)pInterface; (void)iPosition; (void)iCount; (void)pAttrs; (void)iFlags;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreFindNextAttrTransition(ITextStoreACP* pInterface, LONG iStart, LONG iHalt, ULONG iCount, const TS_ATTRID* pAttrs, DWORD iFlags, LONG* pNext, WINBOOL* pFound, LONG* pFoundOffset)
{
	(void)pInterface; (void)iStart; (void)iCount; (void)pAttrs; (void)iFlags;
	if ( pNext == NULL || pFound == NULL || pFoundOffset == NULL ) return E_INVALIDARG;
	*pNext = iHalt;
	*pFound = FALSE;
	*pFoundOffset = 0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreRetrieveRequestedAttrs(ITextStoreACP* pInterface, ULONG iCount, TS_ATTRVAL* pValues, ULONG* pFetched)
{
	(void)pInterface; (void)iCount; (void)pValues;
	if ( pFetched == NULL ) return E_INVALIDARG;
	*pFetched = 0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetEndACP(ITextStoreACP* pInterface, LONG* pEnd)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	if ( !__xgeTsfStoreHasReadLock(pStore) ) return TS_E_NOLOCK;
	if ( pEnd == NULL ) return E_INVALIDARG;
	*pEnd = pStore->iTextLength;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetActiveView(ITextStoreACP* pInterface, TsViewCookie* pView)
{
	(void)pInterface;
	if ( pView == NULL ) return E_INVALIDARG;
	*pView = XGE_TSF_VIEW_COOKIE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetACPFromPoint(ITextStoreACP* pInterface, TsViewCookie iView, const POINT* pPoint, DWORD iFlags, LONG* pPosition)
{
	(void)pInterface; (void)iView; (void)pPoint; (void)iFlags;
	if ( pPosition == NULL ) return E_INVALIDARG;
	*pPosition = 0;
	return TS_E_INVALIDPOINT;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetTextExt(ITextStoreACP* pInterface, TsViewCookie iView, LONG iStart, LONG iEnd, RECT* pRect, WINBOOL* pClipped)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromInterface(pInterface);
	(void)iView;
	if ( !__xgeTsfStoreHasReadLock(pStore) ) return TS_E_NOLOCK;
	if ( pRect == NULL || pClipped == NULL ) return E_INVALIDARG;
	if ( iStart < 0 || iEnd < iStart || iEnd > pStore->iTextLength ) return TS_E_INVALIDPOS;
	if ( !g_xgeWin32Ime.bHasCandidateRect ) return TS_E_NOLAYOUT;
	*pRect = g_xgeWin32Ime.tCandidateScreenRect;
	*pClipped = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetScreenExt(ITextStoreACP* pInterface, TsViewCookie iView, RECT* pRect)
{
	RECT tClient;
	POINT tOrigin;
	(void)pInterface; (void)iView;
	if ( pRect == NULL || g_xgeWin32Ime.hWnd == NULL ) return E_INVALIDARG;
	if ( !GetClientRect(g_xgeWin32Ime.hWnd, &tClient) ) return E_FAIL;
	tOrigin.x = 0;
	tOrigin.y = 0;
	if ( !ClientToScreen(g_xgeWin32Ime.hWnd, &tOrigin) ) return E_FAIL;
	OffsetRect(&tClient, tOrigin.x, tOrigin.y);
	*pRect = tClient;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfStoreGetWnd(ITextStoreACP* pInterface, TsViewCookie iView, HWND* pWnd)
{
	(void)pInterface; (void)iView;
	if ( pWnd == NULL ) return E_INVALIDARG;
	*pWnd = g_xgeWin32Ime.hWnd;
	return (*pWnd != NULL) ? S_OK : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionQueryInterface(ITfContextOwnerCompositionSink* pInterface, REFIID pIid, void** ppObject)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromComposition(pInterface);
	return __xgeTsfStoreQueryInterface(&pStore->tStore, pIid, ppObject);
}

static ULONG STDMETHODCALLTYPE __xgeTsfCompositionAddRef(ITfContextOwnerCompositionSink* pInterface)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromComposition(pInterface);
	return __xgeTsfStoreAddRef(&pStore->tStore);
}

static ULONG STDMETHODCALLTYPE __xgeTsfCompositionRelease(ITfContextOwnerCompositionSink* pInterface)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromComposition(pInterface);
	return __xgeTsfStoreRelease(&pStore->tStore);
}

static int __xgeTsfRangeGetExtent(ITfRange* pRange, LONG* pStart, LONG* pEnd)
{
	ITfRangeACP* pRangeAcp;
	LONG iStart;
	LONG iLength;
	HRESULT iRet;

	if ( pRange == NULL || pStart == NULL || pEnd == NULL ) return 0;
	pRangeAcp = NULL;
	iRet = pRange->lpVtbl->QueryInterface(pRange, &IID_ITfRangeACP, (void**)&pRangeAcp);
	if ( FAILED(iRet) || pRangeAcp == NULL ) return 0;
	iRet = pRangeAcp->lpVtbl->GetExtent(pRangeAcp, &iStart, &iLength);
	pRangeAcp->lpVtbl->Release(pRangeAcp);
	if ( FAILED(iRet) || iLength < 0 ) return 0;
	*pStart = iStart;
	*pEnd = iStart + iLength;
	return 1;
}

static int __xgeTsfCompositionGetExtent(ITfCompositionView* pComposition,
	LONG* pStart, LONG* pEnd)
{
	ITfRange* pRange;
	HRESULT iRet;
	int bRet;

	if ( pComposition == NULL ) return 0;
	pRange = NULL;
	iRet = pComposition->lpVtbl->GetRange(pComposition, &pRange);
	if ( FAILED(iRet) || pRange == NULL ) return 0;
	bRet = __xgeTsfRangeGetExtent(pRange, pStart, pEnd);
	pRange->lpVtbl->Release(pRange);
	return bRet;
}

static int __xgeTsfStoreCollapseSelectionForEmptyComposition(
	xge_tsf_text_store_t* pStore, int bHasViewRange,
	LONG iCompositionStart, LONG iCompositionEnd,
	LONG iOriginalStart, LONG iOriginalEnd)
{
	if ( pStore == NULL || !bHasViewRange ||
	     iCompositionStart != iCompositionEnd ||
	     iOriginalEnd <= iOriginalStart ||
	     iCompositionStart < iOriginalStart ||
	     iCompositionStart > iOriginalEnd ) return 0;
	pStore->iSelectionStart = iCompositionStart;
	pStore->iSelectionEnd = iCompositionStart;
	return 1;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionStart(ITfContextOwnerCompositionSink* pInterface, ITfCompositionView* pComposition, WINBOOL* pOk)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromComposition(pInterface);
	LONG iStart;
	LONG iEnd;
	LONG iOriginalStart;
	LONG iOriginalEnd;
	int bHasViewRange;

	if ( pOk != NULL ) *pOk = FALSE;
	bHasViewRange = __xgeTsfCompositionGetExtent(pComposition, &iStart, &iEnd);
	if ( pStore->bPendingCompositionEdit ) {
		if ( !bHasViewRange ) {
			iStart = pStore->iPendingCompositionStart;
			iEnd = pStore->iPendingCompositionEnd;
		}
	} else {
		iOriginalStart = pStore->iSelectionStart;
		iOriginalEnd = pStore->iSelectionEnd;
		if ( iOriginalStart == iOriginalEnd && bHasViewRange && iEnd > iStart ) {
			iOriginalStart = iStart;
			iOriginalEnd = iEnd;
		}
		if ( !__xgeTsfStoreCaptureOriginal(pStore, iOriginalStart, iOriginalEnd) ) return S_OK;
		__xgeTsfStoreSetReplacementRange(pStore, iOriginalStart, iOriginalEnd);
		(void)__xgeTsfStoreCollapseSelectionForEmptyComposition(
			pStore, bHasViewRange, iStart, iEnd, iOriginalStart, iOriginalEnd);
		if ( !bHasViewRange ) {
			iStart = iOriginalStart;
			iEnd = iOriginalEnd;
		}
	}
	iStart = __xgeTsfClampLong(iStart, 0, pStore->iTextLength);
	iEnd = __xgeTsfClampLong(iEnd, iStart, pStore->iTextLength);
	pStore->bComposing = 1;
	pStore->bEndPending = 0;
	pStore->bPendingCompositionEdit = 0;
	pStore->iCompositionStart = iStart;
	pStore->iCompositionEnd = iEnd;
	pStore->bHasCompositionRange = 1;
	if ( g_xgeWin32Ime.bImmFallbackActive ) {
		if ( pStore->bHasReplacementRange ) {
			g_xgeWin32Ime.bImmReplacementRange = 1;
			g_xgeWin32Ime.iImmReplacementStart = pStore->iReplacementStart;
			g_xgeWin32Ime.iImmReplacementEnd = pStore->iReplacementEnd;
		}
	} else {
		(void)__xgeImeQueuePushRange(XGE_EVENT_IME_START, "", 0, 0, 0, 0,
			pStore->bHasReplacementRange, pStore->iReplacementStart, pStore->iReplacementEnd);
	}
	if ( pStore->iLockType == 0 && pStore->bTextChanged ) {
		__xgeTsfStoreEmitUpdate(pStore);
		pStore->bTextChanged = 0;
	}
	__xgeRenderRequestInternal();
	if ( pOk != NULL ) *pOk = TRUE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionUpdate(ITfContextOwnerCompositionSink* pInterface, ITfCompositionView* pComposition, ITfRange* pNewRange)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromComposition(pInterface);
	LONG iStart;
	LONG iEnd;
	int bHasRange;

	bHasRange = __xgeTsfRangeGetExtent(pNewRange, &iStart, &iEnd);
	if ( !bHasRange ) bHasRange = __xgeTsfCompositionGetExtent(pComposition, &iStart, &iEnd);
	if ( bHasRange ) {
		iStart = __xgeTsfClampLong(iStart, 0, pStore->iTextLength);
		iEnd = __xgeTsfClampLong(iEnd, iStart, pStore->iTextLength);
		pStore->iCompositionStart = iStart;
		pStore->iCompositionEnd = iEnd;
		pStore->bHasCompositionRange = 1;
	}
	if ( pStore->iLockType == 0 ) {
		__xgeTsfStoreEmitUpdate(pStore);
		pStore->bTextChanged = 0;
		__xgeRenderRequestInternal();
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfCompositionEnd(ITfContextOwnerCompositionSink* pInterface, ITfCompositionView* pComposition)
{
	xge_tsf_text_store_t* pStore = __xgeTsfStoreFromComposition(pInterface);
	(void)pComposition;
	pStore->bEndPending = 1;
	if ( pStore->iLockType == 0 ) __xgeTsfStoreFlushEvents(pStore);
	return S_OK;
}

static void __xgeTsfStoreInitialize(xge_tsf_text_store_t* pStore)
{
	memset(pStore, 0, sizeof(*pStore));
	pStore->tStore.lpVtbl = &g_xgeTsfStoreVtbl;
	pStore->tCompositionSink.lpVtbl = &g_xgeTsfCompositionVtbl;
	pStore->iRefCount = 1;
}

static void __xgeTsfStoreUnit(xge_tsf_text_store_t* pStore)
{
	if ( pStore->pSink != NULL ) {
		pStore->pSink->lpVtbl->Release(pStore->pSink);
		pStore->pSink = NULL;
	}
	if ( pStore->sText != NULL ) {
		xrtFree(pStore->sText);
		pStore->sText = NULL;
	}
	if ( pStore->sCompositionOriginal != NULL ) {
		xrtFree(pStore->sCompositionOriginal);
		pStore->sCompositionOriginal = NULL;
	}
	pStore->iTextLength = 0;
	pStore->iTextCapacity = 0;
	pStore->iCompositionOriginalLength = 0;
	pStore->iCompositionOriginalCapacity = 0;
}

static xge_tsf_ui_element_sink_t* __xgeTsfUiSinkFromInterface(ITfUIElementSink* pInterface)
{
	return (xge_tsf_ui_element_sink_t*)((char*)pInterface - offsetof(xge_tsf_ui_element_sink_t, tSink));
}

static void __xgeTsfCandidateFreeText(void)
{
	int i;

	for ( i = 0; i < g_xgeWin32Ime.iCandidateCount; i++ ) {
		if ( g_xgeWin32Ime.pCandidateText[i] != NULL ) xrtFree(g_xgeWin32Ime.pCandidateText[i]);
		g_xgeWin32Ime.pCandidateText[i] = NULL;
	}
	g_xgeWin32Ime.iCandidateCount = 0;
}

static int __xgeTsfCandidateReserve(int iCapacity)
{
	char** pNew;
	int iNewCapacity;

	if ( iCapacity <= g_xgeWin32Ime.iCandidateCapacity ) return 1;
	iNewCapacity = (g_xgeWin32Ime.iCandidateCapacity > 0) ? g_xgeWin32Ime.iCandidateCapacity * 2 : 16;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	pNew = (char**)xrtRealloc(g_xgeWin32Ime.pCandidateText, sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return 0;
	memset(pNew + g_xgeWin32Ime.iCandidateCapacity, 0,
	       sizeof(*pNew) * (size_t)(iNewCapacity - g_xgeWin32Ime.iCandidateCapacity));
	g_xgeWin32Ime.pCandidateText = pNew;
	g_xgeWin32Ime.iCandidateCapacity = iNewCapacity;
	return 1;
}

static void __xgeTsfCandidateClose(int bEmitEvent)
{
	if ( g_xgeWin32Ime.pCandidateBehavior != NULL ) {
		g_xgeWin32Ime.pCandidateBehavior->lpVtbl->Release(g_xgeWin32Ime.pCandidateBehavior);
		g_xgeWin32Ime.pCandidateBehavior = NULL;
	}
	__xgeTsfCandidateFreeText();
	g_xgeWin32Ime.iCandidateSelection = -1;
	g_xgeWin32Ime.iCandidatePageStart = 0;
	g_xgeWin32Ime.iCandidatePageSize = 0;
	g_xgeWin32Ime.iCandidateElementId = TF_INVALID_UIELEMENTID;
	g_xgeWin32Ime.bCandidateCanSelect = 0;
	if ( g_xgeWin32Ime.bCandidateVisible && bEmitEvent ) {
		(void)__xgeImeQueuePush(XGE_EVENT_IME_CANDIDATE_END, "", 0, -1, 0, 0);
		__xgeRenderRequestInternal();
	}
	g_xgeWin32Ime.bCandidateVisible = 0;
}

static int __xgeTsfCandidateRefresh(DWORD iElementId, int iEventType)
{
	ITfUIElement* pElement;
	ITfCandidateListUIElement* pCandidate;
	ITfCandidateListUIElementBehavior* pBehavior;
	UINT iCount;
	UINT iSelection;
	UINT iCurrentPage;
	UINT iPageCount;
	UINT* pPages;
	BSTR sCandidate;
	char* sUtf8;
	int iUtf8Length;
	int i;
	HRESULT iRet;

	if ( g_xgeWin32Ime.pUiElementMgr == NULL ) return 0;
	pElement = NULL;
	iRet = g_xgeWin32Ime.pUiElementMgr->lpVtbl->GetUIElement(g_xgeWin32Ime.pUiElementMgr, iElementId, &pElement);
	if ( FAILED(iRet) || pElement == NULL ) return 0;
	pCandidate = NULL;
	iRet = pElement->lpVtbl->QueryInterface(pElement, &XGE_IID_ITfCandidateListUIElement, (void**)&pCandidate);
	if ( FAILED(iRet) || pCandidate == NULL ) {
		pElement->lpVtbl->Release(pElement);
		return 0;
	}
	iCount = 0;
	iSelection = 0;
	if ( FAILED(pCandidate->lpVtbl->GetCount(pCandidate, &iCount)) ||
	     FAILED(pCandidate->lpVtbl->GetSelection(pCandidate, &iSelection)) ||
	     !__xgeTsfCandidateReserve((int)iCount) ) {
		pCandidate->lpVtbl->Release(pCandidate);
		pElement->lpVtbl->Release(pElement);
		return 0;
	}
	__xgeTsfCandidateFreeText();
	for ( i = 0; i < (int)iCount; i++ ) {
		sCandidate = NULL;
		if ( SUCCEEDED(pCandidate->lpVtbl->GetString(pCandidate, (UINT)i, &sCandidate)) && sCandidate != NULL ) {
			sUtf8 = NULL;
			iUtf8Length = 0;
			if ( __xgeTsfWideToUtf8(sCandidate, (int)SysStringLen(sCandidate), &sUtf8, &iUtf8Length) ) {
				g_xgeWin32Ime.pCandidateText[i] = sUtf8;
			}
			SysFreeString(sCandidate);
		}
		if ( g_xgeWin32Ime.pCandidateText[i] == NULL ) {
			g_xgeWin32Ime.pCandidateText[i] = (char*)xrtMalloc(1u);
			if ( g_xgeWin32Ime.pCandidateText[i] != NULL ) g_xgeWin32Ime.pCandidateText[i][0] = '\0';
		}
	}
	g_xgeWin32Ime.iCandidateCount = (int)iCount;
	g_xgeWin32Ime.iCandidateSelection = (iSelection < iCount) ? (int)iSelection : -1;
	g_xgeWin32Ime.iCandidatePageStart = 0;
	g_xgeWin32Ime.iCandidatePageSize = (int)iCount;
	iCurrentPage = 0;
	iPageCount = 0;
	pPages = NULL;
	if ( iCount > 0 ) pPages = (UINT*)xrtMalloc(sizeof(*pPages) * ((size_t)iCount + 1u));
	if ( pPages != NULL &&
	     SUCCEEDED(pCandidate->lpVtbl->GetPageIndex(pCandidate, pPages, iCount + 1u, &iPageCount)) &&
	     SUCCEEDED(pCandidate->lpVtbl->GetCurrentPage(pCandidate, &iCurrentPage)) &&
	     iPageCount > 0 && iCurrentPage < iPageCount ) {
		UINT iPageStart = pPages[iCurrentPage];
		UINT iPageEnd = (iCurrentPage + 1u < iPageCount) ? pPages[iCurrentPage + 1u] : iCount;
		if ( iPageStart <= iPageEnd && iPageEnd <= iCount ) {
			g_xgeWin32Ime.iCandidatePageStart = (int)iPageStart;
			g_xgeWin32Ime.iCandidatePageSize = (int)(iPageEnd - iPageStart);
		}
	}
	if ( pPages != NULL ) xrtFree(pPages);
	if ( g_xgeWin32Ime.pCandidateBehavior != NULL ) {
		g_xgeWin32Ime.pCandidateBehavior->lpVtbl->Release(g_xgeWin32Ime.pCandidateBehavior);
		g_xgeWin32Ime.pCandidateBehavior = NULL;
	}
	g_xgeWin32Ime.bCandidateCanSelect = 0;
	pBehavior = NULL;
	if ( SUCCEEDED(pElement->lpVtbl->QueryInterface(pElement, &XGE_IID_ITfCandidateListUIElementBehavior, (void**)&pBehavior)) &&
	     pBehavior != NULL ) {
		g_xgeWin32Ime.pCandidateBehavior = pBehavior;
		g_xgeWin32Ime.bCandidateCanSelect = 1;
	}
	g_xgeWin32Ime.iCandidateElementId = iElementId;
	g_xgeWin32Ime.bCandidateVisible = 1;
	(void)__xgeImeQueuePush(iEventType, "", 0, g_xgeWin32Ime.iCandidateSelection,
	                        g_xgeWin32Ime.iCandidatePageStart,
	                        g_xgeWin32Ime.iCandidatePageStart + g_xgeWin32Ime.iCandidatePageSize);
	__xgeRenderRequestInternal();
	pCandidate->lpVtbl->Release(pCandidate);
	pElement->lpVtbl->Release(pElement);
	return 1;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkQueryInterface(ITfUIElementSink* pInterface, REFIID pIid, void** ppObject)
{
	xge_tsf_ui_element_sink_t* pSink;

	if ( ppObject == NULL ) return E_POINTER;
	*ppObject = NULL;
	if ( pInterface == NULL || pIid == NULL ) return E_INVALIDARG;
	pSink = __xgeTsfUiSinkFromInterface(pInterface);
	if ( !IsEqualIID(pIid, &IID_IUnknown) && !IsEqualIID(pIid, &IID_ITfUIElementSink) ) return E_NOINTERFACE;
	*ppObject = &pSink->tSink;
	InterlockedIncrement(&pSink->iRefCount);
	return S_OK;
}

static ULONG STDMETHODCALLTYPE __xgeTsfUiSinkAddRef(ITfUIElementSink* pInterface)
{
	return (ULONG)InterlockedIncrement(&__xgeTsfUiSinkFromInterface(pInterface)->iRefCount);
}

static ULONG STDMETHODCALLTYPE __xgeTsfUiSinkRelease(ITfUIElementSink* pInterface)
{
	xge_tsf_ui_element_sink_t* pSink = __xgeTsfUiSinkFromInterface(pInterface);
	LONG iCount = InterlockedDecrement(&pSink->iRefCount);
	if ( iCount < 1 ) {
		pSink->iRefCount = 1;
		return 1;
	}
	return (ULONG)iCount;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkBegin(ITfUIElementSink* pInterface, DWORD iElementId, WINBOOL* pShow)
{
	int bCandidate;
	(void)pInterface;
	if ( pShow == NULL ) return E_INVALIDARG;
	bCandidate = __xgeTsfCandidateRefresh(iElementId, XGE_EVENT_IME_CANDIDATE_START);
	*pShow = !(bCandidate && g_xgeWin32Ime.iMode == XGE_IME_MODE_FULL &&
		g_xgeWin32Ime.bCandidatePresenterReady && g_xgeWin32Ime.bCandidateCanSelect);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkUpdate(ITfUIElementSink* pInterface, DWORD iElementId)
{
	(void)pInterface;
	if ( iElementId == g_xgeWin32Ime.iCandidateElementId ) {
		(void)__xgeTsfCandidateRefresh(iElementId, XGE_EVENT_IME_CANDIDATE_UPDATE);
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE __xgeTsfUiSinkEnd(ITfUIElementSink* pInterface, DWORD iElementId)
{
	(void)pInterface;
	if ( iElementId == g_xgeWin32Ime.iCandidateElementId ) __xgeTsfCandidateClose(1);
	return S_OK;
}

static void __xgeTsfSetFocus(int bFocused)
{
	if ( !g_xgeWin32Ime.bTsfInitialized || g_xgeWin32Ime.pThreadMgr == NULL ) return;
	if ( bFocused && g_xgeWin32Ime.bEnabled && g_xgeWin32Ime.iMode != XGE_IME_MODE_NATIVE ) {
		(void)g_xgeWin32Ime.pThreadMgr->lpVtbl->SetFocus(g_xgeWin32Ime.pThreadMgr, g_xgeWin32Ime.pDocumentMgr);
	} else {
		(void)g_xgeWin32Ime.pThreadMgr->lpVtbl->SetFocus(g_xgeWin32Ime.pThreadMgr, NULL);
	}
}

static int __xgeTsfInitialize(HWND hWnd)
{
	HRESULT iRet;
	ITfThreadMgrEx* pThreadMgr;
	ITfDocumentMgr* pDocumentMgr;
	ITfContext* pContext;
	ITfKeystrokeMgr* pKeystrokeMgr;
	TfClientId iClientId;
	TfEditCookie iEditCookie;

	__xgeTsfStoreInitialize(&g_xgeWin32Ime.tTextStore);
	iRet = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if ( iRet == S_OK || iRet == S_FALSE ) g_xgeWin32Ime.bComInitialized = 1;
	pThreadMgr = NULL;
	iRet = CoCreateInstance(&CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, &IID_ITfThreadMgrEx, (void**)&pThreadMgr);
	if ( FAILED(iRet) || pThreadMgr == NULL ) return 0;
	iClientId = 0;
	iRet = pThreadMgr->lpVtbl->ActivateEx(pThreadMgr, &iClientId, 0);
	if ( FAILED(iRet) ) {
		pThreadMgr->lpVtbl->Release(pThreadMgr);
		return 0;
	}
	pDocumentMgr = NULL;
	iRet = pThreadMgr->lpVtbl->CreateDocumentMgr(pThreadMgr, &pDocumentMgr);
	if ( FAILED(iRet) || pDocumentMgr == NULL ) goto fail;
	pContext = NULL;
	iEditCookie = 0;
	iRet = pDocumentMgr->lpVtbl->CreateContext(pDocumentMgr, iClientId, 0,
	        (IUnknown*)&g_xgeWin32Ime.tTextStore.tStore, &pContext, &iEditCookie);
	if ( FAILED(iRet) || pContext == NULL ) goto fail;
	iRet = pDocumentMgr->lpVtbl->Push(pDocumentMgr, pContext);
	if ( FAILED(iRet) ) goto fail;
	pKeystrokeMgr = NULL;
	iRet = pThreadMgr->lpVtbl->QueryInterface(pThreadMgr, &IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
	if ( FAILED(iRet) || pKeystrokeMgr == NULL ) goto fail;
	g_xgeWin32Ime.pThreadMgr = pThreadMgr;
	g_xgeWin32Ime.pDocumentMgr = pDocumentMgr;
	g_xgeWin32Ime.pContext = pContext;
	g_xgeWin32Ime.pKeystrokeMgr = pKeystrokeMgr;
	g_xgeWin32Ime.iClientId = iClientId;
	g_xgeWin32Ime.iEditCookie = iEditCookie;
	g_xgeWin32Ime.bTsfInitialized = 1;
	g_xgeWin32Ime.iCandidateElementId = TF_INVALID_UIELEMENTID;
	g_xgeWin32Ime.iCandidateSelection = -1;
	g_xgeWin32Ime.tUiElementSink.tSink.lpVtbl = &g_xgeTsfUiElementSinkVtbl;
	g_xgeWin32Ime.tUiElementSink.iRefCount = 1;
	g_xgeWin32Ime.iUiElementSinkCookie = TF_INVALID_COOKIE;
	(void)pThreadMgr->lpVtbl->QueryInterface(pThreadMgr, &IID_ITfUIElementMgr,
	                                        (void**)&g_xgeWin32Ime.pUiElementMgr);
	if ( SUCCEEDED(pThreadMgr->lpVtbl->QueryInterface(pThreadMgr, &IID_ITfSource,
	                                                (void**)&g_xgeWin32Ime.pUiElementSource)) &&
	     g_xgeWin32Ime.pUiElementSource != NULL ) {
		(void)g_xgeWin32Ime.pUiElementSource->lpVtbl->AdviseSink(
			g_xgeWin32Ime.pUiElementSource, &IID_ITfUIElementSink,
			(IUnknown*)&g_xgeWin32Ime.tUiElementSink.tSink,
			&g_xgeWin32Ime.iUiElementSinkCookie);
	}
	(void)hWnd;
	return 1;

fail:
	if ( pContext != NULL ) pContext->lpVtbl->Release(pContext);
	if ( pDocumentMgr != NULL ) pDocumentMgr->lpVtbl->Release(pDocumentMgr);
	(void)pThreadMgr->lpVtbl->Deactivate(pThreadMgr);
	pThreadMgr->lpVtbl->Release(pThreadMgr);
	return 0;
}

static void __xgeTsfUnit(void)
{
	__xgeTsfCandidateClose(0);
	if ( g_xgeWin32Ime.pCandidateText != NULL ) {
		xrtFree(g_xgeWin32Ime.pCandidateText);
		g_xgeWin32Ime.pCandidateText = NULL;
	}
	g_xgeWin32Ime.iCandidateCapacity = 0;
	if ( g_xgeWin32Ime.pUiElementSource != NULL ) {
		if ( g_xgeWin32Ime.iUiElementSinkCookie != TF_INVALID_COOKIE ) {
			(void)g_xgeWin32Ime.pUiElementSource->lpVtbl->UnadviseSink(
				g_xgeWin32Ime.pUiElementSource, g_xgeWin32Ime.iUiElementSinkCookie);
		}
		g_xgeWin32Ime.pUiElementSource->lpVtbl->Release(g_xgeWin32Ime.pUiElementSource);
		g_xgeWin32Ime.pUiElementSource = NULL;
	}
	if ( g_xgeWin32Ime.pUiElementMgr != NULL ) {
		g_xgeWin32Ime.pUiElementMgr->lpVtbl->Release(g_xgeWin32Ime.pUiElementMgr);
		g_xgeWin32Ime.pUiElementMgr = NULL;
	}
	if ( g_xgeWin32Ime.pKeystrokeMgr != NULL ) {
		g_xgeWin32Ime.pKeystrokeMgr->lpVtbl->Release(g_xgeWin32Ime.pKeystrokeMgr);
		g_xgeWin32Ime.pKeystrokeMgr = NULL;
	}
	if ( g_xgeWin32Ime.pDocumentMgr != NULL ) {
		(void)g_xgeWin32Ime.pDocumentMgr->lpVtbl->Pop(g_xgeWin32Ime.pDocumentMgr, TF_POPF_ALL);
	}
	if ( g_xgeWin32Ime.pContext != NULL ) {
		g_xgeWin32Ime.pContext->lpVtbl->Release(g_xgeWin32Ime.pContext);
		g_xgeWin32Ime.pContext = NULL;
	}
	if ( g_xgeWin32Ime.pDocumentMgr != NULL ) {
		g_xgeWin32Ime.pDocumentMgr->lpVtbl->Release(g_xgeWin32Ime.pDocumentMgr);
		g_xgeWin32Ime.pDocumentMgr = NULL;
	}
	if ( g_xgeWin32Ime.pThreadMgr != NULL ) {
		(void)g_xgeWin32Ime.pThreadMgr->lpVtbl->Deactivate(g_xgeWin32Ime.pThreadMgr);
		g_xgeWin32Ime.pThreadMgr->lpVtbl->Release(g_xgeWin32Ime.pThreadMgr);
		g_xgeWin32Ime.pThreadMgr = NULL;
	}
	g_xgeWin32Ime.bTsfInitialized = 0;
	__xgeTsfStoreUnit(&g_xgeWin32Ime.tTextStore);
	if ( g_xgeWin32Ime.bComInitialized ) {
		CoUninitialize();
		g_xgeWin32Ime.bComInitialized = 0;
	}
}

static int __xgeTsfHandleKey(UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	WINBOOL bEaten;
	HRESULT iRet;

	if ( !g_xgeWin32Ime.bTsfInitialized || !g_xgeWin32Ime.bEnabled ||
	     g_xgeWin32Ime.iMode == XGE_IME_MODE_NATIVE || g_xgeWin32Ime.pKeystrokeMgr == NULL ) return 0;
	bEaten = FALSE;
	if ( iMessage == WM_KEYDOWN || iMessage == WM_SYSKEYDOWN ) {
		iRet = g_xgeWin32Ime.pKeystrokeMgr->lpVtbl->TestKeyDown(g_xgeWin32Ime.pKeystrokeMgr, wParam, lParam, &bEaten);
		if ( FAILED(iRet) || !bEaten ) return 0;
		bEaten = FALSE;
		iRet = g_xgeWin32Ime.pKeystrokeMgr->lpVtbl->KeyDown(g_xgeWin32Ime.pKeystrokeMgr, wParam, lParam, &bEaten);
	} else {
		iRet = g_xgeWin32Ime.pKeystrokeMgr->lpVtbl->TestKeyUp(g_xgeWin32Ime.pKeystrokeMgr, wParam, lParam, &bEaten);
		if ( FAILED(iRet) || !bEaten ) return 0;
		bEaten = FALSE;
		iRet = g_xgeWin32Ime.pKeystrokeMgr->lpVtbl->KeyUp(g_xgeWin32Ime.pKeystrokeMgr, wParam, lParam, &bEaten);
	}
	return SUCCEEDED(iRet) && bEaten ? 1 : 0;
}

static int __xgeImeWin32SetTextClient(const xge_ime_text_client_t* pClient)
{
	if ( g_xge.bInitialized == 0 ) return XGE_ERROR_NOT_INITIALIZED;
	memset(&g_xgeWin32Ime.tTextClient, 0, sizeof(g_xgeWin32Ime.tTextClient));
	g_xgeWin32Ime.bHasTextClient = 0;
	if ( pClient == NULL ) return XGE_OK;
	if ( pClient->iSize != 0u && pClient->iSize < sizeof(*pClient) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pClient->onSnapshot == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	g_xgeWin32Ime.tTextClient = *pClient;
	g_xgeWin32Ime.tTextClient.iSize = sizeof(g_xgeWin32Ime.tTextClient);
	g_xgeWin32Ime.bHasTextClient = 1;
	return __xgeTsfRefreshTextClient();
}

static int __xgeImeWin32SetCandidatePresenterReady(int bReady)
{
	if ( g_xge.bInitialized == 0 ) return XGE_ERROR_NOT_INITIALIZED;
	g_xgeWin32Ime.bCandidatePresenterReady = bReady ? 1 : 0;
	return g_xgeWin32Ime.bTsfInitialized ? XGE_OK : XGE_ERROR_UNSUPPORTED;
}

static int __xgeImeWin32RefreshTextClient(void)
{
	if ( g_xge.bInitialized == 0 ) return XGE_ERROR_NOT_INITIALIZED;
	if ( !g_xgeWin32Ime.bTsfInitialized ) return XGE_ERROR_UNSUPPORTED;
	return __xgeTsfRefreshTextClient();
}

static int __xgeImeWin32CandidateGetInfo(xge_ime_candidate_info_t* pInfo)
{
	if ( pInfo == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->bVisible = g_xgeWin32Ime.bCandidateVisible;
	pInfo->bCanSelect = g_xgeWin32Ime.bCandidateCanSelect;
	pInfo->iCount = g_xgeWin32Ime.iCandidateCount;
	pInfo->iSelection = g_xgeWin32Ime.iCandidateSelection;
	pInfo->iPageStart = g_xgeWin32Ime.iCandidatePageStart;
	pInfo->iPageSize = g_xgeWin32Ime.iCandidatePageSize;
	return XGE_OK;
}

static int __xgeImeWin32CandidateGetText(int iIndex, char* sText, int iCapacity)
{
	const char* sSource;
	int iLength;
	int iCopy;

	if ( iIndex < 0 || iIndex >= g_xgeWin32Ime.iCandidateCount ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL && iCapacity > 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( iCapacity < 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	sSource = g_xgeWin32Ime.pCandidateText[iIndex];
	if ( sSource == NULL ) sSource = "";
	iLength = (int)strlen(sSource);
	if ( iCapacity > 0 ) {
		iCopy = (iLength < iCapacity - 1) ? iLength : iCapacity - 1;
		if ( iCopy > 0 ) memcpy(sText, sSource, (size_t)iCopy);
		sText[iCopy] = '\0';
	}
	return iLength;
}

static int __xgeImeWin32CandidateSelect(int iIndex)
{
	HRESULT iRet;

	if ( !g_xgeWin32Ime.bCandidateVisible || !g_xgeWin32Ime.bCandidateCanSelect ||
	     g_xgeWin32Ime.pCandidateBehavior == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	if ( iIndex < 0 || iIndex >= g_xgeWin32Ime.iCandidateCount ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = g_xgeWin32Ime.pCandidateBehavior->lpVtbl->SetSelection(
		g_xgeWin32Ime.pCandidateBehavior, (UINT)iIndex);
	return SUCCEEDED(iRet) ? XGE_OK : XGE_ERROR_BACKEND_FAILED;
}

static int __xgeImeWin32CandidateFinalize(void)
{
	HRESULT iRet;

	if ( !g_xgeWin32Ime.bCandidateVisible || g_xgeWin32Ime.pCandidateBehavior == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	iRet = g_xgeWin32Ime.pCandidateBehavior->lpVtbl->Finalize(g_xgeWin32Ime.pCandidateBehavior);
	return SUCCEEDED(iRet) ? XGE_OK : XGE_ERROR_BACKEND_FAILED;
}

static int __xgeImeReadImmString(HIMC hImc, DWORD iIndex, int iCursorUtf16, char** psText, int* pCursorUtf8)
{
	WCHAR* sWide;
	LONG iBytes;
	int iWideLength;
	int iTextLength;
	int iCursor;

	if ( psText == NULL ) return 0;
	*psText = NULL;
	if ( pCursorUtf8 != NULL ) *pCursorUtf8 = 0;
	iBytes = ImmGetCompositionStringW(hImc, iIndex, NULL, 0);
	if ( iBytes <= 0 ) return 0;
	iWideLength = (int)(iBytes / (LONG)sizeof(WCHAR));
	sWide = (WCHAR*)xrtMalloc((size_t)iBytes + sizeof(WCHAR));
	if ( sWide == NULL ) return 0;
	memset(sWide, 0, (size_t)iBytes + sizeof(WCHAR));
	if ( ImmGetCompositionStringW(hImc, iIndex, sWide, (DWORD)iBytes) < 0 ||
	     !__xgeTsfWideToUtf8(sWide, iWideLength, psText, &iTextLength) ) {
		xrtFree(sWide);
		return 0;
	}
	if ( pCursorUtf8 != NULL ) {
		iCursor = iCursorUtf16;
		if ( iCursor < 0 || iCursor > iWideLength ) iCursor = iWideLength;
		*pCursorUtf8 = __xgeTsfUtf8Offset(sWide, iCursor);
	}
	xrtFree(sWide);
	return iTextLength;
}

static int __xgeTsfCompositionNeedsImmFallback(const xge_tsf_text_store_t* pStore)
{
	LONG iStart;
	LONG iEnd;

	if ( pStore == NULL || !pStore->bComposing || !pStore->bHasCompositionRange ) return 1;
	iStart = __xgeTsfClampLong(pStore->iCompositionStart, 0, pStore->iTextLength);
	iEnd = __xgeTsfClampLong(pStore->iCompositionEnd, iStart, pStore->iTextLength);
	if ( iEnd <= iStart ) return 1;
	return pStore->sCompositionOriginal != NULL &&
		pStore->iCompositionOriginalLength == iEnd - iStart &&
		memcmp(pStore->sText + iStart, pStore->sCompositionOriginal,
			sizeof(WCHAR) * (size_t)(iEnd - iStart)) == 0;
}

static void __xgeImeQueuePushImmRange(int iType, const char* sText, int iTextLength,
	int iCursor, int iSelectionStart, int iSelectionEnd)
{
	xge_tsf_text_store_t* pStore = &g_xgeWin32Ime.tTextStore;
	int bRange;
	int iStart;
	int iEnd;

	if ( pStore->bHasReplacementRange ) {
		g_xgeWin32Ime.bImmReplacementRange = 1;
		g_xgeWin32Ime.iImmReplacementStart = pStore->iReplacementStart;
		g_xgeWin32Ime.iImmReplacementEnd = pStore->iReplacementEnd;
	}
	bRange = pStore->bHasReplacementRange || g_xgeWin32Ime.bImmReplacementRange;
	iStart = pStore->bHasReplacementRange ? pStore->iReplacementStart :
		g_xgeWin32Ime.iImmReplacementStart;
	iEnd = pStore->bHasReplacementRange ? pStore->iReplacementEnd :
		g_xgeWin32Ime.iImmReplacementEnd;

	(void)__xgeImeQueuePushRange(iType, sText, iTextLength,
		iCursor, iSelectionStart, iSelectionEnd,
		bRange, iStart, iEnd);
}

static LRESULT __xgeImeHandleImmMessage(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam, int* pHandled)
{
	HIMC hImc;
	char* sText;
	LONG iCursorUtf16;
	int iCursorUtf8;
	int iTextLength;
	(void)wParam;
	*pHandled = 0;
	/* Native mode leaves the complete IMM lifecycle to DefWindowProc/Sokol.
	 * Committed text returns through the ordinary WM_CHAR input route. */
	if ( g_xgeWin32Ime.iMode == XGE_IME_MODE_NATIVE ) return 0;
	switch ( iMessage ) {
	case WM_IME_STARTCOMPOSITION:
		g_xgeWin32Ime.bImmComposing = 1;
		g_xgeWin32Ime.bImmFallbackActive = 0;
		g_xgeWin32Ime.bImmFallbackCommitted = 0;
		g_xgeWin32Ime.bImmFallbackEnded = 0;
		g_xgeWin32Ime.bImmReplacementRange = 0;
		__xgeImeWin32ApplyImmCandidateRect();
		if ( !g_xgeWin32Ime.bTsfInitialized ) {
			(void)__xgeImeQueuePush(XGE_EVENT_IME_START, "", 0, 0, 0, 0);
			*pHandled = 1;
		}
		break;
	case WM_IME_COMPOSITION:
		__xgeImeWin32ApplyImmCandidateRect();
		hImc = ImmGetContext(hWnd);
		if ( hImc != NULL ) {
			if ( (lParam & GCS_RESULTSTR) != 0 ) {
				int bUseImmResult;
				sText = NULL;
				iTextLength = __xgeImeReadImmString(hImc, GCS_RESULTSTR, -1, &sText, NULL);
				bUseImmResult = !g_xgeWin32Ime.bTsfInitialized ||
					g_xgeWin32Ime.bImmFallbackActive ||
					__xgeTsfCompositionNeedsImmFallback(&g_xgeWin32Ime.tTextStore);
				if ( iTextLength > 0 && bUseImmResult ) {
					if ( g_xgeWin32Ime.bTsfInitialized ) {
						__xgeImeQueuePushImmRange(XGE_EVENT_IME_COMMIT, sText, iTextLength,
							iTextLength, iTextLength, iTextLength);
						g_xgeWin32Ime.bImmFallbackActive = 1;
						g_xgeWin32Ime.bImmFallbackCommitted =
							g_xgeWin32Ime.tTextStore.bComposing ? 1 : 0;
					} else {
						(void)__xgeImeQueuePush(XGE_EVENT_IME_COMMIT, sText, iTextLength,
							iTextLength, iTextLength, iTextLength);
					}
					*pHandled = 1;
				}
				if ( sText != NULL ) xrtFree(sText);
			}
			if ( (lParam & GCS_COMPSTR) != 0 ) {
				if ( !g_xgeWin32Ime.bImmComposing ) {
					g_xgeWin32Ime.bImmComposing = 1;
				}
				if ( !g_xgeWin32Ime.bTsfInitialized ) {
					(void)__xgeImeQueuePush(XGE_EVENT_IME_START, "", 0, 0, 0, 0);
				}
				iCursorUtf16 = ImmGetCompositionStringW(hImc, GCS_CURSORPOS, NULL, 0);
				sText = NULL;
				iCursorUtf8 = 0;
				iTextLength = __xgeImeReadImmString(hImc, GCS_COMPSTR, (int)iCursorUtf16, &sText, &iCursorUtf8);
				if ( !g_xgeWin32Ime.bTsfInitialized ) {
					(void)__xgeImeQueuePush(XGE_EVENT_IME_UPDATE, sText, iTextLength,
						iCursorUtf8, iCursorUtf8, iCursorUtf8);
					*pHandled = 1;
				} else if ( iTextLength > 0 &&
				            __xgeTsfCompositionNeedsImmFallback(&g_xgeWin32Ime.tTextStore) ) {
					if ( !g_xgeWin32Ime.tTextStore.bComposing &&
					     !g_xgeWin32Ime.bImmFallbackActive ) {
						__xgeImeQueuePushImmRange(XGE_EVENT_IME_START, "", 0, 0, 0, 0);
					}
					__xgeImeQueuePushImmRange(XGE_EVENT_IME_UPDATE, sText, iTextLength,
						iCursorUtf8, iCursorUtf8, iCursorUtf8);
					g_xgeWin32Ime.bImmFallbackActive = 1;
					*pHandled = 1;
				}
				if ( sText != NULL ) xrtFree(sText);
			}
			(void)ImmReleaseContext(hWnd, hImc);
		}
		break;
	case WM_IME_ENDCOMPOSITION:
		if ( !g_xgeWin32Ime.bTsfInitialized && g_xgeWin32Ime.bImmComposing ) {
			(void)__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
		} else if ( g_xgeWin32Ime.bImmFallbackActive ) {
			(void)__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
			g_xgeWin32Ime.bImmFallbackEnded = 1;
		}
		g_xgeWin32Ime.bImmComposing = 0;
		g_xgeWin32Ime.bImmFallbackActive = 0;
		g_xgeWin32Ime.bImmReplacementRange = 0;
		break;
	case WM_KILLFOCUS:
		if ( !g_xgeWin32Ime.bTsfInitialized && g_xgeWin32Ime.bImmComposing ) {
			(void)__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
		}
		g_xgeWin32Ime.bImmComposing = 0;
		g_xgeWin32Ime.bImmFallbackActive = 0;
		g_xgeWin32Ime.bImmFallbackCommitted = 0;
		g_xgeWin32Ime.bImmFallbackEnded = 0;
		g_xgeWin32Ime.bImmReplacementRange = 0;
		break;
	default:
		return 0;
	}
	__xgeRenderRequestInternal();
	return 0;
}

static LRESULT CALLBACK __xgeImeWindowProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int bHandled;
	LRESULT iRet;
	IMECHARPOSITION* pCharPosition;
	RECT tDocumentRect;
	POINT tDocumentOrigin;

	if ( iMessage == WM_SETFOCUS ) {
		__xgeTsfSetFocus(1);
	}
	if ( iMessage == WM_KILLFOCUS ) {
		__xgeTsfSetFocus(0);
	}
	if ( iMessage == WM_IME_REQUEST ) {
		if ( wParam == IMR_QUERYCHARPOSITION && lParam != 0 &&
		     g_xgeWin32Ime.bHasCandidateRect &&
		     GetClientRect(hWnd, &tDocumentRect) ) {
			tDocumentOrigin.x = 0;
			tDocumentOrigin.y = 0;
			if ( ClientToScreen(hWnd, &tDocumentOrigin) ) {
				OffsetRect(&tDocumentRect,
					tDocumentOrigin.x, tDocumentOrigin.y);
				pCharPosition = (IMECHARPOSITION*)lParam;
				if ( __xgeImeFillQueryCharPosition(pCharPosition,
				     &g_xgeWin32Ime.tCandidateScreenRect,
				     &tDocumentRect) ) {
					return 1;
				}
			}
		}
	}
	bHandled = 0;
	iRet = __xgeImeHandleImmMessage(hWnd, iMessage, wParam, lParam, &bHandled);
	if ( bHandled ) return iRet;
	if ( g_xgeWin32Ime.pOriginalWindowProc != NULL ) {
		return CallWindowProcW(g_xgeWin32Ime.pOriginalWindowProc,
			hWnd, iMessage, wParam, lParam);
	}
	return DefWindowProcW(hWnd, iMessage, wParam, lParam);
}

static void __xgeImeInstallWin32(void)
{
	WNDPROC pCurrent;
	LONG_PTR iPrevious;

	if ( g_xgeWin32Ime.hWnd != NULL ) return;
	memset(&g_xgeWin32Ime, 0, sizeof(g_xgeWin32Ime));
	g_xgeWin32Ime.hWnd = (HWND)sapp_win32_get_hwnd();
	g_xgeWin32Ime.bEnabled = 0;
	g_xgeWin32Ime.iMode = g_xge.iImeMode;
	if ( g_xgeWin32Ime.hWnd == NULL ) return;
	(void)__xgeTsfInitialize(g_xgeWin32Ime.hWnd);
	pCurrent = (WNDPROC)GetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC);
	SetLastError(0);
	iPrevious = SetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC, (LONG_PTR)__xgeImeWindowProc);
	if ( iPrevious == 0 && GetLastError() != 0 ) {
		__xgeTsfUnit();
		g_xgeWin32Ime.hWnd = NULL;
		return;
	}
	g_xgeWin32Ime.pOriginalWindowProc = (WNDPROC)iPrevious;
	if ( g_xgeWin32Ime.pOriginalWindowProc == NULL ) g_xgeWin32Ime.pOriginalWindowProc = pCurrent;
	__xgeTsfSetFocus(0);
}

static void __xgeImeUninstallWin32(void)
{
	if ( g_xgeWin32Ime.hWnd != NULL && g_xgeWin32Ime.pOriginalWindowProc != NULL &&
	     (WNDPROC)GetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC) == __xgeImeWindowProc ) {
		(void)SetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC, (LONG_PTR)g_xgeWin32Ime.pOriginalWindowProc);
	}
	__xgeTsfSetFocus(0);
	__xgeTsfUnit();
	memset(&g_xgeWin32Ime, 0, sizeof(g_xgeWin32Ime));
}

static void __xgeImeWin32ApplyImmCandidateRect(void)
{
	HIMC hImc;
	COMPOSITIONFORM tComposition;
	CANDIDATEFORM tCandidate;
	RECT tRect;

	if ( g_xgeWin32Ime.hWnd == NULL || !g_xgeWin32Ime.bHasCandidateRect ) return;
	tRect = g_xgeWin32Ime.tCandidateClientRect;
	hImc = ImmGetContext(g_xgeWin32Ime.hWnd);
	if ( hImc == NULL ) return;
	memset(&tComposition, 0, sizeof(tComposition));
	tComposition.dwStyle = CFS_POINT;
	tComposition.ptCurrentPos.x = tRect.left;
	tComposition.ptCurrentPos.y = tRect.top;
	(void)ImmSetCompositionWindow(hImc, &tComposition);
	__xgeImeFillCandidateForm(&tRect, &tCandidate);
	(void)ImmSetCandidateWindow(hImc, &tCandidate);
	(void)ImmReleaseContext(g_xgeWin32Ime.hWnd, hImc);
}

static int __xgeImeWin32SetCandidateRect(xge_rect_t tRect)
{
	xge_platform_runtime_t tRuntime;
	RECT tClient;
	POINT tOrigin;
	float fScaleX;
	float fScaleY;
	LONG iLeft;
	LONG iTop;
	LONG iRight;
	LONG iBottom;

	if ( g_xgeWin32Ime.hWnd == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	memset(&tRuntime, 0, sizeof(tRuntime));
	memset(&tClient, 0, sizeof(tClient));
	fScaleX = 1.0f;
	fScaleY = 1.0f;
	if ( GetClientRect(g_xgeWin32Ime.hWnd, &tClient) && xgePlatformRuntimeGet(&tRuntime) == XGE_OK ) {
		if ( tRuntime.iFramebufferWidth > 0 ) fScaleX = (float)(tClient.right - tClient.left) / (float)tRuntime.iFramebufferWidth;
		if ( tRuntime.iFramebufferHeight > 0 ) fScaleY = (float)(tClient.bottom - tClient.top) / (float)tRuntime.iFramebufferHeight;
	}
	iLeft = (LONG)floorf(tRect.fX * fScaleX + 0.5f);
	iTop = (LONG)floorf(tRect.fY * fScaleY + 0.5f);
	iRight = (LONG)floorf((tRect.fX + ((tRect.fW > 1.0f) ? tRect.fW : 1.0f)) * fScaleX + 0.5f);
	iBottom = (LONG)floorf((tRect.fY + ((tRect.fH > 1.0f) ? tRect.fH : 1.0f)) * fScaleY + 0.5f);
	g_xgeWin32Ime.tCandidateRect = tRect;
	g_xgeWin32Ime.bHasCandidateRect = 1;
	g_xgeWin32Ime.tCandidateClientRect.left = iLeft;
	g_xgeWin32Ime.tCandidateClientRect.top = iTop;
	g_xgeWin32Ime.tCandidateClientRect.right = iRight;
	g_xgeWin32Ime.tCandidateClientRect.bottom = iBottom;
	tOrigin.x = 0;
	tOrigin.y = 0;
	(void)ClientToScreen(g_xgeWin32Ime.hWnd, &tOrigin);
	g_xgeWin32Ime.tCandidateScreenRect.left = iLeft + tOrigin.x;
	g_xgeWin32Ime.tCandidateScreenRect.top = iTop + tOrigin.y;
	g_xgeWin32Ime.tCandidateScreenRect.right = iRight + tOrigin.x;
	g_xgeWin32Ime.tCandidateScreenRect.bottom = iBottom + tOrigin.y;
	if ( g_xgeWin32Ime.tTextStore.pSink != NULL && (g_xgeWin32Ime.tTextStore.iSinkMask & TS_AS_LAYOUT_CHANGE) != 0 ) {
		(void)g_xgeWin32Ime.tTextStore.pSink->lpVtbl->OnLayoutChange(g_xgeWin32Ime.tTextStore.pSink, TS_LC_CHANGE, XGE_TSF_VIEW_COOKIE);
	}

	__xgeImeWin32ApplyImmCandidateRect();
	return XGE_OK;
}
