#ifndef XL6_TEST_XUI_RADIO_OPTION_H
#define XL6_TEST_XUI_RADIO_OPTION_H
#include <xge3_runtime.h>
int fixtureLayoutProxy(xui_proxy*);
int fixtureLayoutFont(xui_proxy,xui_font*);
void fixtureOptionBudget(int);
void fixtureOptionCheckFont(xui_widget,xui_font);
void fixtureOptionCleanup(void);
void fixtureOptionPrefixBegin(void);
void fixtureOptionPrefixEnd(void);
void fixtureOptionHeapEqual(void);
void fixtureOptionHeapBegin(void);
void fixtureOptionHeapReport(int);
void fixtureOptionNativeCreate(xui_widget);
void fixtureOptionNativeQueries(xui_widget,xui_widget);
#endif
