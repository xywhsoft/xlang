#ifndef XIDE_VIEWS_H
#define XIDE_VIEWS_H

#include "app/xide_app.h"

int xideCreateHomeView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateProjectView(xide_app_t* pApp, xui_widget* ppView);
int xideWorkspaceViewRefresh(xide_app_t* pApp);
int xideCreateCommandView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateHelpView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateEditorView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateOutputView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateTaskView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateDebugView(xide_app_t* pApp, xui_widget* ppView);
int xideCreateToolsView(xide_app_t* pApp, xui_widget* ppView);
int xideCreatePropertyView(xide_app_t* pApp, xui_widget* ppView);

int xideViewCreatePanel(xide_app_t* pApp, xui_widget* ppView, uint32_t bg);
int xideViewAddLabel(xide_app_t* pApp, xui_widget pParent, const char* text, xui_rect_t rect, uint32_t color, uint32_t flags, xui_widget* ppLabel);

#endif
