#ifndef FOREIGN_XUI_RENDER_BACKEND_H
#define FOREIGN_XUI_RENDER_BACKEND_H
#include <xge3_runtime.h>
int fixtureRenderProxy(xui_proxy* output);
int fixtureRenderSecondaryProxy(xui_proxy* output);
int fixtureRenderFont(xui_proxy proxy,xui_font* output);
int fixtureRenderSurface(xui_proxy proxy,xui_surface* output);
int fixtureRenderBalanced(void);
int fixtureRenderSawText(const char* text);
void fixtureRenderResetText(void);
void fixtureRenderBudget(int allowance);
int fixtureRenderNativeFormatter(xui_widget picker);
void fixtureRenderHeapBegin(void);
void fixtureRenderHeapResult(int native);
#endif
