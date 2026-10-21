#ifndef XIDE_C_LANGUAGE_VIEW_H
#define XIDE_C_LANGUAGE_VIEW_H
#include "app/xide_app.h"
int xideCLanguageViewCreate(xide_app* app);
void xideCLanguageViewStop(xide_app* app);
void xideCLanguageViewDestroy(xide_app* app);
void xideCLanguageViewPoll(xide_app* app);
int xideCLanguageViewCanCheck(xide_app* app);
int xideCLanguageViewCanQuery(xide_app* app,int query);
int xideCLanguageViewCanCancel(xide_app* app);
int xideCLanguageViewAction(xide_app* app,int action);
int xideCLanguageViewBindEditor(xide_app* app,xui_widget editor);
int xideCLanguageViewHintOpen(xide_app* app);
size_t xideCLanguageViewDiagnosticCount(xide_app* app);
const char* xideCLanguageViewMessage(xide_app* app);
#endif
