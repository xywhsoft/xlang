#include "foreign_xge3_font_resources.h"
#include <stdlib.h>
#include <xrt_decl.h>
const char* fixtureXgeFontPath(void)
{const char* path=getenv("XLANG_XGE_FONT_FIXTURE");return path ? path:"";}
const char* fixtureXgeFontTemp(void)
{const char* path=getenv("XLANG_XGE_FONT_TEMP");return path ? path:"";}
int fixtureXgeFontHotBegin(void)
{if(!xrtMemDebugEnabled() && !xrtMemDebugEnable(true))return 0;return xrtMemDebugFailAfter(0);}
int fixtureXgeFontHotEnd(void)
{int ok=!xrtMemDebugFailTriggered();xrtMemDebugFailClear();return ok;}
