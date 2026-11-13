#ifndef FOREIGN_XGE3_DECODE_BUDGET_H
#define FOREIGN_XGE3_DECODE_BUDGET_H
#include <xge3_runtime.h>
int fixtureBudgetAttach(xge3d_decode_budget* budget);
int fixtureBudgetTry(xge3d_decode_budget* budget,uint64_t bytes);
xge3d_decode_budget_info_t fixtureBudgetHeldInfo(void);
void fixtureBudgetDetach(void);
int fixtureBudgetDuringCall(xge3d_decode_budget* budget,void (*callback)(void*),void* user);
void fixtureBudgetFail(int allowance);
void fixtureBudgetHeapBegin(void);
void fixtureBudgetHeapReport(int native);
void fixtureBudgetNativePerformance(xge3d_decode_budget* budget);
#endif
