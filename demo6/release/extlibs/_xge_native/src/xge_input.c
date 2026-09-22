static void __xgeInputQueueItemFree(xge_input_queue_item_t* pItem)
{
	if ( pItem == NULL ) return;
	if ( pItem->sHeapText != NULL ) {
		xrtFree(pItem->sHeapText);
		pItem->sHeapText = NULL;
	}
	memset(pItem, 0, sizeof(*pItem));
}

static int __xgeInputEventQueueReserve(int iCapacity)
{
	xge_input_queue_item_t* pNew;
	int iNewCapacity;
	int i;

	if ( iCapacity <= g_xge.iInputEventQueueCapacity ) return 1;
	iNewCapacity = (g_xge.iInputEventQueueCapacity > 0) ? g_xge.iInputEventQueueCapacity * 2 : XGE_INPUT_EVENT_QUEUE_INITIAL_CAPACITY;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	pNew = (xge_input_queue_item_t*)xrtMalloc(sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return 0;
	memset(pNew, 0, sizeof(*pNew) * (size_t)iNewCapacity);
	for ( i = 0; i < g_xge.iInputEventQueueCount; i++ ) {
		int iOld = (g_xge.iInputEventQueueHead + i) % g_xge.iInputEventQueueCapacity;
		pNew[i] = g_xge.pInputEventQueue[iOld];
	}
	if ( g_xge.pInputEventQueue != NULL ) xrtFree(g_xge.pInputEventQueue);
	g_xge.pInputEventQueue = pNew;
	g_xge.iInputEventQueueHead = 0;
	g_xge.iInputEventQueueCapacity = iNewCapacity;
	return 1;
}

static int __xgeInputEventQueuePush(const xge_input_event_t* pEvent)
{
	xge_input_queue_item_t* pItem;
	const char* sText;
	int iTextSize;
	int iTail;

	if ( pEvent == NULL ) return 0;
	if ( !__xgeInputEventQueueReserve(g_xge.iInputEventQueueCount + 1) ) {
		g_xge.iInputEventDroppedCount++;
		return 0;
	}
	iTail = (g_xge.iInputEventQueueHead + g_xge.iInputEventQueueCount) % g_xge.iInputEventQueueCapacity;
	pItem = &g_xge.pInputEventQueue[iTail];
	__xgeInputQueueItemFree(pItem);
	pItem->tEvent = *pEvent;
	pItem->tEvent.iSize = sizeof(pItem->tEvent);
	pItem->tEvent.iSequence = ++g_xge.iInputEventSequence;
	if ( pItem->tEvent.fTime <= 0.0 ) pItem->tEvent.fTime = xrtTimer();
	sText = pEvent->sText;
	iTextSize = pEvent->iTextSize;
	if ( sText == NULL ) {
		sText = "";
		iTextSize = 0;
	} else if ( iTextSize < 0 ) {
		iTextSize = (int)strlen(sText);
	}
	pItem->tEvent.iTextSize = iTextSize;
	if ( iTextSize < XGE_INPUT_EVENT_INLINE_TEXT_CAPACITY ) {
		if ( iTextSize > 0 ) memcpy(pItem->sInlineText, sText, (size_t)iTextSize);
		pItem->sInlineText[iTextSize] = '\0';
		pItem->tEvent.sText = pItem->sInlineText;
	} else {
		pItem->sHeapText = (char*)xrtMalloc((size_t)iTextSize + 1u);
		if ( pItem->sHeapText == NULL ) {
			__xgeInputQueueItemFree(pItem);
			g_xge.iInputEventDroppedCount++;
			return 0;
		}
		memcpy(pItem->sHeapText, sText, (size_t)iTextSize);
		pItem->sHeapText[iTextSize] = '\0';
		pItem->tEvent.sText = pItem->sHeapText;
	}
	g_xge.iInputEventQueueCount++;
	return 1;
}

static void __xgeInputEventQueueReset(void)
{
	int i;

	for ( i = 0; i < g_xge.iInputEventQueueCount; i++ ) {
		int iIndex = (g_xge.iInputEventQueueHead + i) % g_xge.iInputEventQueueCapacity;
		__xgeInputQueueItemFree(&g_xge.pInputEventQueue[iIndex]);
	}
	g_xge.iInputEventQueueHead = 0;
	g_xge.iInputEventQueueCount = 0;
}

static void __xgeInputEventQueueUnit(void)
{
	__xgeInputEventQueueReset();
	if ( g_xge.pInputEventQueue != NULL ) {
		xrtFree(g_xge.pInputEventQueue);
		g_xge.pInputEventQueue = NULL;
	}
	if ( g_xge.sInputEventText != NULL ) {
		xrtFree(g_xge.sInputEventText);
		g_xge.sInputEventText = NULL;
	}
	g_xge.iInputEventQueueCapacity = 0;
}

static int __xgeImeQueueReserve(int iCapacity)
{
	xge_ime_queue_item_t* pNew;
	int iNewCapacity;

	if ( iCapacity <= g_xge.iImeQueueCapacity ) return 1;
	iNewCapacity = (g_xge.iImeQueueCapacity > 0) ? g_xge.iImeQueueCapacity * 2 : 16;
	while ( iNewCapacity < iCapacity ) iNewCapacity *= 2;
	pNew = (xge_ime_queue_item_t*)xrtRealloc(g_xge.pImeQueue, sizeof(*pNew) * (size_t)iNewCapacity);
	if ( pNew == NULL ) return 0;
	memset(pNew + g_xge.iImeQueueCapacity, 0, sizeof(*pNew) * (size_t)(iNewCapacity - g_xge.iImeQueueCapacity));
	g_xge.pImeQueue = pNew;
	g_xge.iImeQueueCapacity = iNewCapacity;
	return 1;
}

static int __xgeImeQueuePushRange(int iType, const char* sText, int iTextSize,
	int iCursor, int iSelectStart, int iSelectEnd,
	int bReplacementRange, int iReplacementStart, int iReplacementEnd)
{
	xge_ime_queue_item_t* pItem;
	xge_input_event_t tInput;
	char* sCopy;

	if ( sText == NULL ) sText = "";
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	memset(&tInput, 0, sizeof(tInput));
	tInput.iSize = sizeof(tInput);
	tInput.iType = iType;
	tInput.iFlags = XGE_INPUT_EVENT_FLAG_NATIVE_IME;
	tInput.sText = sText;
	tInput.iTextSize = iTextSize;
	tInput.iCursor = (iCursor >= 0) ? iCursor : iTextSize;
	tInput.iSelectStart = (iSelectStart >= 0) ? iSelectStart : tInput.iCursor;
	tInput.iSelectEnd = (iSelectEnd >= 0) ? iSelectEnd : tInput.iCursor;
	tInput.bReplacementRange = bReplacementRange != 0;
	tInput.iReplacementStart = iReplacementStart;
	tInput.iReplacementEnd = iReplacementEnd;
	if ( !__xgeInputEventQueuePush(&tInput) ) return 0;
	sCopy = (char*)xrtMalloc((size_t)iTextSize + 1u);
	if ( sCopy == NULL ) return 0;
	if ( iTextSize > 0 ) memcpy(sCopy, sText, (size_t)iTextSize);
	sCopy[iTextSize] = '\0';
	if ( !__xgeImeQueueReserve(g_xge.iImeQueueCount + 1) ) {
		xrtFree(sCopy);
		return 0;
	}
	pItem = &g_xge.pImeQueue[g_xge.iImeQueueCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = iType;
	pItem->iTextSize = iTextSize;
	pItem->iCursor = (iCursor >= 0) ? iCursor : iTextSize;
	pItem->iSelectStart = (iSelectStart >= 0) ? iSelectStart : pItem->iCursor;
	pItem->iSelectEnd = (iSelectEnd >= 0) ? iSelectEnd : pItem->iCursor;
	pItem->bReplacementRange = bReplacementRange != 0;
	pItem->iReplacementStart = iReplacementStart;
	pItem->iReplacementEnd = iReplacementEnd;
	pItem->sText = sCopy;
	return 1;
}

static int __xgeImeQueuePush(int iType, const char* sText, int iTextSize,
	int iCursor, int iSelectStart, int iSelectEnd)
{
	return __xgeImeQueuePushRange(iType, sText, iTextSize,
		iCursor, iSelectStart, iSelectEnd, 0, 0, 0);
}


#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include "xge_ime_win32_imm.c"
	#else
		#include "xge_ime_win32_tsf.c"
	#endif
#endif

int xgeKeyDown(int iKey)
{
	if ( (iKey < 0) || (iKey >= XGE_KEY_COUNT) ) {
		return 0;
	}
	if ( g_xge.arrKeyConsumed[iKey] ) {
		return 0;
	}
	return g_xge.arrKeyDown[iKey] ? 1 : 0;
}

int xgeKeyPressed(int iKey)
{
	if ( (iKey < 0) || (iKey >= XGE_KEY_COUNT) ) {
		return 0;
	}
	if ( g_xge.arrKeyConsumed[iKey] ) {
		return 0;
	}
	return g_xge.arrKeyPressed[iKey] ? 1 : 0;
}

int xgeKeyRepeated(int iKey)
{
	if ( (iKey < 0) || (iKey >= XGE_KEY_COUNT) ) {
		return 0;
	}
	if ( g_xge.arrKeyConsumed[iKey] ) {
		return 0;
	}
	return g_xge.arrKeyRepeated[iKey] ? 1 : 0;
}

int xgeKeyReleased(int iKey)
{
	if ( (iKey < 0) || (iKey >= XGE_KEY_COUNT) ) {
		return 0;
	}
	if ( g_xge.arrKeyConsumed[iKey] ) {
		return 0;
	}
	return g_xge.arrKeyReleased[iKey] ? 1 : 0;
}

void xgeInputConsumeKey(int iKey)
{
	if ( (iKey < 0) || (iKey >= XGE_KEY_COUNT) ) {
		return;
	}
	g_xge.arrKeyConsumed[iKey] = 1;
}

int xgeInputKeyConsumed(int iKey)
{
	if ( (iKey < 0) || (iKey >= XGE_KEY_COUNT) ) {
		return 0;
	}
	return g_xge.arrKeyConsumed[iKey] ? 1 : 0;
}

void xgeMouseGet(float* pX, float* pY)
{
	if ( pX != NULL ) {
		*pX = g_xge.fMouseX;
	}
	if ( pY != NULL ) {
		*pY = g_xge.fMouseY;
	}
}

void xgeMouseGetDelta(float* pDX, float* pDY)
{
	if ( pDX != NULL ) {
		*pDX = g_xge.fMouseDX;
	}
	if ( pDY != NULL ) {
		*pDY = g_xge.fMouseDY;
	}
}

void xgeMouseGetWheel(float* pX, float* pY)
{
	if ( pX != NULL ) {
		*pX = g_xge.fMouseWheelX;
	}
	if ( pY != NULL ) {
		*pY = g_xge.fMouseWheelY;
	}
}

int xgeMouseDown(int iButton)
{
	return (g_xge.iMouseButtons & (unsigned int)iButton) ? 1 : 0;
}

uint32_t xgeTextGet(void)
{
	return __xgeTextPop();
}

static void __xgeInputDiscardLegacyIme(int iType)
{
	xge_ime_queue_item_t* pItem;

	if ( g_xge.iImeQueueCount <= 0 ) return;
	pItem = &g_xge.pImeQueue[0];
	if ( pItem->iType != iType ) return;
	if ( pItem->sText != NULL ) xrtFree(pItem->sText);
	g_xge.iImeQueueCount--;
	if ( g_xge.iImeQueueCount > 0 ) {
		memmove(g_xge.pImeQueue, g_xge.pImeQueue + 1, sizeof(*g_xge.pImeQueue) * (size_t)g_xge.iImeQueueCount);
	}
	memset(&g_xge.pImeQueue[g_xge.iImeQueueCount], 0, sizeof(*g_xge.pImeQueue));
}

int xgeInputEventGet(xge_input_event_t* pEvent)
{
	xge_input_queue_item_t* pItem;
	const char* sText;
	int iTextSize;
	int iType;
	uint32_t iCodepoint;

	if ( pEvent == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( g_xge.sInputEventText != NULL ) {
		xrtFree(g_xge.sInputEventText);
		g_xge.sInputEventText = NULL;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->iSize = sizeof(*pEvent);
	pEvent->sText = "";
	if ( g_xge.iInputEventQueueCount <= 0 ) return 0;
	pItem = &g_xge.pInputEventQueue[g_xge.iInputEventQueueHead];
	*pEvent = pItem->tEvent;
	iType = pEvent->iType;
	iCodepoint = pEvent->iCodepoint;
	iTextSize = pItem->tEvent.iTextSize;
	sText = pItem->tEvent.sText;
	if ( pItem->sHeapText != NULL ) {
		g_xge.sInputEventText = pItem->sHeapText;
		pItem->sHeapText = NULL;
		pEvent->sText = g_xge.sInputEventText;
	} else if ( (sText != NULL) && (iTextSize > 0) ) {
		memcpy(g_xge.sInputEventInlineText, sText, (size_t)iTextSize);
		g_xge.sInputEventInlineText[iTextSize] = '\0';
		pEvent->sText = g_xge.sInputEventInlineText;
	} else {
		g_xge.sInputEventInlineText[0] = '\0';
		pEvent->sText = g_xge.sInputEventInlineText;
	}
	memset(pItem, 0, sizeof(*pItem));
	g_xge.iInputEventQueueHead = (g_xge.iInputEventQueueHead + 1) % g_xge.iInputEventQueueCapacity;
	g_xge.iInputEventQueueCount--;
	if ( (iType == XGE_EVENT_TEXT) && (g_xge.iTextQueueCount > 0) &&
	     (g_xge.arrTextQueue[g_xge.iTextQueueHead] == iCodepoint) ) {
		(void)__xgeTextPop();
	} else if ( (iType == XGE_EVENT_IME_START) || (iType == XGE_EVENT_IME_UPDATE) ||
	            (iType == XGE_EVENT_IME_COMMIT) || (iType == XGE_EVENT_IME_END) ||
	            (iType == XGE_EVENT_IME_CANDIDATE_START) ||
	            (iType == XGE_EVENT_IME_CANDIDATE_UPDATE) ||
	            (iType == XGE_EVENT_IME_CANDIDATE_END) ) {
		__xgeInputDiscardLegacyIme(iType);
	}
	return 1;
}

int xgeInputEventPost(const xge_input_event_t* pEvent)
{
	xge_input_event_t tEvent;

	if ( pEvent == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !g_xge.bInitialized ) return XGE_ERROR_NOT_INITIALIZED;
	if ( (pEvent->iSize != 0u) && (pEvent->iSize < sizeof(*pEvent)) ) return XGE_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XGE_EVENT_KEY_DOWN:
	case XGE_EVENT_KEY_UP:
	case XGE_EVENT_TEXT:
	case XGE_EVENT_MOUSE_DOWN:
	case XGE_EVENT_MOUSE_UP:
	case XGE_EVENT_MOUSE_MOVE:
	case XGE_EVENT_MOUSE_WHEEL:
	case XGE_EVENT_TOUCH_BEGIN:
	case XGE_EVENT_TOUCH_MOVE:
	case XGE_EVENT_TOUCH_END:
	case XGE_EVENT_TOUCH_CANCEL:
	case XGE_EVENT_IME_START:
	case XGE_EVENT_IME_UPDATE:
	case XGE_EVENT_IME_COMMIT:
	case XGE_EVENT_IME_END:
	case XGE_EVENT_IME_CANDIDATE_START:
	case XGE_EVENT_IME_CANDIDATE_UPDATE:
	case XGE_EVENT_IME_CANDIDATE_END:
		break;
	default:
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tEvent = *pEvent;
	tEvent.iSize = sizeof(tEvent);
	tEvent.iFlags |= XGE_INPUT_EVENT_FLAG_SYNTHETIC;
	return __xgeInputEventQueuePush(&tEvent) ? XGE_OK : XGE_ERROR_OUT_OF_MEMORY;
}

int xgeInputEventPendingCount(void)
{
	return g_xge.iInputEventQueueCount;
}

uint64_t xgeInputEventDroppedCount(void)
{
	return g_xge.iInputEventDroppedCount;
}

int xgeImeEventGet(xge_ime_event_t* pEvent)
{
	xge_ime_queue_item_t* pItem;

	if ( pEvent == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( g_xge.sImeEventText != NULL ) {
		xrtFree(g_xge.sImeEventText);
		g_xge.sImeEventText = NULL;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->iSize = sizeof(*pEvent);
	pEvent->sText = "";
	if ( g_xge.iImeQueueCount <= 0 ) {
		return 0;
	}
	pItem = &g_xge.pImeQueue[0];
	g_xge.sImeEventText = pItem->sText;
	pEvent->iType = pItem->iType;
	pEvent->sText = (g_xge.sImeEventText != NULL) ? g_xge.sImeEventText : "";
	pEvent->iTextSize = pItem->iTextSize;
	pEvent->iCursor = pItem->iCursor;
	pEvent->iSelectStart = pItem->iSelectStart;
	pEvent->iSelectEnd = pItem->iSelectEnd;
	pEvent->bReplacementRange = pItem->bReplacementRange;
	pEvent->iReplacementStart = pItem->iReplacementStart;
	pEvent->iReplacementEnd = pItem->iReplacementEnd;
	g_xge.iImeQueueCount--;
	if ( g_xge.iImeQueueCount > 0 ) {
		memmove(g_xge.pImeQueue, g_xge.pImeQueue + 1, sizeof(*g_xge.pImeQueue) * (size_t)g_xge.iImeQueueCount);
	}
	memset(&g_xge.pImeQueue[g_xge.iImeQueueCount], 0, sizeof(*g_xge.pImeQueue));
	return 1;
}

int xgeImeGetEnabled(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_xgeWin32Ime.bEnabled ? 1 : 0;
#else
	return 1;
#endif
}

int xgeImeSetEnabled(int bEnabled)
{
#if defined(_WIN32) || defined(_WIN64)
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	g_xgeWin32Ime.bEnabled = bEnabled ? 1 : 0;
	__xgeTsfSetFocus(g_xgeWin32Ime.bEnabled && GetFocus() == g_xgeWin32Ime.hWnd);
	return XGE_OK;
#else
	(void)bEnabled;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeGetMode(void)
{
	return g_xge.iImeMode;
}

int xgeImeSetMode(int iMode)
{
	if ( iMode < XGE_IME_MODE_NATIVE || iMode > XGE_IME_MODE_FULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( g_xge.bInitialized == 0 ) return XGE_ERROR_NOT_INITIALIZED;
	g_xge.iImeMode = iMode;
	if ( !g_xge.bSokolRunning ) return XGE_OK;
	g_xgeWin32Ime.iMode = iMode;
	__xgeTsfSetFocus(g_xgeWin32Ime.bEnabled && GetFocus() == g_xgeWin32Ime.hWnd);
	#if defined(__TINYC__)
		return (iMode != XGE_IME_MODE_FULL) ? XGE_OK : XGE_ERROR_UNSUPPORTED;
	#else
		return (iMode == XGE_IME_MODE_NATIVE || g_xgeWin32Ime.bTsfInitialized) ? XGE_OK : XGE_ERROR_UNSUPPORTED;
	#endif
#else
	g_xge.iImeMode = iMode;
	return (iMode == XGE_IME_MODE_NATIVE) ? XGE_OK : XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeSetCandidatePresenterReady(int bReady)
{
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32SetCandidatePresenterReady(bReady);
#else
	(void)bReady;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeSetCandidateRect(xge_rect_t tRect)
{
	if ( tRect.fW < 0.0f || tRect.fH < 0.0f ) return XGE_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32SetCandidateRect(tRect);
#else
	(void)tRect;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeGetCandidateRect(xge_rect_t* pRect)
{
	if ( pRect == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32) || defined(_WIN64)
	if ( !g_xgeWin32Ime.bHasCandidateRect ) return XGE_ERROR_FILE_NOT_FOUND;
	*pRect = g_xgeWin32Ime.tCandidateRect;
	return XGE_OK;
#else
	memset(pRect, 0, sizeof(*pRect));
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeSetTextClient(const xge_ime_text_client_t* pClient)
{
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32SetTextClient(pClient);
#else
	(void)pClient;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeRefreshTextClient(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32RefreshTextClient();
#else
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeCandidateGetInfo(xge_ime_candidate_info_t* pInfo)
{
	if ( pInfo == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32CandidateGetInfo(pInfo);
#else
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeCandidateGetText(int iIndex, char* sText, int iCapacity)
{
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32CandidateGetText(iIndex, sText, iCapacity);
#else
	(void)iIndex; (void)sText; (void)iCapacity;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeCandidateSelect(int iIndex)
{
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32CandidateSelect(iIndex);
#else
	(void)iIndex;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeImeCandidateFinalize(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return __xgeImeWin32CandidateFinalize();
#else
	return XGE_ERROR_UNSUPPORTED;
#endif
}

int xgeTouchGetCount(void)
{
	return g_xge.iTouchCount;
}

int xgeTouchGet(int iIndex, xge_touch_point_t* pPoint)
{
	if ( (iIndex < 0) || (iIndex >= g_xge.iTouchCount) || (pPoint == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pPoint = g_xge.arrTouches[iIndex];
	return XGE_OK;
}

int xgeTouchFind(uint64_t iId, xge_touch_point_t* pPoint)
{
	int iIndex;

	if ( pPoint == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeTouchFindIndex(iId);
	if ( iIndex < 0 ) {
		return XGE_ERROR_FILE_NOT_FOUND;
	}
	*pPoint = g_xge.arrTouches[iIndex];
	return XGE_OK;
}

int xgeGamepadConnected(int iGamepad)
{
	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) ) {
		return 0;
	}
	return g_xge.arrGamepads[iGamepad].bConnected ? 1 : 0;
}

int xgeGamepadGetState(int iGamepad, xge_gamepad_state_t* pState)
{
	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) || (pState == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pState = g_xge.arrGamepads[iGamepad];
	return XGE_OK;
}

int xgeGamepadButtonDown(int iGamepad, uint32_t iButton)
{
	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) ) {
		return 0;
	}
	return (g_xge.arrGamepads[iGamepad].iButtons & iButton) ? 1 : 0;
}

int xgeGamepadButtonPressed(int iGamepad, uint32_t iButton)
{
	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) ) {
		return 0;
	}
	return (g_xge.arrGamepads[iGamepad].iButtonsPressed & iButton) ? 1 : 0;
}

int xgeGamepadButtonReleased(int iGamepad, uint32_t iButton)
{
	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) ) {
		return 0;
	}
	return (g_xge.arrGamepads[iGamepad].iButtonsReleased & iButton) ? 1 : 0;
}

float xgeGamepadAxis(int iGamepad, int iAxis)
{
	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) || (iAxis < 0) || (iAxis >= XGE_GAMEPAD_AXIS_COUNT) ) {
		return 0.0f;
	}
	return g_xge.arrGamepads[iGamepad].arrAxes[iAxis];
}

int xgeGamepadSetConnected(int iGamepad, int bConnected)
{
	xge_event_t tEvent;
	int bWasConnected;

	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	bConnected = bConnected ? 1 : 0;
	bWasConnected = g_xge.arrGamepads[iGamepad].bConnected ? 1 : 0;
	if ( bWasConnected == bConnected ) {
		return XGE_OK;
	}
	g_xge.tPlatformRuntime.iGamepadEventCount++;
	memset(&g_xge.arrGamepads[iGamepad], 0, sizeof(g_xge.arrGamepads[iGamepad]));
	g_xge.arrGamepads[iGamepad].bConnected = bConnected;
	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iType = bConnected ? XGE_EVENT_GAMEPAD_CONNECTED : XGE_EVENT_GAMEPAD_DISCONNECTED;
	tEvent.iParam1 = iGamepad;
	tEvent.pData = &g_xge.arrGamepads[iGamepad];
	xgeSceneDispatchEvent(&tEvent);
	return XGE_OK;
}

int xgeGamepadSetState(int iGamepad, const xge_gamepad_state_t* pState)
{
	xge_gamepad_state_t* pDst;
	uint32_t iOldButtons;
	uint32_t iNewButtons;

	if ( (iGamepad < 0) || (iGamepad >= XGE_GAMEPAD_MAX) || (pState == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pState->bConnected == 0 ) {
		return xgeGamepadSetConnected(iGamepad, 0);
	}
	if ( g_xge.arrGamepads[iGamepad].bConnected == 0 ) {
		xgeGamepadSetConnected(iGamepad, 1);
	}
	pDst = &g_xge.arrGamepads[iGamepad];
	iOldButtons = pDst->iButtons;
	iNewButtons = pState->iButtons;
	pDst->bConnected = 1;
	g_xge.tPlatformRuntime.iGamepadEventCount++;
	pDst->iButtons = iNewButtons;
	pDst->iButtonsPressed |= (iNewButtons & ~iOldButtons);
	pDst->iButtonsReleased |= (iOldButtons & ~iNewButtons);
	memcpy(pDst->arrAxes, pState->arrAxes, sizeof(pDst->arrAxes));
	return XGE_OK;
}
