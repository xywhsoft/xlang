#ifndef XLANG_IDE_EDITOR_FONT_H
#define XLANG_IDE_EDITOR_FONT_H
#include "app/xide_app.h"
char* xideWindowsFontPath(const char* filename);
char* xideBundledFontPath(void);
char* xideEditorFontDefaultPath(void);
int xideFontLoad(xide_app* app,const char* path,int size,xui_font* font,xide_model_error* error);
int xideEditorFontStart(xide_app* app);
int xideEditorFontApply(xide_app* app,const char* path,int size,xide_model_error* error);
float xideEditorLineHeight(xide_app* app);
int xideCodeWidgetSetFont(xide_app* app,xui_widget widget,xui_font font);
#endif
