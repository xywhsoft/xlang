#ifndef XLANG_IDE_UI_FONT_H
#define XLANG_IDE_UI_FONT_H
#include "app/xide_app.h"
typedef struct xide_ui_layout { xui_widget widgets[64]; xui_rect_t rectangles[64]; xui_vec2_t preferred[64]; size_t count; } xide_ui_layout;
char* xideUiFontDefaultPath(void);
int xideUiFontStart(xide_app* app);
int xideUiFontInitialize(xide_app* app);
int xideUiFontBindWidget(xide_app* app,xui_widget widget);
int xideUiFontBindMenu(xide_app* app,xui_widget menu);
int xideUiFontApply(xide_app* app,const char* path,int size,xide_model_error* error);
float xideUiFontScale(xide_app* app);
int xideUiHeight(xide_app* app,int base);
xui_rect_t xideUiRect(xide_app* app,xui_rect_t base);
int xideUiLayoutCapture(xide_ui_layout* layout,xui_widget content);
void xideUiLayoutApply(xide_app* app,const xide_ui_layout* layout,int manual);
#endif
