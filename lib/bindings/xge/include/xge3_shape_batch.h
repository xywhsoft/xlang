#ifndef XL6_XGE3_SHAPE_BATCH_H
#define XL6_XGE3_SHAPE_BATCH_H
#include "xge3_render_resources.h"
XL6_XGE_RECORD_POLICY(xl6Xge3ShapeBatchPolicy,"xge.shapeBatch",xge_shape_batch_t,xgeShapeBatchFree)
#define xl6Xge3ShapeBatchPolicy (*xl6Xge3ShapeBatchPolicyGet())
#endif
