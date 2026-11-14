#ifndef XL6_TEST_XUI_INPUT_EXTENDED_H
#define XL6_TEST_XUI_INPUT_EXTENDED_H
#include <xge3_runtime.h>
int fixtureInputProxy(xui_proxy*);
int fixtureLayoutFont(xui_proxy,xui_font*);
void fixtureInputCleanupCheck(void);
void fixtureInputObserveErrors(xui_context);
int fixtureInputErrorCount(void);
void fixtureInputField(int,int (*)(const xui_event_t*,void*),void*);
void fixtureInputBudget(int);
void fixtureInputHeapBegin(void);
void fixtureInputHeapReport(int);
void fixtureInputNativeHot(xui_context);
int fixtureInputTextCapacity(void);
#endif
