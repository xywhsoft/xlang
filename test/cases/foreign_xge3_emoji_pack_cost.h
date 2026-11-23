#ifndef XL6_FIXTURE_XGE3_EMOJI_COST_H
#define XL6_FIXTURE_XGE3_EMOJI_COST_H
#include <xge3_runtime.h>

int fixtureEmojiBegin(void);
int fixtureEmojiEnd(void);
int fixtureEmojiNative(xge_emoji_pack pack);
int fixtureEmojiFail(int budget);
void fixtureEmojiClear(void);

#endif
