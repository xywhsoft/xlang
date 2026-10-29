/*
 * Windows IMM32 输入法后端。
 *
 * TCC 不附带完整 TSF SDK 头文件，因此使用系统自带的 IMM32 接口承载
 * 组合文本、提交文本和候选列表。完整 TSF 后端仍由非 TCC 编译器使用。
 */
#include <imm.h>



/* TCC 的精简 imm.h 只声明基础结构，这里补齐当前后端需要的常量。 */
#ifndef IMN_CHANGECANDIDATE
	#define IMN_CHANGECANDIDATE 0x0003
#endif
#ifndef IMN_CLOSECANDIDATE
	#define IMN_CLOSECANDIDATE 0x0004
#endif
#ifndef IMN_OPENCANDIDATE
	#define IMN_OPENCANDIDATE 0x0005
#endif
#ifndef NI_SELECTCANDIDATESTR
	#define NI_SELECTCANDIDATESTR 0x0012
#endif
#ifndef NI_COMPOSITIONSTR
	#define NI_COMPOSITIONSTR 0x0015
#endif
#ifndef CPS_COMPLETE
	#define CPS_COMPLETE 0x0001
#endif
#ifndef IACE_DEFAULT
	#define IACE_DEFAULT 0x0010
#endif



typedef struct xge_imm_candidate_list_t {
	DWORD iSize;
	DWORD iStyle;
	DWORD iCount;
	DWORD iSelection;
	DWORD iPageStart;
	DWORD iPageSize;
	DWORD arrOffset[1];
} xge_imm_candidate_list_t;

typedef HIMC (WINAPI *xge_imm_get_context_proc)(HWND hWnd);
typedef WINBOOL (WINAPI *xge_imm_release_context_proc)(HWND hWnd, HIMC hImc);
typedef LONG (WINAPI *xge_imm_get_composition_proc)(HIMC hImc, DWORD iIndex, void* pBuffer, DWORD iBufferSize);
typedef WINBOOL (WINAPI *xge_imm_set_composition_window_proc)(HIMC hImc, const COMPOSITIONFORM* pForm);
typedef WINBOOL (WINAPI *xge_imm_set_candidate_window_proc)(HIMC hImc, const CANDIDATEFORM* pForm);
typedef DWORD (WINAPI *xge_imm_get_candidate_list_proc)(HIMC hImc, DWORD iIndex, void* pList, DWORD iBufferSize);
typedef WINBOOL (WINAPI *xge_imm_notify_proc)(HIMC hImc, DWORD iAction, DWORD iIndex, DWORD iValue);
typedef WINBOOL (WINAPI *xge_imm_associate_context_proc)(HWND hWnd, HIMC hImc, DWORD iFlags);

typedef struct xge_win32_ime_state_t {
	HWND hWnd;
	WNDPROC pOriginalWindowProc;
	HMODULE hImm32;
	xge_imm_get_context_proc pGetContext;
	xge_imm_release_context_proc pReleaseContext;
	xge_imm_get_composition_proc pGetComposition;
	xge_imm_set_composition_window_proc pSetCompositionWindow;
	xge_imm_set_candidate_window_proc pSetCandidateWindow;
	xge_imm_get_candidate_list_proc pGetCandidateList;
	xge_imm_notify_proc pNotify;
	xge_imm_associate_context_proc pAssociateContext;
	int bEnabled;
	int iMode;
	int bTsfInitialized;
	int bImmComposing;
	int bHasCandidateRect;
	xge_rect_t tCandidateRect;
	RECT tCandidateClientRect;
	char** pCandidateText;
	int iCandidateCount;
	int iCandidateSelection;
	int iCandidatePageStart;
	int iCandidatePageSize;
	int bCandidateVisible;
} xge_win32_ime_state_t;

static xge_win32_ime_state_t g_xgeWin32Ime;



/* 释放当前候选列表快照。 */
static void __xgeImmCandidateClear(void)
{
	int i;

	for ( i = 0; i < g_xgeWin32Ime.iCandidateCount; i++ ) {
		if ( g_xgeWin32Ime.pCandidateText[i] != NULL ) {
			xrtFree(g_xgeWin32Ime.pCandidateText[i]);
		}
	}
	if ( g_xgeWin32Ime.pCandidateText != NULL ) {
		xrtFree(g_xgeWin32Ime.pCandidateText);
	}
	g_xgeWin32Ime.pCandidateText = NULL;
	g_xgeWin32Ime.iCandidateCount = 0;
	g_xgeWin32Ime.iCandidateSelection = 0;
	g_xgeWin32Ime.iCandidatePageStart = 0;
	g_xgeWin32Ime.iCandidatePageSize = 0;
	g_xgeWin32Ime.bCandidateVisible = 0;
}



/* 将指定长度的 UTF-16 文本复制为 XRT 管理的 UTF-8 字符串。 */
static char* __xgeImmUtf8Copy(const WCHAR* sWide, int iWideLength, int* pTextLength)
{
	char* sText;
	int iLength;

	if ( pTextLength != NULL ) *pTextLength = 0;
	if ( (sWide == NULL) || (iWideLength < 0) ) return NULL;
	iLength = WideCharToMultiByte(CP_UTF8, 0, sWide, iWideLength, NULL, 0, NULL, NULL);
	if ( iLength < 0 ) return NULL;
	sText = (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sText == NULL ) return NULL;
	if ( iLength > 0 ) {
		if ( WideCharToMultiByte(CP_UTF8, 0, sWide, iWideLength, sText, iLength, NULL, NULL) != iLength ) {
			xrtFree(sText);
			return NULL;
		}
	}
	sText[iLength] = '\0';
	if ( pTextLength != NULL ) *pTextLength = iLength;
	return sText;
}



/* 将 UTF-16 光标偏移换算为 UTF-8 字节偏移。 */
static int __xgeImmUtf8Offset(const WCHAR* sWide, int iWideLength)
{
	if ( (sWide == NULL) || (iWideLength <= 0) ) return 0;
	return WideCharToMultiByte(CP_UTF8, 0, sWide, iWideLength, NULL, 0, NULL, NULL);
}



/* 读取 IMM32 组合字符串并转换为 UTF-8。 */
static int __xgeImmReadString(HIMC hImc, DWORD iIndex, int iCursorUtf16, char** psText, int* pCursorUtf8)
{
	WCHAR* sWide;
	LONG iBytes;
	int iWideLength;
	int iTextLength;
	int iCursor;

	if ( psText == NULL ) return 0;
	*psText = NULL;
	if ( pCursorUtf8 != NULL ) *pCursorUtf8 = 0;
	if ( g_xgeWin32Ime.pGetComposition == NULL ) return 0;
	iBytes = g_xgeWin32Ime.pGetComposition(hImc, iIndex, NULL, 0);
	if ( iBytes <= 0 ) return 0;
	iWideLength = (int)(iBytes / (LONG)sizeof(WCHAR));
	sWide = (WCHAR*)xrtMalloc((size_t)iBytes + sizeof(WCHAR));
	if ( sWide == NULL ) return 0;
	memset(sWide, 0, (size_t)iBytes + sizeof(WCHAR));
	if ( g_xgeWin32Ime.pGetComposition(hImc, iIndex, sWide, (DWORD)iBytes) < 0 ) {
		xrtFree(sWide);
		return 0;
	}
	*psText = __xgeImmUtf8Copy(sWide, iWideLength, &iTextLength);
	if ( *psText == NULL ) {
		xrtFree(sWide);
		return 0;
	}
	if ( pCursorUtf8 != NULL ) {
		iCursor = iCursorUtf16;
		if ( (iCursor < 0) || (iCursor > iWideLength) ) iCursor = iWideLength;
		*pCursorUtf8 = __xgeImmUtf8Offset(sWide, iCursor);
	}
	xrtFree(sWide);
	return iTextLength;
}



/* 从 IMM32 刷新候选列表快照。 */
static int __xgeImmCandidateRefresh(HIMC hImc)
{
	xge_imm_candidate_list_t* pList;
	char** pText;
	char* sItem;
	WCHAR* sWide;
	DWORD iBytes;
	DWORD iOffset;
	DWORD iHeaderSize;
	DWORD iMaxCount;
	DWORD iRemaining;
	int iWideLength;
	int i;

	if ( (hImc == NULL) || (g_xgeWin32Ime.pGetCandidateList == NULL) ) return XGE_ERROR_UNSUPPORTED;
	iBytes = g_xgeWin32Ime.pGetCandidateList(hImc, 0u, NULL, 0u);
	if ( iBytes < (DWORD)(sizeof(DWORD) * 6u) ) {
		__xgeImmCandidateClear();
		return XGE_ERROR_FILE_NOT_FOUND;
	}
	pList = (xge_imm_candidate_list_t*)xrtMalloc((size_t)iBytes);
	if ( pList == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	if ( g_xgeWin32Ime.pGetCandidateList(hImc, 0u, pList, iBytes) == 0u ) {
		xrtFree(pList);
		return XGE_ERROR_BACKEND_FAILED;
	}
	iHeaderSize = (DWORD)(sizeof(DWORD) * 6u);
	iMaxCount = (iBytes - iHeaderSize) / (DWORD)sizeof(DWORD);
	if ( (pList->iSize > iBytes) || (pList->iCount > iMaxCount) || (pList->iCount > 4096u) ) {
		xrtFree(pList);
		return XGE_ERROR_BACKEND_FAILED;
	}
	pText = NULL;
	if ( pList->iCount > 0u ) {
		pText = (char**)xrtMalloc(sizeof(char*) * (size_t)pList->iCount);
		if ( pText == NULL ) {
			xrtFree(pList);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pText, 0, sizeof(char*) * (size_t)pList->iCount);
	}
	for ( i = 0; i < (int)pList->iCount; i++ ) {
		iOffset = pList->arrOffset[i];
		if ( (iOffset >= iBytes) || ((iOffset & 1u) != 0u) ) break;
		sWide = (WCHAR*)((unsigned char*)pList + iOffset);
		iRemaining = (iBytes - iOffset) / (DWORD)sizeof(WCHAR);
		for ( iWideLength = 0; (DWORD)iWideLength < iRemaining; iWideLength++ ) {
			if ( sWide[iWideLength] == L'\0' ) break;
		}
		if ( (DWORD)iWideLength >= iRemaining ) break;
		sItem = __xgeImmUtf8Copy(sWide, iWideLength, NULL);
		if ( sItem == NULL ) break;
		pText[i] = sItem;
	}
	if ( i != (int)pList->iCount ) {
		while ( i > 0 ) {
			i--;
			xrtFree(pText[i]);
		}
		if ( pText != NULL ) xrtFree(pText);
		xrtFree(pList);
		return XGE_ERROR_BACKEND_FAILED;
	}
	__xgeImmCandidateClear();
	g_xgeWin32Ime.pCandidateText = pText;
	g_xgeWin32Ime.iCandidateCount = (int)pList->iCount;
	g_xgeWin32Ime.iCandidateSelection = (int)pList->iSelection;
	g_xgeWin32Ime.iCandidatePageStart = (int)pList->iPageStart;
	g_xgeWin32Ime.iCandidatePageSize = (int)pList->iPageSize;
	g_xgeWin32Ime.bCandidateVisible = (pList->iCount > 0u) ? 1 : 0;
	xrtFree(pList);
	return XGE_OK;
}



/* 装载 IMM32 动态接口，避免让 TCC 依赖额外导入库。 */
static int __xgeImmLoad(void)
{
	g_xgeWin32Ime.hImm32 = LoadLibraryW(L"imm32.dll");
	if ( g_xgeWin32Ime.hImm32 == NULL ) return 0;
	g_xgeWin32Ime.pGetContext = (xge_imm_get_context_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmGetContext");
	g_xgeWin32Ime.pReleaseContext = (xge_imm_release_context_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmReleaseContext");
	g_xgeWin32Ime.pGetComposition = (xge_imm_get_composition_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmGetCompositionStringW");
	g_xgeWin32Ime.pSetCompositionWindow = (xge_imm_set_composition_window_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmSetCompositionWindow");
	g_xgeWin32Ime.pSetCandidateWindow = (xge_imm_set_candidate_window_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmSetCandidateWindow");
	g_xgeWin32Ime.pGetCandidateList = (xge_imm_get_candidate_list_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmGetCandidateListW");
	g_xgeWin32Ime.pNotify = (xge_imm_notify_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmNotifyIME");
	g_xgeWin32Ime.pAssociateContext = (xge_imm_associate_context_proc)GetProcAddress(g_xgeWin32Ime.hImm32, "ImmAssociateContextEx");
	if ( (g_xgeWin32Ime.pGetContext == NULL) ||
	     (g_xgeWin32Ime.pReleaseContext == NULL) ||
	     (g_xgeWin32Ime.pGetComposition == NULL) ) {
		FreeLibrary(g_xgeWin32Ime.hImm32);
		g_xgeWin32Ime.hImm32 = NULL;
		return 0;
	}
	return 1;
}



/* 将候选框位置同步给当前输入法上下文。 */
static void __xgeImeWin32ApplyImmCandidateRect(void)
{
	COMPOSITIONFORM tComposition;
	CANDIDATEFORM tCandidate;
	HIMC hImc;

	if ( (g_xgeWin32Ime.hWnd == NULL) || !g_xgeWin32Ime.bHasCandidateRect ||
	     (g_xgeWin32Ime.pGetContext == NULL) ) return;
	hImc = g_xgeWin32Ime.pGetContext(g_xgeWin32Ime.hWnd);
	if ( hImc == NULL ) return;
	memset(&tComposition, 0, sizeof(tComposition));
	tComposition.dwStyle = CFS_POINT;
	tComposition.ptCurrentPos.x = g_xgeWin32Ime.tCandidateClientRect.left;
	tComposition.ptCurrentPos.y = g_xgeWin32Ime.tCandidateClientRect.top;
	if ( g_xgeWin32Ime.pSetCompositionWindow != NULL ) {
		(void)g_xgeWin32Ime.pSetCompositionWindow(hImc, &tComposition);
	}
	memset(&tCandidate, 0, sizeof(tCandidate));
	tCandidate.dwIndex = 0u;
	tCandidate.dwStyle = CFS_EXCLUDE;
	tCandidate.ptCurrentPos = tComposition.ptCurrentPos;
	tCandidate.rcArea = g_xgeWin32Ime.tCandidateClientRect;
	if ( g_xgeWin32Ime.pSetCandidateWindow != NULL ) {
		(void)g_xgeWin32Ime.pSetCandidateWindow(hImc, &tCandidate);
	}
	(void)g_xgeWin32Ime.pReleaseContext(g_xgeWin32Ime.hWnd, hImc);
}



/* 更新 IMM32 焦点与启用状态。 */
static void __xgeTsfSetFocus(int bFocused)
{
	if ( (g_xgeWin32Ime.hWnd == NULL) || (g_xgeWin32Ime.pAssociateContext == NULL) ) return;
	if ( bFocused && g_xgeWin32Ime.bEnabled ) {
		(void)g_xgeWin32Ime.pAssociateContext(g_xgeWin32Ime.hWnd, NULL, IACE_DEFAULT);
	} else {
		(void)g_xgeWin32Ime.pAssociateContext(g_xgeWin32Ime.hWnd, NULL, 0u);
	}
}



/* IMM32 由窗口消息接管按键，不需要 TSF 的预翻译步骤。 */
static int __xgeTsfHandleKey(UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	(void)iMessage;
	(void)wParam;
	(void)lParam;
	return 0;
}



/* 处理组合文本与候选列表窗口消息。 */
static LRESULT __xgeImeHandleImmMessage(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam, int* pHandled)
{
	HIMC hImc;
	char* sText;
	LONG iCursorUtf16;
	int iCursorUtf8;
	int iTextLength;
	int iEventType;

	*pHandled = 0;
	if ( !g_xgeWin32Ime.bEnabled || (g_xgeWin32Ime.iMode == XGE_IME_MODE_NATIVE) ) return 0;
	switch ( iMessage ) {
	case WM_IME_STARTCOMPOSITION:
		g_xgeWin32Ime.bImmComposing = 1;
		__xgeImeWin32ApplyImmCandidateRect();
		(void)__xgeImeQueuePush(XGE_EVENT_IME_START, "", 0, 0, 0, 0);
		*pHandled = 1;
		break;
	case WM_IME_COMPOSITION:
		__xgeImeWin32ApplyImmCandidateRect();
		hImc = g_xgeWin32Ime.pGetContext(hWnd);
		if ( hImc != NULL ) {
			if ( (lParam & GCS_RESULTSTR) != 0 ) {
				sText = NULL;
				iTextLength = __xgeImmReadString(hImc, GCS_RESULTSTR, -1, &sText, NULL);
				if ( iTextLength > 0 ) {
					(void)__xgeImeQueuePush(XGE_EVENT_IME_COMMIT, sText, iTextLength,
						iTextLength, iTextLength, iTextLength);
					*pHandled = 1;
				}
				if ( sText != NULL ) xrtFree(sText);
			}
			if ( (lParam & GCS_COMPSTR) != 0 ) {
				iCursorUtf16 = g_xgeWin32Ime.pGetComposition(hImc, GCS_CURSORPOS, NULL, 0);
				sText = NULL;
				iCursorUtf8 = 0;
				iTextLength = __xgeImmReadString(hImc, GCS_COMPSTR, (int)iCursorUtf16, &sText, &iCursorUtf8);
				(void)__xgeImeQueuePush(XGE_EVENT_IME_UPDATE, sText, iTextLength,
					iCursorUtf8, iCursorUtf8, iCursorUtf8);
				if ( sText != NULL ) xrtFree(sText);
				*pHandled = 1;
			}
			(void)g_xgeWin32Ime.pReleaseContext(hWnd, hImc);
		}
		break;
	case WM_IME_ENDCOMPOSITION:
		if ( g_xgeWin32Ime.bImmComposing ) {
			(void)__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
		}
		g_xgeWin32Ime.bImmComposing = 0;
		*pHandled = 1;
		break;
	case WM_IME_NOTIFY:
		if ( (wParam == IMN_OPENCANDIDATE) || (wParam == IMN_CHANGECANDIDATE) ) {
			hImc = g_xgeWin32Ime.pGetContext(hWnd);
			if ( hImc != NULL ) {
				if ( __xgeImmCandidateRefresh(hImc) == XGE_OK ) {
					iEventType = (wParam == IMN_OPENCANDIDATE) ?
						XGE_EVENT_IME_CANDIDATE_START : XGE_EVENT_IME_CANDIDATE_UPDATE;
					(void)__xgeImeQueuePush(iEventType, "", 0, 0, 0, 0);
				}
				(void)g_xgeWin32Ime.pReleaseContext(hWnd, hImc);
			}
		} else if ( wParam == IMN_CLOSECANDIDATE ) {
			__xgeImmCandidateClear();
			(void)__xgeImeQueuePush(XGE_EVENT_IME_CANDIDATE_END, "", 0, 0, 0, 0);
		}
		break;
	case WM_KILLFOCUS:
		if ( g_xgeWin32Ime.bImmComposing ) {
			(void)__xgeImeQueuePush(XGE_EVENT_IME_END, "", 0, 0, 0, 0);
		}
		g_xgeWin32Ime.bImmComposing = 0;
		__xgeImmCandidateClear();
		break;
	default:
		return 0;
	}
	__xgeRenderRequestInternal();
	return 0;
}



/* 接管 XGE 窗口过程以接收 IMM32 消息。 */
static LRESULT CALLBACK __xgeImeWindowProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int bHandled;
	LRESULT iRet;

	if ( iMessage == WM_SETFOCUS ) __xgeTsfSetFocus(1);
	if ( iMessage == WM_KILLFOCUS ) __xgeTsfSetFocus(0);
	bHandled = 0;
	iRet = __xgeImeHandleImmMessage(hWnd, iMessage, wParam, lParam, &bHandled);
	if ( bHandled ) return iRet;
	if ( g_xgeWin32Ime.pOriginalWindowProc != NULL ) {
		return CallWindowProcW(g_xgeWin32Ime.pOriginalWindowProc, hWnd, iMessage, wParam, lParam);
	}
	return DefWindowProcW(hWnd, iMessage, wParam, lParam);
}



/* 初始化 TCC 使用的 Windows 输入法后端。 */
static void __xgeImeInstallWin32(void)
{
	WNDPROC pCurrent;
	LONG_PTR iPrevious;

	if ( g_xgeWin32Ime.hWnd != NULL ) return;
	memset(&g_xgeWin32Ime, 0, sizeof(g_xgeWin32Ime));
	g_xgeWin32Ime.hWnd = (HWND)sapp_win32_get_hwnd();
	g_xgeWin32Ime.iMode = g_xge.iImeMode;
	if ( g_xgeWin32Ime.hWnd == NULL ) return;
	if ( !__xgeImmLoad() ) {
		g_xgeWin32Ime.hWnd = NULL;
		return;
	}
	pCurrent = (WNDPROC)GetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC);
	SetLastError(0);
	iPrevious = SetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC, (LONG_PTR)__xgeImeWindowProc);
	if ( (iPrevious == 0) && (GetLastError() != 0) ) {
		FreeLibrary(g_xgeWin32Ime.hImm32);
		memset(&g_xgeWin32Ime, 0, sizeof(g_xgeWin32Ime));
		return;
	}
	g_xgeWin32Ime.pOriginalWindowProc = (WNDPROC)iPrevious;
	if ( g_xgeWin32Ime.pOriginalWindowProc == NULL ) g_xgeWin32Ime.pOriginalWindowProc = pCurrent;
	__xgeTsfSetFocus(0);
}



/* 释放输入法状态并恢复原窗口过程。 */
static void __xgeImeUninstallWin32(void)
{
	if ( (g_xgeWin32Ime.hWnd != NULL) && (g_xgeWin32Ime.pOriginalWindowProc != NULL) &&
	     ((WNDPROC)GetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC) == __xgeImeWindowProc) ) {
		(void)SetWindowLongPtrW(g_xgeWin32Ime.hWnd, GWLP_WNDPROC, (LONG_PTR)g_xgeWin32Ime.pOriginalWindowProc);
	}
	__xgeTsfSetFocus(0);
	__xgeImmCandidateClear();
	if ( g_xgeWin32Ime.hImm32 != NULL ) FreeLibrary(g_xgeWin32Ime.hImm32);
	memset(&g_xgeWin32Ime, 0, sizeof(g_xgeWin32Ime));
}



/* 设置候选框的窗口客户区位置。 */
static int __xgeImeWin32SetCandidateRect(xge_rect_t tRect)
{
	xge_platform_runtime_t tRuntime;
	RECT tClient;
	float fScaleX;
	float fScaleY;

	if ( g_xgeWin32Ime.hWnd == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	memset(&tRuntime, 0, sizeof(tRuntime));
	memset(&tClient, 0, sizeof(tClient));
	fScaleX = 1.0f;
	fScaleY = 1.0f;
	if ( GetClientRect(g_xgeWin32Ime.hWnd, &tClient) && (xgePlatformRuntimeGet(&tRuntime) == XGE_OK) ) {
		if ( tRuntime.iFramebufferWidth > 0 ) {
			fScaleX = (float)(tClient.right - tClient.left) / (float)tRuntime.iFramebufferWidth;
		}
		if ( tRuntime.iFramebufferHeight > 0 ) {
			fScaleY = (float)(tClient.bottom - tClient.top) / (float)tRuntime.iFramebufferHeight;
		}
	}
	g_xgeWin32Ime.tCandidateRect = tRect;
	g_xgeWin32Ime.bHasCandidateRect = 1;
	g_xgeWin32Ime.tCandidateClientRect.left = (LONG)floorf((tRect.fX * fScaleX) + 0.5f);
	g_xgeWin32Ime.tCandidateClientRect.top = (LONG)floorf((tRect.fY * fScaleY) + 0.5f);
	g_xgeWin32Ime.tCandidateClientRect.right = (LONG)floorf(((tRect.fX + ((tRect.fW > 1.0f) ? tRect.fW : 1.0f)) * fScaleX) + 0.5f);
	g_xgeWin32Ime.tCandidateClientRect.bottom = (LONG)floorf(((tRect.fY + ((tRect.fH > 1.0f) ? tRect.fH : 1.0f)) * fScaleY) + 0.5f);
	__xgeImeWin32ApplyImmCandidateRect();
	return XGE_OK;
}



/* TCC 后端不提供 TSF 周边文本存储。 */
static int __xgeImeWin32SetTextClient(const xge_ime_text_client_t* pClient)
{
	if ( g_xge.bInitialized == 0 ) return XGE_ERROR_NOT_INITIALIZED;
	if ( pClient == NULL ) return XGE_OK;
	if ( (pClient->iSize != 0u) && (pClient->iSize < sizeof(*pClient)) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pClient->onSnapshot == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	return XGE_ERROR_UNSUPPORTED;
}



/* TCC 后端没有需要主动刷新的 TSF 文本快照。 */
static int __xgeImeWin32RefreshTextClient(void)
{
	return (g_xge.bInitialized != 0) ? XGE_ERROR_UNSUPPORTED : XGE_ERROR_NOT_INITIALIZED;
}



/* IMM32 仍由系统绘制候选窗口，不能切换到自定义呈现器。 */
static int __xgeImeWin32SetCandidatePresenterReady(int bReady)
{
	if ( g_xge.bInitialized == 0 ) return XGE_ERROR_NOT_INITIALIZED;
	return bReady ? XGE_ERROR_UNSUPPORTED : XGE_OK;
}



/* 读取候选列表元数据。 */
static int __xgeImeWin32CandidateGetInfo(xge_ime_candidate_info_t* pInfo)
{
	if ( pInfo == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->bVisible = g_xgeWin32Ime.bCandidateVisible;
	pInfo->bCanSelect = (g_xgeWin32Ime.pNotify != NULL) ? 1 : 0;
	pInfo->iCount = g_xgeWin32Ime.iCandidateCount;
	pInfo->iSelection = g_xgeWin32Ime.iCandidateSelection;
	pInfo->iPageStart = g_xgeWin32Ime.iCandidatePageStart;
	pInfo->iPageSize = g_xgeWin32Ime.iCandidatePageSize;
	return XGE_OK;
}



/* 复制候选文本并返回完整 UTF-8 字节长度。 */
static int __xgeImeWin32CandidateGetText(int iIndex, char* sText, int iCapacity)
{
	const char* sSource;
	int iLength;
	int iCopy;

	if ( (iIndex < 0) || (iIndex >= g_xgeWin32Ime.iCandidateCount) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (sText == NULL) && (iCapacity > 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( iCapacity < 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	sSource = g_xgeWin32Ime.pCandidateText[iIndex];
	if ( sSource == NULL ) sSource = "";
	iLength = (int)strlen(sSource);
	if ( iCapacity > 0 ) {
		iCopy = (iLength < (iCapacity - 1)) ? iLength : (iCapacity - 1);
		if ( iCopy > 0 ) memcpy(sText, sSource, (size_t)iCopy);
		sText[iCopy] = '\0';
	}
	return iLength;
}



/* 请求 IMM32 选择指定候选项。 */
static int __xgeImeWin32CandidateSelect(int iIndex)
{
	HIMC hImc;
	WINBOOL bRet;

	if ( (iIndex < 0) || (iIndex >= g_xgeWin32Ime.iCandidateCount) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (g_xgeWin32Ime.hWnd == NULL) || (g_xgeWin32Ime.pNotify == NULL) ) return XGE_ERROR_UNSUPPORTED;
	hImc = g_xgeWin32Ime.pGetContext(g_xgeWin32Ime.hWnd);
	if ( hImc == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	bRet = g_xgeWin32Ime.pNotify(hImc, NI_SELECTCANDIDATESTR, 0u, (DWORD)iIndex);
	(void)g_xgeWin32Ime.pReleaseContext(g_xgeWin32Ime.hWnd, hImc);
	if ( bRet ) g_xgeWin32Ime.iCandidateSelection = iIndex;
	return bRet ? XGE_OK : XGE_ERROR_BACKEND_FAILED;
}



/* 请求 IMM32 完成当前组合。 */
static int __xgeImeWin32CandidateFinalize(void)
{
	HIMC hImc;
	WINBOOL bRet;

	if ( (g_xgeWin32Ime.hWnd == NULL) || (g_xgeWin32Ime.pNotify == NULL) ) return XGE_ERROR_UNSUPPORTED;
	hImc = g_xgeWin32Ime.pGetContext(g_xgeWin32Ime.hWnd);
	if ( hImc == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	bRet = g_xgeWin32Ime.pNotify(hImc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0u);
	(void)g_xgeWin32Ime.pReleaseContext(g_xgeWin32Ime.hWnd, hImc);
	return bRet ? XGE_OK : XGE_ERROR_BACKEND_FAILED;
}
