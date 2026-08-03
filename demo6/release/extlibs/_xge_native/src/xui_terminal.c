#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif
#endif

#include "xui_internal.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "../xge_tcc_compat.h"
#endif

#define XUI_TERMINAL_DEFAULT_COLS 80
#define XUI_TERMINAL_DEFAULT_ROWS 24
#define XUI_TERMINAL_DEFAULT_SCROLLBACK 1000
#define XUI_TERMINAL_DEFAULT_PARSE_BUDGET 4096
#define XUI_TERMINAL_MAX_CSI_PARAMS 16
#define XUI_TERMINAL_MAX_OSC 256
#define XUI_TERMINAL_MAX_LINK_ID 65535u
#define XUI_TERMINAL_TRIPLE_CLICK_SECONDS 0.45
#define XUI_TERMINAL_TRIPLE_CLICK_DISTANCE 6.0f
#define XUI_TERMINAL_SESSION_FAKE 1
#define XUI_TERMINAL_SESSION_PROCESS 2
#define XUI_TERMINAL_PROCESS_READ_CHUNK 4096
#define XUI_TERMINAL_PROCESS_POLL_LIMIT 16
#define XUI_TERMINAL_MENU_TITLE_COUNT 6

struct xui_terminal_session_t {
	xui_widget pWidget;
	char* sPrompt;
	xui_terminal_session_resize_proc onResize;
	void* pResizeUser;
	int iLastColumns;
	int iLastRows;
	int bEcho;
	int bRunning;
	int iKind;
	uint32_t iProcessFlags;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hProcess;
	HANDLE hInputWrite;
	HANDLE hOutputRead;
	HPCON hPseudoConsole;
#endif
};

typedef struct xui_terminal_parser_t {
	int iState;
	int iUtfExpected;
	int iUtfSeen;
	uint32_t iUtfCodepoint;
	int arrParams[XUI_TERMINAL_MAX_CSI_PARAMS];
	int iParamCount;
	int iCurrentParam;
	int bParamActive;
	int bPrivate;
	char sOsc[XUI_TERMINAL_MAX_OSC];
	int iOscSize;
} xui_terminal_parser_t;

#if defined(_WIN32) || defined(_WIN64)
typedef HRESULT (WINAPI* xui_terminal_create_pseudo_console_proc)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT (WINAPI* xui_terminal_resize_pseudo_console_proc)(HPCON, COORD);
typedef void (WINAPI* xui_terminal_close_pseudo_console_proc)(HPCON);

typedef struct xui_terminal_conpty_api_t {
	xui_terminal_create_pseudo_console_proc createPseudoConsole;
	xui_terminal_resize_pseudo_console_proc resizePseudoConsole;
	xui_terminal_close_pseudo_console_proc closePseudoConsole;
} xui_terminal_conpty_api_t;
#endif

typedef struct xui_terminal_data_t {
	xui_terminal_cell_t* pMain;
	xui_terminal_cell_t* pAlt;
	char** ppScrollback;
	uint16_t** ppScrollbackLinks;
	int* pScrollbackColumns;
	uint8_t* pQueue;
	uint8_t* pTabStops;
	uint8_t* pDirtyRows;
	xui_surface pLastCacheSurface;
	xui_font pBaseFont;
	xui_font pFont;
	xui_scroll_model_t tScroll;
	xui_terminal_parser_t tParser;
	xui_terminal_session_t* pSession;
	xui_widget pMenu;
	char* arrMenuTitle[XUI_TERMINAL_MENU_TITLE_COUNT];
	char* sSearchText;
	xui_terminal_data_proc onData;
	void* pDataUser;
	xui_terminal_resize_proc onResize;
	void* pResizeUser;
	xui_terminal_title_proc onTitle;
	void* pTitleUser;
	xui_terminal_link_proc onLink;
	void* pLinkUser;
	char* sHoverLink;
	char** ppLinks;
	int iLinkCount;
	int iLinkCapacity;
	uint16_t iCurrentLinkId;
	uint32_t arrBasePalette[256];
	uint32_t arrPalette[256];
	uint32_t iBaseBackgroundColor;
	uint32_t iBaseForegroundColor;
	uint32_t iBaseCursorColor;
	uint32_t iBaseSelectionColor;
	uint32_t iBaseSelectionTextColor;
	uint32_t iBaseSearchHighlightColor;
	uint32_t iBaseFocusColor;
	uint32_t iBaseLinkHoverColor;
	uint32_t iBackgroundColor;
	uint32_t iForegroundColor;
	uint32_t iCursorColor;
	uint32_t iSelectionColor;
	uint32_t iSelectionTextColor;
	uint32_t iSearchHighlightColor;
	uint32_t iFocusColor;
	uint32_t iLinkHoverColor;
	uint32_t iCurrentFg;
	uint32_t iCurrentBg;
	uint32_t iCurrentFlags;
	int iColumns;
	int iRows;
	int iCursorX;
	int iCursorY;
	int iSavedCursorX;
	int iSavedCursorY;
	int iScrollTop;
	int iScrollBottom;
	int iScrollbackLimit;
	int iScrollbackStart;
	int iScrollbackCount;
	int iQueueSize;
	int iQueueCapacity;
	int iParseBudget;
	int iChangeCount;
	int iDirtyRowCount;
	int iLastRenderTopLine;
	int iLastRenderColumns;
	int iLastRenderRows;
	int iSearchLine;
	int iSearchColumn;
	int iSearchLength;
	int iHoverLinkLine;
	int iHoverLinkColumn;
	int iHoverLinkLength;
	uint32_t iSearchFlags;
	int bAltScreen;
	int bCursorVisible;
	int bBracketedPaste;
	int bLigaturesEnabled;
	int bFullCacheDirty;
	int bCacheRendered;
	int bSelectAll;
	int bSelecting;
	int bDoubleClickPending;
	int iSelectAnchorLine;
	int iSelectAnchorColumn;
	int iSelectEndLine;
	int iSelectEndColumn;
	int iDoubleClickLine;
	float fDoubleClickX;
	float fDoubleClickY;
	double fDoubleClickTime;
	float fBaseCellWidth;
	float fBaseCellHeight;
	float fBasePadding;
	float fCellWidth;
	float fCellHeight;
	float fPadding;
} xui_terminal_data_t;

static void __xuiTerminalResolveStyle(xui_widget pWidget, xui_terminal_data_t* pData);
static int __xuiTerminalPointerToCell(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent, int* pLine, int* pColumn);

static int __xuiTerminalMax(int a, int b) { return (a > b) ? a : b; }
static int __xuiTerminalMin(int a, int b) { return (a < b) ? a : b; }
static int __xuiTerminalAlpha(uint32_t iColor) { return (int)(iColor & 0xffu); }

static char* __xuiTerminalStrDup(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void __xuiTerminalFreeLinks(xui_terminal_data_t* pData)
{
	int i;

	if ( pData == NULL ) {
		return;
	}
	if ( pData->ppLinks != NULL ) {
		for ( i = 0; i < pData->iLinkCount; i++ ) {
			if ( pData->ppLinks[i] != NULL ) {
				xrtFree(pData->ppLinks[i]);
			}
		}
		xrtFree(pData->ppLinks);
	}
	pData->ppLinks = NULL;
	pData->iLinkCount = 0;
	pData->iLinkCapacity = 0;
	pData->iCurrentLinkId = 0;
}

static const char* __xuiTerminalGetLinkUrl(const xui_terminal_data_t* pData, uint16_t iLinkId)
{
	int iIndex;

	if ( pData == NULL || iLinkId == 0 ) {
		return NULL;
	}
	iIndex = (int)iLinkId - 1;
	if ( iIndex < 0 || iIndex >= pData->iLinkCount || pData->ppLinks == NULL ) {
		return NULL;
	}
	return pData->ppLinks[iIndex];
}

static uint16_t __xuiTerminalGetOrAddLink(xui_terminal_data_t* pData, const char* sUrl)
{
	char** ppNew;
	char* sCopy;
	int iNewCapacity;
	int i;

	if ( pData == NULL || sUrl == NULL || sUrl[0] == '\0' ) {
		return 0;
	}
	for ( i = 0; i < pData->iLinkCount; i++ ) {
		if ( pData->ppLinks != NULL && pData->ppLinks[i] != NULL && strcmp(pData->ppLinks[i], sUrl) == 0 ) {
			return (uint16_t)(i + 1);
		}
	}
	if ( (uint32_t)pData->iLinkCount >= XUI_TERMINAL_MAX_LINK_ID ) {
		return 0;
	}
	if ( pData->iLinkCount >= pData->iLinkCapacity ) {
		iNewCapacity = (pData->iLinkCapacity > 0) ? (pData->iLinkCapacity * 2) : 16;
		if ( iNewCapacity <= pData->iLinkCount ) iNewCapacity = pData->iLinkCount + 1;
		if ( (uint32_t)iNewCapacity > XUI_TERMINAL_MAX_LINK_ID ) iNewCapacity = (int)XUI_TERMINAL_MAX_LINK_ID;
		ppNew = (char**)xrtRealloc(pData->ppLinks, sizeof(*ppNew) * (size_t)iNewCapacity);
		if ( ppNew == NULL ) {
			return 0;
		}
		memset(ppNew + pData->iLinkCapacity, 0, sizeof(*ppNew) * (size_t)(iNewCapacity - pData->iLinkCapacity));
		pData->ppLinks = ppNew;
		pData->iLinkCapacity = iNewCapacity;
	}
	sCopy = __xuiTerminalStrDup(sUrl);
	if ( sCopy == NULL ) {
		return 0;
	}
	pData->ppLinks[pData->iLinkCount] = sCopy;
	pData->iLinkCount++;
	return (uint16_t)pData->iLinkCount;
}

#if defined(_WIN32) || defined(_WIN64)
static int __xuiTerminalConptyGetApi(xui_terminal_conpty_api_t* pApi)
{
	HMODULE hKernel;

	if ( pApi == NULL ) return 0;
	memset(pApi, 0, sizeof(*pApi));
	hKernel = GetModuleHandleA("kernel32.dll");
	if ( hKernel == NULL ) return 0;
	pApi->createPseudoConsole = (xui_terminal_create_pseudo_console_proc)GetProcAddress(hKernel, "CreatePseudoConsole");
	pApi->resizePseudoConsole = (xui_terminal_resize_pseudo_console_proc)GetProcAddress(hKernel, "ResizePseudoConsole");
	pApi->closePseudoConsole = (xui_terminal_close_pseudo_console_proc)GetProcAddress(hKernel, "ClosePseudoConsole");
	return (pApi->createPseudoConsole != NULL) && (pApi->resizePseudoConsole != NULL) && (pApi->closePseudoConsole != NULL);
}

static COORD __xuiTerminalConptySize(int iColumns, int iRows)
{
	COORD tSize;

	if ( iColumns <= 0 ) iColumns = XUI_TERMINAL_DEFAULT_COLS;
	if ( iRows <= 0 ) iRows = XUI_TERMINAL_DEFAULT_ROWS;
	if ( iColumns > 32767 ) iColumns = 32767;
	if ( iRows > 32767 ) iRows = 32767;
	tSize.X = (SHORT)iColumns;
	tSize.Y = (SHORT)iRows;
	return tSize;
}
#endif

static uint32_t __xuiTerminalProcessDescFlags(const xui_terminal_process_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 0u;
	if ( pDesc->iSize < (uint32_t)(offsetof(xui_terminal_process_desc_t, iFlags) + sizeof(pDesc->iFlags)) ) return 0u;
	return pDesc->iFlags;
}

static int __xuiTerminalProcessDescColumns(const xui_terminal_process_desc_t* pDesc)
{
	if ( pDesc == NULL ) return XUI_TERMINAL_DEFAULT_COLS;
	if ( pDesc->iSize < (uint32_t)(offsetof(xui_terminal_process_desc_t, iColumns) + sizeof(pDesc->iColumns)) ) return XUI_TERMINAL_DEFAULT_COLS;
	return (pDesc->iColumns > 0) ? pDesc->iColumns : XUI_TERMINAL_DEFAULT_COLS;
}

static int __xuiTerminalProcessDescRows(const xui_terminal_process_desc_t* pDesc)
{
	if ( pDesc == NULL ) return XUI_TERMINAL_DEFAULT_ROWS;
	if ( pDesc->iSize < (uint32_t)(offsetof(xui_terminal_process_desc_t, iRows) + sizeof(pDesc->iRows)) ) return XUI_TERMINAL_DEFAULT_ROWS;
	return (pDesc->iRows > 0) ? pDesc->iRows : XUI_TERMINAL_DEFAULT_ROWS;
}

static int __xuiTerminalEncodeUtf8(uint32_t iCodepoint, char* sBuffer)
{
	if ( sBuffer == NULL ) {
		return 0;
	}
	if ( iCodepoint == 0 ) {
		return 0;
	}
	if ( iCodepoint < 0x80u ) {
		sBuffer[0] = (char)iCodepoint;
		return 1;
	}
	if ( iCodepoint < 0x800u ) {
		sBuffer[0] = (char)(0xc0u | (iCodepoint >> 6));
		sBuffer[1] = (char)(0x80u | (iCodepoint & 0x3fu));
		return 2;
	}
	if ( iCodepoint < 0x10000u ) {
		sBuffer[0] = (char)(0xe0u | (iCodepoint >> 12));
		sBuffer[1] = (char)(0x80u | ((iCodepoint >> 6) & 0x3fu));
		sBuffer[2] = (char)(0x80u | (iCodepoint & 0x3fu));
		return 3;
	}
	if ( iCodepoint <= 0x10ffffu ) {
		sBuffer[0] = (char)(0xf0u | (iCodepoint >> 18));
		sBuffer[1] = (char)(0x80u | ((iCodepoint >> 12) & 0x3fu));
		sBuffer[2] = (char)(0x80u | ((iCodepoint >> 6) & 0x3fu));
		sBuffer[3] = (char)(0x80u | (iCodepoint & 0x3fu));
		return 4;
	}
	return 0;
}

static int __xuiTerminalCodepointWidth(uint32_t iCodepoint)
{
	if ( iCodepoint == 0 ) {
		return 1;
	}
	if ( (iCodepoint >= 0x1100u && iCodepoint <= 0x115fu) ||
	     (iCodepoint >= 0x2329u && iCodepoint <= 0x232au) ||
	     (iCodepoint >= 0x2e80u && iCodepoint <= 0xa4cfu) ||
	     (iCodepoint >= 0xac00u && iCodepoint <= 0xd7a3u) ||
	     (iCodepoint >= 0xf900u && iCodepoint <= 0xfaffu) ||
	     (iCodepoint >= 0xfe10u && iCodepoint <= 0xfe19u) ||
	     (iCodepoint >= 0xfe30u && iCodepoint <= 0xfe6fu) ||
	     (iCodepoint >= 0xff00u && iCodepoint <= 0xff60u) ||
	     (iCodepoint >= 0xffe0u && iCodepoint <= 0xffe6u) ) {
		return 2;
	}
	return 1;
}

static xui_terminal_cell_t __xuiTerminalBlankCell(const xui_terminal_data_t* pData)
{
	xui_terminal_cell_t tCell;

	memset(&tCell, 0, sizeof(tCell));
	tCell.iSize = sizeof(tCell);
	tCell.iCodepoint = ' ';
	tCell.iFgColor = (pData != NULL) ? pData->iForegroundColor : XUI_COLOR_RGBA(220, 230, 240, 255);
	tCell.iBgColor = (pData != NULL) ? pData->iBackgroundColor : XUI_COLOR_RGBA(18, 24, 32, 255);
	tCell.iWidth = 1;
	return tCell;
}

static xui_terminal_cell_t* __xuiTerminalScreen(xui_terminal_data_t* pData)
{
	return (pData != NULL && pData->bAltScreen) ? pData->pAlt : ((pData != NULL) ? pData->pMain : NULL);
}

static const xui_terminal_cell_t* __xuiTerminalScreenConst(const xui_terminal_data_t* pData)
{
	return (pData != NULL && pData->bAltScreen) ? pData->pAlt : ((pData != NULL) ? pData->pMain : NULL);
}

static void __xuiTerminalMarkDirtyRow(xui_terminal_data_t* pData, int iRow)
{
	if ( pData == NULL || pData->pDirtyRows == NULL || iRow < 0 || iRow >= pData->iRows ) {
		return;
	}
	if ( pData->pDirtyRows[iRow] == 0u ) {
		pData->pDirtyRows[iRow] = 1u;
		pData->iDirtyRowCount++;
	}
}

static void __xuiTerminalMarkDirtyRows(xui_terminal_data_t* pData, int iStartRow, int iEndRow)
{
	int i;

	if ( pData == NULL || pData->iRows <= 0 ) {
		return;
	}
	if ( iStartRow < 0 ) iStartRow = 0;
	if ( iEndRow >= pData->iRows ) iEndRow = pData->iRows - 1;
	if ( iEndRow < iStartRow ) return;
	for ( i = iStartRow; i <= iEndRow; i++ ) {
		__xuiTerminalMarkDirtyRow(pData, i);
	}
}

static void __xuiTerminalMarkAllRowsDirty(xui_terminal_data_t* pData)
{
	if ( pData == NULL || pData->pDirtyRows == NULL || pData->iRows <= 0 ) {
		return;
	}
	memset(pData->pDirtyRows, 1, (size_t)pData->iRows);
	pData->iDirtyRowCount = pData->iRows;
}

static void __xuiTerminalMarkFullCacheDirty(xui_terminal_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	pData->bFullCacheDirty = 1;
	__xuiTerminalMarkAllRowsDirty(pData);
}

static void __xuiTerminalClearDirtyRows(xui_terminal_data_t* pData)
{
	if ( pData == NULL || pData->pDirtyRows == NULL || pData->iRows <= 0 ) {
		return;
	}
	memset(pData->pDirtyRows, 0, (size_t)pData->iRows);
	pData->iDirtyRowCount = 0;
}

static void __xuiTerminalResetParser(xui_terminal_parser_t* pParser)
{
	if ( pParser == NULL ) {
		return;
	}
	memset(pParser, 0, sizeof(*pParser));
}

static void __xuiTerminalDefaultPalette(xui_terminal_data_t* pData)
{
	static const uint32_t arrBase[16] = {
		XUI_COLOR_RGBA(0, 0, 0, 255),
		XUI_COLOR_RGBA(205, 49, 49, 255),
		XUI_COLOR_RGBA(13, 188, 121, 255),
		XUI_COLOR_RGBA(229, 229, 16, 255),
		XUI_COLOR_RGBA(36, 114, 200, 255),
		XUI_COLOR_RGBA(188, 63, 188, 255),
		XUI_COLOR_RGBA(17, 168, 205, 255),
		XUI_COLOR_RGBA(229, 229, 229, 255),
		XUI_COLOR_RGBA(102, 102, 102, 255),
		XUI_COLOR_RGBA(241, 76, 76, 255),
		XUI_COLOR_RGBA(35, 209, 139, 255),
		XUI_COLOR_RGBA(245, 245, 67, 255),
		XUI_COLOR_RGBA(59, 142, 234, 255),
		XUI_COLOR_RGBA(214, 112, 214, 255),
		XUI_COLOR_RGBA(41, 184, 219, 255),
		XUI_COLOR_RGBA(255, 255, 255, 255)
	};
	int i;
	int r;
	int g;
	int b;
	int v;
	int idx;
	int arrCube[6] = {0, 95, 135, 175, 215, 255};

	if ( pData == NULL ) {
		return;
	}
	for ( i = 0; i < 16; i++ ) {
		pData->arrBasePalette[i] = arrBase[i];
	}
	idx = 16;
	for ( r = 0; r < 6; r++ ) {
		for ( g = 0; g < 6; g++ ) {
			for ( b = 0; b < 6; b++ ) {
				pData->arrBasePalette[idx++] = XUI_COLOR_RGBA(arrCube[r], arrCube[g], arrCube[b], 255);
			}
		}
	}
	for ( i = 0; i < 24; i++ ) {
		v = 8 + i * 10;
		pData->arrBasePalette[232 + i] = XUI_COLOR_RGBA(v, v, v, 255);
	}
	memcpy(pData->arrPalette, pData->arrBasePalette, sizeof(pData->arrPalette));
}

static void __xuiTerminalFreeScrollback(xui_terminal_data_t* pData)
{
	int i;
	int idx;

	if ( pData == NULL ) {
		return;
	}
	for ( i = 0; i < pData->iScrollbackCount; i++ ) {
		idx = (pData->iScrollbackStart + i) % pData->iScrollbackLimit;
		if ( pData->ppScrollback != NULL && pData->ppScrollback[idx] != NULL ) {
			xrtFree(pData->ppScrollback[idx]);
			pData->ppScrollback[idx] = NULL;
		}
		if ( pData->ppScrollbackLinks != NULL && pData->ppScrollbackLinks[idx] != NULL ) {
			xrtFree(pData->ppScrollbackLinks[idx]);
			pData->ppScrollbackLinks[idx] = NULL;
		}
		if ( pData->pScrollbackColumns != NULL ) {
			pData->pScrollbackColumns[idx] = 0;
		}
	}
	if ( pData->ppScrollback != NULL ) xrtFree(pData->ppScrollback);
	if ( pData->ppScrollbackLinks != NULL ) xrtFree(pData->ppScrollbackLinks);
	if ( pData->pScrollbackColumns != NULL ) xrtFree(pData->pScrollbackColumns);
	pData->ppScrollback = NULL;
	pData->ppScrollbackLinks = NULL;
	pData->pScrollbackColumns = NULL;
	pData->iScrollbackStart = 0;
	pData->iScrollbackCount = 0;
}

static int __xuiTerminalAllocScrollback(xui_terminal_data_t* pData, int iLimit)
{
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTerminalFreeScrollback(pData);
	pData->iScrollbackLimit = (iLimit > 0) ? iLimit : XUI_TERMINAL_DEFAULT_SCROLLBACK;
	pData->ppScrollback = (char**)xrtCalloc((size_t)pData->iScrollbackLimit, sizeof(char*));
	pData->ppScrollbackLinks = (uint16_t**)xrtCalloc((size_t)pData->iScrollbackLimit, sizeof(uint16_t*));
	pData->pScrollbackColumns = (int*)xrtCalloc((size_t)pData->iScrollbackLimit, sizeof(int));
	if ( pData->ppScrollback == NULL || pData->ppScrollbackLinks == NULL || pData->pScrollbackColumns == NULL ) {
		if ( pData->ppScrollback != NULL ) xrtFree(pData->ppScrollback);
		if ( pData->ppScrollbackLinks != NULL ) xrtFree(pData->ppScrollbackLinks);
		if ( pData->pScrollbackColumns != NULL ) xrtFree(pData->pScrollbackColumns);
		pData->ppScrollback = NULL;
		pData->ppScrollbackLinks = NULL;
		pData->pScrollbackColumns = NULL;
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	return XUI_OK;
}

static char* __xuiTerminalSerializeRow(const xui_terminal_data_t* pData, const xui_terminal_cell_t* pRow)
{
	char sUtf[8];
	char* sLine;
	int iLast;
	int iSize;
	int iCapacity;
	int i;
	int n;

	if ( (pData == NULL) || (pRow == NULL) || (pData->iColumns <= 0) ) {
		return __xuiTerminalStrDup("");
	}
	iLast = pData->iColumns - 1;
	while ( (iLast >= 0) &&
	        ((pRow[iLast].iCodepoint == 0u) || (pRow[iLast].iCodepoint == ' ') || ((pRow[iLast].iFlags & XUI_TERMINAL_CELL_WIDE_CONT) != 0u)) ) {
		iLast--;
	}
	iCapacity = (iLast + 2) * 4;
	if ( iCapacity < 1 ) {
		iCapacity = 1;
	}
	sLine = (char*)xrtMalloc((size_t)iCapacity);
	if ( sLine == NULL ) {
		return NULL;
	}
	iSize = 0;
	for ( i = 0; i <= iLast; i++ ) {
		if ( (pRow[i].iFlags & XUI_TERMINAL_CELL_WIDE_CONT) != 0u ) {
			continue;
		}
		n = __xuiTerminalEncodeUtf8(pRow[i].iCodepoint ? pRow[i].iCodepoint : ' ', sUtf);
		if ( (iSize + n + 1) >= iCapacity ) {
			char* sNew;
			iCapacity *= 2;
			sNew = (char*)xrtRealloc(sLine, (size_t)iCapacity);
			if ( sNew == NULL ) {
				xrtFree(sLine);
				return NULL;
			}
			sLine = sNew;
		}
		if ( n > 0 ) {
			memcpy(sLine + iSize, sUtf, (size_t)n);
			iSize += n;
		}
	}
	sLine[iSize] = '\0';
	return sLine;
}

static void __xuiTerminalClearScrollbackSlot(xui_terminal_data_t* pData, int idx)
{
	if ( pData == NULL || idx < 0 || idx >= pData->iScrollbackLimit ) {
		return;
	}
	if ( pData->ppScrollback != NULL && pData->ppScrollback[idx] != NULL ) {
		xrtFree(pData->ppScrollback[idx]);
		pData->ppScrollback[idx] = NULL;
	}
	if ( pData->ppScrollbackLinks != NULL && pData->ppScrollbackLinks[idx] != NULL ) {
		xrtFree(pData->ppScrollbackLinks[idx]);
		pData->ppScrollbackLinks[idx] = NULL;
	}
	if ( pData->pScrollbackColumns != NULL ) {
		pData->pScrollbackColumns[idx] = 0;
	}
}

static uint16_t* __xuiTerminalCopyRowLinks(const xui_terminal_cell_t* pRow, int iColumns)
{
	uint16_t* pLinks;
	int bHasLink;
	int i;

	if ( pRow == NULL || iColumns <= 0 ) {
		return NULL;
	}
	bHasLink = 0;
	for ( i = 0; i < iColumns; i++ ) {
		if ( pRow[i].iLinkId != 0 ) {
			bHasLink = 1;
			break;
		}
	}
	if ( !bHasLink ) {
		return NULL;
	}
	pLinks = (uint16_t*)xrtMalloc(sizeof(*pLinks) * (size_t)iColumns);
	if ( pLinks == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iColumns; i++ ) {
		pLinks[i] = pRow[i].iLinkId;
	}
	return pLinks;
}

static int __xuiTerminalPushScrollback(xui_terminal_data_t* pData, const char* sLine, const xui_terminal_cell_t* pRow, int iColumns)
{
	char* sCopy;
	uint16_t* pLinks;
	int idx;

	if ( (pData == NULL) || (pData->ppScrollback == NULL) || (pData->iScrollbackLimit <= 0) ) {
		return XUI_OK;
	}
	sCopy = __xuiTerminalStrDup(sLine);
	if ( sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pLinks = __xuiTerminalCopyRowLinks(pRow, iColumns);
	if ( pRow != NULL && iColumns > 0 && pLinks == NULL ) {
		int i;
		for ( i = 0; i < iColumns; i++ ) {
			if ( pRow[i].iLinkId != 0 ) {
				xrtFree(sCopy);
				return XUI_ERROR_OUT_OF_MEMORY;
			}
		}
	}
	if ( pData->iScrollbackCount < pData->iScrollbackLimit ) {
		idx = (pData->iScrollbackStart + pData->iScrollbackCount) % pData->iScrollbackLimit;
		pData->iScrollbackCount++;
	} else {
		idx = pData->iScrollbackStart;
		__xuiTerminalClearScrollbackSlot(pData, idx);
		pData->iScrollbackStart = (pData->iScrollbackStart + 1) % pData->iScrollbackLimit;
	}
	pData->ppScrollback[idx] = sCopy;
	if ( pData->ppScrollbackLinks != NULL ) {
		pData->ppScrollbackLinks[idx] = pLinks;
	}
	if ( pData->pScrollbackColumns != NULL ) {
		pData->pScrollbackColumns[idx] = (pLinks != NULL) ? iColumns : 0;
	}
	return XUI_OK;
}

static const char* __xuiTerminalGetScrollbackLine(const xui_terminal_data_t* pData, int iLine)
{
	int idx;

	if ( (pData == NULL) || (pData->ppScrollback == NULL) || (iLine < 0) || (iLine >= pData->iScrollbackCount) || (pData->iScrollbackLimit <= 0) ) {
		return "";
	}
	idx = (pData->iScrollbackStart + iLine) % pData->iScrollbackLimit;
	return (pData->ppScrollback[idx] != NULL) ? pData->ppScrollback[idx] : "";
}

static const uint16_t* __xuiTerminalGetScrollbackLinks(const xui_terminal_data_t* pData, int iLine, int* pColumns)
{
	int idx;

	if ( pColumns != NULL ) *pColumns = 0;
	if ( (pData == NULL) || (pData->ppScrollbackLinks == NULL) || (iLine < 0) || (iLine >= pData->iScrollbackCount) || (pData->iScrollbackLimit <= 0) ) {
		return NULL;
	}
	idx = (pData->iScrollbackStart + iLine) % pData->iScrollbackLimit;
	if ( pColumns != NULL && pData->pScrollbackColumns != NULL ) {
		*pColumns = pData->pScrollbackColumns[idx];
	}
	return pData->ppScrollbackLinks[idx];
}

static void __xuiTerminalBlankRow(xui_terminal_data_t* pData, xui_terminal_cell_t* pCells, int iRow)
{
	xui_terminal_cell_t tBlank;
	int i;

	if ( (pData == NULL) || (pCells == NULL) || (iRow < 0) || (iRow >= pData->iRows) ) {
		return;
	}
	tBlank = __xuiTerminalBlankCell(pData);
	for ( i = 0; i < pData->iColumns; i++ ) {
		pCells[iRow * pData->iColumns + i] = tBlank;
	}
	__xuiTerminalMarkDirtyRow(pData, iRow);
}

static void __xuiTerminalClearCells(xui_terminal_data_t* pData, xui_terminal_cell_t* pCells)
{
	int y;

	if ( (pData == NULL) || (pCells == NULL) ) {
		return;
	}
	for ( y = 0; y < pData->iRows; y++ ) {
		__xuiTerminalBlankRow(pData, pCells, y);
	}
}

static void __xuiTerminalSetDefaultTabStops(uint8_t* pTabStops, int iColumns)
{
	int i;

	if ( pTabStops == NULL || iColumns <= 0 ) return;
	for ( i = 0; i < iColumns; i++ ) {
		pTabStops[i] = (uint8_t)((i > 0 && (i % 8) == 0) ? 1 : 0);
	}
}

static int __xuiTerminalNextTabStop(const xui_terminal_data_t* pData)
{
	int i;

	if ( pData == NULL || pData->iColumns <= 0 ) return 0;
	if ( pData->pTabStops != NULL ) {
		for ( i = pData->iCursorX + 1; i < pData->iColumns; i++ ) {
			if ( pData->pTabStops[i] != 0u ) return i;
		}
	}
	return pData->iColumns - 1;
}

static void __xuiTerminalSetTabStop(xui_terminal_data_t* pData, int iColumn, int bSet)
{
	if ( pData == NULL || pData->pTabStops == NULL || iColumn < 0 || iColumn >= pData->iColumns ) return;
	pData->pTabStops[iColumn] = (uint8_t)(bSet ? 1 : 0);
}

static void __xuiTerminalClearAllTabStops(xui_terminal_data_t* pData)
{
	if ( pData == NULL || pData->pTabStops == NULL || pData->iColumns <= 0 ) return;
	memset(pData->pTabStops, 0, (size_t)pData->iColumns);
}

static int __xuiTerminalResizeBuffers(xui_terminal_data_t* pData, int iColumns, int iRows)
{
	xui_terminal_cell_t* pNewMain;
	xui_terminal_cell_t* pNewAlt;
	uint8_t* pNewTabStops;
	uint8_t* pNewDirtyRows;
	xui_terminal_cell_t* pOldMain;
	xui_terminal_cell_t* pOldAlt;
	uint8_t* pOldTabStops;
	uint8_t* pOldDirtyRows;
	int iOldColumns;
	int iOldRows;
	int x;
	int y;
	int iCopyColumns;
	int iCopyRows;

	if ( (pData == NULL) || (iColumns <= 0) || (iRows <= 0) || (iColumns > 1000) || (iRows > 1000) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pData->iColumns == iColumns) && (pData->iRows == iRows) && (pData->pMain != NULL) && (pData->pAlt != NULL) && (pData->pTabStops != NULL) && (pData->pDirtyRows != NULL) ) {
		return XUI_OK;
	}
	pNewMain = (xui_terminal_cell_t*)xrtMalloc(sizeof(*pNewMain) * (size_t)iColumns * (size_t)iRows);
	pNewAlt = (xui_terminal_cell_t*)xrtMalloc(sizeof(*pNewAlt) * (size_t)iColumns * (size_t)iRows);
	pNewTabStops = (uint8_t*)xrtMalloc((size_t)iColumns);
	pNewDirtyRows = (uint8_t*)xrtCalloc((size_t)iRows, sizeof(*pNewDirtyRows));
	if ( (pNewMain == NULL) || (pNewAlt == NULL) || (pNewTabStops == NULL) || (pNewDirtyRows == NULL) ) {
		if ( pNewMain != NULL ) xrtFree(pNewMain);
		if ( pNewAlt != NULL ) xrtFree(pNewAlt);
		if ( pNewTabStops != NULL ) xrtFree(pNewTabStops);
		if ( pNewDirtyRows != NULL ) xrtFree(pNewDirtyRows);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pOldMain = pData->pMain;
	pOldAlt = pData->pAlt;
	pOldTabStops = pData->pTabStops;
	pOldDirtyRows = pData->pDirtyRows;
	iOldColumns = pData->iColumns;
	iOldRows = pData->iRows;
	pData->pMain = pNewMain;
	pData->pAlt = pNewAlt;
	pData->pTabStops = pNewTabStops;
	pData->pDirtyRows = pNewDirtyRows;
	pData->iDirtyRowCount = 0;
	pData->iColumns = iColumns;
	pData->iRows = iRows;
	__xuiTerminalClearCells(pData, pData->pMain);
	__xuiTerminalClearCells(pData, pData->pAlt);
	__xuiTerminalSetDefaultTabStops(pData->pTabStops, pData->iColumns);
	if ( pOldTabStops != NULL && iOldColumns > 0 ) {
		memcpy(pData->pTabStops, pOldTabStops, (size_t)__xuiTerminalMin(iOldColumns, iColumns));
		xrtFree(pOldTabStops);
	}
	if ( pOldDirtyRows != NULL ) {
		xrtFree(pOldDirtyRows);
	}
	if ( (pOldMain != NULL) && (pOldAlt != NULL) && (iOldColumns > 0) && (iOldRows > 0) ) {
		iCopyColumns = __xuiTerminalMin(iOldColumns, iColumns);
		iCopyRows = __xuiTerminalMin(iOldRows, iRows);
		for ( y = 0; y < iCopyRows; y++ ) {
			for ( x = 0; x < iCopyColumns; x++ ) {
				pData->pMain[y * iColumns + x] = pOldMain[y * iOldColumns + x];
				pData->pAlt[y * iColumns + x] = pOldAlt[y * iOldColumns + x];
			}
		}
		xrtFree(pOldMain);
		xrtFree(pOldAlt);
	}
	pData->iCursorX = __xuiTerminalMin(pData->iCursorX, iColumns - 1);
	pData->iCursorY = __xuiTerminalMin(pData->iCursorY, iRows - 1);
	pData->iSavedCursorX = __xuiTerminalMin(pData->iSavedCursorX, iColumns - 1);
	pData->iSavedCursorY = __xuiTerminalMin(pData->iSavedCursorY, iRows - 1);
	pData->iScrollTop = 0;
	pData->iScrollBottom = iRows - 1;
	__xuiTerminalMarkFullCacheDirty(pData);
	return XUI_OK;
}

static int __xuiTerminalIsAtBottom(xui_terminal_data_t* pData)
{
	float fOffsetY;
	float fMaxY;
	int iOffsetLine;
	int iMaxLine;

	if ( pData == NULL ) return 1;
	fOffsetY = 0.0f;
	fMaxY = 0.0f;
	if ( xuiScrollModelGetOffset(&pData->tScroll, NULL, &fOffsetY) != XUI_OK ) return 1;
	if ( xuiScrollModelGetMaxOffset(&pData->tScroll, NULL, &fMaxY) != XUI_OK ) return 1;
	if ( pData->fCellHeight > 0.0f ) {
		iOffsetLine = (int)(fOffsetY / pData->fCellHeight);
		iMaxLine = (int)(fMaxY / pData->fCellHeight);
		return iOffsetLine >= iMaxLine;
	}
	return fOffsetY >= fMaxY - 1.0f;
}

static int __xuiTerminalScrollToBottom(xui_terminal_data_t* pData)
{
	float fOffsetX;
	float fOffsetY;
	float fMaxY;

	if ( pData == NULL ) return 0;
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	fMaxY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, &fOffsetX, &fOffsetY);
	if ( xuiScrollModelGetMaxOffset(&pData->tScroll, NULL, &fMaxY) != XUI_OK ) return 0;
	(void)xuiScrollModelSetOffset(&pData->tScroll, fOffsetX, fMaxY);
	return fOffsetY != fMaxY;
}

static void __xuiTerminalSyncScrollModel(xui_widget pWidget, xui_terminal_data_t* pData)
{
	xui_rect_t tViewport;
	float fContentHeight;
	float fVisibleHeight;
	int bFollowBottom;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	bFollowBottom = __xuiTerminalIsAtBottom(pData);
	tViewport = xuiWidgetGetContentRect(pWidget);
	xuiScrollModelSetViewport(&pData->tScroll, tViewport);
	fVisibleHeight = (float)pData->iRows * pData->fCellHeight + pData->fPadding * 2.0f;
	if ( fVisibleHeight < tViewport.fH ) {
		fVisibleHeight = tViewport.fH;
	}
	fContentHeight = (float)pData->iScrollbackCount * pData->fCellHeight + fVisibleHeight;
	xuiScrollModelSetContentSize(&pData->tScroll, tViewport.fW, fContentHeight);
	if ( bFollowBottom ) {
		(void)__xuiTerminalScrollToBottom(pData);
	}
}

static void __xuiTerminalNotifyResize(xui_widget pWidget, xui_terminal_data_t* pData)
{
	xui_terminal_session_t* pSession;

	if ( pData == NULL || pData->iColumns <= 0 || pData->iRows <= 0 ) return;
	if ( pData->onResize != NULL ) {
		pData->onResize(pWidget, pData->iColumns, pData->iRows, pData->pResizeUser);
	}
	pSession = pData->pSession;
	if ( pSession != NULL ) {
		(void)xuiTerminalSessionResize(pSession, pData->iColumns, pData->iRows);
	}
}

static void __xuiTerminalInvalidate(xui_widget pWidget, xui_terminal_data_t* pData, uint32_t iFlags)
{
	if ( pData != NULL ) {
		pData->iChangeCount++;
		if ( (iFlags & XUI_WIDGET_DIRTY_CACHE) != 0u ) {
			__xuiTerminalMarkFullCacheDirty(pData);
		}
	}
	if ( pWidget != NULL ) {
		(void)xuiWidgetInvalidate(pWidget, iFlags);
	}
}

static void __xuiTerminalInvalidateDirty(xui_widget pWidget, xui_terminal_data_t* pData, uint32_t iFlags)
{
	if ( pData != NULL ) {
		pData->iChangeCount++;
	}
	if ( pWidget != NULL ) {
		(void)xuiWidgetInvalidate(pWidget, iFlags);
	}
}

static int __xuiTerminalScrollUp(xui_widget pWidget, xui_terminal_data_t* pData, int iTop, int iBottom, int iCount)
{
	xui_terminal_cell_t* pCells;
	char* sLine;
	int i;
	int iRows;
	int iWidth;

	if ( (pData == NULL) || (iCount <= 0) ) {
		return XUI_OK;
	}
	pCells = __xuiTerminalScreen(pData);
	if ( pCells == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iTop < 0 ) iTop = 0;
	if ( iBottom < 0 || iBottom >= pData->iRows ) iBottom = pData->iRows - 1;
	if ( iBottom < iTop ) return XUI_OK;
	iRows = iBottom - iTop + 1;
	if ( iCount > iRows ) iCount = iRows;
	if ( !pData->bAltScreen && iTop == 0 ) {
		for ( i = 0; i < iCount; i++ ) {
			sLine = __xuiTerminalSerializeRow(pData, pCells + (iTop + i) * pData->iColumns);
			if ( sLine != NULL ) {
				(void)__xuiTerminalPushScrollback(pData, sLine, pCells + (iTop + i) * pData->iColumns, pData->iColumns);
				xrtFree(sLine);
			}
		}
		__xuiTerminalMarkFullCacheDirty(pData);
	}
	iWidth = pData->iColumns;
	memmove(pCells + iTop * iWidth, pCells + (iTop + iCount) * iWidth, sizeof(*pCells) * (size_t)(iRows - iCount) * (size_t)iWidth);
	for ( i = iBottom - iCount + 1; i <= iBottom; i++ ) {
		__xuiTerminalBlankRow(pData, pCells, i);
	}
	__xuiTerminalMarkDirtyRows(pData, iTop, iBottom);
	__xuiTerminalSyncScrollModel(pWidget, pData);
	return XUI_OK;
}

static void __xuiTerminalLineFeed(xui_widget pWidget, xui_terminal_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	if ( pData->iCursorY >= pData->iScrollBottom ) {
		(void)__xuiTerminalScrollUp(pWidget, pData, pData->iScrollTop, pData->iScrollBottom, 1);
		pData->iCursorY = pData->iScrollBottom;
	} else if ( pData->iCursorY < pData->iRows - 1 ) {
		pData->iCursorY++;
	}
}

static void __xuiTerminalPutCodepoint(xui_widget pWidget, xui_terminal_data_t* pData, uint32_t iCodepoint)
{
	xui_terminal_cell_t* pCells;
	xui_terminal_cell_t tCell;
	int iWidth;
	int iIndex;

	if ( (pData == NULL) || (pData->iColumns <= 0) || (pData->iRows <= 0) ) {
		return;
	}
	pCells = __xuiTerminalScreen(pData);
	if ( pCells == NULL ) {
		return;
	}
	iWidth = __xuiTerminalCodepointWidth(iCodepoint);
	if ( iWidth > pData->iColumns ) {
		iWidth = 1;
	}
	if ( (pData->iCursorX + iWidth) > pData->iColumns ) {
		pData->iCursorX = 0;
		__xuiTerminalLineFeed(pWidget, pData);
	}
	tCell = __xuiTerminalBlankCell(pData);
	tCell.iCodepoint = iCodepoint;
	tCell.iFgColor = pData->iCurrentFg;
	tCell.iBgColor = pData->iCurrentBg;
	tCell.iFlags = pData->iCurrentFlags;
	tCell.iWidth = (uint8_t)iWidth;
	tCell.iLinkId = pData->iCurrentLinkId;
	if ( iWidth == 2 ) {
		tCell.iFlags |= XUI_TERMINAL_CELL_WIDE;
	}
	iIndex = pData->iCursorY * pData->iColumns + pData->iCursorX;
	pCells[iIndex] = tCell;
	if ( iWidth == 2 && pData->iCursorX + 1 < pData->iColumns ) {
		pCells[iIndex + 1] = __xuiTerminalBlankCell(pData);
		pCells[iIndex + 1].iFlags = XUI_TERMINAL_CELL_WIDE_CONT;
		pCells[iIndex + 1].iWidth = 0;
		pCells[iIndex + 1].iLinkId = pData->iCurrentLinkId;
	}
	pData->iCursorX += iWidth;
	__xuiTerminalMarkDirtyRow(pData, pData->iCursorY);
	if ( pData->iCursorX >= pData->iColumns ) {
		pData->iCursorX = 0;
		__xuiTerminalLineFeed(pWidget, pData);
	}
}

static void __xuiTerminalEraseLine(xui_terminal_data_t* pData, int iMode)
{
	xui_terminal_cell_t* pCells;
	xui_terminal_cell_t tBlank;
	int iStart;
	int iEnd;
	int i;

	if ( pData == NULL ) return;
	pCells = __xuiTerminalScreen(pData);
	if ( pCells == NULL ) return;
	tBlank = __xuiTerminalBlankCell(pData);
	iStart = 0;
	iEnd = pData->iColumns - 1;
	if ( iMode == 0 ) {
		iStart = pData->iCursorX;
	} else if ( iMode == 1 ) {
		iEnd = pData->iCursorX;
	}
	for ( i = iStart; i <= iEnd; i++ ) {
		pCells[pData->iCursorY * pData->iColumns + i] = tBlank;
	}
	__xuiTerminalMarkDirtyRow(pData, pData->iCursorY);
}

static void __xuiTerminalEraseDisplay(xui_terminal_data_t* pData, int iMode)
{
	xui_terminal_cell_t* pCells;
	xui_terminal_cell_t tBlank;
	int iStart;
	int iEnd;
	int i;

	if ( pData == NULL ) return;
	pCells = __xuiTerminalScreen(pData);
	if ( pCells == NULL ) return;
	tBlank = __xuiTerminalBlankCell(pData);
	iStart = 0;
	iEnd = pData->iColumns * pData->iRows - 1;
	if ( iMode == 0 ) {
		iStart = pData->iCursorY * pData->iColumns + pData->iCursorX;
	} else if ( iMode == 1 ) {
		iEnd = pData->iCursorY * pData->iColumns + pData->iCursorX;
	}
	for ( i = iStart; i <= iEnd; i++ ) {
		pCells[i] = tBlank;
	}
	__xuiTerminalMarkDirtyRows(pData, iStart / pData->iColumns, iEnd / pData->iColumns);
}

static int __xuiTerminalParam(const xui_terminal_parser_t* pParser, int iIndex, int iDefault)
{
	if ( (pParser == NULL) || (iIndex < 0) || (iIndex >= pParser->iParamCount) || (pParser->arrParams[iIndex] < 0) ) {
		return iDefault;
	}
	return pParser->arrParams[iIndex];
}

static void __xuiTerminalCsiReset(xui_terminal_parser_t* pParser)
{
	int i;

	if ( pParser == NULL ) return;
	for ( i = 0; i < XUI_TERMINAL_MAX_CSI_PARAMS; i++ ) {
		pParser->arrParams[i] = -1;
	}
	pParser->iParamCount = 0;
	pParser->iCurrentParam = -1;
	pParser->bParamActive = 0;
	pParser->bPrivate = 0;
}

static void __xuiTerminalCsiAppendDigit(xui_terminal_parser_t* pParser, int iDigit)
{
	if ( pParser == NULL ) return;
	if ( pParser->iParamCount <= 0 ) {
		pParser->iParamCount = 1;
		pParser->iCurrentParam = 0;
		pParser->arrParams[0] = 0;
	}
	if ( !pParser->bParamActive || pParser->arrParams[pParser->iCurrentParam] < 0 ) {
		pParser->arrParams[pParser->iCurrentParam] = 0;
	}
	pParser->arrParams[pParser->iCurrentParam] = pParser->arrParams[pParser->iCurrentParam] * 10 + iDigit;
	pParser->bParamActive = 1;
}

static void __xuiTerminalCsiNextParam(xui_terminal_parser_t* pParser)
{
	if ( pParser == NULL ) return;
	if ( pParser->iParamCount <= 0 ) {
		pParser->iParamCount = 1;
		pParser->iCurrentParam = 0;
	}
	if ( pParser->iParamCount < XUI_TERMINAL_MAX_CSI_PARAMS ) {
		pParser->iCurrentParam = pParser->iParamCount;
		pParser->arrParams[pParser->iCurrentParam] = -1;
		pParser->iParamCount++;
	}
	pParser->bParamActive = 0;
}

static void __xuiTerminalApplySgr(xui_terminal_data_t* pData, const xui_terminal_parser_t* pParser)
{
	int i;
	int p;
	int r;
	int g;
	int b;
	int count;

	if ( (pData == NULL) || (pParser == NULL) ) return;
	count = pParser->iParamCount;
	if ( count <= 0 ) {
		count = 1;
	}
	for ( i = 0; i < count; i++ ) {
		p = __xuiTerminalParam(pParser, i, 0);
		if ( p == 0 ) {
			pData->iCurrentFg = pData->iForegroundColor;
			pData->iCurrentBg = pData->iBackgroundColor;
			pData->iCurrentFlags = 0;
		} else if ( p == 1 ) {
			pData->iCurrentFlags |= XUI_TERMINAL_CELL_BOLD;
		} else if ( p == 2 ) {
			pData->iCurrentFlags |= XUI_TERMINAL_CELL_DIM;
		} else if ( p == 4 ) {
			pData->iCurrentFlags |= XUI_TERMINAL_CELL_UNDERLINE;
		} else if ( p == 7 ) {
			pData->iCurrentFlags |= XUI_TERMINAL_CELL_INVERSE;
		} else if ( p == 22 ) {
			pData->iCurrentFlags &= ~(XUI_TERMINAL_CELL_BOLD | XUI_TERMINAL_CELL_DIM);
		} else if ( p == 24 ) {
			pData->iCurrentFlags &= ~XUI_TERMINAL_CELL_UNDERLINE;
		} else if ( p == 27 ) {
			pData->iCurrentFlags &= ~XUI_TERMINAL_CELL_INVERSE;
		} else if ( p >= 30 && p <= 37 ) {
			pData->iCurrentFg = pData->arrPalette[p - 30];
		} else if ( p == 39 ) {
			pData->iCurrentFg = pData->iForegroundColor;
		} else if ( p >= 40 && p <= 47 ) {
			pData->iCurrentBg = pData->arrPalette[p - 40];
		} else if ( p == 49 ) {
			pData->iCurrentBg = pData->iBackgroundColor;
		} else if ( p >= 90 && p <= 97 ) {
			pData->iCurrentFg = pData->arrPalette[8 + p - 90];
		} else if ( p >= 100 && p <= 107 ) {
			pData->iCurrentBg = pData->arrPalette[8 + p - 100];
		} else if ( (p == 38 || p == 48) && (i + 2 < count) && (__xuiTerminalParam(pParser, i + 1, 0) == 5) ) {
			int idx = __xuiTerminalParam(pParser, i + 2, 0);
			if ( idx < 0 ) idx = 0;
			if ( idx > 255 ) idx = 255;
			if ( p == 38 ) pData->iCurrentFg = pData->arrPalette[idx];
			else pData->iCurrentBg = pData->arrPalette[idx];
			i += 2;
		} else if ( (p == 38 || p == 48) && (i + 4 < count) && (__xuiTerminalParam(pParser, i + 1, 0) == 2) ) {
			r = __xuiTerminalParam(pParser, i + 2, 0);
			g = __xuiTerminalParam(pParser, i + 3, 0);
			b = __xuiTerminalParam(pParser, i + 4, 0);
			if ( r < 0 ) r = 0;
			if ( r > 255 ) r = 255;
			if ( g < 0 ) g = 0;
			if ( g > 255 ) g = 255;
			if ( b < 0 ) b = 0;
			if ( b > 255 ) b = 255;
			if ( p == 38 ) pData->iCurrentFg = XUI_COLOR_RGBA(r, g, b, 255);
			else pData->iCurrentBg = XUI_COLOR_RGBA(r, g, b, 255);
			i += 4;
		}
	}
}

static void __xuiTerminalSetAltScreen(xui_terminal_data_t* pData, int bEnabled)
{
	if ( pData == NULL || pData->bAltScreen == bEnabled ) {
		return;
	}
	pData->bAltScreen = bEnabled ? 1 : 0;
	pData->iCursorX = 0;
	pData->iCursorY = 0;
	pData->iScrollTop = 0;
	pData->iScrollBottom = pData->iRows - 1;
	if ( bEnabled ) {
		__xuiTerminalClearCells(pData, pData->pAlt);
	}
	__xuiTerminalMarkFullCacheDirty(pData);
}

static void __xuiTerminalDispatchCsi(xui_widget pWidget, xui_terminal_data_t* pData, int iFinal)
{
	int n;
	int row;
	int col;
	int mode;
	int i;

	if ( (pData == NULL) || (pData->iColumns <= 0) || (pData->iRows <= 0) ) return;
	switch ( iFinal ) {
	case 'A':
		n = __xuiTerminalParam(&pData->tParser, 0, 1);
		if ( n <= 0 ) n = 1;
		pData->iCursorY = __xuiTerminalMax(0, pData->iCursorY - n);
		break;
	case 'B':
		n = __xuiTerminalParam(&pData->tParser, 0, 1);
		if ( n <= 0 ) n = 1;
		pData->iCursorY = __xuiTerminalMin(pData->iRows - 1, pData->iCursorY + n);
		break;
	case 'C':
		n = __xuiTerminalParam(&pData->tParser, 0, 1);
		if ( n <= 0 ) n = 1;
		pData->iCursorX = __xuiTerminalMin(pData->iColumns - 1, pData->iCursorX + n);
		break;
	case 'D':
		n = __xuiTerminalParam(&pData->tParser, 0, 1);
		if ( n <= 0 ) n = 1;
		pData->iCursorX = __xuiTerminalMax(0, pData->iCursorX - n);
		break;
	case 'H':
	case 'f':
		row = __xuiTerminalParam(&pData->tParser, 0, 1) - 1;
		col = __xuiTerminalParam(&pData->tParser, 1, 1) - 1;
		if ( row < 0 ) row = 0;
		if ( col < 0 ) col = 0;
		pData->iCursorY = __xuiTerminalMin(row, pData->iRows - 1);
		pData->iCursorX = __xuiTerminalMin(col, pData->iColumns - 1);
		break;
	case 'J':
		__xuiTerminalEraseDisplay(pData, __xuiTerminalParam(&pData->tParser, 0, 0));
		break;
	case 'K':
		__xuiTerminalEraseLine(pData, __xuiTerminalParam(&pData->tParser, 0, 0));
		break;
	case 'g':
		mode = __xuiTerminalParam(&pData->tParser, 0, 0);
		if ( mode == 0 ) {
			__xuiTerminalSetTabStop(pData, pData->iCursorX, 0);
		} else if ( mode == 3 ) {
			__xuiTerminalClearAllTabStops(pData);
		}
		break;
	case 'm':
		__xuiTerminalApplySgr(pData, &pData->tParser);
		break;
	case 'r':
		row = __xuiTerminalParam(&pData->tParser, 0, 1) - 1;
		col = __xuiTerminalParam(&pData->tParser, 1, pData->iRows) - 1;
		if ( row < 0 ) row = 0;
		if ( col < row || col >= pData->iRows ) col = pData->iRows - 1;
		pData->iScrollTop = row;
		pData->iScrollBottom = col;
		pData->iCursorX = 0;
		pData->iCursorY = 0;
		break;
	case 's':
		pData->iSavedCursorX = pData->iCursorX;
		pData->iSavedCursorY = pData->iCursorY;
		break;
	case 'u':
		pData->iCursorX = __xuiTerminalMin(pData->iSavedCursorX, pData->iColumns - 1);
		pData->iCursorY = __xuiTerminalMin(pData->iSavedCursorY, pData->iRows - 1);
		break;
	case 'h':
	case 'l':
		if ( pData->tParser.bPrivate ) {
			for ( i = 0; i < pData->tParser.iParamCount; i++ ) {
				mode = __xuiTerminalParam(&pData->tParser, i, 0);
				if ( mode == 25 ) {
					pData->bCursorVisible = (iFinal == 'h');
				} else if ( mode == 1049 ) {
					__xuiTerminalSetAltScreen(pData, iFinal == 'h');
				} else if ( mode == 2004 ) {
					pData->bBracketedPaste = (iFinal == 'h');
				}
			}
		}
		break;
	default:
		(void)pWidget;
		break;
	}
}

static void __xuiTerminalDispatchOsc8(xui_terminal_data_t* pData, const char* sPayload)
{
	const char* sUrl;
	uint16_t iLinkId;

	if ( pData == NULL || sPayload == NULL ) {
		return;
	}
	if ( sPayload[0] != '8' || sPayload[1] != ';' ) {
		return;
	}
	sUrl = strchr(sPayload + 2, ';');
	if ( sUrl == NULL ) {
		return;
	}
	sUrl++;
	if ( sUrl[0] == '\0' ) {
		pData->iCurrentLinkId = 0;
		return;
	}
	iLinkId = __xuiTerminalGetOrAddLink(pData, sUrl);
	if ( iLinkId != 0 ) {
		pData->iCurrentLinkId = iLinkId;
	}
}

static void __xuiTerminalDispatchOsc(xui_widget pWidget, xui_terminal_data_t* pData)
{
	char* sSemi;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->tParser.iOscSize <= 0) ) {
		return;
	}
	pData->tParser.sOsc[pData->tParser.iOscSize] = '\0';
	__xuiTerminalDispatchOsc8(pData, pData->tParser.sOsc);
	sSemi = strchr(pData->tParser.sOsc, ';');
	if ( (sSemi != NULL) && (pData->onTitle != NULL) &&
	     ((pData->tParser.sOsc[0] == '0') || (pData->tParser.sOsc[0] == '2')) ) {
		pData->onTitle(pWidget, sSemi + 1, pData->pTitleUser);
	}
}

static void __xuiTerminalProcessByte(xui_widget pWidget, xui_terminal_data_t* pData, uint8_t b)
{
	uint32_t cp;

	if ( pData == NULL ) return;
	if ( pData->tParser.iUtfExpected > 0 ) {
		if ( (b & 0xc0u) == 0x80u ) {
			pData->tParser.iUtfCodepoint = (pData->tParser.iUtfCodepoint << 6) | (uint32_t)(b & 0x3fu);
			pData->tParser.iUtfSeen++;
			if ( pData->tParser.iUtfSeen >= pData->tParser.iUtfExpected ) {
				cp = pData->tParser.iUtfCodepoint;
				pData->tParser.iUtfExpected = 0;
				pData->tParser.iUtfSeen = 0;
				pData->tParser.iUtfCodepoint = 0;
				__xuiTerminalPutCodepoint(pWidget, pData, cp);
			}
			return;
		}
		pData->tParser.iUtfExpected = 0;
		pData->tParser.iUtfSeen = 0;
		pData->tParser.iUtfCodepoint = 0;
	}
	switch ( pData->tParser.iState ) {
	case 0:
		if ( b == 0x1bu ) {
			pData->tParser.iState = 1;
		} else if ( b == '\a' ) {
			return;
		} else if ( b == '\b' ) {
			if ( pData->iCursorX > 0 ) pData->iCursorX--;
		} else if ( b == '\t' ) {
			pData->iCursorX = __xuiTerminalNextTabStop(pData);
		} else if ( b == '\n' ) {
			__xuiTerminalLineFeed(pWidget, pData);
		} else if ( b == '\r' ) {
			pData->iCursorX = 0;
		} else if ( b < 0x20u ) {
			return;
		} else if ( b < 0x80u ) {
			__xuiTerminalPutCodepoint(pWidget, pData, b);
		} else if ( (b & 0xe0u) == 0xc0u ) {
			pData->tParser.iUtfExpected = 1;
			pData->tParser.iUtfSeen = 0;
			pData->tParser.iUtfCodepoint = (uint32_t)(b & 0x1fu);
		} else if ( (b & 0xf0u) == 0xe0u ) {
			pData->tParser.iUtfExpected = 2;
			pData->tParser.iUtfSeen = 0;
			pData->tParser.iUtfCodepoint = (uint32_t)(b & 0x0fu);
		} else if ( (b & 0xf8u) == 0xf0u ) {
			pData->tParser.iUtfExpected = 3;
			pData->tParser.iUtfSeen = 0;
			pData->tParser.iUtfCodepoint = (uint32_t)(b & 0x07u);
		}
		break;
	case 1:
		if ( b == '[' ) {
			__xuiTerminalCsiReset(&pData->tParser);
			pData->tParser.iState = 2;
		} else if ( b == ']' ) {
			pData->tParser.iOscSize = 0;
			pData->tParser.iState = 3;
		} else if ( b == '7' ) {
			pData->iSavedCursorX = pData->iCursorX;
			pData->iSavedCursorY = pData->iCursorY;
			pData->tParser.iState = 0;
		} else if ( b == '8' ) {
			pData->iCursorX = __xuiTerminalMin(pData->iSavedCursorX, pData->iColumns - 1);
			pData->iCursorY = __xuiTerminalMin(pData->iSavedCursorY, pData->iRows - 1);
			pData->tParser.iState = 0;
		} else if ( b == 'H' ) {
			__xuiTerminalSetTabStop(pData, pData->iCursorX, 1);
			pData->tParser.iState = 0;
		} else {
			pData->tParser.iState = 0;
		}
		break;
	case 2:
		if ( b == '?' ) {
			pData->tParser.bPrivate = 1;
		} else if ( b >= '0' && b <= '9' ) {
			__xuiTerminalCsiAppendDigit(&pData->tParser, b - '0');
		} else if ( b == ';' ) {
			__xuiTerminalCsiNextParam(&pData->tParser);
		} else if ( b >= 0x40u && b <= 0x7eu ) {
			__xuiTerminalDispatchCsi(pWidget, pData, b);
			pData->tParser.iState = 0;
		}
		break;
	case 3:
		if ( b == '\a' ) {
			__xuiTerminalDispatchOsc(pWidget, pData);
			pData->tParser.iState = 0;
		} else if ( b == 0x1bu ) {
			pData->tParser.iState = 4;
		} else if ( pData->tParser.iOscSize < XUI_TERMINAL_MAX_OSC - 1 ) {
			pData->tParser.sOsc[pData->tParser.iOscSize++] = (char)b;
		}
		break;
	case 4:
		if ( b == '\\' ) {
			__xuiTerminalDispatchOsc(pWidget, pData);
			pData->tParser.iState = 0;
		} else {
			pData->tParser.iState = 3;
		}
		break;
	default:
		pData->tParser.iState = 0;
		break;
	}
}

static int __xuiTerminalProcessQueue(xui_widget pWidget, xui_terminal_data_t* pData, int iBudget)
{
	int iCount;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	if ( pData->iQueueSize <= 0 ) return XUI_OK;
	iCount = pData->iQueueSize;
	if ( (iBudget > 0) && (iCount > iBudget) ) {
		iCount = iBudget;
	}
	__xuiTerminalMarkDirtyRow(pData, pData->iCursorY);
	for ( i = 0; i < iCount; i++ ) {
		__xuiTerminalProcessByte(pWidget, pData, pData->pQueue[i]);
	}
	__xuiTerminalMarkDirtyRow(pData, pData->iCursorY);
	if ( iCount < pData->iQueueSize ) {
		memmove(pData->pQueue, pData->pQueue + iCount, (size_t)(pData->iQueueSize - iCount));
		pData->iQueueSize -= iCount;
	} else {
		pData->iQueueSize = 0;
	}
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalInvalidateDirty(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

static int __xuiTerminalAppendQueue(xui_terminal_data_t* pData, const void* pBytes, int iSize)
{
	uint8_t* pNew;
	int iCapacity;

	if ( (pData == NULL) || (pBytes == NULL) || (iSize < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iSize == 0 ) {
		return XUI_OK;
	}
	if ( pData->iQueueSize + iSize > pData->iQueueCapacity ) {
		iCapacity = pData->iQueueCapacity;
		if ( iCapacity < 256 ) iCapacity = 256;
		while ( iCapacity < pData->iQueueSize + iSize ) {
			iCapacity *= 2;
		}
		pNew = (uint8_t*)xrtRealloc(pData->pQueue, (size_t)iCapacity);
		if ( pNew == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pData->pQueue = pNew;
		pData->iQueueCapacity = iCapacity;
	}
	memcpy(pData->pQueue + pData->iQueueSize, pBytes, (size_t)iSize);
	pData->iQueueSize += iSize;
	return XUI_OK;
}

static int __xuiTerminalSerializeToBuffer(xui_terminal_data_t* pData, char* sBuffer, int iCapacity)
{
	const xui_terminal_cell_t* pCells;
	char* sLine;
	int iNeeded;
	int iOffset;
	int i;
	int n;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNeeded = 0;
	iOffset = 0;
	if ( (sBuffer != NULL) && (iCapacity > 0) ) {
		sBuffer[0] = '\0';
	}
	for ( i = 0; i < pData->iScrollbackCount; i++ ) {
		const char* sScroll = __xuiTerminalGetScrollbackLine(pData, i);
		n = (int)strlen(sScroll);
		iNeeded += n + 1;
		if ( (sBuffer != NULL) && (iCapacity > 0) && (iOffset < iCapacity - 1) ) {
			int iCopy = __xuiTerminalMin(n, iCapacity - 1 - iOffset);
			if ( iCopy > 0 ) memcpy(sBuffer + iOffset, sScroll, (size_t)iCopy);
			iOffset += iCopy;
			if ( iOffset < iCapacity - 1 ) sBuffer[iOffset++] = '\n';
			sBuffer[iOffset] = '\0';
		}
	}
	pCells = __xuiTerminalScreenConst(pData);
	for ( i = 0; i < pData->iRows; i++ ) {
		sLine = __xuiTerminalSerializeRow(pData, pCells + i * pData->iColumns);
		if ( sLine == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		n = (int)strlen(sLine);
		iNeeded += n + ((i + 1 < pData->iRows) ? 1 : 0);
		if ( (sBuffer != NULL) && (iCapacity > 0) && (iOffset < iCapacity - 1) ) {
			int iCopy = __xuiTerminalMin(n, iCapacity - 1 - iOffset);
			if ( iCopy > 0 ) memcpy(sBuffer + iOffset, sLine, (size_t)iCopy);
			iOffset += iCopy;
			if ( (i + 1 < pData->iRows) && (iOffset < iCapacity - 1) ) sBuffer[iOffset++] = '\n';
			sBuffer[iOffset] = '\0';
		}
		xrtFree(sLine);
	}
	return iNeeded;
}

static int __xuiTerminalLogicalLineCount(const xui_terminal_data_t* pData)
{
	return (pData != NULL) ? (pData->iScrollbackCount + pData->iRows) : 0;
}

static int __xuiTerminalClampLine(const xui_terminal_data_t* pData, int iLine)
{
	int iCount = __xuiTerminalLogicalLineCount(pData);
	if ( iCount <= 0 ) return 0;
	if ( iLine < 0 ) return 0;
	if ( iLine >= iCount ) return iCount - 1;
	return iLine;
}

static int __xuiTerminalClampColumn(const xui_terminal_data_t* pData, int iColumn)
{
	int iMax = (pData != NULL && pData->iColumns > 0) ? pData->iColumns : 0;
	if ( iColumn < 0 ) return 0;
	if ( iColumn > iMax ) return iMax;
	return iColumn;
}

static int __xuiTerminalIsWordChar(char ch)
{
	unsigned char c = (unsigned char)ch;
	return isalnum(c) || ch == '_' || ch == '-' || ch == '.' || ch == '/' || ch == '\\' || ch == ':' || ch == '@';
}

static char* __xuiTerminalLogicalLineCopy(xui_terminal_data_t* pData, int iLine)
{
	const xui_terminal_cell_t* pCells;

	if ( pData == NULL ) return NULL;
	iLine = __xuiTerminalClampLine(pData, iLine);
	if ( iLine < pData->iScrollbackCount ) {
		return __xuiTerminalStrDup(__xuiTerminalGetScrollbackLine(pData, iLine));
	}
	pCells = __xuiTerminalScreenConst(pData);
	if ( pCells == NULL ) return __xuiTerminalStrDup("");
	iLine -= pData->iScrollbackCount;
	if ( iLine < 0 || iLine >= pData->iRows ) return __xuiTerminalStrDup("");
	return __xuiTerminalSerializeRow(pData, pCells + iLine * pData->iColumns);
}

static int __xuiTerminalUtf8Next(const char* sText, int iRemaining, uint32_t* pCodepoint, int* pBytes)
{
	const unsigned char* p;
	uint32_t cp;
	int n;

	if ( (sText == NULL) || (sText[0] == '\0') || (iRemaining == 0) ) return 0;
	p = (const unsigned char*)sText;
	cp = p[0];
	n = 1;
	if ( (p[0] & 0x80u) == 0u ) {
		cp = p[0];
		n = 1;
	} else if ( (p[0] & 0xe0u) == 0xc0u && iRemaining != 1 && p[1] != '\0' && ((p[1] & 0xc0u) == 0x80u) ) {
		cp = ((uint32_t)(p[0] & 0x1fu) << 6) | (uint32_t)(p[1] & 0x3fu);
		n = 2;
	} else if ( (p[0] & 0xf0u) == 0xe0u && (iRemaining < 0 || iRemaining >= 3) &&
	            p[1] != '\0' && p[2] != '\0' &&
	            ((p[1] & 0xc0u) == 0x80u) && ((p[2] & 0xc0u) == 0x80u) ) {
		cp = ((uint32_t)(p[0] & 0x0fu) << 12) | ((uint32_t)(p[1] & 0x3fu) << 6) | (uint32_t)(p[2] & 0x3fu);
		n = 3;
	} else if ( (p[0] & 0xf8u) == 0xf0u && (iRemaining < 0 || iRemaining >= 4) &&
	            p[1] != '\0' && p[2] != '\0' && p[3] != '\0' &&
	            ((p[1] & 0xc0u) == 0x80u) && ((p[2] & 0xc0u) == 0x80u) && ((p[3] & 0xc0u) == 0x80u) ) {
		cp = ((uint32_t)(p[0] & 0x07u) << 18) | ((uint32_t)(p[1] & 0x3fu) << 12) |
		     ((uint32_t)(p[2] & 0x3fu) << 6) | (uint32_t)(p[3] & 0x3fu);
		n = 4;
	}
	if ( pCodepoint != NULL ) *pCodepoint = cp;
	if ( pBytes != NULL ) *pBytes = n;
	return 1;
}

static int __xuiTerminalUtf8DisplayColumns(const char* sText, int iBytes)
{
	uint32_t cp;
	int iOffset;
	int iStep;
	int iColumns;
	int iRemaining;

	if ( sText == NULL ) return 0;
	iOffset = 0;
	iColumns = 0;
	while ( sText[iOffset] != '\0' && (iBytes < 0 || iOffset < iBytes) ) {
		iRemaining = (iBytes < 0) ? -1 : (iBytes - iOffset);
		if ( !__xuiTerminalUtf8Next(sText + iOffset, iRemaining, &cp, &iStep) || iStep <= 0 ) break;
		if ( iBytes >= 0 && iOffset + iStep > iBytes ) break;
		iColumns += __xuiTerminalCodepointWidth(cp);
		iOffset += iStep;
	}
	return iColumns;
}

static int __xuiTerminalSearchMatchAt(const char* sHaystack, const char* sNeedle, uint32_t iFlags)
{
	while ( (sNeedle != NULL) && (*sNeedle != '\0') ) {
		char a;
		char b;
		if ( (sHaystack == NULL) || (*sHaystack == '\0') ) return 0;
		a = *sHaystack++;
		b = *sNeedle++;
		if ( (iFlags & XUI_TERMINAL_SEARCH_CASE_SENSITIVE) == 0u ) {
			a = (char)tolower((unsigned char)a);
			b = (char)tolower((unsigned char)b);
		}
		if ( a != b ) return 0;
	}
	return 1;
}

static int __xuiTerminalFindInLine(const char* sLine, const char* sText, uint32_t iFlags, int iStartColumn, int bForward, int* pColumn)
{
	int iByte;
	int iColumn;
	int iBest;

	if ( (sLine == NULL) || (sText == NULL) || (sText[0] == '\0') ) return 0;
	iBest = -1;
	for ( iByte = 0; sLine[iByte] != '\0'; iByte++ ) {
		if ( !__xuiTerminalSearchMatchAt(sLine + iByte, sText, iFlags) ) {
			continue;
		}
		iColumn = __xuiTerminalUtf8DisplayColumns(sLine, iByte);
		if ( bForward ) {
			if ( iColumn >= iStartColumn ) {
				if ( pColumn != NULL ) *pColumn = iColumn;
				return 1;
			}
		} else if ( iColumn <= iStartColumn ) {
			iBest = iColumn;
		}
	}
	if ( !bForward && iBest >= 0 ) {
		if ( pColumn != NULL ) *pColumn = iBest;
		return 1;
	}
	return 0;
}

static void __xuiTerminalClearFindMatch(xui_terminal_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->iSearchLine = -1;
	pData->iSearchColumn = 0;
	pData->iSearchLength = 0;
}

static void __xuiTerminalClearHoverLinkData(xui_terminal_data_t* pData)
{
	if ( pData == NULL ) return;
	if ( pData->sHoverLink != NULL ) {
		xrtFree(pData->sHoverLink);
		pData->sHoverLink = NULL;
	}
	pData->iHoverLinkLine = -1;
	pData->iHoverLinkColumn = 0;
	pData->iHoverLinkLength = 0;
}

static int __xuiTerminalUrlPrefixAt(const char* sText, int iOffset)
{
	static const char* arrPrefixes[] = {
		"http://",
		"https://",
		"file://",
		"mailto:"
	};
	int i;
	int j;

	if ( sText == NULL || iOffset < 0 ) return 0;
	for ( i = 0; i < (int)(sizeof(arrPrefixes) / sizeof(arrPrefixes[0])); i++ ) {
		for ( j = 0; arrPrefixes[i][j] != '\0'; j++ ) {
			char a = sText[iOffset + j];
			char b = arrPrefixes[i][j];
			if ( a == '\0' ) break;
			if ( (char)tolower((unsigned char)a) != b ) break;
		}
		if ( arrPrefixes[i][j] == '\0' ) return j;
	}
	return 0;
}

static int __xuiTerminalIsUrlChar(unsigned char c)
{
	if ( c <= 0x20u ) return 0;
	if ( c == '"' || c == '\'' || c == '<' || c == '>' ) return 0;
	return 1;
}

static int __xuiTerminalIsUrlTrimChar(unsigned char c)
{
	return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' ||
	       c == '?' || c == ')' || c == ']' || c == '}';
}

static int __xuiTerminalDetectUrlInText(const char* sLine, int iColumn, int* pStartColumn, int* pLength, int* pStartByte, int* pByteLength)
{
	int iLen;
	int iByte;
	int iPrefix;
	int iEnd;
	int iStartColumn;
	int iEndColumn;

	if ( sLine == NULL || iColumn < 0 ) return 0;
	iLen = (int)strlen(sLine);
	for ( iByte = 0; iByte < iLen; iByte++ ) {
		iPrefix = __xuiTerminalUrlPrefixAt(sLine, iByte);
		if ( iPrefix <= 0 ) continue;
		iEnd = iByte + iPrefix;
		while ( iEnd < iLen && __xuiTerminalIsUrlChar((unsigned char)sLine[iEnd]) ) iEnd++;
		while ( iEnd > iByte + iPrefix && __xuiTerminalIsUrlTrimChar((unsigned char)sLine[iEnd - 1]) ) iEnd--;
		if ( iEnd <= iByte + iPrefix ) continue;
		iStartColumn = __xuiTerminalUtf8DisplayColumns(sLine, iByte);
		iEndColumn = __xuiTerminalUtf8DisplayColumns(sLine, iEnd);
		if ( iColumn >= iStartColumn && iColumn < iEndColumn ) {
			if ( pStartColumn != NULL ) *pStartColumn = iStartColumn;
			if ( pLength != NULL ) *pLength = iEndColumn - iStartColumn;
			if ( pStartByte != NULL ) *pStartByte = iByte;
			if ( pByteLength != NULL ) *pByteLength = iEnd - iByte;
			return 1;
		}
	}
	return 0;
}

static int __xuiTerminalOscLinkAt(xui_terminal_data_t* pData, int iLine, int iColumn, char** ppUrl, int* pStartColumn, int* pLength)
{
	const xui_terminal_cell_t* pCells;
	const xui_terminal_cell_t* pRow;
	const uint16_t* pLinks;
	const char* sUrl;
	uint16_t iLinkId;
	int iScreenRow;
	int iLinkColumns;
	int iStart;
	int iEnd;
	int iUrlLength;

	if ( ppUrl != NULL ) *ppUrl = NULL;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iLine < 0 || iLine >= __xuiTerminalLogicalLineCount(pData) || iColumn < 0 ) return 0;
	if ( iLine < pData->iScrollbackCount ) {
		pLinks = __xuiTerminalGetScrollbackLinks(pData, iLine, &iLinkColumns);
		if ( pLinks == NULL || iColumn >= iLinkColumns ) return 0;
		iLinkId = pLinks[iColumn];
		if ( iLinkId == 0 ) return 0;
		sUrl = __xuiTerminalGetLinkUrl(pData, iLinkId);
		if ( sUrl == NULL || sUrl[0] == '\0' ) return 0;
		iStart = iColumn;
		while ( iStart > 0 && pLinks[iStart - 1] == iLinkId ) {
			iStart--;
		}
		iEnd = iColumn + 1;
		while ( iEnd < iLinkColumns && pLinks[iEnd] == iLinkId ) {
			iEnd++;
		}
	} else {
		if ( iColumn >= pData->iColumns ) return 0;
		iScreenRow = iLine - pData->iScrollbackCount;
		if ( iScreenRow < 0 || iScreenRow >= pData->iRows ) return 0;
		pCells = __xuiTerminalScreenConst(pData);
		if ( pCells == NULL ) return 0;
		pRow = pCells + iScreenRow * pData->iColumns;
		iLinkId = pRow[iColumn].iLinkId;
		if ( iLinkId == 0 ) return 0;
		sUrl = __xuiTerminalGetLinkUrl(pData, iLinkId);
		if ( sUrl == NULL || sUrl[0] == '\0' ) return 0;
		iStart = iColumn;
		while ( iStart > 0 && pRow[iStart - 1].iLinkId == iLinkId ) {
			iStart--;
		}
		iEnd = iColumn + 1;
		while ( iEnd < pData->iColumns && pRow[iEnd].iLinkId == iLinkId ) {
			iEnd++;
		}
	}
	if ( iLinkId == 0 ) return 0;
	iUrlLength = (int)strlen(sUrl);
	if ( pStartColumn != NULL ) *pStartColumn = iStart;
	if ( pLength != NULL ) *pLength = iEnd - iStart;
	if ( ppUrl != NULL ) {
		*ppUrl = __xuiTerminalStrDup(sUrl);
		if ( *ppUrl == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	}
	return iUrlLength;
}

static int __xuiTerminalLogicalUrlAt(xui_terminal_data_t* pData, int iLine, int iColumn, char** ppUrl, int* pStartColumn, int* pLength)
{
	char* sLine;
	char* sUrl;
	int iStartByte;
	int iByteLength;
	int iRet;

	if ( ppUrl != NULL ) *ppUrl = NULL;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iLine < 0 || iLine >= __xuiTerminalLogicalLineCount(pData) || iColumn < 0 ) return 0;
	iRet = __xuiTerminalOscLinkAt(pData, iLine, iColumn, ppUrl, pStartColumn, pLength);
	if ( iRet != 0 ) return iRet;
	sLine = __xuiTerminalLogicalLineCopy(pData, iLine);
	if ( sLine == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = __xuiTerminalDetectUrlInText(sLine, iColumn, pStartColumn, pLength, &iStartByte, &iByteLength);
	if ( iRet > 0 && ppUrl != NULL ) {
		sUrl = (char*)xrtMalloc((size_t)iByteLength + 1u);
		if ( sUrl == NULL ) {
			xrtFree(sLine);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(sUrl, sLine + iStartByte, (size_t)iByteLength);
		sUrl[iByteLength] = '\0';
		*ppUrl = sUrl;
	}
	xrtFree(sLine);
	return iRet ? iByteLength : 0;
}

static int __xuiTerminalSetHoverLink(xui_widget pWidget, xui_terminal_data_t* pData, int iLine, int iColumn, int iLength, const char* sUrl)
{
	char* sCopy;
	int bChanged;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sUrl == NULL || sUrl[0] == '\0' || iLine < 0 || iLength <= 0 ) {
		bChanged = (pData->sHoverLink != NULL || pData->iHoverLinkLine >= 0 || pData->iHoverLinkLength > 0);
		__xuiTerminalClearHoverLinkData(pData);
		if ( bChanged ) __xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_OK;
	}
	bChanged = (pData->sHoverLink == NULL) ||
	           (strcmp(pData->sHoverLink, sUrl) != 0) ||
	           (pData->iHoverLinkLine != iLine) ||
	           (pData->iHoverLinkColumn != iColumn) ||
	           (pData->iHoverLinkLength != iLength);
	if ( !bChanged ) return XUI_OK;
	sCopy = __xuiTerminalStrDup(sUrl);
	if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	__xuiTerminalClearHoverLinkData(pData);
	pData->sHoverLink = sCopy;
	pData->iHoverLinkLine = iLine;
	pData->iHoverLinkColumn = iColumn;
	pData->iHoverLinkLength = iLength;
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

static int __xuiTerminalUpdateHoverLink(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	char* sUrl;
	int iLine;
	int iColumn;
	int iStartColumn;
	int iLength;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) != XUI_OK ) {
		return __xuiTerminalSetHoverLink(pWidget, pData, -1, 0, 0, NULL);
	}
	sUrl = NULL;
	iStartColumn = 0;
	iLength = 0;
	iRet = __xuiTerminalLogicalUrlAt(pData, iLine, iColumn, &sUrl, &iStartColumn, &iLength);
	if ( iRet < 0 ) return iRet;
	if ( iRet > 0 && sUrl != NULL ) {
		iRet = __xuiTerminalSetHoverLink(pWidget, pData, iLine, iStartColumn, iLength, sUrl);
		xrtFree(sUrl);
		return iRet;
	}
	return __xuiTerminalSetHoverLink(pWidget, pData, -1, 0, 0, NULL);
}

static int __xuiTerminalSetSearchText(xui_terminal_data_t* pData, const char* sText, uint32_t iFlags)
{
	char* sCopy;

	if ( (pData == NULL) || (sText == NULL) || (sText[0] == '\0') ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pData->sSearchText != NULL) && (strcmp(pData->sSearchText, sText) == 0) && (pData->iSearchFlags == iFlags) ) {
		return XUI_OK;
	}
	sCopy = __xuiTerminalStrDup(sText);
	if ( sCopy == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( pData->sSearchText != NULL ) xrtFree(pData->sSearchText);
	pData->sSearchText = sCopy;
	pData->iSearchFlags = iFlags;
	__xuiTerminalClearFindMatch(pData);
	return XUI_OK;
}

static int __xuiTerminalFindNavigate(xui_widget pWidget, xui_terminal_data_t* pData, const char* sText, uint32_t iFlags, int bForward, int bContinue, int* pLine, int* pColumn)
{
	char* sLine;
	int iCount;
	int iStartLine;
	int iStartColumn;
	int iLine;
	int iColumn;
	int i;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (sText == NULL) || (sText[0] == '\0') ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTerminalSetSearchText(pData, sText, iFlags);
	if ( iRet != XUI_OK ) return iRet;
	iCount = __xuiTerminalLogicalLineCount(pData);
	if ( iCount <= 0 ) return 0;
	if ( bContinue && pData->iSearchLine >= 0 ) {
		iStartLine = __xuiTerminalClampLine(pData, pData->iSearchLine);
		iStartColumn = bForward ? (pData->iSearchColumn + __xuiTerminalMax(pData->iSearchLength, 1)) : (pData->iSearchColumn - 1);
	} else {
		iStartLine = bForward ? 0 : (iCount - 1);
		iStartColumn = bForward ? 0 : pData->iColumns;
	}
	if ( iStartColumn < 0 ) iStartColumn = -1;
	if ( iStartColumn > pData->iColumns ) iStartColumn = pData->iColumns;
	for ( i = 0; i < iCount; i++ ) {
		iLine = bForward ? ((iStartLine + i) % iCount) : ((iStartLine - i + iCount) % iCount);
		iColumn = (i == 0) ? iStartColumn : (bForward ? 0 : pData->iColumns);
		sLine = __xuiTerminalLogicalLineCopy(pData, iLine);
		if ( sLine == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		iRet = __xuiTerminalFindInLine(sLine, sText, iFlags, iColumn, bForward, &iColumn);
		xrtFree(sLine);
		if ( iRet ) {
			pData->iSearchLine = iLine;
			pData->iSearchColumn = __xuiTerminalClampColumn(pData, iColumn);
			pData->iSearchLength = __xuiTerminalMax(1, __xuiTerminalUtf8DisplayColumns(sText, -1));
			if ( pLine != NULL ) *pLine = pData->iSearchLine;
			if ( pColumn != NULL ) *pColumn = pData->iSearchColumn;
			__xuiTerminalSyncScrollModel(pWidget, pData);
			(void)xuiScrollModelSetOffset(&pData->tScroll, 0.0f, (float)pData->iSearchLine * pData->fCellHeight);
			__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return 1;
		}
	}
	__xuiTerminalClearFindMatch(pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return 0;
}

static void __xuiTerminalClearSelectionData(xui_terminal_data_t* pData)
{
	if ( pData == NULL ) return;
	pData->bSelectAll = 0;
	pData->bSelecting = 0;
	pData->iSelectAnchorLine = -1;
	pData->iSelectAnchorColumn = 0;
	pData->iSelectEndLine = -1;
	pData->iSelectEndColumn = 0;
}

static void __xuiTerminalSetSelectionRange(xui_terminal_data_t* pData, int iAnchorLine, int iAnchorColumn, int iEndLine, int iEndColumn)
{
	if ( pData == NULL ) return;
	pData->bSelectAll = 0;
	pData->iSelectAnchorLine = __xuiTerminalClampLine(pData, iAnchorLine);
	pData->iSelectAnchorColumn = __xuiTerminalClampColumn(pData, iAnchorColumn);
	pData->iSelectEndLine = __xuiTerminalClampLine(pData, iEndLine);
	pData->iSelectEndColumn = __xuiTerminalClampColumn(pData, iEndColumn);
}

static int __xuiTerminalSelectionRange(xui_terminal_data_t* pData, int* pLine0, int* pColumn0, int* pLine1, int* pColumn1)
{
	int l0;
	int c0;
	int l1;
	int c1;

	if ( pData == NULL ) return 0;
	if ( pData->bSelectAll ) {
		if ( pLine0 != NULL ) *pLine0 = 0;
		if ( pColumn0 != NULL ) *pColumn0 = 0;
		if ( pLine1 != NULL ) *pLine1 = __xuiTerminalLogicalLineCount(pData) - 1;
		if ( pColumn1 != NULL ) *pColumn1 = pData->iColumns;
		return __xuiTerminalLogicalLineCount(pData) > 0;
	}
	if ( pData->iSelectAnchorLine < 0 || pData->iSelectEndLine < 0 ) return 0;
	l0 = pData->iSelectAnchorLine;
	c0 = pData->iSelectAnchorColumn;
	l1 = pData->iSelectEndLine;
	c1 = pData->iSelectEndColumn;
	if ( (l0 > l1) || (l0 == l1 && c0 > c1) ) {
		int t;
		t = l0; l0 = l1; l1 = t;
		t = c0; c0 = c1; c1 = t;
	}
	if ( l0 == l1 && c0 == c1 ) return 0;
	if ( pLine0 != NULL ) *pLine0 = l0;
	if ( pColumn0 != NULL ) *pColumn0 = c0;
	if ( pLine1 != NULL ) *pLine1 = l1;
	if ( pColumn1 != NULL ) *pColumn1 = c1;
	return 1;
}

static int __xuiTerminalPointerToCell(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent, int* pLine, int* pColumn)
{
	xui_rect_t tWorld;
	float fOffsetY;
	float fLocalX;
	float fLocalY;
	int iTopLine;
	int iRow;
	int iColumn;
	int iLine;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX - pData->fPadding;
	fLocalY = pEvent->fY - tWorld.fY - pData->fPadding;
	fOffsetY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, NULL, &fOffsetY);
	iTopLine = (pData->fCellHeight > 0.0f) ? (int)(fOffsetY / pData->fCellHeight) : 0;
	if ( fLocalY < 0.0f ) {
		iRow = 0;
	} else {
		iRow = (pData->fCellHeight > 0.0f) ? (int)(fLocalY / pData->fCellHeight) : 0;
		if ( iRow >= pData->iRows ) iRow = pData->iRows - 1;
	}
	if ( fLocalX < 0.0f ) {
		iColumn = 0;
	} else {
		iColumn = (pData->fCellWidth > 0.0f) ? (int)(fLocalX / pData->fCellWidth) : 0;
		if ( iColumn > pData->iColumns ) iColumn = pData->iColumns;
	}
	iLine = __xuiTerminalClampLine(pData, iTopLine + iRow);
	if ( pLine != NULL ) *pLine = iLine;
	if ( pColumn != NULL ) *pColumn = __xuiTerminalClampColumn(pData, iColumn);
	return XUI_OK;
}

static int __xuiTerminalSelectionTextToBuffer(xui_terminal_data_t* pData, char* sBuffer, int iCapacity)
{
	char* sLine;
	int iLine0;
	int iColumn0;
	int iLine1;
	int iColumn1;
	int iLine;
	int iNeeded;
	int iOffset;
	int iLen;
	int iStart;
	int iEnd;
	int iCopy;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiTerminalSelectionRange(pData, &iLine0, &iColumn0, &iLine1, &iColumn1) ) {
		if ( sBuffer != NULL && iCapacity > 0 ) sBuffer[0] = '\0';
		return 0;
	}
	if ( pData->bSelectAll ) {
		return __xuiTerminalSerializeToBuffer(pData, sBuffer, iCapacity);
	}
	iNeeded = 0;
	iOffset = 0;
	if ( sBuffer != NULL && iCapacity > 0 ) sBuffer[0] = '\0';
	for ( iLine = iLine0; iLine <= iLine1; iLine++ ) {
		sLine = __xuiTerminalLogicalLineCopy(pData, iLine);
		if ( sLine == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		iLen = (int)strlen(sLine);
		iStart = (iLine == iLine0) ? iColumn0 : 0;
		iEnd = (iLine == iLine1) ? iColumn1 : iLen;
		if ( iStart < 0 ) iStart = 0;
		if ( iEnd < 0 ) iEnd = 0;
		if ( iStart > iLen ) iStart = iLen;
		if ( iEnd > iLen ) iEnd = iLen;
		if ( iEnd < iStart ) iEnd = iStart;
		iNeeded += iEnd - iStart;
		if ( sBuffer != NULL && iCapacity > 0 && iOffset < iCapacity - 1 ) {
			iCopy = __xuiTerminalMin(iEnd - iStart, iCapacity - 1 - iOffset);
			if ( iCopy > 0 ) memcpy(sBuffer + iOffset, sLine + iStart, (size_t)iCopy);
			iOffset += iCopy;
			sBuffer[iOffset] = '\0';
		}
		xrtFree(sLine);
		if ( iLine < iLine1 ) {
			iNeeded++;
			if ( sBuffer != NULL && iCapacity > 0 && iOffset < iCapacity - 1 ) {
				sBuffer[iOffset++] = '\n';
				sBuffer[iOffset] = '\0';
			}
		}
	}
	return iNeeded;
}

static int __xuiTerminalCopySingleLineSelectionText(xui_terminal_data_t* pData, char** ppText)
{
	char* sText;
	int iNeeded;
	int iRet;

	if ( ppText != NULL ) *ppText = NULL;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNeeded = __xuiTerminalSelectionTextToBuffer(pData, NULL, 0);
	if ( iNeeded <= 0 ) return iNeeded;
	sText = (char*)xrtMalloc((size_t)iNeeded + 1u);
	if ( sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = __xuiTerminalSelectionTextToBuffer(pData, sText, iNeeded + 1);
	if ( iRet <= 0 ) {
		xrtFree(sText);
		return iRet;
	}
	if ( strchr(sText, '\n') != NULL || strchr(sText, '\r') != NULL ) {
		xrtFree(sText);
		return 0;
	}
	if ( ppText != NULL ) {
		*ppText = sText;
	} else {
		xrtFree(sText);
	}
	return 1;
}

static int __xuiTerminalSelectWordAt(xui_widget pWidget, xui_terminal_data_t* pData, int iLine, int iColumn)
{
	char* sLine;
	int iLen;
	int iStart;
	int iEnd;

	(void)pWidget;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sLine = __xuiTerminalLogicalLineCopy(pData, iLine);
	if ( sLine == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iLen = (int)strlen(sLine);
	if ( iLen <= 0 ) {
		__xuiTerminalSetSelectionRange(pData, iLine, 0, iLine, 0);
		xrtFree(sLine);
		return XUI_OK;
	}
	if ( iColumn >= iLen ) iColumn = iLen - 1;
	if ( iColumn < 0 ) iColumn = 0;
	if ( isspace((unsigned char)sLine[iColumn]) ) {
		__xuiTerminalSetSelectionRange(pData, iLine, iColumn, iLine, iColumn);
		xrtFree(sLine);
		return XUI_OK;
	}
	if ( !__xuiTerminalIsWordChar(sLine[iColumn]) ) {
		__xuiTerminalSetSelectionRange(pData, iLine, iColumn, iLine, iColumn + 1);
		xrtFree(sLine);
		return XUI_OK;
	}
	iStart = iColumn;
	iEnd = iColumn + 1;
	while ( iStart > 0 && __xuiTerminalIsWordChar(sLine[iStart - 1]) ) iStart--;
	while ( iEnd < iLen && __xuiTerminalIsWordChar(sLine[iEnd]) ) iEnd++;
	__xuiTerminalSetSelectionRange(pData, iLine, iStart, iLine, iEnd);
	xrtFree(sLine);
	return XUI_OK;
}

static int __xuiTerminalSelectLineAt(xui_widget pWidget, xui_terminal_data_t* pData, int iLine)
{
	(void)pWidget;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalSetSelectionRange(pData, iLine, 0, iLine, pData->iColumns);
	return XUI_OK;
}

static int __xuiTerminalEmitInput(xui_widget pWidget, xui_terminal_data_t* pData, const uint8_t* pBytes, int iSize)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pBytes == NULL) || (iSize < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iSize == 0 ) return XUI_OK;
	__xuiTerminalSyncScrollModel(pWidget, pData);
	if ( __xuiTerminalScrollToBottom(pData) ) {
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( pData->onData != NULL ) {
		pData->onData(pWidget, pBytes, iSize, pData->pDataUser);
	}
	if ( pData->pSession != NULL ) {
		return xuiTerminalSessionWrite(pData->pSession, pBytes, iSize);
	}
	return XUI_OK;
}

static int __xuiTerminalEmitInputMaybeAlt(xui_widget pWidget, xui_terminal_data_t* pData, const uint8_t* pBytes, int iSize, uint32_t iModifiers)
{
	uint8_t arrBytes[32];

	if ( (iModifiers & XUI_MOD_ALT) == 0u ) {
		return __xuiTerminalEmitInput(pWidget, pData, pBytes, iSize);
	}
	if ( iSize < 0 || iSize + 1 > (int)sizeof(arrBytes) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	arrBytes[0] = 0x1bu;
	if ( iSize > 0 ) {
		memcpy(arrBytes + 1, pBytes, (size_t)iSize);
	}
	return __xuiTerminalEmitInput(pWidget, pData, arrBytes, iSize + 1);
}

static int __xuiTerminalInputModifierCode(uint32_t iModifiers)
{
	int iCode;

	iCode = 1;
	if ( (iModifiers & XUI_MOD_SHIFT) != 0u ) iCode += 1;
	if ( (iModifiers & XUI_MOD_ALT) != 0u ) iCode += 2;
	if ( (iModifiers & XUI_MOD_CTRL) != 0u ) iCode += 4;
	return iCode;
}

static int __xuiTerminalBuildModifiedCursor(char* sSeq, int iCapacity, char cFinal, uint32_t iModifiers)
{
	int iCode;

	if ( sSeq == NULL || iCapacity <= 0 ) return 0;
	iCode = __xuiTerminalInputModifierCode(iModifiers);
	if ( iCode <= 1 ) {
		if ( iCapacity < 4 ) return 0;
		sSeq[0] = 0x1b;
		sSeq[1] = '[';
		sSeq[2] = cFinal;
		return 3;
	}
	return snprintf(sSeq, (size_t)iCapacity, "\x1b[1;%d%c", iCode, cFinal);
}

static int __xuiTerminalBuildModifiedTilde(char* sSeq, int iCapacity, int iPrefix, uint32_t iModifiers)
{
	int iCode;

	if ( sSeq == NULL || iCapacity <= 0 ) return 0;
	iCode = __xuiTerminalInputModifierCode(iModifiers);
	return snprintf(sSeq, (size_t)iCapacity, "\x1b[%d;%d~", iPrefix, iCode);
}

static int __xuiTerminalPasteClipboard(xui_widget pWidget, xui_terminal_data_t* pData)
{
	xui_proxy pProxy;
	char sSmall[4096];
	char* sText;
	int iLength;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->clipboardGetText == NULL) ) return XUI_ERROR_UNSUPPORTED;
	memset(sSmall, 0, sizeof(sSmall));
	iLength = pProxy->clipboardGetText(pProxy, sSmall, (int)sizeof(sSmall));
	if ( iLength < 0 ) return iLength;
	if ( iLength == 0 ) return XUI_OK;
	if ( iLength < (int)sizeof(sSmall) ) {
		return xuiTerminalPasteText(pWidget, sSmall);
	}
	sText = (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = pProxy->clipboardGetText(pProxy, sText, iLength + 1);
	if ( iRet >= 0 ) {
		iRet = xuiTerminalPasteText(pWidget, sText);
	}
	xrtFree(sText);
	return iRet;
}

static int __xuiTerminalKeyDown(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	char sSeq[16];
	uint8_t b;
	int iSize;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) {
		return XUI_OK;
	}
	iSize = 0;
	switch ( pEvent->iKey ) {
	case XUI_KEY_ENTER:
		sSeq[0] = '\r'; iSize = 1; break;
	case XUI_KEY_BACKSPACE:
		b = ((pEvent->iModifiers & XUI_MOD_CTRL) != 0u) ? 0x08u : 0x7fu;
		return __xuiTerminalEmitInputMaybeAlt(pWidget, pData, &b, 1, pEvent->iModifiers) | XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_TAB:
		sSeq[0] = '\t'; iSize = 1; break;
	case XUI_KEY_UP:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedCursor(sSeq, (int)sizeof(sSeq), 'A', pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		memcpy(sSeq, "\x1b[A", 3); iSize = 3; break;
	case XUI_KEY_DOWN:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedCursor(sSeq, (int)sizeof(sSeq), 'B', pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		memcpy(sSeq, "\x1b[B", 3); iSize = 3; break;
	case XUI_KEY_RIGHT:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedCursor(sSeq, (int)sizeof(sSeq), 'C', pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		memcpy(sSeq, "\x1b[C", 3); iSize = 3; break;
	case XUI_KEY_LEFT:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedCursor(sSeq, (int)sizeof(sSeq), 'D', pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		memcpy(sSeq, "\x1b[D", 3); iSize = 3; break;
	case XUI_KEY_HOME:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedCursor(sSeq, (int)sizeof(sSeq), 'H', pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		memcpy(sSeq, "\x1b[H", 3); iSize = 3; break;
	case XUI_KEY_END:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedCursor(sSeq, (int)sizeof(sSeq), 'F', pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		memcpy(sSeq, "\x1b[F", 3); iSize = 3; break;
	case XUI_KEY_PAGE_UP:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedTilde(sSeq, (int)sizeof(sSeq), 5, pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		(void)xuiScrollModelScrollBy(&pData->tScroll, 0.0f, -(float)pData->iRows * pData->fCellHeight);
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_DOWN:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iSize = __xuiTerminalBuildModifiedTilde(sSeq, (int)sizeof(sSeq), 6, pEvent->iModifiers);
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sSeq, iSize) | XUI_EVENT_DISPATCH_STOP;
		}
		(void)xuiScrollModelScrollBy(&pData->tScroll, 0.0f, (float)pData->iRows * pData->fCellHeight);
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		if ( ((pEvent->iModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) == (XUI_MOD_CTRL | XUI_MOD_SHIFT)) &&
		     (pEvent->iKey == 'C' || pEvent->iKey == 'c') ) {
			return xuiTerminalCopySelection(pWidget) | XUI_EVENT_DISPATCH_STOP;
		}
		if ( ((pEvent->iModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) == (XUI_MOD_CTRL | XUI_MOD_SHIFT)) &&
		     (pEvent->iKey == 'V' || pEvent->iKey == 'v') ) {
			return __xuiTerminalPasteClipboard(pWidget, pData) | XUI_EVENT_DISPATCH_STOP;
		}
		if ( ((pEvent->iModifiers & XUI_MOD_CTRL) != 0u) && pEvent->iKey >= 'A' && pEvent->iKey <= 'Z' ) {
			b = (uint8_t)(pEvent->iKey - 'A' + 1);
			return __xuiTerminalEmitInputMaybeAlt(pWidget, pData, &b, 1, pEvent->iModifiers) | XUI_EVENT_DISPATCH_STOP;
		}
		if ( ((pEvent->iModifiers & XUI_MOD_ALT) != 0u) && pEvent->iKey >= 0x20 && pEvent->iKey <= 0x7e ) {
			b = (uint8_t)pEvent->iKey;
			if ( b >= 'A' && b <= 'Z' && (pEvent->iModifiers & XUI_MOD_SHIFT) == 0u ) {
				b = (uint8_t)(b - 'A' + 'a');
			}
			return __xuiTerminalEmitInputMaybeAlt(pWidget, pData, &b, 1, pEvent->iModifiers) | XUI_EVENT_DISPATCH_STOP;
		}
		break;
	}
	if ( iSize > 0 ) {
		return __xuiTerminalEmitInputMaybeAlt(pWidget, pData, (const uint8_t*)sSeq, iSize, pEvent->iModifiers) | XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTerminalPointerDown(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iLine;
	int iColumn;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	pContext = xuiWidgetGetContext(pWidget);
	(void)xuiSetFocusWidget(pContext, pWidget);
	if ( __xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) != XUI_OK ) return XUI_OK;
	if ( (pEvent->iModifiers & XUI_MOD_SHIFT) != 0u ) {
		if ( !__xuiTerminalSelectionRange(pData, NULL, NULL, NULL, NULL) ) {
			pData->iSelectAnchorLine = __xuiTerminalClampLine(pData, pData->iScrollbackCount + pData->iCursorY);
			pData->iSelectAnchorColumn = __xuiTerminalClampColumn(pData, pData->iCursorX);
		}
		pData->bSelectAll = 0;
		pData->iSelectEndLine = iLine;
		pData->iSelectEndColumn = iColumn;
	} else {
		__xuiTerminalSetSelectionRange(pData, iLine, iColumn, iLine, iColumn);
	}
	pData->bSelecting = 1;
	(void)xuiSetPointerCapture(pContext, pWidget);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTerminalPointerMove(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	int iLine;
	int iColumn;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	if ( !pData->bSelecting ) {
		return __xuiTerminalUpdateHoverLink(pWidget, pData, pEvent);
	}
	if ( (pEvent->iButtons & XUI_POINTER_BUTTON_LEFT) == 0u ) {
		pData->bSelecting = 0;
		if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		}
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( __xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) == XUI_OK ) {
		pData->iSelectEndLine = iLine;
		pData->iSelectEndColumn = iColumn;
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTerminalPointerUp(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	int iLine;
	int iColumn;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	if ( pData->bSelecting && __xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) == XUI_OK ) {
		pData->iSelectEndLine = iLine;
		pData->iSelectEndColumn = iColumn;
	}
	pData->bSelecting = 0;
	if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTerminalPointerDoubleClick(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	int iLine;
	int iColumn;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	if ( __xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) != XUI_OK ) return XUI_EVENT_DISPATCH_STOP;
	iRet = __xuiTerminalSelectWordAt(pWidget, pData, iLine, iColumn);
	pData->bSelecting = 0;
	pData->bDoubleClickPending = 1;
	pData->iDoubleClickLine = iLine;
	pData->fDoubleClickX = pEvent->fX;
	pData->fDoubleClickY = pEvent->fY;
	pData->fDoubleClickTime = xrtTimer();
	if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTerminalPointerClick(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	char* sUrl;
	double fNow;
	float fDX;
	float fDY;
	int iLine;
	int iColumn;
	int iStartColumn;
	int iLength;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	sUrl = NULL;
	iStartColumn = 0;
	iLength = 0;
	if ( pData->onLink != NULL &&
	     __xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) == XUI_OK &&
	     __xuiTerminalLogicalUrlAt(pData, iLine, iColumn, &sUrl, &iStartColumn, &iLength) > 0 &&
	     sUrl != NULL ) {
		(void)iStartColumn;
		(void)iLength;
		pData->onLink(pWidget, sUrl, pData->pLinkUser);
		xrtFree(sUrl);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( sUrl != NULL ) xrtFree(sUrl);
	if ( !pData->bDoubleClickPending ) return XUI_EVENT_DISPATCH_STOP;
	fNow = xrtTimer();
	fDX = pEvent->fX - pData->fDoubleClickX;
	fDY = pEvent->fY - pData->fDoubleClickY;
	if ( ((fNow - pData->fDoubleClickTime) <= XUI_TERMINAL_TRIPLE_CLICK_SECONDS) &&
	     ((fDX * fDX + fDY * fDY) <= (XUI_TERMINAL_TRIPLE_CLICK_DISTANCE * XUI_TERMINAL_TRIPLE_CLICK_DISTANCE)) &&
	     (__xuiTerminalPointerToCell(pWidget, pData, pEvent, &iLine, &iColumn) == XUI_OK) &&
	     (iLine == pData->iDoubleClickLine) ) {
		(void)iColumn;
		iRet = __xuiTerminalSelectLineAt(pWidget, pData, iLine);
		pData->bDoubleClickPending = 0;
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	}
	pData->bDoubleClickPending = 0;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTerminalMenuTitleIndexForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_TERMINAL_MENU_COPY: return 0;
	case XUI_TERMINAL_MENU_PASTE: return 1;
	case XUI_TERMINAL_MENU_SELECT_ALL: return 2;
	case XUI_TERMINAL_MENU_CLEAR_SCREEN: return 3;
	case XUI_TERMINAL_MENU_CLEAR_SCROLLBACK: return 4;
	case XUI_TERMINAL_MENU_FIND: return 5;
	default: return -1;
	}
}

static int __xuiTerminalMenuTitleTranslationForCommand(int iCommand)
{
	switch ( iCommand ) {
	case XUI_TERMINAL_MENU_COPY: return XUI_TR_EDIT_COPY;
	case XUI_TERMINAL_MENU_PASTE: return XUI_TR_EDIT_PASTE;
	case XUI_TERMINAL_MENU_SELECT_ALL: return XUI_TR_EDIT_SELECT_ALL;
	case XUI_TERMINAL_MENU_CLEAR_SCREEN: return XUI_TR_TERMINAL_CLEAR_SCREEN;
	case XUI_TERMINAL_MENU_CLEAR_SCROLLBACK: return XUI_TR_TERMINAL_CLEAR_SCROLLBACK;
	case XUI_TERMINAL_MENU_FIND: return XUI_TR_FIND_TITLE;
	default: return 0;
	}
}

static const char* __xuiTerminalMenuTitleForCommand(xui_widget pWidget, xui_terminal_data_t* pData, int iCommand)
{
	xui_context pContext;
	int iIndex;
	int iTranslation;

	if ( pData == NULL ) return "";
	iIndex = __xuiTerminalMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_TERMINAL_MENU_TITLE_COUNT ) return "";
	if ( pData->arrMenuTitle[iIndex] != NULL ) return pData->arrMenuTitle[iIndex];
	pContext = (pWidget != NULL) ? xuiWidgetGetContext(pWidget) : NULL;
	iTranslation = __xuiTerminalMenuTitleTranslationForCommand(iCommand);
	return (iTranslation != 0) ? xuiTranslate(pContext, iTranslation) : "";
}

static int __xuiTerminalUpdateMenu(xui_widget pWidget, xui_terminal_data_t* pData)
{
	xui_menu_item_t arrItems[8];
	xui_proxy pProxy;
	char sProbe[4];
	int bHasSelection;
	int bCanPaste;
	int bCanFind;
	uint32_t iEnabled;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(arrItems, 0, sizeof(arrItems));
	iEnabled = XUI_MENU_ITEM_ENABLED;
	bHasSelection = (__xuiTerminalSelectionTextToBuffer(pData, NULL, 0) > 0);
	bCanPaste = 0;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->clipboardGetText != NULL) ) {
		memset(sProbe, 0, sizeof(sProbe));
		bCanPaste = (pProxy->clipboardGetText(pProxy, sProbe, (int)sizeof(sProbe)) > 0);
	}
	bCanFind = (__xuiTerminalCopySingleLineSelectionText(pData, NULL) > 0) ||
	           (pData->sSearchText != NULL && pData->sSearchText[0] != '\0');
	arrItems[0].sText = __xuiTerminalMenuTitleForCommand(pWidget, pData, XUI_TERMINAL_MENU_COPY);
	arrItems[0].sShortcut = "Ctrl+Shift+C";
	arrItems[0].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[0].iState = bHasSelection ? iEnabled : 0u;
	arrItems[0].iValue = XUI_TERMINAL_MENU_COPY;
	arrItems[1].sText = __xuiTerminalMenuTitleForCommand(pWidget, pData, XUI_TERMINAL_MENU_PASTE);
	arrItems[1].sShortcut = "Ctrl+Shift+V";
	arrItems[1].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[1].iState = bCanPaste ? iEnabled : 0u;
	arrItems[1].iValue = XUI_TERMINAL_MENU_PASTE;
	arrItems[2].sText = __xuiTerminalMenuTitleForCommand(pWidget, pData, XUI_TERMINAL_MENU_SELECT_ALL);
	arrItems[2].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[2].iState = iEnabled;
	arrItems[2].iValue = XUI_TERMINAL_MENU_SELECT_ALL;
	arrItems[3].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[4].sText = __xuiTerminalMenuTitleForCommand(pWidget, pData, XUI_TERMINAL_MENU_CLEAR_SCREEN);
	arrItems[4].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[4].iState = iEnabled;
	arrItems[4].iValue = XUI_TERMINAL_MENU_CLEAR_SCREEN;
	arrItems[5].sText = __xuiTerminalMenuTitleForCommand(pWidget, pData, XUI_TERMINAL_MENU_CLEAR_SCROLLBACK);
	arrItems[5].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[5].iState = iEnabled;
	arrItems[5].iValue = XUI_TERMINAL_MENU_CLEAR_SCROLLBACK;
	arrItems[6].iType = XUI_MENU_ITEM_SEPARATOR;
	arrItems[7].sText = __xuiTerminalMenuTitleForCommand(pWidget, pData, XUI_TERMINAL_MENU_FIND);
	arrItems[7].sShortcut = "Ctrl+F";
	arrItems[7].iType = XUI_MENU_ITEM_NORMAL;
	arrItems[7].iState = bCanFind ? iEnabled : 0u;
	arrItems[7].iValue = XUI_TERMINAL_MENU_FIND;
	return xuiMenuSetItems(pData->pMenu, arrItems, 8);
}

static int __xuiTerminalRunFindMenuCommand(xui_widget pWidget, xui_terminal_data_t* pData)
{
	char* sText;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	sText = NULL;
	iRet = __xuiTerminalCopySingleLineSelectionText(pData, &sText);
	if ( iRet < 0 ) return iRet;
	if ( iRet > 0 && sText != NULL ) {
		iRet = xuiTerminalFindText(pWidget, sText, 0, NULL, NULL);
		xrtFree(sText);
		return (iRet < 0) ? iRet : XUI_OK;
	}
	if ( pData->sSearchText != NULL && pData->sSearchText[0] != '\0' ) {
		iRet = xuiTerminalFindNext(pWidget, pData->sSearchText, pData->iSearchFlags, NULL, NULL);
		return (iRet < 0) ? iRet : XUI_OK;
	}
	return XUI_OK;
}

static void __xuiTerminalMenuSelect(xui_widget pMenu, int iIndex, int iValue, void* pUser)
{
	xui_widget pTerminal;
	xui_terminal_data_t* pData;

	(void)pMenu;
	(void)iIndex;
	pTerminal = (xui_widget)pUser;
	if ( pTerminal == NULL ) return;
	pData = (xui_terminal_data_t*)xuiWidgetGetTypeData(pTerminal);
	if ( pData == NULL ) return;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pTerminal), pTerminal);
	switch ( iValue ) {
	case XUI_TERMINAL_MENU_COPY:
		(void)xuiTerminalCopySelection(pTerminal);
		break;
	case XUI_TERMINAL_MENU_PASTE:
		(void)__xuiTerminalPasteClipboard(pTerminal, pData);
		break;
	case XUI_TERMINAL_MENU_SELECT_ALL:
		(void)xuiTerminalSelectAll(pTerminal);
		break;
	case XUI_TERMINAL_MENU_CLEAR_SCREEN:
		(void)xuiTerminalClear(pTerminal);
		break;
	case XUI_TERMINAL_MENU_CLEAR_SCROLLBACK:
		(void)xuiTerminalClearScrollback(pTerminal);
		break;
	case XUI_TERMINAL_MENU_FIND:
		(void)__xuiTerminalRunFindMenuCommand(pTerminal, pData);
		break;
	default:
		break;
	}
}

static int __xuiTerminalContextMenu(xui_widget pWidget, xui_terminal_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	float fOffsetY;
	float fX;
	float fY;
	int iTopLine;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	pData->bSelecting = 0;
	if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
		__xuiTerminalResolveStyle(pWidget, pData);
		tWorld = xuiWidgetGetWorldRect(pWidget);
		fOffsetY = 0.0f;
		(void)xuiScrollModelGetOffset(&pData->tScroll, NULL, &fOffsetY);
		iTopLine = (pData->fCellHeight > 0.0f) ? (int)(fOffsetY / pData->fCellHeight) : 0;
		fX = tWorld.fX + pData->fPadding + (float)pData->iCursorX * pData->fCellWidth;
		fY = tWorld.fY + pData->fPadding + (float)(pData->iCursorY + pData->iScrollbackCount - iTopLine + 1) * pData->fCellHeight;
		if ( fX < tWorld.fX ) fX = tWorld.fX;
		if ( fY < tWorld.fY ) fY = tWorld.fY;
		if ( fX > tWorld.fX + tWorld.fW ) fX = tWorld.fX + tWorld.fW;
		if ( fY > tWorld.fY + tWorld.fH ) fY = tWorld.fY + tWorld.fH;
	} else {
		fX = pEvent->fX;
		fY = pEvent->fY;
	}
	(void)xuiTerminalOpenMenu(pWidget, fX, fY);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTerminalEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_terminal_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_OK;
	pData = (xui_terminal_data_t*)xuiWidgetGetTypeData(pWidget);
	if ( pData == NULL ) return XUI_OK;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_LEAVE:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			return __xuiTerminalSetHoverLink(pWidget, pData, -1, 0, 0, NULL);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiTerminalPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_MOVE:
		return __xuiTerminalPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiTerminalPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		return __xuiTerminalPointerClick(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
		return __xuiTerminalPointerDoubleClick(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bSelecting = 0;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_CONTEXT_MENU:
		return __xuiTerminalContextMenu(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			(void)xuiScrollModelScrollBy(&pData->tScroll, -pEvent->fWheelX, -pEvent->fWheelY);
			__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_TEXT:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_BUBBLE && pEvent->iTextSize > 0 ) {
			return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)pEvent->sText, pEvent->iTextSize) | XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		return __xuiTerminalKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_BOUNDS_CHANGED:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( pEvent->iType == XUI_EVENT_BOUNDS_CHANGED ) {
			(void)xuiTerminalFit(pWidget);
		}
		__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiTerminalInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOUBLE_CLICK, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_TEXT, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiTerminalEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiTerminalEvent, NULL);
	return iRet;
}

static int __xuiTerminalInitMenu(xui_widget pWidget, xui_terminal_data_t* pData)
{
	xui_menu_desc_t tDesc;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pOwner = pWidget;
	tDesc.pFont = pData->pFont;
	iRet = xuiMenuCreate(xuiWidgetGetContext(pWidget), &pData->pMenu, &tDesc);
	if ( iRet != XUI_OK ) {
		pData->pMenu = NULL;
		return iRet;
	}
	iRet = xuiMenuSetSelect(pData->pMenu, __xuiTerminalMenuSelect, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTerminalUpdateMenu(pWidget, pData);
}

static int __xuiTerminalFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiTerminalStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pColor == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiTerminalStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pValue == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiTerminalFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiTerminalStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	if ( pWidget == NULL ) return pBaseFont;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) return pFont;
	}
	return pBaseFont;
}

static uint32_t __xuiTerminalMapPaletteColor(uint32_t iColor, const uint32_t* arrOldPalette, const uint32_t* arrNewPalette)
{
	int i;

	if ( (arrOldPalette == NULL) || (arrNewPalette == NULL) ) return iColor;
	for ( i = 0; i < 256; i++ ) {
		if ( arrOldPalette[i] == iColor ) return arrNewPalette[i];
	}
	return iColor;
}

static void __xuiTerminalResolveStyle(xui_widget pWidget, xui_terminal_data_t* pData)
{
	xui_proxy pProxy;
	xui_vec2_t tSize;
	xui_font_metrics_t tMetrics;
	uint32_t arrOldPalette[256];
	uint32_t iOldBackground;
	uint32_t iOldForeground;
	uint32_t iOldCursor;
	uint32_t iOldSelection;
	uint32_t iOldSelectionText;
	uint32_t iOldSearch;
	uint32_t iOldFocus;
	uint32_t iOldLink;
	uint32_t iOldCurrentFg;
	uint32_t iOldCurrentBg;
	xui_font pOldFont;
	float fOldCellWidth;
	float fOldCellHeight;
	float fOldPadding;
	char sName[64];
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) return;
	memcpy(arrOldPalette, pData->arrPalette, sizeof(arrOldPalette));
	iOldBackground = pData->iBackgroundColor;
	iOldForeground = pData->iForegroundColor;
	iOldCursor = pData->iCursorColor;
	iOldSelection = pData->iSelectionColor;
	iOldSelectionText = pData->iSelectionTextColor;
	iOldSearch = pData->iSearchHighlightColor;
	iOldFocus = pData->iFocusColor;
	iOldLink = pData->iLinkHoverColor;
	iOldCurrentFg = pData->iCurrentFg;
	iOldCurrentBg = pData->iCurrentBg;
	pOldFont = pData->pFont;
	fOldCellWidth = pData->fCellWidth;
	fOldCellHeight = pData->fCellHeight;
	fOldPadding = pData->fPadding;

	pData->pFont = __xuiTerminalStyleFont(pWidget, (pData->pBaseFont != NULL) ? pData->pBaseFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget)));
	pData->iBackgroundColor = pData->iBaseBackgroundColor;
	pData->iForegroundColor = pData->iBaseForegroundColor;
	pData->iCursorColor = pData->iBaseCursorColor;
	pData->iSelectionColor = pData->iBaseSelectionColor;
	pData->iSelectionTextColor = pData->iBaseSelectionTextColor;
	pData->iSearchHighlightColor = pData->iBaseSearchHighlightColor;
	pData->iFocusColor = pData->iBaseFocusColor;
	pData->iLinkHoverColor = pData->iBaseLinkHoverColor;
	pData->fCellWidth = pData->fBaseCellWidth;
	pData->fCellHeight = pData->fBaseCellHeight;
	pData->fPadding = pData->fBasePadding;
	memcpy(pData->arrPalette, pData->arrBasePalette, sizeof(pData->arrPalette));

	(void)__xuiTerminalStyleColor(pWidget, "terminal.background.color", &pData->iBackgroundColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.foreground.color", &pData->iForegroundColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.cursor.color", &pData->iCursorColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.selection.color", &pData->iSelectionColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.selection.text_color", &pData->iSelectionTextColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.search.highlight_color", &pData->iSearchHighlightColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.focus.color", &pData->iFocusColor);
	(void)__xuiTerminalStyleColor(pWidget, "terminal.link.hover_color", &pData->iLinkHoverColor);
	for ( i = 0; i < 16; i++ ) {
		snprintf(sName, sizeof(sName), "terminal.palette.%d", i);
		(void)__xuiTerminalStyleColor(pWidget, sName, &pData->arrPalette[i]);
	}
	(void)__xuiTerminalStyleFloat(pWidget, "terminal.cell.width", &pData->fCellWidth);
	(void)__xuiTerminalStyleFloat(pWidget, "terminal.cell.height", &pData->fCellHeight);
	(void)__xuiTerminalStyleFloat(pWidget, "terminal.padding", &pData->fPadding);

	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pData->fCellWidth <= 0.0f && pProxy != NULL && pProxy->textMeasure != NULL ) {
		if ( pProxy->textMeasure(pProxy, pData->pFont, "M", &tSize) == XUI_OK && tSize.fX > 0.0f ) {
			pData->fCellWidth = tSize.fX;
		}
	}
	if ( pData->fCellHeight <= 0.0f && pProxy != NULL && pProxy->fontGetMetrics != NULL ) {
		memset(&tMetrics, 0, sizeof(tMetrics));
		if ( pProxy->fontGetMetrics(pProxy, pData->pFont, &tMetrics) == XUI_OK && tMetrics.fLineHeight > 0.0f ) {
			pData->fCellHeight = tMetrics.fLineHeight;
		}
	}
	if ( pData->fCellWidth <= 0.0f ) pData->fCellWidth = 8.0f;
	if ( pData->fCellHeight <= 0.0f ) pData->fCellHeight = 16.0f;
	if ( pData->fPadding < 0.0f ) pData->fPadding = 0.0f;

	if ( iOldCurrentFg == iOldForeground ) {
		pData->iCurrentFg = pData->iForegroundColor;
	} else {
		pData->iCurrentFg = __xuiTerminalMapPaletteColor(pData->iCurrentFg, arrOldPalette, pData->arrPalette);
	}
	if ( iOldCurrentBg == iOldBackground ) {
		pData->iCurrentBg = pData->iBackgroundColor;
	} else {
		pData->iCurrentBg = __xuiTerminalMapPaletteColor(pData->iCurrentBg, arrOldPalette, pData->arrPalette);
	}
	if ( (pData->pMenu != NULL) && (pOldFont != pData->pFont) ) {
		(void)xuiMenuSetFont(pData->pMenu, pData->pFont);
	}
	if ( (pOldFont != pData->pFont) ||
	     (fOldCellWidth != pData->fCellWidth) ||
	     (fOldCellHeight != pData->fCellHeight) ||
	     (fOldPadding != pData->fPadding) ||
	     (iOldBackground != pData->iBackgroundColor) ||
	     (iOldForeground != pData->iForegroundColor) ||
	     (iOldCursor != pData->iCursorColor) ||
	     (iOldSelection != pData->iSelectionColor) ||
	     (iOldSelectionText != pData->iSelectionTextColor) ||
	     (iOldSearch != pData->iSearchHighlightColor) ||
	     (iOldFocus != pData->iFocusColor) ||
	     (iOldLink != pData->iLinkHoverColor) ||
	     (memcmp(arrOldPalette, pData->arrPalette, sizeof(arrOldPalette)) != 0) ) {
		__xuiTerminalMarkFullCacheDirty(pData);
	}
}

static int __xuiTerminalContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_terminal_data_t* pData;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_terminal_data_t*)xuiWidgetGetTypeData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	pSize->fX = pData->fPadding * 2.0f + (float)pData->iColumns * pData->fCellWidth;
	pSize->fY = pData->fPadding * 2.0f + (float)pData->iRows * pData->fCellHeight;
	return XUI_OK;
}

static int __xuiTerminalArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	(void)tContentRect;
	(void)pUser;
	(void)xuiTerminalFit(pWidget);
	return XUI_OK;
}

static int __xuiTerminalDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pDraw == NULL) || (pFont == NULL) || (sText == NULL) || (sText[0] == '\0') ) {
		return XUI_OK;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiTerminalAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawText(pProxy, pDraw, pFont, sText, xuiInternalSnapRect(tRect), iColor, iFlags | XUI_TEXT_CLIP);
}

static int __xuiTerminalRenderSearchHighlightLine(xui_proxy pProxy, xui_draw_context pDraw, xui_terminal_data_t* pData, int iLogicalLine, float fY, float fWidth)
{
	xui_rect_t tMatch;
	int iStart;
	int iEnd;

	if ( pProxy == NULL || pDraw == NULL || pData == NULL || pProxy->drawRectFill == NULL ) return XUI_OK;
	if ( __xuiTerminalAlpha(pData->iSearchHighlightColor) == 0 ) return XUI_OK;
	if ( pData->iSearchLine != iLogicalLine || pData->iSearchLength <= 0 || pData->iSearchColumn < 0 ) return XUI_OK;
	iStart = __xuiTerminalClampColumn(pData, pData->iSearchColumn);
	iEnd = __xuiTerminalClampColumn(pData, pData->iSearchColumn + pData->iSearchLength);
	if ( iEnd <= iStart ) return XUI_OK;
	tMatch.fX = pData->fPadding + (float)iStart * pData->fCellWidth;
	tMatch.fY = fY;
	tMatch.fW = (float)(iEnd - iStart) * pData->fCellWidth;
	tMatch.fH = pData->fCellHeight;
	if ( tMatch.fW > fWidth ) tMatch.fW = fWidth;
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tMatch), pData->iSearchHighlightColor);
}

static int __xuiTerminalRenderSelectionLine(xui_proxy pProxy, xui_draw_context pDraw, xui_terminal_data_t* pData, int iLogicalLine, float fY, float fWidth)
{
	xui_rect_t tSel;
	int iLine0;
	int iColumn0;
	int iLine1;
	int iColumn1;
	int iStart;
	int iEnd;

	if ( pProxy == NULL || pDraw == NULL || pData == NULL || pProxy->drawRectFill == NULL ) return XUI_OK;
	if ( __xuiTerminalAlpha(pData->iSelectionColor) == 0 ) return XUI_OK;
	if ( !__xuiTerminalSelectionRange(pData, &iLine0, &iColumn0, &iLine1, &iColumn1) ) return XUI_OK;
	if ( iLogicalLine < iLine0 || iLogicalLine > iLine1 ) return XUI_OK;
	if ( pData->bSelectAll ) {
		iStart = 0;
		iEnd = pData->iColumns;
	} else if ( iLine0 == iLine1 ) {
		iStart = iColumn0;
		iEnd = iColumn1;
	} else if ( iLogicalLine == iLine0 ) {
		iStart = iColumn0;
		iEnd = pData->iColumns;
	} else if ( iLogicalLine == iLine1 ) {
		iStart = 0;
		iEnd = iColumn1;
	} else {
		iStart = 0;
		iEnd = pData->iColumns;
	}
	iStart = __xuiTerminalClampColumn(pData, iStart);
	iEnd = __xuiTerminalClampColumn(pData, iEnd);
	if ( iEnd <= iStart ) return XUI_OK;
	tSel.fX = pData->fPadding + (float)iStart * pData->fCellWidth;
	tSel.fY = fY;
	tSel.fW = (float)(iEnd - iStart) * pData->fCellWidth;
	tSel.fH = pData->fCellHeight;
	if ( tSel.fW > fWidth ) tSel.fW = fWidth;
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tSel), pData->iSelectionColor);
}

static int __xuiTerminalRenderHoverLinkLine(xui_proxy pProxy, xui_draw_context pDraw, xui_terminal_data_t* pData, int iLogicalLine, float fY)
{
	float fX0;
	float fX1;
	float fLineY;

	if ( pProxy == NULL || pProxy->drawLine == NULL || pData == NULL ) return XUI_OK;
	if ( __xuiTerminalAlpha(pData->iLinkHoverColor) == 0 ) return XUI_OK;
	if ( pData->sHoverLink == NULL || pData->iHoverLinkLine != iLogicalLine || pData->iHoverLinkLength <= 0 ) return XUI_OK;
	fX0 = pData->fPadding + (float)pData->iHoverLinkColumn * pData->fCellWidth;
	fX1 = pData->fPadding + (float)(pData->iHoverLinkColumn + pData->iHoverLinkLength) * pData->fCellWidth;
	fLineY = fY + pData->fCellHeight - 2.0f;
	return pProxy->drawLine(pProxy, pDraw, fX0, fLineY, fX1, fLineY, 1.0f, pData->iLinkHoverColor);
}

static int __xuiTerminalRenderScreenRow(xui_proxy pProxy, xui_draw_context pDraw, xui_widget pWidget, xui_terminal_data_t* pData, int iScreenRow, float fY)
{
	const xui_terminal_cell_t* pCells;
	char sRun[1024];
	char sUtf[8];
	xui_rect_t tCellRect;
	xui_rect_t tTextRect;
	uint32_t iFg;
	uint32_t iBg;
	uint32_t iFlags;
	int iRunStart;
	int iRunBytes;
	int x;
	int n;
	int iRet;
	int iTextFlags;
	float fX0;

	(void)pWidget;
	pCells = __xuiTerminalScreenConst(pData);
	if ( (pCells == NULL) || (iScreenRow < 0) || (iScreenRow >= pData->iRows) ) return XUI_OK;
	pCells += iScreenRow * pData->iColumns;
	for ( x = 0; x < pData->iColumns; x++ ) {
		iBg = pCells[x].iBgColor;
		if ( (pCells[x].iFlags & XUI_TERMINAL_CELL_INVERSE) != 0u ) {
			iBg = pCells[x].iFgColor;
		}
		if ( iBg != pData->iBackgroundColor && __xuiTerminalAlpha(iBg) != 0 && pProxy != NULL && pProxy->drawRectFill != NULL ) {
			tCellRect.fX = pData->fPadding + (float)x * pData->fCellWidth;
			tCellRect.fY = fY;
			tCellRect.fW = pData->fCellWidth;
			tCellRect.fH = pData->fCellHeight;
			iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tCellRect), iBg);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	x = 0;
	while ( x < pData->iColumns ) {
		while ( x < pData->iColumns &&
		        (((pCells[x].iFlags & XUI_TERMINAL_CELL_WIDE_CONT) != 0u) ||
		         (pCells[x].iCodepoint == 0u) ||
		         (pCells[x].iCodepoint == ' ')) ) {
			x++;
		}
		if ( x >= pData->iColumns ) break;
		iRunStart = x;
		iFg = pCells[x].iFgColor;
		iBg = pCells[x].iBgColor;
		iFlags = pCells[x].iFlags & (XUI_TERMINAL_CELL_BOLD | XUI_TERMINAL_CELL_DIM | XUI_TERMINAL_CELL_UNDERLINE | XUI_TERMINAL_CELL_INVERSE);
		iRunBytes = 0;
		while ( x < pData->iColumns &&
		        ((pCells[x].iFlags & XUI_TERMINAL_CELL_WIDE_CONT) == 0u) &&
		        (pCells[x].iCodepoint != 0u) &&
		        (pCells[x].iCodepoint != ' ') &&
		        (pCells[x].iFgColor == iFg) &&
		        (pCells[x].iBgColor == iBg) &&
		        ((pCells[x].iFlags & (XUI_TERMINAL_CELL_BOLD | XUI_TERMINAL_CELL_DIM | XUI_TERMINAL_CELL_UNDERLINE | XUI_TERMINAL_CELL_INVERSE)) == iFlags) ) {
			n = __xuiTerminalEncodeUtf8(pCells[x].iCodepoint, sUtf);
			if ( n <= 0 || iRunBytes + n >= (int)sizeof(sRun) - 1 ) break;
			memcpy(sRun + iRunBytes, sUtf, (size_t)n);
			iRunBytes += n;
			x += (pCells[x].iWidth > 0) ? pCells[x].iWidth : 1;
		}
		sRun[iRunBytes] = '\0';
		fX0 = pData->fPadding + (float)iRunStart * pData->fCellWidth;
		tTextRect.fX = fX0;
		tTextRect.fY = fY;
		tTextRect.fW = ((float)(x - iRunStart) + 1.0f) * pData->fCellWidth;
		tTextRect.fH = pData->fCellHeight;
		if ( (iFlags & XUI_TERMINAL_CELL_INVERSE) != 0u ) iFg = iBg;
		iTextFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
		if ( (iFlags & XUI_TERMINAL_CELL_UNDERLINE) != 0u ) iTextFlags |= XUI_TEXT_UNDERLINE;
		iRet = __xuiTerminalDrawText(pProxy, pDraw, pData->pFont, sRun, tTextRect, iFg, iTextFlags);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTerminalCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_terminal_data_t* pData;
	xui_proxy pProxy;
	xui_surface pCache;
	xui_rect_t tRect;
	xui_rect_t tLineRect;
	xui_rect_t tCursor;
	float fOffsetY;
	int iTopLine;
	int iLine;
	int i;
	int iScreenLine;
	int iRet;
	int bFullRender;
	uint32_t iFocusColor;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_terminal_data_t*)xuiWidgetGetTypeData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	pCache = xuiWidgetGetCacheSurface(pWidget, iStateId);
	if ( pCache != pData->pLastCacheSurface ) {
		__xuiTerminalMarkFullCacheDirty(pData);
	}
	__xuiTerminalResolveStyle(pWidget, pData);
	__xuiTerminalSyncScrollModel(pWidget, pData);
	tRect = xuiWidgetGetContentRect(pWidget);
	fOffsetY = 0.0f;
	(void)xuiScrollModelGetOffset(&pData->tScroll, NULL, &fOffsetY);
	iTopLine = (int)(fOffsetY / pData->fCellHeight);
	bFullRender = pData->bFullCacheDirty ||
	              !pData->bCacheRendered ||
	              pData->iLastRenderTopLine != iTopLine ||
	              pData->iLastRenderColumns != pData->iColumns ||
	              pData->iLastRenderRows != pData->iRows;
	if ( !bFullRender && iTopLine != pData->iScrollbackCount ) {
		bFullRender = 1;
	}
	if ( !bFullRender && pData->iDirtyRowCount <= 0 ) {
		return XUI_OK;
	}
	if ( bFullRender && pProxy->drawRectFill != NULL && __xuiTerminalAlpha(pData->iBackgroundColor) != 0 ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), pData->iBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pData->iRows; i++ ) {
		iLine = iTopLine + i;
		iScreenLine = iLine - pData->iScrollbackCount;
		tLineRect.fX = pData->fPadding;
		tLineRect.fY = pData->fPadding + (float)i * pData->fCellHeight;
		tLineRect.fW = tRect.fW - pData->fPadding * 2.0f;
		tLineRect.fH = pData->fCellHeight;
		if ( !bFullRender ) {
			if ( iScreenLine < 0 || iScreenLine >= pData->iRows || pData->pDirtyRows == NULL || pData->pDirtyRows[iScreenLine] == 0u ) {
				continue;
			}
			if ( pProxy->drawRectFill != NULL && __xuiTerminalAlpha(pData->iBackgroundColor) != 0 ) {
				iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tLineRect), pData->iBackgroundColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		iRet = __xuiTerminalRenderSearchHighlightLine(pProxy, pDraw, pData, iLine, tLineRect.fY, tLineRect.fW);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTerminalRenderSelectionLine(pProxy, pDraw, pData, iLine, tLineRect.fY, tLineRect.fW);
		if ( iRet != XUI_OK ) return iRet;
		if ( iLine < pData->iScrollbackCount ) {
			iRet = __xuiTerminalDrawText(pProxy, pDraw, pData->pFont, __xuiTerminalGetScrollbackLine(pData, iLine), tLineRect, pData->iForegroundColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP);
			if ( iRet != XUI_OK ) return iRet;
		} else {
			iRet = __xuiTerminalRenderScreenRow(pProxy, pDraw, pWidget, pData, iScreenLine, tLineRect.fY);
			if ( iRet != XUI_OK ) return iRet;
		}
		iRet = __xuiTerminalRenderHoverLinkLine(pProxy, pDraw, pData, iLine, tLineRect.fY);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->bCursorVisible && xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget && pProxy->drawRectStroke != NULL && __xuiTerminalAlpha(pData->iCursorColor) != 0 ) {
		tCursor.fX = pData->fPadding + (float)pData->iCursorX * pData->fCellWidth;
		tCursor.fY = pData->fPadding + (float)(pData->iCursorY + pData->iScrollbackCount - iTopLine) * pData->fCellHeight;
		tCursor.fW = pData->fCellWidth;
		tCursor.fH = pData->fCellHeight;
		iRet = pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tCursor), 1.0f, pData->iCursorColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget && pProxy->drawRectStroke != NULL ) {
		iFocusColor = xuiWidgetGetEnabled(pWidget) ? pData->iFocusColor : XUI_COLOR_RGBA(115, 135, 155, 150);
		if ( __xuiTerminalAlpha(iFocusColor) == 0 ) {
			return XUI_OK;
		}
		iRet = pProxy->drawRectStroke(pProxy, pDraw, xuiInternalInsetRect(xuiInternalSnapRect(tRect), 0.5f), 1.0f, iFocusColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	__xuiTerminalClearDirtyRows(pData);
	pData->bFullCacheDirty = 0;
	pData->bCacheRendered = 1;
	pData->pLastCacheSurface = pCache;
	pData->iLastRenderTopLine = iTopLine;
	pData->iLastRenderColumns = pData->iColumns;
	pData->iLastRenderRows = pData->iRows;
	return XUI_OK;
}

static int __xuiTerminalUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_terminal_data_t* pData;

	(void)fDelta;
	pData = (xui_terminal_data_t*)pUser;
	if ( pData == NULL ) return XUI_OK;
	if ( pData->pSession != NULL ) {
		(void)xuiTerminalSessionPoll(pData->pSession);
	}
	if ( pData->iQueueSize > 0 ) __xuiTerminalClearHoverLinkData(pData);
	return __xuiTerminalProcessQueue(pWidget, pData, pData->iParseBudget);
}

static void __xuiTerminalDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 640.0f;
	pLayout->fPreferredHeight = 360.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fGrow = 0.0f;
	pLayout->fShrink = 1.0f;
}

static void __xuiTerminalDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = 0;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiTerminalInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_terminal_data_t* pData;
	const xui_terminal_desc_t* pDesc;
	xui_context pContext;
	int iRet;
	int iColumns;
	int iRows;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_terminal_data_t*)pTypeData;
	pDesc = (const xui_terminal_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	pContext = xuiWidgetGetContext(pWidget);
	pData->pBaseFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->pFont = pData->pBaseFont;
	pData->iBaseBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0u) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(18, 24, 32, 255);
	pData->iBaseForegroundColor = (pDesc != NULL && pDesc->iForegroundColor != 0u) ? pDesc->iForegroundColor : XUI_COLOR_RGBA(220, 230, 240, 255);
	pData->iBaseCursorColor = (pDesc != NULL && pDesc->iCursorColor != 0u) ? pDesc->iCursorColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iBaseSelectionColor = (pDesc != NULL && pDesc->iSelectionColor != 0u) ? pDesc->iSelectionColor : XUI_COLOR_RGBA(64, 130, 220, 95);
	pData->iBaseSelectionTextColor = (pDesc != NULL && pDesc->iSelectionTextColor != 0u) ? pDesc->iSelectionTextColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iBaseSearchHighlightColor = (pDesc != NULL && pDesc->iSearchHighlightColor != 0u) ? pDesc->iSearchHighlightColor : XUI_COLOR_RGBA(255, 197, 66, 110);
	pData->iBaseFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0u) ? pDesc->iFocusColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iBaseLinkHoverColor = (pDesc != NULL && pDesc->iLinkHoverColor != 0u) ? pDesc->iLinkHoverColor : XUI_COLOR_RGBA(86, 156, 214, 255);
	pData->iBackgroundColor = pData->iBaseBackgroundColor;
	pData->iForegroundColor = pData->iBaseForegroundColor;
	pData->iCursorColor = pData->iBaseCursorColor;
	pData->iSelectionColor = pData->iBaseSelectionColor;
	pData->iSelectionTextColor = pData->iBaseSelectionTextColor;
	pData->iSearchHighlightColor = pData->iBaseSearchHighlightColor;
	pData->iFocusColor = pData->iBaseFocusColor;
	pData->iLinkHoverColor = pData->iBaseLinkHoverColor;
	pData->iCurrentFg = pData->iForegroundColor;
	pData->iCurrentBg = pData->iBackgroundColor;
	pData->iScrollbackLimit = (pDesc != NULL && pDesc->iScrollbackLimit > 0) ? pDesc->iScrollbackLimit : XUI_TERMINAL_DEFAULT_SCROLLBACK;
	pData->iParseBudget = (pDesc != NULL && pDesc->iParseBudgetBytes > 0) ? pDesc->iParseBudgetBytes : XUI_TERMINAL_DEFAULT_PARSE_BUDGET;
	pData->fBaseCellWidth = (pDesc != NULL) ? pDesc->fCellWidth : 0.0f;
	pData->fBaseCellHeight = (pDesc != NULL) ? pDesc->fCellHeight : 0.0f;
	pData->fBasePadding = (pDesc != NULL && pDesc->fPadding > 0.0f) ? pDesc->fPadding : 8.0f;
	pData->fCellWidth = pData->fBaseCellWidth;
	pData->fCellHeight = pData->fBaseCellHeight;
	pData->fPadding = pData->fBasePadding;
	pData->bCursorVisible = 1;
	__xuiTerminalClearSelectionData(pData);
	__xuiTerminalClearFindMatch(pData);
	__xuiTerminalClearHoverLinkData(pData);
	__xuiTerminalDefaultPalette(pData);
	__xuiTerminalResetParser(&pData->tParser);
	xuiScrollModelInit(&pData->tScroll);
	iRet = __xuiTerminalAllocScrollback(pData, pData->iScrollbackLimit);
	if ( iRet != XUI_OK ) return iRet;
	iColumns = (pDesc != NULL && pDesc->iColumns > 0) ? pDesc->iColumns : XUI_TERMINAL_DEFAULT_COLS;
	iRows = (pDesc != NULL && pDesc->iRows > 0) ? pDesc->iRows : XUI_TERMINAL_DEFAULT_ROWS;
	iRet = __xuiTerminalResizeBuffers(pData, iColumns, iRows);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetImeMode(pWidget, XUI_IME_ENABLED);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	iRet = __xuiTerminalInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTerminalInitMenu(pWidget, pData);
}

static void __xuiTerminalDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_terminal_data_t* pData;
	xui_widget pPopup;
	int i;

	(void)pUser;
	pData = (xui_terminal_data_t*)pTypeData;
	if ( pData == NULL ) return;
	if ( xuiInternalWidgetIsValid(pWidget) && xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	if ( pData->pMenu != NULL ) {
		pPopup = xuiMenuGetPopupWidget(pData->pMenu);
		if ( pPopup != NULL ) {
			xuiWidgetDestroy(pPopup);
		} else {
			xuiWidgetDestroy(pData->pMenu);
		}
		pData->pMenu = NULL;
	}
	if ( pData->pMain != NULL ) xrtFree(pData->pMain);
	if ( pData->pAlt != NULL ) xrtFree(pData->pAlt);
	if ( pData->pQueue != NULL ) xrtFree(pData->pQueue);
	if ( pData->pTabStops != NULL ) xrtFree(pData->pTabStops);
	if ( pData->pDirtyRows != NULL ) xrtFree(pData->pDirtyRows);
	if ( pData->sSearchText != NULL ) xrtFree(pData->sSearchText);
	if ( pData->sHoverLink != NULL ) xrtFree(pData->sHoverLink);
	for ( i = 0; i < XUI_TERMINAL_MENU_TITLE_COUNT; ++i ) {
		xrtFree(pData->arrMenuTitle[i]);
		pData->arrMenuTitle[i] = NULL;
	}
	__xuiTerminalFreeLinks(pData);
	__xuiTerminalFreeScrollback(pData);
	if ( pData->pSession != NULL && pData->pSession->pWidget == pWidget ) {
		pData->pSession->pWidget = NULL;
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiTerminalRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiTerminalRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;
	char sName[64];
	int i;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.foreground.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.cursor.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.selection.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.selection.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.search.highlight_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.link.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.cell.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.cell.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTerminalRegisterStyleProperty(pContext, pType, "terminal.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	for ( i = 0; i < 16; i++ ) {
		snprintf(sName, sizeof(sName), "terminal.palette.%d", i);
		__xuiTerminalRegisterStyleProperty(pContext, pType, sName, XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	}
	__xuiTerminalRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiTerminalGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_cache_policy_t tPolicy;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "terminal");
	if ( pType != NULL ) {
		__xuiTerminalRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "terminal";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_terminal_data_t);
	tDesc.onInit = __xuiTerminalInit;
	tDesc.onDestroy = __xuiTerminalDestroy;
	tDesc.onContentMeasure = __xuiTerminalContentMeasure;
	tDesc.onLayoutArrange = __xuiTerminalArrange;
	tDesc.onCacheRender = __xuiTerminalCacheRender;
	tDesc.onUpdate = __xuiTerminalUpdate;
	__xuiTerminalDefaultLayout(&tDesc.tLayout);
	__xuiTerminalDefaultCachePolicy(&tPolicy);
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiTerminalRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiTerminalCreate(xui_context pContext, xui_widget* ppWidget, const xui_terminal_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( ppWidget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiTerminalGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

static xui_terminal_data_t* __xuiTerminalGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "terminal");
	if ( pType == NULL || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_terminal_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API int xuiTerminalWrite(xui_widget pWidget, const void* pDataBytes, int iSize)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalAppendQueue(pData, pDataBytes, iSize);
}

XUI_API int xuiTerminalWriteText(xui_widget pWidget, const char* sText)
{
	if ( sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTerminalWrite(pWidget, sText, (int)strlen(sText));
}

XUI_API int xuiTerminalFlush(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iQueueSize > 0 ) __xuiTerminalClearHoverLinkData(pData);
	return __xuiTerminalProcessQueue(pWidget, pData, 0);
}

XUI_API int xuiTerminalClear(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	__xuiTerminalClearCells(pData, __xuiTerminalScreen(pData));
	pData->iCursorX = 0;
	pData->iCursorY = 0;
	__xuiTerminalClearSelectionData(pData);
	__xuiTerminalClearFindMatch(pData);
	__xuiTerminalClearHoverLinkData(pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalClearScrollback(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalFreeScrollback(pData);
	if ( __xuiTerminalAllocScrollback(pData, pData->iScrollbackLimit) != XUI_OK ) return XUI_ERROR_OUT_OF_MEMORY;
	__xuiTerminalClearSelectionData(pData);
	__xuiTerminalClearFindMatch(pData);
	__xuiTerminalClearHoverLinkData(pData);
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalSetScrollbackLimit(xui_widget pWidget, int iLimit)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	char** ppOldScrollback;
	uint16_t** ppOldLinks;
	int* pOldColumns;
	char** ppNewScrollback;
	uint16_t** ppNewLinks;
	int* pNewColumns;
	int iOldLimit;
	int iOldStart;
	int iOldCount;
	int iKeepCount;
	int iDropCount;
	int i;
	int iOldIndex;

	if ( pData == NULL || iLimit <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iLimit == pData->iScrollbackLimit ) return XUI_OK;
	ppNewScrollback = (char**)xrtCalloc((size_t)iLimit, sizeof(char*));
	ppNewLinks = (uint16_t**)xrtCalloc((size_t)iLimit, sizeof(uint16_t*));
	pNewColumns = (int*)xrtCalloc((size_t)iLimit, sizeof(int));
	if ( ppNewScrollback == NULL || ppNewLinks == NULL || pNewColumns == NULL ) {
		if ( ppNewScrollback != NULL ) xrtFree(ppNewScrollback);
		if ( ppNewLinks != NULL ) xrtFree(ppNewLinks);
		if ( pNewColumns != NULL ) xrtFree(pNewColumns);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	ppOldScrollback = pData->ppScrollback;
	ppOldLinks = pData->ppScrollbackLinks;
	pOldColumns = pData->pScrollbackColumns;
	iOldLimit = pData->iScrollbackLimit;
	iOldStart = pData->iScrollbackStart;
	iOldCount = pData->iScrollbackCount;
	iKeepCount = (iOldCount < iLimit) ? iOldCount : iLimit;
	iDropCount = iOldCount - iKeepCount;
	if ( iOldLimit <= 0 ) {
		iOldLimit = 1;
		iOldStart = 0;
		iOldCount = 0;
		iKeepCount = 0;
		iDropCount = 0;
	}
	for ( i = 0; i < iDropCount; ++i ) {
		iOldIndex = (iOldStart + i) % iOldLimit;
		if ( ppOldScrollback != NULL && ppOldScrollback[iOldIndex] != NULL ) xrtFree(ppOldScrollback[iOldIndex]);
		if ( ppOldLinks != NULL && ppOldLinks[iOldIndex] != NULL ) xrtFree(ppOldLinks[iOldIndex]);
	}
	for ( i = 0; i < iKeepCount; ++i ) {
		iOldIndex = (iOldStart + iDropCount + i) % iOldLimit;
		if ( ppOldScrollback != NULL ) {
			ppNewScrollback[i] = ppOldScrollback[iOldIndex];
			ppOldScrollback[iOldIndex] = NULL;
		}
		if ( ppOldLinks != NULL ) {
			ppNewLinks[i] = ppOldLinks[iOldIndex];
			ppOldLinks[iOldIndex] = NULL;
		}
		if ( pOldColumns != NULL ) {
			pNewColumns[i] = pOldColumns[iOldIndex];
			pOldColumns[iOldIndex] = 0;
		}
	}
	if ( ppOldScrollback != NULL ) xrtFree(ppOldScrollback);
	if ( ppOldLinks != NULL ) xrtFree(ppOldLinks);
	if ( pOldColumns != NULL ) xrtFree(pOldColumns);
	pData->ppScrollback = ppNewScrollback;
	pData->ppScrollbackLinks = ppNewLinks;
	pData->pScrollbackColumns = pNewColumns;
	pData->iScrollbackLimit = iLimit;
	pData->iScrollbackStart = 0;
	pData->iScrollbackCount = iKeepCount;
	__xuiTerminalClearSelectionData(pData);
	__xuiTerminalClearFindMatch(pData);
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetScrollbackLimit(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->iScrollbackLimit : 0;
}

XUI_API int xuiTerminalSetParseBudget(xui_widget pWidget, int iBytesPerUpdate)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL || iBytesPerUpdate < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iParseBudget = iBytesPerUpdate;
	return XUI_OK;
}

XUI_API int xuiTerminalGetParseBudget(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->iParseBudget : 0;
}

XUI_API int xuiTerminalSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pBaseFont = pFont;
	__xuiTerminalResolveStyle(pWidget, pData);
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API xui_font xuiTerminalGetFont(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->pBaseFont : NULL;
}

XUI_API int xuiTerminalSetMetrics(xui_widget pWidget, float fCellWidth, float fCellHeight, float fPadding)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);

	if ( pData == NULL || fCellWidth < 0.0f || fCellHeight < 0.0f || fPadding < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBaseCellWidth = fCellWidth;
	pData->fBaseCellHeight = fCellHeight;
	pData->fBasePadding = fPadding;
	__xuiTerminalResolveStyle(pWidget, pData);
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetMetrics(xui_widget pWidget, float* pCellWidth, float* pCellHeight, float* pPadding)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	if ( pCellWidth != NULL ) *pCellWidth = pData->fCellWidth;
	if ( pCellHeight != NULL ) *pCellHeight = pData->fCellHeight;
	if ( pPadding != NULL ) *pPadding = pData->fPadding;
	return XUI_OK;
}

XUI_API int xuiTerminalSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iForeground, uint32_t iCursor, uint32_t iSelection, uint32_t iSelectionText, uint32_t iSearchHighlight, uint32_t iFocus, uint32_t iLinkHover)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBaseBackgroundColor = iBackground;
	pData->iBaseForegroundColor = iForeground;
	pData->iBaseCursorColor = iCursor;
	pData->iBaseSelectionColor = iSelection;
	pData->iBaseSelectionTextColor = iSelectionText;
	pData->iBaseSearchHighlightColor = iSearchHighlight;
	pData->iBaseFocusColor = iFocus;
	pData->iBaseLinkHoverColor = iLinkHover;
	__xuiTerminalResolveStyle(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pForeground, uint32_t* pCursor, uint32_t* pSelection, uint32_t* pSelectionText, uint32_t* pSearchHighlight, uint32_t* pFocus, uint32_t* pLinkHover)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pForeground != NULL ) *pForeground = pData->iForegroundColor;
	if ( pCursor != NULL ) *pCursor = pData->iCursorColor;
	if ( pSelection != NULL ) *pSelection = pData->iSelectionColor;
	if ( pSelectionText != NULL ) *pSelectionText = pData->iSelectionTextColor;
	if ( pSearchHighlight != NULL ) *pSearchHighlight = pData->iSearchHighlightColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pLinkHover != NULL ) *pLinkHover = pData->iLinkHoverColor;
	return XUI_OK;
}

XUI_API int xuiTerminalSetLigaturesEnabled(xui_widget pWidget, int bEnabled)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bLigaturesEnabled = bEnabled ? 1 : 0;
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetLigaturesEnabled(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->bLigaturesEnabled : 0;
}

XUI_API int xuiTerminalFit(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	xui_rect_t tRect;
	int iColumns;
	int iRows;
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalResolveStyle(pWidget, pData);
	tRect = xuiWidgetGetContentRect(pWidget);
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XUI_OK;
	iColumns = (int)((tRect.fW - pData->fPadding * 2.0f) / pData->fCellWidth);
	iRows = (int)((tRect.fH - pData->fPadding * 2.0f) / pData->fCellHeight);
	if ( iColumns < 1 ) iColumns = 1;
	if ( iRows < 1 ) iRows = 1;
	if ( iColumns == pData->iColumns && iRows == pData->iRows ) return XUI_OK;
	iRet = __xuiTerminalResizeBuffers(pData, iColumns, iRows);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalNotifyResize(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalResize(xui_widget pWidget, int iColumns, int iRows)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTerminalResizeBuffers(pData, iColumns, iRows);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTerminalSyncScrollModel(pWidget, pData);
	__xuiTerminalNotifyResize(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetColumns(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->iColumns : 0;
}

XUI_API int xuiTerminalGetRows(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->iRows : 0;
}

XUI_API int xuiTerminalGetCursor(xui_widget pWidget, int* pColumn, int* pRow)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pColumn != NULL ) *pColumn = pData->iCursorX;
	if ( pRow != NULL ) *pRow = pData->iCursorY;
	return XUI_OK;
}

XUI_API int xuiTerminalSetInputCallback(xui_widget pWidget, xui_terminal_data_proc onData, void* pUser)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onData = onData;
	pData->pDataUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTerminalSetResizeCallback(xui_widget pWidget, xui_terminal_resize_proc onResize, void* pUser)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onResize = onResize;
	pData->pResizeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTerminalSetTitleCallback(xui_widget pWidget, xui_terminal_title_proc onTitle, void* pUser)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onTitle = onTitle;
	pData->pTitleUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTerminalSetLinkCallback(xui_widget pWidget, xui_terminal_link_proc onLink, void* pUser)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onLink = onLink;
	pData->pLinkUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTerminalSetPalette(xui_widget pWidget, int iIndex, uint32_t iColor)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL || iIndex < 0 || iIndex >= 256 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrBasePalette[iIndex] = iColor;
	__xuiTerminalResolveStyle(pWidget, pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API uint32_t xuiTerminalGetPalette(xui_widget pWidget, int iIndex)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL || iIndex < 0 || iIndex >= 256 ) return 0;
	__xuiTerminalResolveStyle(pWidget, pData);
	return pData->arrPalette[iIndex];
}

XUI_API xui_scroll_model_t* xuiTerminalGetScrollModel(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? &pData->tScroll : NULL;
}

XUI_API int xuiTerminalGetCell(xui_widget pWidget, int iColumn, int iRow, xui_terminal_cell_t* pCell)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	const xui_terminal_cell_t* pCells;
	if ( pData == NULL || pCell == NULL || iColumn < 0 || iRow < 0 || iColumn >= pData->iColumns || iRow >= pData->iRows ) return XUI_ERROR_INVALID_ARGUMENT;
	pCells = __xuiTerminalScreenConst(pData);
	*pCell = pCells[iRow * pData->iColumns + iColumn];
	pCell->iSize = sizeof(*pCell);
	return XUI_OK;
}

XUI_API int xuiTerminalSetBracketedPaste(xui_widget pWidget, int bEnabled)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bBracketedPaste = bEnabled ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiTerminalGetBracketedPaste(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->bBracketedPaste : 0;
}

XUI_API int xuiTerminalInputText(xui_widget pWidget, const char* sText)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sText, (int)strlen(sText));
}

XUI_API int xuiTerminalPasteText(xui_widget pWidget, const char* sText)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	int iRet;

	if ( pData == NULL || sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bBracketedPaste ) {
		iRet = __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)"\x1b[200~", 6);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sText, (int)strlen(sText));
		if ( iRet != XUI_OK ) return iRet;
		return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)"\x1b[201~", 6);
	}
	return __xuiTerminalEmitInput(pWidget, pData, (const uint8_t*)sText, (int)strlen(sText));
}

XUI_API int xuiTerminalSelectAll(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bSelectAll = 1;
	pData->bSelecting = 0;
	pData->iSelectAnchorLine = 0;
	pData->iSelectAnchorColumn = 0;
	pData->iSelectEndLine = __xuiTerminalLogicalLineCount(pData) - 1;
	pData->iSelectEndColumn = pData->iColumns;
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalClearSelection(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalClearSelectionData(pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalSetSelectionRange(xui_widget pWidget, int iAnchorLine, int iAnchorColumn, int iEndLine, int iEndColumn)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTerminalSetSelectionRange(pData, iAnchorLine, iAnchorColumn, iEndLine, iEndColumn);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetSelectionRange(xui_widget pWidget, int* pAnchorLine, int* pAnchorColumn, int* pEndLine, int* pEndColumn)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	int iLine0;
	int iColumn0;
	int iLine1;
	int iColumn1;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiTerminalSelectionRange(pData, &iLine0, &iColumn0, &iLine1, &iColumn1) ) {
		iLine0 = -1;
		iColumn0 = 0;
		iLine1 = -1;
		iColumn1 = 0;
	}
	if ( pAnchorLine != NULL ) *pAnchorLine = iLine0;
	if ( pAnchorColumn != NULL ) *pAnchorColumn = iColumn0;
	if ( pEndLine != NULL ) *pEndLine = iLine1;
	if ( pEndColumn != NULL ) *pEndColumn = iColumn1;
	return XUI_OK;
}

XUI_API int xuiTerminalGetSelectionText(xui_widget pWidget, char* sBuffer, int iCapacity)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalSelectionTextToBuffer(pData, sBuffer, iCapacity);
}

XUI_API int xuiTerminalCopySelection(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	xui_proxy pProxy;
	char* sText;
	int iNeeded;
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNeeded = xuiTerminalGetSelectionText(pWidget, NULL, 0);
	if ( iNeeded <= 0 ) return XUI_OK;
	sText = (char*)xrtMalloc((size_t)iNeeded + 1u);
	if ( sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	(void)xuiTerminalGetSelectionText(pWidget, sText, iNeeded + 1);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	iRet = (pProxy != NULL && pProxy->clipboardSetText != NULL) ? pProxy->clipboardSetText(pProxy, sText) : XUI_ERROR_UNSUPPORTED;
	xrtFree(sText);
	return iRet;
}

XUI_API int xuiTerminalSerializeText(xui_widget pWidget, char* sBuffer, int iCapacity)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalSerializeToBuffer(pData, sBuffer, iCapacity);
}

XUI_API int xuiTerminalFindText(xui_widget pWidget, const char* sText, uint32_t iFlags, int* pLine, int* pColumn)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalFindNavigate(pWidget, pData, sText, iFlags, 1, 0, pLine, pColumn);
}

XUI_API int xuiTerminalFindNext(xui_widget pWidget, const char* sText, uint32_t iFlags, int* pLine, int* pColumn)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalFindNavigate(pWidget, pData, sText, iFlags, 1, 1, pLine, pColumn);
}

XUI_API int xuiTerminalFindPrev(xui_widget pWidget, const char* sText, uint32_t iFlags, int* pLine, int* pColumn)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTerminalFindNavigate(pWidget, pData, sText, iFlags, 0, 1, pLine, pColumn);
}

XUI_API int xuiTerminalClearFind(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->sSearchText != NULL ) {
		xrtFree(pData->sSearchText);
		pData->sSearchText = NULL;
	}
	pData->iSearchFlags = 0;
	__xuiTerminalClearFindMatch(pData);
	__xuiTerminalInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiTerminalGetFindMatch(xui_widget pWidget, int* pLine, int* pColumn, int* pLength)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pLine != NULL ) *pLine = pData->iSearchLine;
	if ( pColumn != NULL ) *pColumn = pData->iSearchColumn;
	if ( pLength != NULL ) *pLength = pData->iSearchLength;
	return (pData->iSearchLine >= 0 && pData->iSearchLength > 0) ? 1 : 0;
}

XUI_API int xuiTerminalGetLinkAt(xui_widget pWidget, int iLine, int iColumn, char* sBuffer, int iCapacity, int* pStartColumn, int* pLength)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	char* sUrl;
	int iRet;
	int iCopy;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sBuffer != NULL && iCapacity > 0 ) sBuffer[0] = '\0';
	sUrl = NULL;
	iRet = __xuiTerminalLogicalUrlAt(pData, iLine, iColumn, &sUrl, pStartColumn, pLength);
	if ( iRet <= 0 ) return iRet;
	if ( sBuffer != NULL && iCapacity > 0 && sUrl != NULL ) {
		iCopy = __xuiTerminalMin(iRet, iCapacity - 1);
		if ( iCopy > 0 ) memcpy(sBuffer, sUrl, (size_t)iCopy);
		sBuffer[iCopy] = '\0';
	}
	if ( sUrl != NULL ) xrtFree(sUrl);
	return iRet;
}

XUI_API int xuiTerminalOpenMenu(xui_widget pWidget, float fX, float fY)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || (pData->pMenu == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTerminalUpdateMenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiMenuSetSelect(pData->pMenu, __xuiTerminalMenuSelect, pWidget);
	return xuiMenuOpenAt(pData->pMenu, pWidget, fX, fY);
}

XUI_API xui_widget xuiTerminalGetMenuWidget(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->pMenu : NULL;
}

XUI_API int xuiTerminalSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_terminal_data_t* pData;
	char* sNew;
	int iIndex;

	pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iIndex = __xuiTerminalMenuTitleIndexForCommand(iCommand);
	if ( iIndex < 0 || iIndex >= XUI_TERMINAL_MENU_TITLE_COUNT ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sTitle == NULL || sTitle[0] == '\0' ) {
		xrtFree(pData->arrMenuTitle[iIndex]);
		pData->arrMenuTitle[iIndex] = NULL;
		if ( pData->pMenu != NULL ) (void)__xuiTerminalUpdateMenu(pWidget, pData);
		return XUI_OK;
	}
	sNew = __xuiTerminalStrDup(sTitle);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	xrtFree(pData->arrMenuTitle[iIndex]);
	pData->arrMenuTitle[iIndex] = sNew;
	if ( pData->pMenu != NULL ) (void)__xuiTerminalUpdateMenu(pWidget, pData);
	return XUI_OK;
}

XUI_API const char* xuiTerminalGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_terminal_data_t* pData;

	pData = __xuiTerminalGetData(pWidget);
	return __xuiTerminalMenuTitleForCommand(pWidget, pData, iCommand);
}

XUI_API int xuiTerminalAttachSession(xui_widget pWidget, xui_terminal_session_t* pSession)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL || pSession == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pSession != NULL && pData->pSession->pWidget == pWidget ) pData->pSession->pWidget = NULL;
	pData->pSession = pSession;
	pSession->pWidget = pWidget;
	__xuiTerminalNotifyResize(pWidget, pData);
	if ( pSession->sPrompt != NULL && pSession->sPrompt[0] != '\0' ) {
		(void)xuiTerminalWriteText(pWidget, pSession->sPrompt);
		(void)xuiTerminalFlush(pWidget);
	}
	return XUI_OK;
}

XUI_API int xuiTerminalDetachSession(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pSession != NULL && pData->pSession->pWidget == pWidget ) pData->pSession->pWidget = NULL;
	pData->pSession = NULL;
	return XUI_OK;
}

static void __xuiTerminalProcessCloseHandles(xui_terminal_session_t* pSession)
{
#if defined(_WIN32) || defined(_WIN64)
	xui_terminal_conpty_api_t tApi;

	if ( pSession == NULL ) return;
	if ( pSession->hPseudoConsole != NULL ) {
		if ( __xuiTerminalConptyGetApi(&tApi) ) {
			tApi.closePseudoConsole(pSession->hPseudoConsole);
		}
		pSession->hPseudoConsole = NULL;
	}
	if ( pSession->hInputWrite != NULL ) {
		CloseHandle(pSession->hInputWrite);
		pSession->hInputWrite = NULL;
	}
	if ( pSession->hOutputRead != NULL ) {
		CloseHandle(pSession->hOutputRead);
		pSession->hOutputRead = NULL;
	}
	if ( pSession->hProcess != NULL ) {
		CloseHandle(pSession->hProcess);
		pSession->hProcess = NULL;
	}
#else
	(void)pSession;
#endif
}

static int __xuiTerminalSessionResizeTransport(xui_terminal_session_t* pSession, int iColumns, int iRows)
{
#if defined(_WIN32) || defined(_WIN64)
	xui_terminal_conpty_api_t tApi;
	HRESULT iResult;

	if ( pSession == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pSession->iKind != XUI_TERMINAL_SESSION_PROCESS ) return XUI_OK;
	if ( pSession->hPseudoConsole == NULL ) return XUI_OK;
	if ( !__xuiTerminalConptyGetApi(&tApi) ) return XUI_ERROR_UNSUPPORTED;
	iResult = tApi.resizePseudoConsole(pSession->hPseudoConsole, __xuiTerminalConptySize(iColumns, iRows));
	return SUCCEEDED(iResult) ? XUI_OK : XUI_ERROR_BACKEND_FAILED;
#else
	(void)pSession;
	(void)iColumns;
	(void)iRows;
	return XUI_ERROR_UNSUPPORTED;
#endif
}

static int __xuiTerminalProcessRefreshRunning(xui_terminal_session_t* pSession)
{
#if defined(_WIN32) || defined(_WIN64)
	DWORD iWait;

	if ( pSession == NULL || pSession->iKind != XUI_TERMINAL_SESSION_PROCESS ) return 0;
	if ( pSession->hProcess == NULL ) {
		pSession->bRunning = 0;
		return 0;
	}
	if ( !pSession->bRunning ) return 0;
	iWait = WaitForSingleObject(pSession->hProcess, 0);
	if ( iWait == WAIT_OBJECT_0 ) {
		pSession->bRunning = 0;
	}
	return pSession->bRunning;
#else
	(void)pSession;
	return 0;
#endif
}

static int __xuiTerminalProcessTerminate(xui_terminal_session_t* pSession)
{
#if defined(_WIN32) || defined(_WIN64)
	int iRet;

	if ( pSession == NULL || pSession->iKind != XUI_TERMINAL_SESSION_PROCESS ) return XUI_OK;
	iRet = XUI_OK;
	if ( pSession->hProcess != NULL && __xuiTerminalProcessRefreshRunning(pSession) ) {
		if ( !TerminateProcess(pSession->hProcess, 1u) ) {
			iRet = XUI_ERROR_BACKEND_FAILED;
		} else {
			(void)WaitForSingleObject(pSession->hProcess, 1000);
			pSession->bRunning = 0;
		}
	}
	__xuiTerminalProcessCloseHandles(pSession);
	return iRet;
#else
	(void)pSession;
	return XUI_ERROR_UNSUPPORTED;
#endif
}

XUI_API xui_terminal_session_t* xuiTerminalCreateFakeSession(const xui_terminal_session_desc_t* pDesc)
{
	xui_terminal_session_t* pSession;

	pSession = (xui_terminal_session_t*)xrtCalloc(1, sizeof(*pSession));
	if ( pSession == NULL ) return NULL;
	pSession->iKind = XUI_TERMINAL_SESSION_FAKE;
	pSession->bRunning = 1;
	pSession->bEcho = (pDesc == NULL) ? 1 : (pDesc->bEcho ? 1 : 0);
	if ( pDesc != NULL && pDesc->iSize >= sizeof(*pDesc) ) {
		pSession->onResize = pDesc->onResize;
		pSession->pResizeUser = pDesc->pResizeUser;
	}
	pSession->sPrompt = __xuiTerminalStrDup((pDesc != NULL && pDesc->sPrompt != NULL) ? pDesc->sPrompt : "");
	if ( pSession->sPrompt == NULL ) {
		xrtFree(pSession);
		return NULL;
	}
	return pSession;
}

static int __xuiTerminalProcessCreatePipeTransport(xui_terminal_session_t* pSession, const xui_terminal_process_desc_t* pDesc, char* sCommandLine)
{
#if defined(_WIN32) || defined(_WIN64)
	SECURITY_ATTRIBUTES tSecurity;
	STARTUPINFOA tStartup;
	PROCESS_INFORMATION tProcessInfo;
	HANDLE hInputRead;
	HANDLE hInputWrite;
	HANDLE hOutputRead;
	HANDLE hOutputWrite;
	BOOL bCreated;

	hInputRead = NULL;
	hInputWrite = NULL;
	hOutputRead = NULL;
	hOutputWrite = NULL;
	memset(&tSecurity, 0, sizeof(tSecurity));
	tSecurity.nLength = sizeof(tSecurity);
	tSecurity.bInheritHandle = TRUE;
	if ( !CreatePipe(&hOutputRead, &hOutputWrite, &tSecurity, 0) ) goto failed;
	if ( !SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0) ) goto failed;
	if ( !CreatePipe(&hInputRead, &hInputWrite, &tSecurity, 0) ) goto failed;
	if ( !SetHandleInformation(hInputWrite, HANDLE_FLAG_INHERIT, 0) ) goto failed;

	memset(&tStartup, 0, sizeof(tStartup));
	memset(&tProcessInfo, 0, sizeof(tProcessInfo));
	tStartup.cb = sizeof(tStartup);
	tStartup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	tStartup.wShowWindow = SW_HIDE;
	tStartup.hStdInput = hInputRead;
	tStartup.hStdOutput = hOutputWrite;
	tStartup.hStdError = hOutputWrite;
	bCreated = CreateProcessA(NULL, sCommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, pDesc->sWorkingDirectory, &tStartup, &tProcessInfo);
	if ( hInputRead != NULL ) {
		CloseHandle(hInputRead);
		hInputRead = NULL;
	}
	if ( hOutputWrite != NULL ) {
		CloseHandle(hOutputWrite);
		hOutputWrite = NULL;
	}
	if ( !bCreated ) goto failed;
	if ( tProcessInfo.hThread != NULL ) CloseHandle(tProcessInfo.hThread);
	pSession->hProcess = tProcessInfo.hProcess;
	pSession->hInputWrite = hInputWrite;
	pSession->hOutputRead = hOutputRead;
	hInputWrite = NULL;
	hOutputRead = NULL;
	return XUI_OK;

failed:
	if ( hInputRead != NULL ) CloseHandle(hInputRead);
	if ( hInputWrite != NULL ) CloseHandle(hInputWrite);
	if ( hOutputRead != NULL ) CloseHandle(hOutputRead);
	if ( hOutputWrite != NULL ) CloseHandle(hOutputWrite);
	return XUI_ERROR_BACKEND_FAILED;
#else
	(void)pSession;
	(void)pDesc;
	(void)sCommandLine;
	return XUI_ERROR_UNSUPPORTED;
#endif
}

static int __xuiTerminalProcessCreateConptyTransport(xui_terminal_session_t* pSession, const xui_terminal_process_desc_t* pDesc, char* sCommandLine)
{
#if defined(_WIN32) || defined(_WIN64)
	SECURITY_ATTRIBUTES tSecurity;
	STARTUPINFOEXA tStartup;
	PROCESS_INFORMATION tProcessInfo;
	xui_terminal_conpty_api_t tApi;
	LPPROC_THREAD_ATTRIBUTE_LIST pAttributeList;
	SIZE_T iAttributeListSize;
	HANDLE hInputRead;
	HANDLE hInputWrite;
	HANDLE hOutputRead;
	HANDLE hOutputWrite;
	HPCON hPseudoConsole;
	HRESULT iResult;
	BOOL bCreated;
	int bAttributeListReady;

	if ( !__xuiTerminalConptyGetApi(&tApi) ) return XUI_ERROR_UNSUPPORTED;
	hInputRead = NULL;
	hInputWrite = NULL;
	hOutputRead = NULL;
	hOutputWrite = NULL;
	hPseudoConsole = NULL;
	pAttributeList = NULL;
	iAttributeListSize = 0;
	bAttributeListReady = 0;
	memset(&tSecurity, 0, sizeof(tSecurity));
	tSecurity.nLength = sizeof(tSecurity);
	tSecurity.bInheritHandle = TRUE;
	if ( !CreatePipe(&hInputRead, &hInputWrite, &tSecurity, 0) ) goto failed;
	if ( !SetHandleInformation(hInputWrite, HANDLE_FLAG_INHERIT, 0) ) goto failed;
	if ( !CreatePipe(&hOutputRead, &hOutputWrite, &tSecurity, 0) ) goto failed;
	if ( !SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0) ) goto failed;
	iResult = tApi.createPseudoConsole(__xuiTerminalConptySize(__xuiTerminalProcessDescColumns(pDesc), __xuiTerminalProcessDescRows(pDesc)), hInputRead, hOutputWrite, 0, &hPseudoConsole);
	if ( FAILED(iResult) || hPseudoConsole == NULL ) goto failed;
	InitializeProcThreadAttributeList(NULL, 1, 0, &iAttributeListSize);
	if ( iAttributeListSize == 0 ) goto failed;
	pAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)xrtMalloc((size_t)iAttributeListSize);
	if ( pAttributeList == NULL ) goto failed;
	if ( !InitializeProcThreadAttributeList(pAttributeList, 1, 0, &iAttributeListSize) ) goto failed;
	bAttributeListReady = 1;
	if ( !UpdateProcThreadAttribute(pAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPseudoConsole, sizeof(hPseudoConsole), NULL, NULL) ) goto failed;
	memset(&tStartup, 0, sizeof(tStartup));
	memset(&tProcessInfo, 0, sizeof(tProcessInfo));
	tStartup.StartupInfo.cb = sizeof(tStartup);
	tStartup.lpAttributeList = pAttributeList;
	bCreated = CreateProcessA(NULL, sCommandLine, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, pDesc->sWorkingDirectory, &tStartup.StartupInfo, &tProcessInfo);
	if ( !bCreated ) goto failed;
	if ( hInputRead != NULL ) {
		CloseHandle(hInputRead);
		hInputRead = NULL;
	}
	if ( hOutputWrite != NULL ) {
		CloseHandle(hOutputWrite);
		hOutputWrite = NULL;
	}
	if ( tProcessInfo.hThread != NULL ) CloseHandle(tProcessInfo.hThread);
	if ( bAttributeListReady ) DeleteProcThreadAttributeList(pAttributeList);
	xrtFree(pAttributeList);
	pSession->hProcess = tProcessInfo.hProcess;
	pSession->hInputWrite = hInputWrite;
	pSession->hOutputRead = hOutputRead;
	pSession->hPseudoConsole = hPseudoConsole;
	hInputWrite = NULL;
	hOutputRead = NULL;
	hPseudoConsole = NULL;
	return XUI_OK;

failed:
	if ( bAttributeListReady ) DeleteProcThreadAttributeList(pAttributeList);
	if ( pAttributeList != NULL ) xrtFree(pAttributeList);
	if ( hPseudoConsole != NULL ) tApi.closePseudoConsole(hPseudoConsole);
	if ( hInputRead != NULL ) CloseHandle(hInputRead);
	if ( hInputWrite != NULL ) CloseHandle(hInputWrite);
	if ( hOutputRead != NULL ) CloseHandle(hOutputRead);
	if ( hOutputWrite != NULL ) CloseHandle(hOutputWrite);
	return XUI_ERROR_BACKEND_FAILED;
#else
	(void)pSession;
	(void)pDesc;
	(void)sCommandLine;
	return XUI_ERROR_UNSUPPORTED;
#endif
}

XUI_API xui_terminal_session_t* xuiTerminalCreateProcessSession(const xui_terminal_process_desc_t* pDesc)
{
	xui_terminal_session_t* pSession;
	char* sCommandLine;
	uint32_t iFlags;
	int iRet;

	if ( pDesc == NULL || pDesc->iSize < (uint32_t)offsetof(xui_terminal_process_desc_t, iFlags) || pDesc->sCommandLine == NULL || pDesc->sCommandLine[0] == '\0' ) return NULL;
	pSession = (xui_terminal_session_t*)xrtCalloc(1, sizeof(*pSession));
	if ( pSession == NULL ) return NULL;
	sCommandLine = __xuiTerminalStrDup(pDesc->sCommandLine);
	if ( sCommandLine == NULL ) {
		xrtFree(pSession);
		return NULL;
	}
	iFlags = __xuiTerminalProcessDescFlags(pDesc);
	pSession->iKind = XUI_TERMINAL_SESSION_PROCESS;
	pSession->bRunning = 1;
	pSession->bEcho = 0;
	pSession->iProcessFlags = iFlags;
	pSession->iLastColumns = __xuiTerminalProcessDescColumns(pDesc);
	pSession->iLastRows = __xuiTerminalProcessDescRows(pDesc);
	pSession->onResize = pDesc->onResize;
	pSession->pResizeUser = pDesc->pResizeUser;
	pSession->sPrompt = __xuiTerminalStrDup("");
	if ( pSession->sPrompt == NULL ) {
		xrtFree(sCommandLine);
		xrtFree(pSession);
		return NULL;
	}
	if ( (iFlags & XUI_TERMINAL_PROCESS_CONPTY) != 0u ) {
		iRet = __xuiTerminalProcessCreateConptyTransport(pSession, pDesc, sCommandLine);
	} else {
		iRet = __xuiTerminalProcessCreatePipeTransport(pSession, pDesc, sCommandLine);
	}
	xrtFree(sCommandLine);
	if ( iRet != XUI_OK ) {
		__xuiTerminalProcessCloseHandles(pSession);
		if ( pSession->sPrompt != NULL ) xrtFree(pSession->sPrompt);
		xrtFree(pSession);
		return NULL;
	}
	return pSession;
}

XUI_API void xuiTerminalSessionDestroy(xui_terminal_session_t* pSession)
{
	if ( pSession == NULL ) return;
	if ( pSession->pWidget != NULL ) {
		(void)xuiTerminalDetachSession(pSession->pWidget);
	}
	if ( pSession->iKind == XUI_TERMINAL_SESSION_PROCESS ) {
		(void)__xuiTerminalProcessTerminate(pSession);
	}
	if ( pSession->sPrompt != NULL ) xrtFree(pSession->sPrompt);
	xrtFree(pSession);
}

XUI_API int xuiTerminalSessionWrite(xui_terminal_session_t* pSession, const void* pDataBytes, int iSize)
{
	const uint8_t* pBytes;
#if defined(_WIN32) || defined(_WIN64)
	DWORD iWritten;
#endif
	int i;

	if ( pSession == NULL || iSize < 0 || (pDataBytes == NULL && iSize > 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iSize == 0 ) return XUI_OK;
	if ( pSession->iKind == XUI_TERMINAL_SESSION_PROCESS ) {
#if defined(_WIN32) || defined(_WIN64)
		if ( pSession->hInputWrite == NULL || !__xuiTerminalProcessRefreshRunning(pSession) ) return XUI_ERROR_BACKEND_FAILED;
		if ( !WriteFile(pSession->hInputWrite, pDataBytes, (DWORD)iSize, &iWritten, NULL) || iWritten != (DWORD)iSize ) return XUI_ERROR_BACKEND_FAILED;
		return XUI_OK;
#else
		return XUI_ERROR_UNSUPPORTED;
#endif
	}
	if ( !pSession->bEcho || pSession->pWidget == NULL ) return XUI_OK;
	pBytes = (const uint8_t*)pDataBytes;
	for ( i = 0; i < iSize; i++ ) {
		if ( pBytes[i] == '\r' || pBytes[i] == '\n' ) {
			(void)xuiTerminalWriteText(pSession->pWidget, "\r\n");
			if ( pSession->sPrompt != NULL && pSession->sPrompt[0] != '\0' ) {
				(void)xuiTerminalWriteText(pSession->pWidget, pSession->sPrompt);
			}
		} else if ( pBytes[i] == 0x7fu || pBytes[i] == 0x08u ) {
			(void)xuiTerminalWriteText(pSession->pWidget, "\b \b");
		} else {
			(void)xuiTerminalWrite(pSession->pWidget, &pBytes[i], 1);
		}
	}
	return xuiTerminalFlush(pSession->pWidget);
}

XUI_API int xuiTerminalSessionPoll(xui_terminal_session_t* pSession)
{
#if defined(_WIN32) || defined(_WIN64)
	uint8_t arrBuffer[XUI_TERMINAL_PROCESS_READ_CHUNK];
	DWORD iAvailable;
	DWORD iRead;
	DWORD iChunk;
	DWORD iError;
	int iTotal;
	int iRet;
	int iPoll;

	if ( pSession == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pSession->iKind != XUI_TERMINAL_SESSION_PROCESS ) return XUI_OK;
	if ( pSession->hOutputRead == NULL ) {
		(void)__xuiTerminalProcessRefreshRunning(pSession);
		return XUI_OK;
	}
	iTotal = 0;
	for ( iPoll = 0; iPoll < XUI_TERMINAL_PROCESS_POLL_LIMIT; iPoll++ ) {
		iAvailable = 0;
		if ( !PeekNamedPipe(pSession->hOutputRead, NULL, 0, NULL, &iAvailable, NULL) ) {
			iError = GetLastError();
			if ( iError == ERROR_BROKEN_PIPE || iError == ERROR_HANDLE_EOF ) {
				pSession->bRunning = 0;
				break;
			}
			return (iTotal > 0) ? iTotal : XUI_ERROR_BACKEND_FAILED;
		}
		if ( iAvailable == 0 ) break;
		iChunk = (iAvailable > XUI_TERMINAL_PROCESS_READ_CHUNK) ? XUI_TERMINAL_PROCESS_READ_CHUNK : iAvailable;
		if ( !ReadFile(pSession->hOutputRead, arrBuffer, iChunk, &iRead, NULL) ) {
			iError = GetLastError();
			if ( iError == ERROR_BROKEN_PIPE || iError == ERROR_HANDLE_EOF ) {
				pSession->bRunning = 0;
				break;
			}
			return (iTotal > 0) ? iTotal : XUI_ERROR_BACKEND_FAILED;
		}
		if ( iRead == 0 ) break;
		iTotal += (int)iRead;
		if ( pSession->pWidget != NULL ) {
			iRet = xuiTerminalWrite(pSession->pWidget, arrBuffer, (int)iRead);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( iTotal > 0 && pSession->pWidget != NULL ) {
		iRet = xuiTerminalFlush(pSession->pWidget);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)__xuiTerminalProcessRefreshRunning(pSession);
	return iTotal;
#else
	if ( pSession == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return (pSession->iKind == XUI_TERMINAL_SESSION_PROCESS) ? XUI_ERROR_UNSUPPORTED : XUI_OK;
#endif
}

XUI_API int xuiTerminalSessionIsRunning(xui_terminal_session_t* pSession)
{
	if ( pSession == NULL ) return 0;
	if ( pSession->iKind == XUI_TERMINAL_SESSION_PROCESS ) {
		return __xuiTerminalProcessRefreshRunning(pSession);
	}
	return pSession->bRunning;
}

XUI_API int xuiTerminalSessionTerminate(xui_terminal_session_t* pSession)
{
	if ( pSession == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pSession->iKind == XUI_TERMINAL_SESSION_PROCESS ) {
		return __xuiTerminalProcessTerminate(pSession);
	}
	pSession->bRunning = 0;
	return XUI_OK;
}

XUI_API int xuiTerminalSessionResize(xui_terminal_session_t* pSession, int iColumns, int iRows)
{
	int iRet;

	if ( pSession == NULL || iColumns <= 0 || iRows <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTerminalSessionResizeTransport(pSession, iColumns, iRows);
	if ( iRet != XUI_OK ) return iRet;
	pSession->iLastColumns = iColumns;
	pSession->iLastRows = iRows;
	if ( pSession->onResize != NULL ) {
		pSession->onResize(pSession, iColumns, iRows, pSession->pResizeUser);
	}
	return XUI_OK;
}

XUI_API int xuiTerminalSessionSetResizeCallback(xui_terminal_session_t* pSession, xui_terminal_session_resize_proc onResize, void* pUser)
{
	xui_terminal_data_t* pData;

	if ( pSession == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSession->onResize = onResize;
	pSession->pResizeUser = pUser;
	if ( pSession->pWidget != NULL && pSession->onResize != NULL ) {
		pData = __xuiTerminalGetData(pSession->pWidget);
		if ( pData != NULL && pData->pSession == pSession && pData->iColumns > 0 && pData->iRows > 0 ) {
			return xuiTerminalSessionResize(pSession, pData->iColumns, pData->iRows);
		}
	}
	return XUI_OK;
}

XUI_API int xuiTerminalGetChangeCount(xui_widget pWidget)
{
	xui_terminal_data_t* pData = __xuiTerminalGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
