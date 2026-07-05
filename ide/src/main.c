#include "app/xide_app.h"
#include "i18n/xide_i18n.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	static xide_app_t app;
	xge_desc_t desc;
	int ret;
	int autoRun;
	memset(&app, 0, sizeof(app));
	ret = xideAppParseArgs(&app, argc, argv);
	if ( ret == 1 ) return 0;
	if ( ret != XGE_OK ) return 1;
	autoRun = (app.iMaxFrames > 0) || (app.fMaxSeconds > 0.0);
	xrtInit();
	memset(&desc, 0, sizeof(desc));
	desc.iWidth = XIDE_WINDOW_W;
	desc.iHeight = XIDE_WINDOW_H;
	desc.sTitle = xideTrLang(app.iLanguageId, XIDE_TR_APP_TITLE);
	desc.iFlags = XGE_INIT_WINDOW | XGE_INIT_VSYNC;
	if ( !autoRun ) desc.iFlags |= XGE_INIT_ON_DEMAND;
	desc.iRunMode = XGE_RUN_GAME_LOOP;
	desc.iTargetFPS = autoRun ? 60 : 30;
	ret = xgeInit(&desc);
	if ( ret != XGE_OK ) {
		printf("xlang_ide: xgeInit failed: %d\n", ret);
		xrtUnit();
		return 1;
	}
	ret = xideAppCreate(&app);
	if ( ret != XUI_OK ) {
		printf("xlang_ide: create failed: %d\n", ret);
		xideAppDestroy(&app);
		xgeUnit();
		xrtUnit();
		return 1;
	}
	if ( app.bCheckLanguageRules ) {
		ret = xideAppCheckLanguageRules(&app);
		if ( ret != XUI_OK ) printf("xlang_ide: language rule check failed: %d\n", ret);
		xideAppDestroy(&app);
		xgeUnit();
		xrtUnit();
		return ret == XUI_OK ? 0 : 1;
	}
	if ( app.bCheckWorkspace ) {
		ret = xideAppCheckWorkspace(&app);
		if ( ret != XUI_OK ) printf("xlang_ide: workspace check failed: %d\n", ret);
		xideAppDestroy(&app);
		xgeUnit();
		xrtUnit();
		return ret == XUI_OK ? 0 : 1;
	}
	xgeRenderRequest();
	ret = xgeRun(xideAppFrame, &app);
	xideAppDestroy(&app);
	xgeUnit();
	xrtUnit();
	return ret == XGE_OK ? 0 : 1;
}
