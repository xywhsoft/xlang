#include "views/xide_views.h"
#include "i18n/xide_i18n.h"

int xideCreateDebugView(xide_app_t* pApp, xui_widget* ppView)
{
	int ret;
	ret = xideViewCreatePanel(pApp, ppView, XUI_COLOR_RGBA(250, 252, 255, 255));
	if ( ret != XUI_OK ) return ret;
	return xideViewAddLabel(pApp, *ppView,
		xideTr(pApp->pContext, XIDE_TR_DEBUG_BODY),
		(xui_rect_t){16.0f, 16.0f, 360.0f, 100.0f},
		XUI_COLOR_RGBA(52, 65, 84, 255), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP, NULL);
}
