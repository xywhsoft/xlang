#include "views/xide_views.h"

#include <stdint.h>
#include <string.h>

static int xidePanelRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	uintptr_t colorBits;
	xui_rect_t r;
	(void)iStateId;
	if ( pWidget == NULL || pDraw == NULL || pApp == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	colorBits = (uintptr_t)xuiWidgetGetUserData(pWidget);
	r = xuiWidgetGetContentRect(pWidget);
	return pApp->tProxy.drawRectFill(&pApp->tProxy, pDraw, r, (uint32_t)colorBits);
}

int xideViewCreatePanel(xide_app_t* pApp, xui_widget* ppView, uint32_t bg)
{
	int ret;
	if ( pApp == NULL || ppView == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	ret = xuiWidgetCreate(pApp->pContext, ppView);
	if ( ret != XUI_OK ) return ret;
	(void)xuiWidgetSetLayoutType(*ppView, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetUserData(*ppView, (void*)(uintptr_t)bg);
	return xuiWidgetSetCacheRenderCallback(*ppView, xidePanelRender, pApp);
}

int xideViewAddLabel(xide_app_t* pApp, xui_widget pParent, const char* text, xui_rect_t rect, uint32_t color, uint32_t flags, xui_widget* ppLabel)
{
	xui_label_desc_t desc;
	xui_widget label = NULL;
	int ret;
	if ( pApp == NULL || pParent == NULL || text == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.sText = text;
	desc.pFont = pApp->pUiFont;
	desc.iTextColor = color;
	desc.iTextFlags = flags;
	desc.iWrapMode = XUI_TEXT_WRAP_WORD;
	ret = xuiLabelCreate(pApp->pContext, &label, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiWidgetSetRect(label, rect);
	ret = xuiWidgetAddChild(pParent, label);
	if ( ret != XUI_OK ) return ret;
	if ( ppLabel != NULL ) *ppLabel = label;
	return XUI_OK;
}
