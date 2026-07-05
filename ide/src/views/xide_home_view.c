#include "views/xide_views.h"
#include "i18n/xide_i18n.h"

int xideCreateHomeView(xide_app_t* pApp, xui_widget* ppView)
{
	int ret;
	ret = xideViewCreatePanel(pApp, ppView, XUI_COLOR_RGBA(250, 252, 255, 255));
	if ( ret != XUI_OK ) return ret;
	ret = xideViewAddLabel(pApp, *ppView, xideTr(pApp->pContext, XIDE_TR_APP_TITLE), (xui_rect_t){28.0f, 24.0f, 260.0f, 32.0f},
		XUI_COLOR_RGBA(24, 34, 50, 255), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP, NULL);
	if ( ret != XUI_OK ) return ret;
	return xideViewAddLabel(pApp, *ppView,
		xideTr(pApp->pContext, XIDE_TR_HOME_BODY),
		(xui_rect_t){28.0f, 68.0f, 560.0f, 72.0f},
		XUI_COLOR_RGBA(84, 99, 122, 255), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP, NULL);
}
