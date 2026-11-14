#ifndef FOREIGN_XUI_TEXT_LAYOUT_H
#define FOREIGN_XUI_TEXT_LAYOUT_H
#include "xge3_runtime.h"
int fixtureLayoutProxy(xui_proxy* output);
int fixtureLayoutFont(xui_proxy proxy,xui_font* output);
int fixtureLayoutSurface(xui_proxy proxy,xui_surface* output);
void fixtureLayoutReady(int ready);
void fixtureLayoutBudget(int allowance);
void fixtureLayoutArmPartialFailure(void);
void fixtureLayoutCheck(void);
void fixtureLayoutHeapBegin(void);
void fixtureLayoutHeapReport(int native);
void fixtureLayoutNativeHot(xui_text_layout layout);
#endif
