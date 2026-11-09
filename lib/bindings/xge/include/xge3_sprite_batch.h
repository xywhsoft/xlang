#ifndef XL6_XGE3_SPRITE_BATCH_H
#define XL6_XGE3_SPRITE_BATCH_H
#include "xge3_render_resources.h"
XL6_XGE_RECORD_POLICY(xl6Xge3SpriteBatchPolicy,"xge.spriteBatch",xge_sprite_batch_t,xgeSpriteBatchFree)
#define xl6Xge3SpriteBatchPolicy (*xl6Xge3SpriteBatchPolicyGet())
#endif
