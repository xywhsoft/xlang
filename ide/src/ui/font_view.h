#ifndef XLANG_IDE_FONT_VIEW_H
#define XLANG_IDE_FONT_VIEW_H
#include "app/xide_app.h"
enum { XIDE_FONT_PATH, XIDE_FONT_SIZE, XIDE_FONT_BROWSE, XIDE_FONT_RESET, XIDE_FONT_APPLY, XIDE_FONT_CANCEL, XIDE_FONT_PREVIEW, XIDE_FONT_CONTROL_COUNT };
int xideFontViewCreate(xide_app* app);
void xideFontViewDestroy(xide_app* app);
int xideFontViewOpen(xide_app* app);
int xideFontViewOpenUi(xide_app* app);
int xideFontViewUiFont(xide_app* app);
int xideFontViewIsOpen(xide_app* app);
void xideFontViewLayout(xide_app* app);
int xideFontViewCodeFont(xide_app* app);
xui_widget xideFontViewWindow(xide_app* app);
xui_widget xideFontViewControl(xide_app* app,int control);
#endif
