#ifndef XL6_TEST_XUI_MENU_ITEMS_H
#define XL6_TEST_XUI_MENU_ITEMS_H
#include "test/cases/foreign_xui_render_backend.h"
void fixtureMenuBoundBegin(void);
int fixtureMenuBoundEnd(void);
void fixtureMenuHeapBegin(void);
void fixtureMenuHeapEqual(void);
void fixtureMenuHotReport(int);
void fixtureMenuNativeHot(xui_widget,xui_widget,int);
void fixtureMenuCheck(xui_widget,int,xui_widget);
void fixtureMenuNullCheck(xui_widget);
void fixtureMenuSelfCheck(xui_widget);
#endif
