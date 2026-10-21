#ifndef XIDE_DIALOG_H
#define XIDE_DIALOG_H
#include "xui.h"
typedef struct xide_app xide_app;
typedef struct xide_dialog xide_dialog;
int xideDialogCreate(xide_app*,xui_widget*,const char*,float,float,float);
void xideDialogDestroy(xui_widget);
void xideDialogDestroyAll(xide_app*);
void xideDialogLayoutAll(xide_app*);
int xideDialogSetOpen(xui_widget,int);
int xideDialogIsOpen(xui_widget);
xui_widget xideDialogGetContentWidget(xui_widget);
xui_widget xideDialogGetScrollViewWidget(xui_widget);
int xideDialogSetContentSize(xui_widget,float,float);
int xideDialogGetContentSize(xui_widget,float*,float*);
int xideDialogSetMaxSize(xui_widget,float,float);
int xideDialogSetFocusRestore(xui_widget,xui_widget);
int xideDialogSetScroll(xui_widget,float,float);
int xideDialogGetScroll(xui_widget,float*,float*);
int xideDialogSetChange(xui_widget,xui_popup_change_proc,void*);
int xideDialogSetCancel(xui_widget,xui_widget_action_proc,void*);
#endif
