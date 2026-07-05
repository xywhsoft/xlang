#include "views/xide_views.h"
#include "i18n/xide_i18n.h"

#include <string.h>

int xideCreateOutputView(xide_app_t* pApp, xui_widget* ppView)
{
	xui_terminal_desc_t desc;
	int ret;
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.pFont = pApp->pCodeFont != NULL ? pApp->pCodeFont : pApp->pUiFont;
	desc.iColumns = 100;
	desc.iRows = 12;
	desc.iScrollbackLimit = 2000;
	desc.iBackgroundColor = XUI_COLOR_RGBA(18, 24, 34, 255);
	desc.iForegroundColor = XUI_COLOR_RGBA(224, 232, 244, 255);
	desc.iCursorColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	ret = xuiTerminalCreate(pApp->pContext, &pApp->pOutputTerminal, &desc);
	if ( ret != XUI_OK ) return ret;
	(void)xuiTerminalWriteText(pApp->pOutputTerminal, xideTr(pApp->pContext, XIDE_TR_OUTPUT_INITIAL));
	*ppView = pApp->pOutputTerminal;
	return XUI_OK;
}
