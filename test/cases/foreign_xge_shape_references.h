#ifndef FOREIGN_XGE_SHAPE_REFERENCES_H
#define FOREIGN_XGE_SHAPE_REFERENCES_H

#include "xge3_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
void shapeReferencesHeapBegin(void);
void shapeReferencesHeapEnd(int mode);
void shapeReferencesHeapBudget(int allowance);
bool shapeReferencesAllocationFailed(void);
void shapeReferencesNativeHot(xge_shape_ex_scene scene);
#ifdef __cplusplus
}
#endif

#endif
