#ifndef FOREIGN_XGE3_DEBUG_LIVE_H
#define FOREIGN_XGE3_DEBUG_LIVE_H
#include <xge3_runtime.h>
void fixtureDebugHeapBegin(void);
void fixtureDebugHeapEnd(int native);
void fixtureDebugNativeSamples(int repetitions);
void fixtureDebugBudget(int allowance);
#endif
