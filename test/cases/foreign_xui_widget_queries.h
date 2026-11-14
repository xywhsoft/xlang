#ifndef XL6_TEST_XUI_WIDGET_QUERIES_H
#define XL6_TEST_XUI_WIDGET_QUERIES_H
#include <xge3_runtime.h>
int fixtureLayoutProxy(xui_proxy*);
int fixtureLayoutFont(xui_proxy,xui_font*);
void fixtureQueryBudget(int);
void fixtureQueryCleanup(void);
void fixtureQueryHeapBegin(void);
void fixtureQueryHeapReport(int);
void fixtureQueryNativeHot(xui_widget,xui_widget);
void fixtureQueryWarm(xui_widget);
#endif
