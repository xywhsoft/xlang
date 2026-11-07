#ifndef XIDE_FIND_VIEW_H
#define XIDE_FIND_VIEW_H
#include "app/xide_app.h"
enum xide_find_control {
 XIDE_FIND_QUERY, XIDE_FIND_REPLACEMENT, XIDE_FIND_CASE, XIDE_FIND_WORD,
 XIDE_FIND_REGEX, XIDE_FIND_ESCAPE, XIDE_FIND_SELECTION, XIDE_FIND_ALL_EDITORS,
 XIDE_FIND_PREVIOUS, XIDE_FIND_NEXT, XIDE_FIND_ALL, XIDE_FIND_REPLACE,
 XIDE_FIND_REPLACE_ALL, XIDE_FIND_CLOSE, XIDE_FIND_RESULTS, XIDE_FIND_STATUS,
 XIDE_FIND_TARGET, XIDE_FIND_COUNT
};
int xideFindViewOpen(xide_app*,int replace);
void xideFindViewClose(xide_app*);
void xideFindViewChanged(xide_app*);
void xideFindViewPoll(xide_app*);
void xideFindViewLayout(xide_app*);
void xideFindViewDestroy(xide_app*);
xui_widget xideFindViewWindow(xide_app*);
xui_widget xideFindViewControl(xide_app*,int);
int xideFindViewRefresh(xide_app*);
int xideFindViewNavigate(xide_app*,int previous);
#endif
