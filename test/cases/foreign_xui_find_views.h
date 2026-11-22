#ifndef FOREIGN_XUI_FIND_VIEWS_H
#define FOREIGN_XUI_FIND_VIEWS_H

#include "xge3_runtime.h"

void findViewsHeapBegin(void);
void findViewsHeapEnd(int expected);
void findViewsNativeScopeHot(xui_code_find_scope scope,xui_context context,xui_widget editor);
int findViewsSetRawScope(xui_widget editor,int attached);
void findViewsNativeHot(xui_code_find_scope scope, xui_context context,
    xui_widget editor, xui_widget window);

#endif
