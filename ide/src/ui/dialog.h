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
/* Logical DIP constraints and native flow-layout building blocks. Content
 * alone scrolls; actions belong to the separate, wrapping bottom bar. */
int xideDialogSetMinSize(xui_widget,float,float);
int xideDialogFormBegin(xui_widget,float,float);
int xideDialogFormField(xui_widget,const char*,xui_widget,xui_widget,xui_widget);
int xideDialogFormNamedField(xui_widget,xui_widget,xui_widget,xui_widget,xui_widget);
int xideDialogFormBlock(xui_widget,xui_widget,float,int);
int xideDialogFormText(xui_widget,const char*);
int xideDialogFormRow(xui_widget,const xui_widget*,size_t,float,int);
int xideDialogFormActions(xui_widget,const xui_widget*,size_t);
/* Optional control at the left of the fixed action bar, vertically centered. */
int xideDialogFormFooterLeading(xui_widget,xui_widget);
int xideDialogFormEnd(xui_widget);
int xideDialogSetFixedBody(xui_widget,int);
xui_widget xideDialogGetFooterWidget(xui_widget);
xui_widget xideDialogGetDividerWidget(xui_widget);
int xideDialogGetBodySize(xui_widget,float*,float*);
int xideDialogStyleMessage(xide_app*,xui_msgbox);
int xideDialogSetFocusRestore(xui_widget,xui_widget);
int xideDialogSetScroll(xui_widget,float,float);
int xideDialogGetScroll(xui_widget,float*,float*);
int xideDialogSetChange(xui_widget,xui_popup_change_proc,void*);
int xideDialogSetCancel(xui_widget,xui_widget_action_proc,void*);
#endif
