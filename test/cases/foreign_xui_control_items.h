#ifndef XL6_TEST_XUI_CONTROL_ITEMS_H
#define XL6_TEST_XUI_CONTROL_ITEMS_H
#include "test/cases/foreign_xui_render_backend.h"
void fixtureControlBoundBegin(void);
int fixtureControlBoundEnd(void);
void fixtureControlHeapBegin(void);
void fixtureControlHeapEqual(void);
void fixtureControlReport(int,int);
void fixtureControlSeed(int,xui_widget,xui_widget);
int fixtureControlNativeSet(int,xui_widget,xui_widget);
void fixtureControlNativeHot(int,xui_widget,xui_widget);
void fixtureControlCheck(int,xui_widget,xui_widget,int);
#endif
