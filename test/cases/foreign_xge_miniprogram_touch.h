#ifndef FOREIGN_XGE_MINIPROGRAM_TOUCH_H
#define FOREIGN_XGE_MINIPROGRAM_TOUCH_H
#include "xge3_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif
void touchHeapBegin(void);
void touchHeapEnd(int allocations);
void touchSetBudget(int allowance);
void touchNativeHot(void);
#ifdef __cplusplus
}
#endif
#endif
