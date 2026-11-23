#include "foreign_xge3_font_resources.h"
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include <xrt_decl.h>
const char* fixtureXgeFontPath(void)
{const char* path=getenv("XLANG_XGE_FONT_FIXTURE");return path ? path:"";}
const char* fixtureXgeFontTemp(void)
{const char* path=getenv("XLANG_XGE_FONT_TEMP");return path ? path:"";}
int fixtureXgeFontHotBegin(void)
{if(!xrtMemDebugEnabled() && !xrtMemDebugEnable(true))return 0;return xrtMemDebugFailAfter(0);}
int fixtureXgeFontHotEnd(void)
{int ok=!xrtMemDebugFailTriggered();xrtMemDebugFailClear();return ok;}

static xmemdebugsnapshot shareBefore;
static double shareStarted;

void fixtureXgeFontShareSampleBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&shareBefore);
    shareStarted = xrtTimer();
}

void fixtureXgeFontShareSampleEnd(int language)
{
    double milliseconds = (xrtTimer() - shareStarted) * 1000.0;
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == shareBefore.LiveCount && after.LiveBytes == shareBefore.LiveBytes);
    printf("XGE_FONT_SHARE_SAMPLE language=%d count=256 alloc=%" PRIu64
        " free=%" PRIu64 " realloc=%" PRIu64 " wallMs=%.6f\n", language,
        after.AllocCount - shareBefore.AllocCount, after.FreeCount - shareBefore.FreeCount,
        after.ReallocCount - shareBefore.ReallocCount, milliseconds);
}

void fixtureXgeFontShareSampleC(xge_font_face face, xge_font font)
{
    fixtureXgeFontShareSampleBegin();
    for (int index = 0; index < 256; ++index) {
        xge_font_face sharedFace = NULL;
        assert(xgeFontFaceShareOwned(face, &sharedFace) == XGE_OK && sharedFace);
        xgeFontFaceFree(sharedFace);
        xge_font sharedFont = NULL;
        assert(xgeFontShareOwned(font, &sharedFont) == XGE_OK && sharedFont);
        xgeFontFree(sharedFont);
    }
    fixtureXgeFontShareSampleEnd(0);
}
