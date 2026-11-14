#ifndef XL6_XUI_CONSTRUCTOR_RETIREMENT_H
#define XL6_XUI_CONSTRUCTOR_RETIREMENT_H
#include <xge3_runtime.h>
int fixtureRenderProxy(xui_proxy*);
int fixtureRenderFont(xui_proxy,xui_font*);
int fixtureRenderBalanced(void);
void fixtureRenderBudget(int);
void fixtureRetireSelect(int,int);
void fixtureRetireBoundBegin(void);
int fixtureRetireBoundEnd(void);
void fixtureRetireHeapBegin(void);
void fixtureRetireHeapEqual(void);
void fixtureRetireMenuCheck(xui_widget,int);
#endif
