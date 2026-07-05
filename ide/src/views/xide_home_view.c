#include "views/xide_views.h"
#include "lib/xrt/xrt.h"

#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <wchar.h>

#define XIDE_HOME_HISTORY_FILE "History.json"

static void xideHomeCopy(char* sDst, int iCapacity, const char* sSrc)
{
	int len;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	if ( sSrc == NULL ) sSrc = "";
	len = (int)strlen(sSrc);
	if ( len >= iCapacity ) len = iCapacity - 1;
	if ( len > 0 ) memcpy(sDst, sSrc, (size_t)len);
	sDst[len] = '\0';
}

static int xideHomeGetExeDir(char* sBuffer, int iCapacity)
{
	wchar_t wPath[MAX_PATH];
	wchar_t* p;
	int len;
	if ( sBuffer == NULL || iCapacity <= 0 ) return 0;
	sBuffer[0] = '\0';
	len = (int)GetModuleFileNameW(NULL, wPath, (DWORD)(sizeof(wPath) / sizeof(wPath[0])));
	if ( len <= 0 || len >= (int)(sizeof(wPath) / sizeof(wPath[0])) ) return 0;
	p = wcsrchr(wPath, L'\\');
	if ( p == NULL ) p = wcsrchr(wPath, L'/');
	if ( p == NULL ) return 0;
	*p = L'\0';
	return WideCharToMultiByte(CP_UTF8, 0, wPath, -1, sBuffer, iCapacity, NULL, NULL) > 0;
}

static int xideHomePathJoin(char* sBuffer, int iCapacity, const char* sBase, const char* sName)
{
	int baseLen;
	int nameLen;
	if ( sBuffer == NULL || sBase == NULL || sName == NULL || iCapacity <= 0 ) return 0;
	baseLen = (int)strlen(sBase);
	nameLen = (int)strlen(sName);
	if ( baseLen + 1 + nameLen >= iCapacity ) return 0;
	memcpy(sBuffer, sBase, (size_t)baseLen + 1u);
	if ( baseLen > 0 && sBuffer[baseLen - 1] != '\\' && sBuffer[baseLen - 1] != '/' ) {
		sBuffer[baseLen++] = '\\';
		sBuffer[baseLen] = '\0';
	}
	memcpy(sBuffer + baseLen, sName, (size_t)nameLen + 1u);
	return 1;
}

static int xideHomeGetOptionDir(char* sBuffer, int iCapacity)
{
	char sExeDir[XIDE_WORKSPACE_PATH_MAX];
	if ( !xideHomeGetExeDir(sExeDir, (int)sizeof(sExeDir)) ) return 0;
	return xideHomePathJoin(sBuffer, iCapacity, sExeDir, "option");
}

static int xideHomeGetHistoryPath(char* sBuffer, int iCapacity)
{
	char sOptionDir[XIDE_WORKSPACE_PATH_MAX];
	if ( !xideHomeGetOptionDir(sOptionDir, (int)sizeof(sOptionDir)) ) return 0;
	return xideHomePathJoin(sBuffer, iCapacity, sOptionDir, XIDE_HOME_HISTORY_FILE);
}

static int xideHomeEnsureHistoryFile(char* sHistoryPath, int iCapacity)
{
	char sOptionDir[XIDE_WORKSPACE_PATH_MAX];
	if ( !xideHomeGetHistoryPath(sHistoryPath, iCapacity) ) return 0;
	if ( !xideHomeGetOptionDir(sOptionDir, (int)sizeof(sOptionDir)) ) return 0;
	(void)xrtDirCreateAll((str)sOptionDir);
	if ( !xrtFileExists((str)sHistoryPath) ) {
		return xrtFileWriteAll((str)sHistoryPath, (str)"[]\n", 3, XRT_CP_UTF8) == 3;
	}
	return 1;
}

static const char* xideHomeJsonText(xvalue pObj, const char* sKey)
{
	xvalue pVal;
	if ( pObj == NULL || sKey == NULL || !xvoIsTable(pObj) ) return "";
	pVal = xvoTableGetValue(pObj, sKey, 0);
	return xvoIsText(pVal) ? (const char*)xvoGetText(pVal) : "";
}

static void xideHomePathName(char* sDst, int iCapacity, const char* sPath, int bNoExt)
{
	str sName;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	sDst[0] = '\0';
	if ( sPath == NULL || sPath[0] == '\0' ) return;
	sName = bNoExt ? xrtPathGetName((str)sPath, 0) : xrtPathGetNameExt((str)sPath, 0);
	if ( sName != NULL && sName[0] != '\0' ) xideHomeCopy(sDst, iCapacity, (const char*)sName);
	if ( sName != NULL ) xrtFree(sName);
}

static int xideHomeHasExt(const char* sPath, const char* sExt)
{
	const char* p;
	if ( sPath == NULL || sExt == NULL ) return 0;
	p = strrchr(sPath, '.');
	return p != NULL && _stricmp(p, sExt) == 0;
}

static void xideHomeNow(char* sBuffer, int iCapacity)
{
	SYSTEMTIME st;
	if ( sBuffer == NULL || iCapacity <= 0 ) return;
	GetLocalTime(&st);
	snprintf(sBuffer, (size_t)iCapacity, "%04u-%02u-%02u %02u:%02u:%02u",
		(unsigned)st.wYear,
		(unsigned)st.wMonth,
		(unsigned)st.wDay,
		(unsigned)st.wHour,
		(unsigned)st.wMinute,
		(unsigned)st.wSecond);
}

static int xideHomeSaveHistory(xide_app_t* pApp)
{
	char sHistoryPath[XIDE_WORKSPACE_PATH_MAX];
	xvalue pArr;
	int i;
	int ret;
	if ( pApp == NULL || !xideHomeEnsureHistoryFile(sHistoryPath, (int)sizeof(sHistoryPath)) ) return 0;
	pArr = xvoCreateArray();
	if ( pArr == NULL ) return 0;
	for ( i = 0; i < pApp->iHomeHistoryCount; i++ ) {
		xvalue pObj = xvoCreateTable();
		if ( pObj == NULL ) continue;
		(void)xvoTableSetText(pObj, "Icon", 0, (str)pApp->arrHomeHistory[i].sIcon, 0, FALSE);
		(void)xvoTableSetText(pObj, "Title", 0, (str)pApp->arrHomeHistory[i].sTitle, 0, FALSE);
		(void)xvoTableSetText(pObj, "Date", 0, (str)pApp->arrHomeHistory[i].sDate, 0, FALSE);
		(void)xvoTableSetText(pObj, "Path", 0, (str)pApp->arrHomeHistory[i].sPath, 0, FALSE);
		(void)xvoArrayAppendValue(pArr, pObj, TRUE);
	}
	ret = xrtStringifyJSON_File((str)sHistoryPath, pArr, TRUE);
	xvoUnref(pArr);
	return ret ? 1 : 0;
}

static void xideHomeLoadHistory(xide_app_t* pApp)
{
	char sHistoryPath[XIDE_WORKSPACE_PATH_MAX];
	xvalue pRoot;
	uint32 i;
	uint32 count;
	if ( pApp == NULL ) return;
	pApp->iHomeHistoryCount = 0;
	if ( !xideHomeEnsureHistoryFile(sHistoryPath, (int)sizeof(sHistoryPath)) ) return;
	pRoot = xrtParseJSON_File((str)sHistoryPath);
	if ( pRoot == NULL || !xvoIsArray(pRoot) ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return;
	}
	count = xvoArrayItemCount(pRoot);
	for ( i = 0; i < count && pApp->iHomeHistoryCount < XIDE_HOME_HISTORY_MAX; i++ ) {
		xvalue pItem = xvoArrayGetValue(pRoot, i);
		const char* sPath;
		xide_home_history_item_t* pHistory;
		if ( pItem == NULL || !xvoIsTable(pItem) ) continue;
		sPath = xideHomeJsonText(pItem, "Path");
		if ( sPath[0] == '\0' || !xrtFileExists((str)sPath) ) continue;
		pHistory = &pApp->arrHomeHistory[pApp->iHomeHistoryCount++];
		memset(pHistory, 0, sizeof(*pHistory));
		xideHomeCopy(pHistory->sIcon, (int)sizeof(pHistory->sIcon), xideHomeJsonText(pItem, "Icon"));
		xideHomeCopy(pHistory->sTitle, (int)sizeof(pHistory->sTitle), xideHomeJsonText(pItem, "Title"));
		xideHomeCopy(pHistory->sDate, (int)sizeof(pHistory->sDate), xideHomeJsonText(pItem, "Date"));
		xideHomeCopy(pHistory->sPath, (int)sizeof(pHistory->sPath), sPath);
		if ( pHistory->sTitle[0] == '\0' ) xideHomePathName(pHistory->sTitle, (int)sizeof(pHistory->sTitle), sPath, 1);
	}
	xvoUnref(pRoot);
}

static void xideHomeRefreshHistoryTable(xide_app_t* pApp)
{
	if ( pApp == NULL || pApp->pHomeHistoryTable == NULL ) return;
	(void)xuiTableViewRefreshAdapter(pApp->pHomeHistoryTable);
	(void)xuiWidgetInvalidate(pApp->pHomeHistoryTable, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void xideHomeHistoryAdd(xide_app_t* pApp, const char* sIcon, const char* sTitle, const char* sPath)
{
	xide_home_history_item_t oldItems[XIDE_HOME_HISTORY_MAX];
	int oldCount;
	int i;
	if ( pApp == NULL || sPath == NULL || sPath[0] == '\0' ) return;
	memcpy(oldItems, pApp->arrHomeHistory, sizeof(oldItems));
	oldCount = pApp->iHomeHistoryCount;
	memset(pApp->arrHomeHistory, 0, sizeof(pApp->arrHomeHistory));
	pApp->iHomeHistoryCount = 1;
	xideHomeCopy(pApp->arrHomeHistory[0].sIcon, (int)sizeof(pApp->arrHomeHistory[0].sIcon), sIcon);
	xideHomeCopy(pApp->arrHomeHistory[0].sTitle, (int)sizeof(pApp->arrHomeHistory[0].sTitle), sTitle);
	xideHomeCopy(pApp->arrHomeHistory[0].sPath, (int)sizeof(pApp->arrHomeHistory[0].sPath), sPath);
	xideHomeNow(pApp->arrHomeHistory[0].sDate, (int)sizeof(pApp->arrHomeHistory[0].sDate));
	if ( pApp->arrHomeHistory[0].sTitle[0] == '\0' ) {
		xideHomePathName(pApp->arrHomeHistory[0].sTitle, (int)sizeof(pApp->arrHomeHistory[0].sTitle), sPath, 1);
	}
	for ( i = 0; i < oldCount && pApp->iHomeHistoryCount < XIDE_HOME_HISTORY_MAX; i++ ) {
		if ( _stricmp(oldItems[i].sPath, sPath) == 0 ) continue;
		pApp->arrHomeHistory[pApp->iHomeHistoryCount++] = oldItems[i];
	}
	(void)xideHomeSaveHistory(pApp);
	xideHomeRefreshHistoryTable(pApp);
}

static int xideHomeWorkspaceHasLooseFile(xide_workspace_t* pWorkspace, const char* sPath)
{
	int i;
	if ( pWorkspace == NULL || sPath == NULL ) return 0;
	for ( i = 0; i < pWorkspace->iLooseFileCount; i++ ) {
		if ( _stricmp(pWorkspace->arrLooseFiles[i].sPath, sPath) == 0 ) return 1;
	}
	return 0;
}

static int xideHomeWorkspaceHasProject(xide_workspace_t* pWorkspace, const char* sPath)
{
	int i;
	if ( pWorkspace == NULL || sPath == NULL ) return 0;
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		if ( _stricmp(pWorkspace->arrProjects[i].sFile, sPath) == 0 ) return 1;
	}
	return 0;
}

static void xideHomeActivateEditor(xide_app_t* pApp)
{
	if ( pApp == NULL || pApp->pDockPanel == NULL || pApp->iDockDocumentPane < 0 ) return;
	(void)xuiDockPanelSetPaneActiveWindow(pApp->pDockPanel, pApp->iDockDocumentPane, pApp->iDockEditor);
}

static int xideHomeOpenPath(xide_app_t* pApp, const char* sPath)
{
	xide_workspace_project_t* pProject = NULL;
	char sTitle[XIDE_WORKSPACE_NAME_MAX];
	int ret;
	if ( pApp == NULL || sPath == NULL || sPath[0] == '\0' || !xrtFileExists((str)sPath) ) return XUI_ERROR_FILE_NOT_FOUND;
	memset(sTitle, 0, sizeof(sTitle));
	if ( xideHomeHasExt(sPath, ".xlw") ) {
		if ( !xideWorkspaceLoadXlw(&pApp->tWorkspace, sPath) ) return XUI_ERROR;
		(void)xideWorkspaceViewRefresh(pApp);
		xideHomeHistoryAdd(pApp, "工作空间", pApp->tWorkspace.sName, sPath);
		return XUI_OK;
	}
	if ( xideHomeHasExt(sPath, ".xlp") ) {
		if ( !xideHomeWorkspaceHasProject(&pApp->tWorkspace, sPath) && !xideWorkspaceLoadXlp(&pApp->tWorkspace, sPath, &pProject) ) return XUI_ERROR;
		(void)xideWorkspaceViewRefresh(pApp);
		if ( pProject != NULL ) xideHomeCopy(sTitle, (int)sizeof(sTitle), pProject->sName);
		if ( sTitle[0] == '\0' ) xideHomePathName(sTitle, (int)sizeof(sTitle), sPath, 1);
		xideHomeHistoryAdd(pApp, "项目", sTitle, sPath);
		return XUI_OK;
	}
	ret = xuiCodeEditLoadTextFile(pApp->pEditor, sPath, XRT_CP_AUTO);
	if ( ret != XUI_OK ) return ret;
	if ( !xideHomeWorkspaceHasLooseFile(&pApp->tWorkspace, sPath) ) {
		xideHomePathName(sTitle, (int)sizeof(sTitle), sPath, 0);
		(void)xideWorkspaceAddLooseFile(&pApp->tWorkspace, sTitle, sPath, 1);
		(void)xideWorkspaceViewRefresh(pApp);
	}
	if ( pApp->pFindScope != NULL ) (void)xuiCodeFindScopeAddEditorNamed(pApp->pFindScope, pApp->pEditor, sPath);
	(void)xuiWidgetInvalidate(pApp->pEditor, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	xideHomeActivateEditor(pApp);
	if ( sTitle[0] == '\0' ) xideHomePathName(sTitle, (int)sizeof(sTitle), sPath, 1);
	xideHomeHistoryAdd(pApp, "代码", sTitle, sPath);
	return XUI_OK;
}

static int xideHomeOpenDialog(char* sPath, int iCapacity)
{
	wchar_t wPath[1024];
	OPENFILENAMEW ofn;
	if ( sPath == NULL || iCapacity <= 0 ) return 0;
	sPath[0] = '\0';
	memset(wPath, 0, sizeof(wPath));
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = wPath;
	ofn.nMaxFile = (DWORD)(sizeof(wPath) / sizeof(wPath[0]));
	ofn.lpstrFilter =
		L"支持的文件 (*.xl;*.xlw;*.xlp)\0*.xl;*.xlw;*.xlp\0"
		L"xlang 源文件 (*.xl)\0*.xl\0"
		L"项目 (*.xlp)\0*.xlp\0"
		L"工作空间 (*.xlw)\0*.xlw\0"
		L"所有文件 (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if ( !GetOpenFileNameW(&ofn) ) return 0;
	return WideCharToMultiByte(CP_UTF8, 0, wPath, -1, sPath, iCapacity, NULL, NULL) > 0;
}

static void xideHomeNewProjectClick(xui_widget pWidget, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	(void)pWidget;
	if ( pApp == NULL ) return;
	(void)xideWorkspaceCreateDefault(&pApp->tWorkspace);
	(void)xideWorkspaceViewRefresh(pApp);
}

static void xideHomeNewFileClick(xui_widget pWidget, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	char sName[64];
	const char* sText = "func main(...) as int\n    return 0\nend\n";
	(void)pWidget;
	if ( pApp == NULL || pApp->pEditor == NULL ) return;
	pApp->iUntitledFileIndex++;
	snprintf(sName, sizeof(sName), "新建文件%d.xl", pApp->iUntitledFileIndex);
	(void)xuiCodeEditSetText(pApp->pEditor, sText);
	(void)xideWorkspaceAddLooseFile(&pApp->tWorkspace, sName, "", 1);
	(void)xideWorkspaceViewRefresh(pApp);
	xideHomeActivateEditor(pApp);
}

static void xideHomeOpenClick(xui_widget pWidget, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	char sPath[XIDE_WORKSPACE_PATH_MAX];
	(void)pWidget;
	if ( pApp == NULL ) return;
	if ( xideHomeOpenDialog(sPath, (int)sizeof(sPath)) ) (void)xideHomeOpenPath(pApp, sPath);
}

static void xideHomeClearHistoryClick(xui_widget pWidget, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	char sHistoryPath[XIDE_WORKSPACE_PATH_MAX];
	(void)pWidget;
	if ( pApp == NULL ) return;
	pApp->iHomeHistoryCount = 0;
	memset(pApp->arrHomeHistory, 0, sizeof(pApp->arrHomeHistory));
	if ( xideHomeEnsureHistoryFile(sHistoryPath, (int)sizeof(sHistoryPath)) ) {
		(void)xrtFileWriteAll((str)sHistoryPath, (str)"[]\n", 3, XRT_CP_UTF8);
	}
	xideHomeRefreshHistoryTable(pApp);
}

static int xideHomeHistoryCount(xui_widget pWidget, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	(void)pWidget;
	return pApp != NULL ? pApp->iHomeHistoryCount : 0;
}

static int xideHomeHistoryCell(xui_widget pWidget, int iRow, int iColumn, xui_table_view_cell_t* pCell, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	const xide_home_history_item_t* pItem;
	(void)pWidget;
	if ( pApp == NULL || pCell == NULL || iRow < 0 || iRow >= pApp->iHomeHistoryCount ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = &pApp->arrHomeHistory[iRow];
	memset(pCell, 0, sizeof(*pCell));
	if ( iColumn == 0 ) pCell->sText = pItem->sTitle;
	else if ( iColumn == 1 ) pCell->sText = pItem->sDate;
	else pCell->sText = pItem->sPath;
	return XUI_OK;
}

static int xideHomeHistoryDoubleClick(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	int row;
	if ( pApp == NULL || pEvent == NULL || pEvent->iType != XUI_EVENT_POINTER_DOUBLE_CLICK ) return XUI_OK;
	row = xuiTableViewGetSelectedRow(pWidget);
	if ( row >= 0 && row < pApp->iHomeHistoryCount ) (void)xideHomeOpenPath(pApp, pApp->arrHomeHistory[row].sPath);
	return XUI_OK;
}

static int xideHomeAddButton(xide_app_t* pApp, xui_widget pParent, const char* sText, xui_rect_t tRect, xui_button_click_proc onClick, int bLink)
{
	xui_button_desc_t desc;
	xui_widget button = NULL;
	int ret;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.sText = sText;
	desc.pFont = pApp->pUiFont;
	desc.iTextColor = bLink ? XUI_COLOR_RGBA(0, 64, 210, 255) : XUI_COLOR_RGBA(28, 35, 48, 255);
	desc.iDisabledTextColor = XUI_COLOR_RGBA(140, 148, 160, 255);
	desc.iTextFlags = XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	desc.iNormalColor = bLink ? XUI_COLOR_RGBA(250, 252, 255, 255) : XUI_COLOR_RGBA(235, 238, 242, 255);
	desc.iHoverColor = bLink ? XUI_COLOR_RGBA(238, 244, 255, 255) : XUI_COLOR_RGBA(228, 233, 241, 255);
	desc.iActiveColor = bLink ? XUI_COLOR_RGBA(225, 236, 255, 255) : XUI_COLOR_RGBA(214, 222, 235, 255);
	desc.iFocusColor = desc.iHoverColor;
	desc.iDisabledColor = XUI_COLOR_RGBA(230, 230, 230, 255);
	desc.fBorderWidth = bLink ? 0.0f : 1.0f;
	desc.iBorderColor = XUI_COLOR_RGBA(170, 176, 186, 255);
	ret = xuiButtonCreate(pApp->pContext, &button, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiWidgetSetRect(button, tRect);
	ret = xuiWidgetAddChild(pParent, button);
	if ( ret != XUI_OK ) return ret;
	if ( onClick != NULL ) (void)xuiButtonSetClick(button, onClick, pApp);
	return XUI_OK;
}

static int xideHomeAddHistoryTable(xide_app_t* pApp, xui_widget pParent)
{
	xui_table_view_desc_t desc;
	xui_table_view_column_t cols[3];
	int ret;
	memset(cols, 0, sizeof(cols));
	cols[0] = (xui_table_view_column_t){1, "name", "名称", 120.0f, 80.0f, 260.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	cols[1] = (xui_table_view_column_t){2, "date", "访问时间", 150.0f, 120.0f, 220.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	cols[2] = (xui_table_view_column_t){3, "path", "文件路径", 360.0f, 180.0f, 1200.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.arrColumns = cols;
	desc.iColumnCount = 3;
	desc.onCount = xideHomeHistoryCount;
	desc.onCell = xideHomeHistoryCell;
	desc.pAdapterUser = pApp;
	desc.pFont = pApp->pUiFont;
	desc.fDefaultRowHeight = 24.0f;
	desc.fHeaderHeight = 28.0f;
	desc.iSelectionMode = XUI_TABLE_VIEW_SELECTION_ROW;
	desc.iScrollbarMode = XUI_SCROLLBAR_POLICY_AUTO;
	desc.tColors.iBackgroundColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	desc.tColors.iHeaderColor = XUI_COLOR_RGBA(242, 245, 249, 255);
	desc.tColors.iHeaderTextColor = XUI_COLOR_RGBA(24, 34, 50, 255);
	desc.tColors.iRowColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	desc.tColors.iAltRowColor = XUI_COLOR_RGBA(250, 252, 255, 255);
	desc.tColors.iHoverColor = XUI_COLOR_RGBA(233, 241, 255, 255);
	desc.tColors.iSelectedColor = XUI_COLOR_RGBA(198, 222, 255, 255);
	desc.tColors.iGridColor = XUI_COLOR_RGBA(218, 224, 232, 255);
	desc.tColors.iTextColor = XUI_COLOR_RGBA(30, 40, 56, 255);
	ret = xuiTableViewCreate(pApp->pContext, &pApp->pHomeHistoryTable, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiWidgetSetRect(pApp->pHomeHistoryTable, (xui_rect_t){12.0f, 83.0f, 640.0f, 400.0f});
	(void)xuiWidgetSetEventHandler(pApp->pHomeHistoryTable, XUI_EVENT_POINTER_DOUBLE_CLICK, xideHomeHistoryDoubleClick, pApp);
	return xuiWidgetAddChild(pParent, pApp->pHomeHistoryTable);
}

int xideCreateHomeView(xide_app_t* pApp, xui_widget* ppView)
{
	xui_widget view = NULL;
	int ret;
	if ( pApp == NULL || ppView == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xideHomeLoadHistory(pApp);
	ret = xideViewCreatePanel(pApp, &view, XUI_COLOR_RGBA(250, 252, 255, 255));
	if ( ret != XUI_OK ) return ret;
	ret = xideHomeAddButton(pApp, view, "新建工程 (&P)", (xui_rect_t){12.0f, 12.0f, 100.0f, 32.0f}, xideHomeNewProjectClick, 0);
	if ( ret != XUI_OK ) return ret;
	ret = xideHomeAddButton(pApp, view, "新建文件 (&F)", (xui_rect_t){118.0f, 12.0f, 100.0f, 32.0f}, xideHomeNewFileClick, 0);
	if ( ret != XUI_OK ) return ret;
	ret = xideHomeAddButton(pApp, view, "打开 (&O)", (xui_rect_t){224.0f, 12.0f, 100.0f, 32.0f}, xideHomeOpenClick, 0);
	if ( ret != XUI_OK ) return ret;
	ret = xideViewAddLabel(pApp, view, "历史记录（双击打开）：", (xui_rect_t){12.0f, 58.0f, 260.0f, 24.0f},
		XUI_COLOR_RGBA(24, 34, 50, 255), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP, NULL);
	if ( ret != XUI_OK ) return ret;
	ret = xideHomeAddButton(pApp, view, "清空历史记录", (xui_rect_t){510.0f, 55.0f, 140.0f, 26.0f}, xideHomeClearHistoryClick, 1);
	if ( ret != XUI_OK ) return ret;
	ret = xideHomeAddHistoryTable(pApp, view);
	if ( ret != XUI_OK ) return ret;
	*ppView = view;
	return XUI_OK;
}
