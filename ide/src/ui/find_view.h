#ifndef XIDE_FIND_VIEW_H
#define XIDE_FIND_VIEW_H
#include "app/xide_app.h"
int xideFindViewOpen(xide_app*,int replace);
void xideFindViewChanged(xide_app*);
void xideFindViewPoll(xide_app*);
xui_widget xideFindViewWindow(xide_app*);
int xideFindViewNavigate(xide_app*,int previous);
#endif
