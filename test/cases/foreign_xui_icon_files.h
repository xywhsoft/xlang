#ifndef FOREIGN_XUI_ICON_FILES_H
#define FOREIGN_XUI_ICON_FILES_H
#include "test/cases/foreign_xui_render_backend.h"
int fixtureIconFilesProxy(xui_proxy* output);
void fixtureIconFilesObserve(int raster,const char* path);
int fixtureIconFilesLoaded(void);
void fixtureIconFilesHeapBegin(void);
int fixtureIconFilesHeapUnchanged(void);
void fixtureIconFilesHeapReport(int native);
void fixtureIconFilesNativePerformance(xui_icon_category category,xui_widget picker);
int fixtureIconFilesFailingIcon(xui_icon_category category,xui_icon* output);
int fixtureIconFilesDropBalanced(void);
#endif
