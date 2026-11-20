#ifndef XL6_FIXTURE_XGE3_EMOJI_COST_H
#define XL6_FIXTURE_XGE3_EMOJI_COST_H
#include <xge3_runtime.h>
#include <stdio.h>

static xmemdebugsnapshot fixtureEmojiBefore;
static int fixtureEmojiBegin(void)
{
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 0;
    xrtMemDebugSnapshot(&fixtureEmojiBefore);
    return 1;
}

static int fixtureEmojiEnd(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    int unchanged=after.AllocCount==fixtureEmojiBefore.AllocCount &&
           after.ReallocCount==fixtureEmojiBefore.ReallocCount &&
           after.FreeCount==fixtureEmojiBefore.FreeCount;
    if (!unchanged) {
        fprintf(stderr,"warm Emoji heap: %llu alloc, %llu realloc, %llu free\n",
                (unsigned long long)(after.AllocCount-fixtureEmojiBefore.AllocCount),
                (unsigned long long)(after.ReallocCount-fixtureEmojiBefore.ReallocCount),
                (unsigned long long)(after.FreeCount-fixtureEmojiBefore.FreeCount));
    }
    return unchanged;
}

static int fixtureEmojiNative(xge_emoji_pack pack)
{
    int bytes=0;
    for (int i=0;i<1000;++i) {
        xge_emoji_match_t match={0};
        if (xgeEmojiPackFind(pack,"\xf0\x9f\x98\x80!",5,&match)!=1) return -1;
        bytes+=match.iTextSize;
    }
    return bytes;
}

static int fixtureEmojiFail(int budget) { return xrtMemDebugFailAfter(budget); }
static void fixtureEmojiClear(void) { xrtMemDebugFailClear(); }
#endif
