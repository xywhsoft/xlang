#include "ui/xide_main_window.h"
#include "i18n/xide_i18n.h"
#include "ui/xide_dock_layout.h"

#include <string.h>

static int xideRootRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	xui_rect_t r;
	(void)iStateId;
	if ( pWidget == NULL || pDraw == NULL || pApp == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	r = xuiWidgetGetContentRect(pWidget);
	return pApp->tProxy.drawRectFill(&pApp->tProxy, pDraw, r, XUI_COLOR_RGBA(245, 248, 252, 255));
}

static int xideCreateMenu(xide_app_t* pApp, xui_widget pOwner, const xui_menu_item_t* pItems, int iCount, xui_widget* ppMenu)
{
	xui_menu_desc_t desc;
	int ret;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pOwner = pOwner;
	desc.pFont = pApp->pUiFont;
	ret = xuiMenuCreate(pApp->pContext, ppMenu, &desc);
	if ( ret != XUI_OK ) return ret;
	return xuiMenuSetItems(*ppMenu, pItems, iCount);
}

static int xideCreateMenuBar(xide_app_t* pApp)
{
	xui_menubar_desc_t desc;
	xui_widget fileMenu = NULL;
	xui_widget editMenu = NULL;
	xui_widget viewMenu = NULL;
	xui_widget runMenu = NULL;
	xui_widget toolsMenu = NULL;
	xui_widget helpMenu = NULL;
	xui_menu_item_t fileItems[8];
	xui_menu_item_t editItems[8];
	xui_menu_item_t viewItems[10];
	xui_menu_item_t runItems[5];
	xui_menu_item_t toolsItems[4];
	xui_menu_item_t helpItems[3];
	xui_menubar_item_t menus[6];
	int ret;
#define T(id) xideTr(pApp->pContext, id)

	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pUiFont;
	ret = xuiMenuBarCreate(pApp->pContext, &pApp->pMenuBar, &desc);
	if ( ret != XUI_OK ) return ret;
	ret = xuiWidgetAddChild(pApp->pRoot, pApp->pMenuBar);
	if ( ret != XUI_OK ) return ret;

	memset(fileItems, 0, sizeof(fileItems));
	fileItems[0] = (xui_menu_item_t){T(XIDE_TR_NEW_PROJECT), "Ctrl+Shift+N", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 101, 0, NULL, NULL};
	fileItems[1] = (xui_menu_item_t){T(XIDE_TR_NEW_FILE), "Ctrl+N", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 102, 0, NULL, NULL};
	fileItems[2] = (xui_menu_item_t){T(XIDE_TR_OPEN), "Ctrl+O", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 103, 0, NULL, NULL};
	fileItems[3] = (xui_menu_item_t){T(XIDE_TR_SAVE), "Ctrl+S", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 104, 0, NULL, NULL};
	fileItems[4] = (xui_menu_item_t){T(XIDE_TR_SAVE_ALL), "Ctrl+Shift+S", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 105, 0, NULL, NULL};
	fileItems[5].iType = XUI_MENU_ITEM_SEPARATOR;
	fileItems[6] = (xui_menu_item_t){T(XIDE_TR_CLOSE), "Ctrl+W", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 106, 0, NULL, NULL};
	fileItems[7] = (xui_menu_item_t){T(XIDE_TR_EXIT), "Alt+F4", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 107, 0, NULL, NULL};
	ret = xideCreateMenu(pApp, pApp->pMenuBar, fileItems, 8, &fileMenu);
	if ( ret != XUI_OK ) return ret;

	memset(editItems, 0, sizeof(editItems));
	editItems[0] = (xui_menu_item_t){T(XIDE_TR_UNDO), "Ctrl+Z", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 201, 0, NULL, NULL};
	editItems[1] = (xui_menu_item_t){T(XIDE_TR_REDO), "Ctrl+Y", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 202, 0, NULL, NULL};
	editItems[2].iType = XUI_MENU_ITEM_SEPARATOR;
	editItems[3] = (xui_menu_item_t){T(XIDE_TR_CUT), "Ctrl+X", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 203, 0, NULL, NULL};
	editItems[4] = (xui_menu_item_t){T(XIDE_TR_COPY), "Ctrl+C", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 204, 0, NULL, NULL};
	editItems[5] = (xui_menu_item_t){T(XIDE_TR_PASTE), "Ctrl+V", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 205, 0, NULL, NULL};
	editItems[6].iType = XUI_MENU_ITEM_SEPARATOR;
	editItems[7] = (xui_menu_item_t){T(XIDE_TR_FIND), "Ctrl+F", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 206, 0, NULL, NULL};
	ret = xideCreateMenu(pApp, pApp->pMenuBar, editItems, 8, &editMenu);
	if ( ret != XUI_OK ) return ret;

	memset(viewItems, 0, sizeof(viewItems));
	viewItems[0] = (xui_menu_item_t){T(XIDE_TR_HOME), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 301, 0, NULL, NULL};
	viewItems[1] = (xui_menu_item_t){T(XIDE_TR_PROJECT), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 302, 0, NULL, NULL};
	viewItems[2] = (xui_menu_item_t){T(XIDE_TR_COMMAND_LIST), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 303, 0, NULL, NULL};
	viewItems[3] = (xui_menu_item_t){T(XIDE_TR_HELP), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 304, 0, NULL, NULL};
	viewItems[4] = (xui_menu_item_t){T(XIDE_TR_OUTPUT), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 305, 0, NULL, NULL};
	viewItems[5] = (xui_menu_item_t){T(XIDE_TR_TASKS), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 306, 0, NULL, NULL};
	viewItems[6] = (xui_menu_item_t){T(XIDE_TR_DEBUG), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 307, 0, NULL, NULL};
	viewItems[7] = (xui_menu_item_t){T(XIDE_TR_TOOLS), NULL, XUI_MENU_ITEM_CHECK, XUI_MENU_ITEM_ENABLED | XUI_MENU_ITEM_CHECKED, 308, 0, NULL, NULL};
	viewItems[8].iType = XUI_MENU_ITEM_SEPARATOR;
	viewItems[9] = (xui_menu_item_t){T(XIDE_TR_RESET_LAYOUT), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 309, 0, NULL, NULL};
	ret = xideCreateMenu(pApp, pApp->pMenuBar, viewItems, 10, &viewMenu);
	if ( ret != XUI_OK ) return ret;

	memset(runItems, 0, sizeof(runItems));
	runItems[0] = (xui_menu_item_t){T(XIDE_TR_RUN), "F5", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 401, 0, NULL, NULL};
	runItems[1] = (xui_menu_item_t){T(XIDE_TR_DEBUG), "F6", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 402, 0, NULL, NULL};
	runItems[2] = (xui_menu_item_t){T(XIDE_TR_BUILD), "F7", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 403, 0, NULL, NULL};
	runItems[3].iType = XUI_MENU_ITEM_SEPARATOR;
	runItems[4] = (xui_menu_item_t){T(XIDE_TR_STOP), "Shift+F5", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 404, 0, NULL, NULL};
	ret = xideCreateMenu(pApp, pApp->pMenuBar, runItems, 5, &runMenu);
	if ( ret != XUI_OK ) return ret;

	memset(toolsItems, 0, sizeof(toolsItems));
	toolsItems[0] = (xui_menu_item_t){T(XIDE_TR_OPTIONS), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 501, 0, NULL, NULL};
	toolsItems[1] = (xui_menu_item_t){T(XIDE_TR_TERMINAL), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 502, 0, NULL, NULL};
	toolsItems[2] = (xui_menu_item_t){T(XIDE_TR_PACKAGE_MANAGER), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 503, 0, NULL, NULL};
	toolsItems[3] = (xui_menu_item_t){T(XIDE_TR_XAPI_INDEX), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 504, 0, NULL, NULL};
	ret = xideCreateMenu(pApp, pApp->pMenuBar, toolsItems, 4, &toolsMenu);
	if ( ret != XUI_OK ) return ret;

	memset(helpItems, 0, sizeof(helpItems));
	helpItems[0] = (xui_menu_item_t){T(XIDE_TR_DOCUMENTATION), "F1", XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 601, 0, NULL, NULL};
	helpItems[1] = (xui_menu_item_t){T(XIDE_TR_SHORTCUTS), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 602, 0, NULL, NULL};
	helpItems[2] = (xui_menu_item_t){T(XIDE_TR_ABOUT), NULL, XUI_MENU_ITEM_NORMAL, XUI_MENU_ITEM_ENABLED, 603, 0, NULL, NULL};
	ret = xideCreateMenu(pApp, pApp->pMenuBar, helpItems, 3, &helpMenu);
	if ( ret != XUI_OK ) return ret;

	memset(menus, 0, sizeof(menus));
	menus[0] = (xui_menubar_item_t){T(XIDE_TR_MENU_FILE), XUI_MENUBAR_ITEM_ENABLED, 1, 0, fileMenu, {0, 0, 0, 0}, NULL};
	menus[1] = (xui_menubar_item_t){T(XIDE_TR_MENU_EDIT), XUI_MENUBAR_ITEM_ENABLED, 2, 0, editMenu, {0, 0, 0, 0}, NULL};
	menus[2] = (xui_menubar_item_t){T(XIDE_TR_MENU_VIEW), XUI_MENUBAR_ITEM_ENABLED, 3, 0, viewMenu, {0, 0, 0, 0}, NULL};
	menus[3] = (xui_menubar_item_t){T(XIDE_TR_MENU_RUN), XUI_MENUBAR_ITEM_ENABLED, 4, 0, runMenu, {0, 0, 0, 0}, NULL};
	menus[4] = (xui_menubar_item_t){T(XIDE_TR_MENU_TOOLS), XUI_MENUBAR_ITEM_ENABLED, 5, 0, toolsMenu, {0, 0, 0, 0}, NULL};
	menus[5] = (xui_menubar_item_t){T(XIDE_TR_MENU_HELP), XUI_MENUBAR_ITEM_ENABLED, 6, 0, helpMenu, {0, 0, 0, 0}, NULL};
	ret = xuiMenuBarSetItems(pApp->pMenuBar, menus, 6);
#undef T
	return ret;
}

static int xideCreateToolbar(xide_app_t* pApp)
{
	xui_toolbar_desc_t desc;
	xui_toolbar_item_t items[12];
	int ret;
#define T(id) xideTr(pApp->pContext, id)
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pUiFont;
	ret = xuiToolbarCreate(pApp->pContext, &pApp->pToolbar, &desc);
	if ( ret != XUI_OK ) return ret;
	memset(items, 0, sizeof(items));
	items[0] = (xui_toolbar_item_t){T(XIDE_TR_NEW_FILE), T(XIDE_TR_TOOLTIP_NEW_FILE), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 701, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[1] = (xui_toolbar_item_t){T(XIDE_TR_OPEN), T(XIDE_TR_TOOLTIP_OPEN_FILE), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 702, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[2] = (xui_toolbar_item_t){T(XIDE_TR_SAVE), T(XIDE_TR_TOOLTIP_SAVE_FILE), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 703, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[3] = (xui_toolbar_item_t){"", NULL, XUI_TOOLBAR_ITEM_SEPARATOR, 0, 0, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[4] = (xui_toolbar_item_t){T(XIDE_TR_UNDO), T(XIDE_TR_UNDO), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 704, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[5] = (xui_toolbar_item_t){T(XIDE_TR_REDO), T(XIDE_TR_REDO), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 705, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[6] = (xui_toolbar_item_t){T(XIDE_TR_FIND), T(XIDE_TR_FIND), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 706, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[7] = (xui_toolbar_item_t){"", NULL, XUI_TOOLBAR_ITEM_SEPARATOR, 0, 0, 0, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[8] = (xui_toolbar_item_t){T(XIDE_TR_RUN), T(XIDE_TR_TOOLTIP_RUN_CURRENT_FILE), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 707, 1, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[9] = (xui_toolbar_item_t){T(XIDE_TR_BUILD), T(XIDE_TR_TOOLTIP_BUILD_PROJECT), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 708, 1, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[10] = (xui_toolbar_item_t){T(XIDE_TR_DEBUG), T(XIDE_TR_TOOLTIP_START_DEBUG), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 709, 1, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	items[11] = (xui_toolbar_item_t){T(XIDE_TR_STOP), T(XIDE_TR_TOOLTIP_STOP_TASK), XUI_TOOLBAR_ITEM_BUTTON, XUI_TOOLBAR_ITEM_ENABLED, 710, 1, NULL, {0, 0, 0, 0}, {0, 0, 0, 0}, NULL};
	ret = xuiToolbarSetItems(pApp->pToolbar, items, 12);
	if ( ret != XUI_OK ) return ret;
#undef T
	return xuiWidgetAddChild(pApp->pRoot, pApp->pToolbar);
}

static int xideCreateStatusBar(xide_app_t* pApp)
{
	xui_statusbar_desc_t desc;
	int ret;
#define T(id) xideTr(pApp->pContext, id)
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pUiFont;
	ret = xuiStatusBarCreate(pApp->pContext, &pApp->pStatusBar, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiStatusBarAddText(pApp->pStatusBar, XUI_STATUSBAR_SECTION_LEFT, T(XIDE_TR_STATUS_READY), 90.0f, 0, 0);
	(void)xuiStatusBarAddText(pApp->pStatusBar, XUI_STATUSBAR_SECTION_LEFT, T(XIDE_TR_STATUS_SKELETON), 180.0f, 0, 0);
	(void)xuiStatusBarAddFlexibleSpacer(pApp->pStatusBar, XUI_STATUSBAR_SECTION_CENTER, 1.0f);
	(void)xuiStatusBarAddText(pApp->pStatusBar, XUI_STATUSBAR_SECTION_RIGHT, "UTF-8", 80.0f, 0, 0);
	(void)xuiStatusBarAddText(pApp->pStatusBar, XUI_STATUSBAR_SECTION_RIGHT, T(XIDE_TR_STATUS_CURSOR), 110.0f, 0, 0);
#undef T
	return xuiWidgetAddChild(pApp->pRoot, pApp->pStatusBar);
}

int xideMainWindowCreate(xide_app_t* pApp)
{
	int ret;
	if ( pApp == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	ret = xuiWidgetCreate(pApp->pContext, &pApp->pRoot);
	if ( ret != XUI_OK ) return ret;
	(void)xuiWidgetSetLayoutType(pApp->pRoot, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetCacheRenderCallback(pApp->pRoot, xideRootRender, pApp);
	ret = xuiSetRootWidget(pApp->pContext, pApp->pRoot);
	if ( ret != XUI_OK ) return ret;
	ret = xideCreateMenuBar(pApp);
	if ( ret != XUI_OK ) return ret;
	ret = xideCreateToolbar(pApp);
	if ( ret != XUI_OK ) return ret;
	ret = xideCreateStatusBar(pApp);
	if ( ret != XUI_OK ) return ret;
	ret = xideDockCreate(pApp);
	if ( ret != XUI_OK ) return ret;
	xideMainWindowLayout(pApp);
	return XUI_OK;
}

void xideMainWindowLayout(xide_app_t* pApp)
{
	float w;
	float h;
	float menuH = 28.0f;
	float toolbarH = 36.0f;
	float statusH = 28.0f;
	if ( pApp == NULL || pApp->pRoot == NULL ) return;
	w = (float)(pApp->iViewportW > 0 ? pApp->iViewportW : XIDE_WINDOW_W);
	h = (float)(pApp->iViewportH > 0 ? pApp->iViewportH : XIDE_WINDOW_H);
	(void)xuiWidgetSetRect(pApp->pRoot, (xui_rect_t){0.0f, 0.0f, w, h});
	if ( pApp->pMenuBar != NULL ) (void)xuiWidgetSetRect(pApp->pMenuBar, (xui_rect_t){0.0f, 0.0f, w, menuH});
	if ( pApp->pToolbar != NULL ) (void)xuiWidgetSetRect(pApp->pToolbar, (xui_rect_t){0.0f, menuH, w, toolbarH});
	if ( pApp->pStatusBar != NULL ) (void)xuiWidgetSetRect(pApp->pStatusBar, (xui_rect_t){0.0f, h - statusH, w, statusH});
	if ( pApp->pDockPanel != NULL ) (void)xuiWidgetSetRect(pApp->pDockPanel, (xui_rect_t){0.0f, menuH + toolbarH, w, h - menuH - toolbarH - statusH});
}
