#pragma once
#include "xge3_runtime.h"
int fixtureXuiErrorMutable(xui_context,int);
int fixtureXuiErrorAmbient(xui_context);
void fixtureXuiErrorBudget(int);
void fixtureXuiErrorHeapBegin(void);
void fixtureXuiErrorHeapReport(int);
void fixtureXuiErrorNativeHot(xui_context);
