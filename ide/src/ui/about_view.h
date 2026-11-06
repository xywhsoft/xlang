#ifndef XIDE_ABOUT_VIEW_H
#define XIDE_ABOUT_VIEW_H
#include "app/xide_app.h"
int xideAboutViewOpen(xide_app*);
int xideAboutViewIsOpen(xide_app*);
void xideAboutViewLayout(xide_app*);
void xideAboutViewDestroy(xide_app*);
xui_widget xideAboutViewWindow(xide_app*);
xui_widget xideAboutViewCloseButton(xide_app*);
const char* xideAboutViewLink(int);
#endif
