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

static int __xgeImeQueuePush(int iType, const char* sText, int iTextSize, int iCursor, int iSelectStart, int iSelectEnd)
{
	xge_ime_queue_item_t* pItem;
	char* sCopy;

	if ( sText == NULL ) sText = "";
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
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
	pItem->sText = sCopy;
	return 1;
}

#if defined(_WIN32) || defined(_WIN64)
#include <imm.h>

typedef BOOL (WINAPI *xge_imm_get_open_status_proc)(HIMC);
typedef BOOL (WINAPI *xge_imm_set_open_status_proc)(HIMC, BOOL);
typedef HIMC (WINAPI *xge_imm_get_context_proc)(HWND);
typedef BOOL (WINAPI *xge_imm_release_context_proc)(HWND, HIMC);
typedef LONG (WINAPI *xge_imm_get_composition_string_w_proc)(HIMC, DWORD, LPVOID, DWORD);

static HMODULE g_xgeImm32 = NULL;
static xge_imm_get_open_status_proc g_xgeImmGetOpenStatus = NULL;
static xge_imm_set_open_status_proc g_xgeImmSetOpenStatus = NULL;
static xge_imm_get_context_proc g_xgeImmGetContext = NULL;
static xge_imm_release_context_proc g_xgeImmReleaseContext = NULL;
static xge_imm_get_composition_string_w_proc g_xgeImmGetCompositionStringW = NULL;
static HWND g_xgeImeWindow = NULL;
static WNDPROC g_xgeImeOriginalWindowProc = NULL;
static int g_xgeImeComposing = 0;
static WCHAR* g_xgeImeSuppressedResult = NULL;
static int g_xgeImeSuppressedCount = 0;
static int g_xgeImeSuppressedIndex = 0;

static int __xgeImeEnsureWin32(void)
{
	if ( g_xgeImm32 != NULL ) {
		return (g_xgeImmGetOpenStatus != NULL) &&
		       (g_xgeImmSetOpenStatus != NULL) &&
		       (g_xgeImmGetContext != NULL) &&
		       (g_xgeImmReleaseContext != NULL) &&
		       (g_xgeImmGetCompositionStringW != NULL);
	}
	g_xgeImm32 = LoadLibraryA("imm32.dll");
	if ( g_xgeImm32 == NULL ) {
		return 0;
	}
	g_xgeImmGetOpenStatus = (xge_imm_get_open_status_proc)GetProcAddress(g_xgeImm32, "ImmGetOpenStatus");
	g_xgeImmSetOpenStatus = (xge_imm_set_open_status_proc)GetProcAddress(g_xgeImm32, "ImmSetOpenStatus");
	g_xgeImmGetContext = (xge_imm_get_context_proc)GetProcAddress(g_xgeImm32, "ImmGetContext");
	g_xgeImmReleaseContext = (xge_imm_release_context_proc)GetProcAddress(g_xgeImm32, "ImmReleaseContext");
	g_xgeImmGetCompositionStringW = (xge_imm_get_composition_string_w_proc)GetProcAddress(g_xgeImm32, "ImmGetCompositionStringW");
	return (g_xgeImmGetOpenStatus != NULL) &&
	       (g_xgeImmSetOpenStatus != NULL) &&
	       (g_xgeImmGetContext != NULL) &&
	       (g_xgeImmReleaseContext != NULL) &&
	       (g_xgeImmGetCompositionStringW != NULL);
}

static int __xgeImeHimcGet(HWND hWnd, HIMC* pHimc)
{
	if ( (pHimc == NULL) || (__xgeImeEnsureWin32() == 0) || (hWnd == NULL) ) {
		return 0;
	}
	*pHimc = g_xgeImmGetContext(hWnd);
	return (*pHimc != NULL) ? 1 : 0;
}

static HWND __xgeImeWindowGet(void)
{
	HWND hWnd;

	hWnd = (HWND)sapp_win32_get_hwnd();
	if ( hWnd == NULL ) {
		hWnd = GetFocus();
	}
	if ( hWnd == NULL ) {
		hWnd = GetActiveWindow();
	}
	if ( hWnd == NULL ) {
		hWnd = GetForegroundWindow();
	}
	return hWnd;
}

static int __xgeImeReadString(HIMC hImc, DWORD iIndex, int iCursorUtf16, char** psText, int* pCursorUtf8)
{
	WCHAR* sWide;
	char* sText;
	LONG iBytes;
	int iWideCount;
	int iTextSize;
	int iCursor;

	if ( psText == NULL ) return 0;
	*psText = NULL;
	if ( pCursorUtf8 != NULL ) *pCursorUtf8 = 0;
	if ( hImc == NULL ) return 0;
	iBytes = g_xgeImmGetCompositionStringW(hImc, iIndex, NULL, 0);
	if ( iBytes <= 0 ) return 0;
	sWide = (WCHAR*)xrtMalloc((size_t)iBytes + sizeof(WCHAR));
	if ( sWide == NULL ) return 0;
	memset(sWide, 0, (size_t)iBytes + sizeof(WCHAR));
	if ( g_xgeImmGetCompositionStringW(hImc, iIndex, sWide, (DWORD)iBytes) < 0 ) {
		xrtFree(sWide);
		return 0;
	}
	iWideCount = (int)(iBytes / (LONG)sizeof(WCHAR));
	iTextSize = WideCharToMultiByte(CP_UTF8, 0, sWide, iWideCount, NULL, 0, NULL, NULL);
	if ( iTextSize <= 0 ) {
		xrtFree(sWide);
		return 0;
	}
	sText = (char*)xrtMalloc((size_t)iTextSize + 1u);
	if ( sText == NULL ) {
		xrtFree(sWide);
		return 0;
	}
	if ( WideCharToMultiByte(CP_UTF8, 0, sWide, iWideCount, sText, iTextSize, NULL, NULL) != iTextSize ) {
		xrtFree(sText);
		xrtFree(sWide);
		return 0;
	}
	sText[iTextSize] = '\0';
	if ( pCursorUtf8 != NULL ) {
		iCursor = iCursorUtf16;
		if ( iCursor < 0 ) iCursor = iWideCount;
		if ( iCursor > iWideCount ) iCursor = iWideCount;
		*pCursorUtf8 = (iCursor > 0) ? WideCharToMultiByte(CP_UTF8, 0, sWide, iCursor, NULL, 0, NULL, NULL) : 0;
	}
	xrtFree(sWide);
	*psText = sText;
	return iTextSize;
}

static void __xgeImeSuppressResultChars(const char* sText, int iTextSize)
{
	int iCount;

	if ( g_xgeImeSuppressedResult != NULL ) {
		xrtFree(g_xgeImeSuppressedResult);
		g_xgeImeSuppressedResult = NULL;
	}
	g_xgeImeSuppressedCount = 0;
	g_xgeImeSuppressedIndex = 0;
	if ( (sText == NULL) || (iTextSize <= 0) ) return;
	iCount = MultiByteToWideChar(CP_UTF8, 0, sText, iTextSize, NULL, 0);
	if ( iCount <= 0 ) return;
	g_xgeImeSuppressedResult = (WCHAR*)xrtMalloc(sizeof(WCHAR) * (size_t)iCount);
	if ( g_xgeImeSuppressedResult == NULL ) return;
	if ( MultiByteToWideChar(CP_UTF8, 0, sText, iTextSize, g_xgeImeSuppressedResult, iCount) != iCount ) {
		xrtFree(g_xgeImeSuppressedResult);
		g_xgeImeSuppressedResult = NULL;
		return;
	}
	g_xgeImeSuppressedCount = iCount;
}

static void __xgeImeEndComposition(void)
{
	if ( g_xgeImeComposing ) {
		__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
	}
	g_xgeImeComposing = 0;
	__xgeRenderRequestInternal();
}

static LRESULT CALLBACK __xgeImeWindowProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	HIMC hImc;
	char* sText;
	LONG iCursorUtf16;
	int iCursorUtf8;
	int iTextSize;

	switch ( iMessage ) {
		case WM_IME_STARTCOMPOSITION:
			if ( !g_xgeImeComposing ) {
				g_xgeImeComposing = 1;
				__xgeImeQueuePush(XGE_EVENT_IME_START, "", 0, 0, 0, 0);
			}
			__xgeRenderRequestInternal();
			break;

		case WM_IME_COMPOSITION:
			if ( __xgeImeHimcGet(hWnd, &hImc) ) {
				if ( (lParam & GCS_RESULTSTR) != 0 ) {
					sText = NULL;
					iTextSize = __xgeImeReadString(hImc, GCS_RESULTSTR, -1, &sText, NULL);
					if ( iTextSize > 0 ) {
						__xgeImeQueuePush(XGE_EVENT_IME_COMMIT, sText, iTextSize, iTextSize, iTextSize, iTextSize);
						__xgeImeSuppressResultChars(sText, iTextSize);
					}
					if ( sText != NULL ) xrtFree(sText);
				}
				if ( (lParam & GCS_COMPSTR) != 0 ) {
					if ( !g_xgeImeComposing ) {
						g_xgeImeComposing = 1;
						__xgeImeQueuePush(XGE_EVENT_IME_START, "", 0, 0, 0, 0);
					}
					iCursorUtf16 = g_xgeImmGetCompositionStringW(hImc, GCS_CURSORPOS, NULL, 0);
					iCursorUtf8 = 0;
					sText = NULL;
					iTextSize = __xgeImeReadString(hImc, GCS_COMPSTR, (int)iCursorUtf16, &sText, &iCursorUtf8);
					__xgeImeQueuePush(XGE_EVENT_IME_UPDATE, sText, iTextSize, iCursorUtf8, iCursorUtf8, iCursorUtf8);
					if ( sText != NULL ) xrtFree(sText);
				}
				(void)g_xgeImmReleaseContext(hWnd, hImc);
			}
			__xgeRenderRequestInternal();
			break;

		case WM_IME_ENDCOMPOSITION:
			__xgeImeEndComposition();
			break;

		case WM_KILLFOCUS:
			__xgeImeEndComposition();
			if ( g_xgeImeSuppressedResult != NULL ) {
				xrtFree(g_xgeImeSuppressedResult);
				g_xgeImeSuppressedResult = NULL;
			}
			g_xgeImeSuppressedCount = 0;
			g_xgeImeSuppressedIndex = 0;
			break;

		case WM_CHAR:
			if ( wParam >= 0x20u ) {
				if ( g_xgeImeSuppressedIndex < g_xgeImeSuppressedCount ) {
					if ( (WCHAR)wParam == g_xgeImeSuppressedResult[g_xgeImeSuppressedIndex] ) {
						g_xgeImeSuppressedIndex++;
						if ( g_xgeImeSuppressedIndex >= g_xgeImeSuppressedCount ) {
							xrtFree(g_xgeImeSuppressedResult);
							g_xgeImeSuppressedResult = NULL;
							g_xgeImeSuppressedCount = 0;
							g_xgeImeSuppressedIndex = 0;
						}
						return 0;
					}
					g_xgeImeSuppressedCount = 0;
					g_xgeImeSuppressedIndex = 0;
					xrtFree(g_xgeImeSuppressedResult);
					g_xgeImeSuppressedResult = NULL;
				}
				if ( g_xgeImeComposing ) {
					return 0;
				}
			}
			break;

		default:
			break;
	}
	if ( g_xgeImeOriginalWindowProc != NULL ) {
		return CallWindowProcW(g_xgeImeOriginalWindowProc, hWnd, iMessage, wParam, lParam);
	}
	return DefWindowProcW(hWnd, iMessage, wParam, lParam);
}

static void __xgeImeInstallWin32(void)
{
	WNDPROC pCurrent;
	LONG_PTR iPrevious;

	if ( g_xgeImeWindow != NULL ) {
		return;
	}
	g_xgeImeWindow = (HWND)sapp_win32_get_hwnd();
	if ( g_xgeImeWindow == NULL ) {
		return;
	}
	pCurrent = (WNDPROC)GetWindowLongPtrW(g_xgeImeWindow, GWLP_WNDPROC);
	if ( pCurrent == __xgeImeWindowProc ) {
		return;
	}
	SetLastError(0);
	iPrevious = SetWindowLongPtrW(g_xgeImeWindow, GWLP_WNDPROC, (LONG_PTR)__xgeImeWindowProc);
	if ( (iPrevious == 0) && (GetLastError() != 0) ) {
		g_xgeImeWindow = NULL;
		return;
	}
	g_xgeImeOriginalWindowProc = (WNDPROC)iPrevious;
	if ( g_xgeImeOriginalWindowProc == NULL ) {
		g_xgeImeOriginalWindowProc = pCurrent;
	}
}

static void __xgeImeUninstallWin32(void)
{
	if ( g_xgeImeWindow != NULL ) {
		if ( ((WNDPROC)GetWindowLongPtrW(g_xgeImeWindow, GWLP_WNDPROC) == __xgeImeWindowProc) && (g_xgeImeOriginalWindowProc != NULL) ) {
			(void)SetWindowLongPtrW(g_xgeImeWindow, GWLP_WNDPROC, (LONG_PTR)g_xgeImeOriginalWindowProc);
		}
	}
	g_xgeImeWindow = NULL;
	g_xgeImeOriginalWindowProc = NULL;
	g_xgeImeComposing = 0;
	if ( g_xgeImeSuppressedResult != NULL ) {
		xrtFree(g_xgeImeSuppressedResult);
		g_xgeImeSuppressedResult = NULL;
	}
	g_xgeImeSuppressedCount = 0;
	g_xgeImeSuppressedIndex = 0;
}
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
	HWND hWnd;
	HIMC hImc;
	int bEnabled;

	if ( g_xge.bInitialized == 0 ) {
		return 1;
	}
	hWnd = __xgeImeWindowGet();
	if ( __xgeImeHimcGet(hWnd, &hImc) == 0 ) {
		return 1;
	}
	bEnabled = g_xgeImmGetOpenStatus(hImc) ? 1 : 0;
	(void)g_xgeImmReleaseContext(hWnd, hImc);
	return bEnabled;
#else
	return 1;
#endif
}

int xgeImeSetEnabled(int bEnabled)
{
#if defined(_WIN32) || defined(_WIN64)
	HWND hWnd;
	HIMC hImc;

	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	hWnd = __xgeImeWindowGet();
	if ( __xgeImeHimcGet(hWnd, &hImc) == 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	(void)g_xgeImmSetOpenStatus(hImc, bEnabled ? TRUE : FALSE);
	(void)g_xgeImmReleaseContext(hWnd, hImc);
	return XGE_OK;
#else
	(void)bEnabled;
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
