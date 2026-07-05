#ifndef XIDE_APP_H
#define XIDE_APP_H

#include "xui.h"
#include "xge.h"
#include "workspace/xide_workspace.h"

#define XIDE_WINDOW_W 1280
#define XIDE_WINDOW_H 800

typedef struct xide_app_t {
	xui_proxy_t tProxy;
	xui_context pContext;
	xui_surface pTarget;
	xui_font pUiFont;
	xui_font pCodeFont;
	xui_code_find_scope pFindScope;
	xui_widget pRoot;
	xui_widget pMenuBar;
	xui_widget pToolbar;
	xui_widget pStatusBar;
	xui_widget pDockPanel;
	xui_widget pHomeView;
	xui_widget pProjectView;
	xui_widget pCommandView;
	xui_widget pHelpView;
	xui_widget pEditorView;
	xui_widget pOutputView;
	xui_widget pTaskView;
	xui_widget pDebugView;
	xui_widget pToolsView;
	xui_widget pPropertyView;
	xui_widget pProjectTree;
	xui_widget pCommandTree;
	xui_widget pEditor;
	xui_widget pOutputTerminal;
	xui_widget pTaskTable;
	xide_workspace_t tWorkspace;
	int iDockHome;
	int iDockProject;
	int iDockCommand;
	int iDockHelp;
	int iDockEditor;
	int iDockOutput;
	int iDockTask;
	int iDockDebug;
	int iDockTools;
	int iDockProperty;
	int iViewportW;
	int iViewportH;
	int iFrame;
	int iMaxFrames;
	int iLanguageId;
	int bCheckLanguageRules;
	int bCheckWorkspace;
	double fMaxSeconds;
	int bHasMouse;
	float fLastMouseX;
	float fLastMouseY;
	uint32_t iLastButtons;
} xide_app_t;

int xideAppParseArgs(xide_app_t* pApp, int argc, char** argv);
int xideAppCreate(xide_app_t* pApp);
void xideAppDestroy(xide_app_t* pApp);
int xideAppCheckLanguageRules(xide_app_t* pApp);
int xideAppCheckWorkspace(xide_app_t* pApp);
int xideAppFrame(void* pUser);

#endif
