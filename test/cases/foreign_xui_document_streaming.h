#pragma once
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixtureStreamBegin(void);
void fixtureStreamBudget(int count);
void fixtureStreamEndBudget(void);
void fixtureStreamBalanced(void);
void fixtureStreamReport(int native);
void fixtureStreamNativeHot(xui_document_prepare prepare);
#ifdef __cplusplus
}
#endif
