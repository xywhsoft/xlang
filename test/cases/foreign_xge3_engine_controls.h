#ifndef XL6_TEST_XGE_ENGINE_CONTROLS_H
#define XL6_TEST_XGE_ENGINE_CONTROLS_H

#include <xge3_runtime.h>

void fixtureStateOomBudget(int allowance);
int fixtureInstallLegacyEngineCallbacks(void);
int fixtureLegacyQuitCalls(void);
int fixtureLegacyErrorCalls(void);

#endif
