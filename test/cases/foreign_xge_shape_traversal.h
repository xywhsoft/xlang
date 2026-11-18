#ifndef FOREIGN_XGE_SHAPE_TRAVERSAL_H
#define FOREIGN_XGE_SHAPE_TRAVERSAL_H

#include "xge3_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
void shapeTraversalHeapBegin(void);
void shapeTraversalHeapEnd(int mode);
void shapeTraversalHeapBudget(int allowance);
bool shapeTraversalAllocationFailed(void);
void shapeTraversalNativeHot(xge_shape_ex_scene scene);
#ifdef __cplusplus
}
#endif

#endif
