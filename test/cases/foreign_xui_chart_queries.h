#ifndef FOREIGN_XUI_CHART_QUERIES_H
#define FOREIGN_XUI_CHART_QUERIES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
int fixtureRenderProxy(xui_proxy* output);
int fixtureRenderBalanced(void);
void fixtureChartView(xui_widget,int,double,double,double,double);
void fixtureChartBrush(xui_widget,int,double,double,double,double);
void fixtureChartAnimation(xui_widget,int,int,float,float);
void fixtureChartArea(xui_widget,int,int,uint32_t);
void fixtureChartRadius(xui_widget,int,int,float,float);
void fixtureChartColor(xui_widget,int,int,uint32_t,uint32_t);
void fixtureChartHeapBegin(void);
void fixtureChartHeapEnd(int native);
void fixtureChartNativeHot(xui_widget,int,int);
#ifdef __cplusplus
}
#endif
#endif
