#pragma once
#include "xge3_runtime.h"
void fixtureDockBudget(int);
void fixtureDockBegin(void);
void fixtureDockReport(int);
float fixtureDockInvalid(int);
void fixtureDockCompare(xui_widget,int,float,float,const xui_dock_drop_info_t*);
void fixtureDockNativeHot(xui_widget,int,float,float);
