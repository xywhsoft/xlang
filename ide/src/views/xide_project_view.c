#include "views/xide_views.h"
#include "i18n/xide_i18n.h"
#include "lib/xrt/xrt.h"

#include <stdio.h>
#include <string.h>

static int xideWorkspaceTreeAddNode(xui_widget pTree, int iId, int iParent, const char* sText, int bExpanded, int bIcon)
{
	int ret;
	ret = xuiTreeViewAddNode(pTree, iId, iParent, sText);
	if ( ret != XUI_OK ) return ret;
	(void)xuiTreeViewSetNodeExpanded(pTree, iId, bExpanded);
	(void)xuiTreeViewSetNodeDecorations(pTree, iId, bIcon, 0);
	return XUI_OK;
}

static int xideWorkspaceTreeAddProject(xui_widget pTree, const xide_workspace_project_t* pProject, int iWorkspaceId)
{
	char sProjectTitle[XIDE_WORKSPACE_NAME_MAX + 4];
	int ret;
	int i;
	if ( pTree == NULL || pProject == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pProject->bChanged ) {
		snprintf(sProjectTitle, sizeof(sProjectTitle), "* %s", pProject->sName);
	} else {
		snprintf(sProjectTitle, sizeof(sProjectTitle), "%s", pProject->sName);
	}
	ret = xideWorkspaceTreeAddNode(pTree, pProject->iNodeId, iWorkspaceId, sProjectTitle, pProject->bExpanded, 1);
	if ( ret != XUI_OK ) return ret;
	ret = xideWorkspaceTreeAddNode(pTree, pProject->iSourceNodeId, pProject->iNodeId, "源代码", pProject->bSourceExpanded, 1);
	if ( ret != XUI_OK ) return ret;
	for ( i = 0; i < pProject->iFileCount; i++ ) {
		ret = xideWorkspaceTreeAddNode(pTree, pProject->arrFiles[i].iNodeId, pProject->iSourceNodeId, pProject->arrFiles[i].sName, 0, 1);
		if ( ret != XUI_OK ) return ret;
	}
	ret = xideWorkspaceTreeAddNode(pTree, pProject->iLibraryNodeId, pProject->iNodeId, "扩展库", pProject->bLibraryExpanded, 1);
	if ( ret != XUI_OK ) return ret;
	for ( i = 0; i < pProject->iLibraryCount; i++ ) {
		ret = xideWorkspaceTreeAddNode(pTree, pProject->arrLibraries[i].iNodeId, pProject->iLibraryNodeId, pProject->arrLibraries[i].sName, 0, 1);
		if ( ret != XUI_OK ) return ret;
	}
	ret = xideWorkspaceTreeAddNode(pTree, pProject->iResourceNodeId, pProject->iNodeId, "资源", pProject->bResourceExpanded, 1);
	if ( ret != XUI_OK ) return ret;
	for ( i = 0; i < pProject->iResourceCount; i++ ) {
		ret = xideWorkspaceTreeAddNode(pTree, pProject->arrResources[i].iNodeId, pProject->iResourceNodeId, pProject->arrResources[i].sName, 0, 1);
		if ( ret != XUI_OK ) return ret;
	}
	return XUI_OK;
}

int xideWorkspaceViewOpenNode(xide_app_t* pApp, int iNodeId)
{
	char sPath[XIDE_WORKSPACE_PATH_MAX];
	int ret;
	if ( pApp == NULL || pApp->pEditor == NULL || iNodeId <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xideWorkspaceFindItemByNodeId(&pApp->tWorkspace, iNodeId) == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xideWorkspaceResolveNodePath(&pApp->tWorkspace, iNodeId, sPath, (int)sizeof(sPath)) ) return XUI_ERROR_FILE_NOT_FOUND;
	ret = xuiCodeEditLoadTextFile(pApp->pEditor, sPath, XRT_CP_AUTO);
	if ( ret != XUI_OK ) return ret;
	if ( pApp->pFindScope != NULL ) (void)xuiCodeFindScopeAddEditorNamed(pApp->pFindScope, pApp->pEditor, sPath);
	(void)xuiWidgetInvalidate(pApp->pEditor, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

static int xideWorkspaceTreeDoubleClick(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	int iNodeId;
	(void)pWidget;
	if ( pApp == NULL || pEvent == NULL || pEvent->iType != XUI_EVENT_POINTER_DOUBLE_CLICK ) return XUI_OK;
	iNodeId = xuiTreeViewGetSelected(pApp->pProjectTree);
	(void)xideWorkspaceViewOpenNode(pApp, iNodeId);
	return XUI_OK;
}

int xideWorkspaceViewRefresh(xide_app_t* pApp)
{
	xide_workspace_t* pWorkspace;
	char sTitle[XIDE_WORKSPACE_NAME_MAX + 16];
	int i;
	int ret;
	if ( pApp == NULL || pApp->pProjectTree == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pWorkspace = &pApp->tWorkspace;
	ret = xuiTreeViewClear(pApp->pProjectTree);
	if ( ret != XUI_OK ) return ret;
	snprintf(sTitle, sizeof(sTitle), "%s%s", pWorkspace->sName, pWorkspace->bTemp ? " [临时]" : "");
	ret = xideWorkspaceTreeAddNode(pApp->pProjectTree, pWorkspace->iNodeId, -1, sTitle, pWorkspace->bExpanded, 1);
	if ( ret != XUI_OK ) return ret;
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		ret = xideWorkspaceTreeAddProject(pApp->pProjectTree, &pWorkspace->arrProjects[i], pWorkspace->iNodeId);
		if ( ret != XUI_OK ) return ret;
	}
	for ( i = 0; i < pWorkspace->iLooseFileCount; i++ ) {
		ret = xideWorkspaceTreeAddNode(pApp->pProjectTree, pWorkspace->arrLooseFiles[i].iNodeId, pWorkspace->iNodeId, pWorkspace->arrLooseFiles[i].sName, 0, 1);
		if ( ret != XUI_OK ) return ret;
	}
	if ( pWorkspace->iProjectCount > 0 && pWorkspace->arrProjects[0].iFileCount > 0 ) {
		(void)xuiTreeViewSetSelected(pApp->pProjectTree, pWorkspace->arrProjects[0].arrFiles[0].iNodeId);
		(void)xuiTreeViewEnsureVisible(pApp->pProjectTree, pWorkspace->arrProjects[0].arrFiles[0].iNodeId);
	}
	return XUI_OK;
}

int xideCreateProjectView(xide_app_t* pApp, xui_widget* ppView)
{
	xui_tree_view_desc_t desc;
	int ret;
	if ( pApp == NULL || ppView == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pUiFont;
	desc.fItemHeight = 24.0f;
	desc.fIndent = 18.0f;
	desc.iScrollbarMode = XUI_SCROLLBAR_POLICY_AUTO;
	desc.iBackgroundColor = XUI_COLOR_RGBA(248, 251, 255, 255);
	desc.iTextColor = XUI_COLOR_RGBA(38, 50, 68, 255);
	ret = xuiTreeViewCreate(pApp->pContext, &pApp->pProjectTree, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiWidgetSetEventHandler(pApp->pProjectTree, XUI_EVENT_POINTER_DOUBLE_CLICK, xideWorkspaceTreeDoubleClick, pApp);
	ret = xideWorkspaceViewRefresh(pApp);
	if ( ret != XUI_OK ) return ret;
	*ppView = pApp->pProjectTree;
	return XUI_OK;
}
