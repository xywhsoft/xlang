#include "ui/xide_dock_layout.h"
#include "i18n/xide_i18n.h"
#include "views/xide_views.h"

#include <string.h>

static int xideDockAddWindow(xide_app_t* pApp, const char* sTitle, xui_widget pClient, int* pWindow)
{
	int ret;
	ret = xuiDockPanelAddWindow(pApp->pDockPanel, sTitle, pClient, pWindow);
	if ( ret == XUI_OK ) ret = xuiDockPanelSetWindowFlags(pApp->pDockPanel, *pWindow, 1, 1);
	return ret;
}

int xideDockCreate(xide_app_t* pApp)
{
	xui_dock_panel_desc_t desc;
	int documentPane = -1;
	int leftPane = -1;
	int rightPane = -1;
	int bottomPane = -1;
	int ret;
	if ( pApp == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pUiFont;
	ret = xuiDockPanelCreate(pApp->pContext, &pApp->pDockPanel, &desc);
	if ( ret != XUI_OK ) return ret;
	ret = xuiWidgetAddChild(pApp->pRoot, pApp->pDockPanel);
	if ( ret != XUI_OK ) return ret;

	if ( (ret = xideCreateHomeView(pApp, &pApp->pHomeView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateEditorView(pApp, &pApp->pEditorView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateProjectView(pApp, &pApp->pProjectView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateCommandView(pApp, &pApp->pCommandView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateHelpView(pApp, &pApp->pHelpView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateOutputView(pApp, &pApp->pOutputView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateTaskView(pApp, &pApp->pTaskView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateDebugView(pApp, &pApp->pDebugView)) != XUI_OK ) return ret;
	if ( (ret = xideCreateToolsView(pApp, &pApp->pToolsView)) != XUI_OK ) return ret;
	if ( (ret = xideCreatePropertyView(pApp, &pApp->pPropertyView)) != XUI_OK ) return ret;

	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_HOME), pApp->pHomeView, &pApp->iDockHome)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, "main.xl", pApp->pEditorView, &pApp->iDockEditor)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_TREE_WORKSPACE), pApp->pProjectView, &pApp->iDockProject)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_COMMAND_LIST), pApp->pCommandView, &pApp->iDockCommand)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_HELP), pApp->pHelpView, &pApp->iDockHelp)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_OUTPUT), pApp->pOutputView, &pApp->iDockOutput)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_TASKS), pApp->pTaskView, &pApp->iDockTask)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_DEBUG), pApp->pDebugView, &pApp->iDockDebug)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_TOOLS), pApp->pToolsView, &pApp->iDockTools)) != XUI_OK ) return ret;
	if ( (ret = xideDockAddWindow(pApp, xideTr(pApp->pContext, XIDE_TR_PROPERTIES), pApp->pPropertyView, &pApp->iDockProperty)) != XUI_OK ) return ret;

	(void)xuiDockPanelSetRegionSize(pApp->pDockPanel, XUI_DOCK_PANEL_REGION_LEFT, XUI_DOCK_PANEL_SIZE_PIXEL, 260.0f);
	(void)xuiDockPanelSetRegionSize(pApp->pDockPanel, XUI_DOCK_PANEL_REGION_RIGHT, XUI_DOCK_PANEL_SIZE_PIXEL, 300.0f);
	(void)xuiDockPanelSetRegionSize(pApp->pDockPanel, XUI_DOCK_PANEL_REGION_BOTTOM, XUI_DOCK_PANEL_SIZE_PIXEL, 190.0f);
	ret = xuiDockPanelDockWindow(pApp->pDockPanel, pApp->iDockHome, XUI_DOCK_PANEL_REGION_DOCUMENT, XUI_DOCK_PANEL_SIDE_FILL, 0.0f, &documentPane);
	if ( ret == XUI_OK ) pApp->iDockDocumentPane = documentPane;
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindowToPane(pApp->pDockPanel, pApp->iDockEditor, documentPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindow(pApp->pDockPanel, pApp->iDockCommand, XUI_DOCK_PANEL_REGION_LEFT, XUI_DOCK_PANEL_SIDE_FILL, 0.0f, &leftPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindow(pApp->pDockPanel, pApp->iDockProject, XUI_DOCK_PANEL_REGION_RIGHT, XUI_DOCK_PANEL_SIDE_FILL, 0.0f, &rightPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindowToPane(pApp->pDockPanel, pApp->iDockHelp, rightPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindowToPane(pApp->pDockPanel, pApp->iDockProperty, rightPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindow(pApp->pDockPanel, pApp->iDockOutput, XUI_DOCK_PANEL_REGION_BOTTOM, XUI_DOCK_PANEL_SIDE_FILL, 0.0f, &bottomPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindowToPane(pApp->pDockPanel, pApp->iDockTask, bottomPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindowToPane(pApp->pDockPanel, pApp->iDockDebug, bottomPane);
	if ( ret == XUI_OK ) ret = xuiDockPanelDockWindowToPane(pApp->pDockPanel, pApp->iDockTools, bottomPane);
	if ( ret == XUI_OK ) (void)xuiDockPanelSetPaneActiveWindow(pApp->pDockPanel, documentPane, pApp->iDockHome);
	if ( ret == XUI_OK ) (void)xuiDockPanelSetPaneActiveWindow(pApp->pDockPanel, rightPane, pApp->iDockProject);
	return ret;
}
