#include "views/xide_views.h"
#include "i18n/xide_i18n.h"

#include <string.h>

int xideCreateCommandView(xide_app_t* pApp, xui_widget* ppView)
{
	xui_tree_view_desc_t desc;
	int ret;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pUiFont;
	desc.fItemHeight = 24.0f;
	desc.fIndent = 18.0f;
	desc.iScrollbarMode = XUI_SCROLLBAR_POLICY_AUTO;
	desc.iBackgroundColor = XUI_COLOR_RGBA(248, 251, 255, 255);
	desc.iTextColor = XUI_COLOR_RGBA(38, 50, 68, 255);
	ret = xuiTreeViewCreate(pApp->pContext, &pApp->pCommandTree, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 1, -1, "xlang");
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 2, 1, xideTr(pApp->pContext, XIDE_TR_TREE_LANGUAGE));
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 3, 2, xideTr(pApp->pContext, XIDE_TR_BUILD));
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 4, 2, xideTr(pApp->pContext, XIDE_TR_RUN));
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 5, 2, xideTr(pApp->pContext, XIDE_TR_DEBUG));
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 6, 1, xideTr(pApp->pContext, XIDE_TR_TREE_STANDARD_LIBRARY));
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 7, 6, "math");
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 8, 6, "string");
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 9, 6, "file");
	(void)xuiTreeViewAddNode(pApp->pCommandTree, 10, 6, "http.server");
	(void)xuiTreeViewSetNodeExpanded(pApp->pCommandTree, 1, 1);
	(void)xuiTreeViewSetNodeExpanded(pApp->pCommandTree, 2, 1);
	(void)xuiTreeViewSetNodeExpanded(pApp->pCommandTree, 6, 1);
	*ppView = pApp->pCommandTree;
	return XUI_OK;
}
