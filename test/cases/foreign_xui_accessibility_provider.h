#ifndef XL6_TEST_XUI_ACCESSIBILITY_PROVIDER_H
#define XL6_TEST_XUI_ACCESSIBILITY_PROVIDER_H
#include <xge3_runtime.h>
void fixtureAccessibleHeapBegin(void);
void fixtureAccessibleBudget(int allowance);
void fixtureAccessibleHeapEnd(void);
void fixtureAccessibleHotEnd(int native);
void fixtureAccessibleNativeCount(xui_widget widget);
#endif
