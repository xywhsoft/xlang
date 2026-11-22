#ifndef FOREIGN_XUI_ICON_MODELS_H
#define FOREIGN_XUI_ICON_MODELS_H
#include <xge3_runtime.h>
int fixtureIconProxy(xui_proxy* output);
void fixtureIconBudget(int count);
void fixtureIconHeapBegin(void);
int fixtureIconHeapUnchanged(void);
int fixtureIconLiveHeapUnchanged(void);
void fixtureIconDashes(void* output, size_t size);
void fixtureIconDrawStyled(xui_context context, xui_surface surface, xui_icon icon);
void fixtureIconReferenceHot(xui_icon_reference_t* reference, xui_icon icon);
#endif
