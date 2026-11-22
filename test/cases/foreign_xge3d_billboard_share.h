#ifndef XL_TEST_XGE3D_BILLBOARD_SHARE_H
#define XL_TEST_XGE3D_BILLBOARD_SHARE_H

#include <xge3d_renderer.h>

void fixtureBillboardHeapBegin(void);
void fixtureBillboardHeapEnd(int native);
void fixtureBillboardHold(xge3d_billboard_batch *batch);
void fixtureBillboardSameStorage(xge3d_billboard_batch *first,xge3d_billboard_batch *second);
void fixtureBillboardDropLast(void);
void fixtureBillboardFail(int prefix);

#endif
