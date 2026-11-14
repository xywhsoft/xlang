#ifndef XL6_TEST_XGE_ERROR_FIELDS_H
#define XL6_TEST_XGE_ERROR_FIELDS_H
#include <xge3_runtime.h>
int fixtureXgeErrorMutable(int mode);
void fixtureXgeErrorBudget(int allowance);
void fixtureXgeErrorHeapBegin(void);
void fixtureXgeErrorHeapReport(int native);
void fixtureXgeErrorNativeHot(void);
#endif
