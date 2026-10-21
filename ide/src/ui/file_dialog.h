#ifndef XIDE_FILE_DIALOG_H
#define XIDE_FILE_DIALOG_H
#include "xui.h"
typedef struct xide_app xide_app;
typedef struct xide_file_dialog xide_file_dialog;
typedef void (*xide_path_selected_proc)(xide_app*,const char*,void*);
/* UTF-8 paths. Callback runs after dispatch and destruction; NULL is cancel. */
int xideFileDialogChoose(xide_app*,xui_widget,int,const char*,const char*,const char*,const char*,xide_path_selected_proc,void*);
int xideFileDialogBusy(xide_app*);
void xideFileDialogPoll(xide_app*);
void xideFileDialogLayout(xide_app*);
void xideFileDialogDestroy(xide_app*);
xui_file_dialog xideFileDialogCurrent(xide_app*);
#endif
