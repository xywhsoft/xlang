#ifndef XLANG_IDE_C_COMPLETION_VIEW_H
#define XLANG_IDE_C_COMPLETION_VIEW_H
#include "app/xide_app.h"
int xideCCompletionViewCreate(xide_app* app);
void xideCCompletionViewStop(xide_app* app);
void xideCCompletionViewDestroy(xide_app* app);
void xideCCompletionViewPoll(xide_app* app);
void xideCCompletionViewRequest(xide_app* app,xui_widget editor,int offset,int force);
int xideCCompletionViewCollect(xide_app* app,xui_widget editor,int offset,xui_code_completion_item_t* items,int capacity,int* count);
int xideCCompletionViewHasName(xide_app* app,xui_widget editor,const char* name);
int xideCCompletionViewInput(xide_app* app,xui_widget editor,const xui_code_input_event_t* event,xui_code_input_action_t* action);
int xideCCompletionViewBindEditor(xide_app* app,xui_widget editor);
int xideCCompletionViewAction(xide_app* app,int action);
int xideCCompletionViewActive(xide_app* app);
const char* xideCCompletionViewServer(xide_app* app);
#endif
