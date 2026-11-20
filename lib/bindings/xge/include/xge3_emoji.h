#ifndef XL6_XGE3_EMOJI_H
#define XL6_XGE3_EMOJI_H
#include "xge3_render_resources.h"

XL6_XGE_NATIVE_POLICY(xl6Xge3EmojiPackPolicy,"xge.emojiPack",struct xge_emoji_pack_t,xgeEmojiPackFree)
#define xl6Xge3EmojiPackPolicy (*xl6Xge3EmojiPackPolicyGet())
#endif
