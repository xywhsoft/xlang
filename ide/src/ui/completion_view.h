#ifndef XLANG_IDE_COMPLETION_VIEW_H
#define XLANG_IDE_COMPLETION_VIEW_H
#include "app/xide_app.h"
int xideCompletionViewCreate(xide_app* app);
void xideCompletionViewConfigurationChanged(xide_app* app);
void xideCompletionViewStop(xide_app* app);
void xideCompletionViewDestroy(xide_app* app);
void xideCompletionViewPoll(xide_app* app);
int xideCompletionViewActive(xide_app* app);
void xideCompletionViewRequest(xide_app* app,xui_widget editor,int offset,int force);
int xideCompletionViewCollect(xide_app* app,xui_widget editor,int offset,xui_code_completion_item_t* items,int capacity,int* count);
int xideCompletionViewHasName(xide_app* app,xui_widget editor,const char* name);
int xideCompletionViewInput(xide_app* app,xui_widget editor,const xui_code_input_event_t* event,xui_code_input_action_t* action);
int xideCompletionViewBindEditor(xide_app* app,xui_widget editor);
/* Shared native connection/document sync; independent signature request and
   nonmodal XUI window. Force is the explicit parameter-info action. */
void xideSignatureViewRequest(xide_app*,xui_widget,int offset,int force);
int xideSignatureViewIsOpen(xide_app*);
int xideSignatureViewAction(xide_app*,int);
#endif
