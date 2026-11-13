#ifndef FOREIGN_XUI_EDIT_SEARCH_H
#define FOREIGN_XUI_EDIT_SEARCH_H
#include <xge3_runtime.h>
void fixtureEditSearchHeapBegin(void);
void fixtureEditSearchHeapReport(int native);
void fixtureEditSearchNativePerformance(xui_widget editor);
int fixtureEditSearchSelection(xui_widget editor,int start,int end);
#endif
