#ifndef XLANG_IDE_LANGUAGE_VIEW_H
#define XLANG_IDE_LANGUAGE_VIEW_H
#include "app/xide_app.h"
int xideLanguageViewCreate(xide_app* app);
/* A model/selection change revokes results before another frame can use them. */
void xideLanguageViewConfigurationChanged(xide_app* app);
void xideLanguageViewStop(xide_app* app);
void xideLanguageViewDestroy(xide_app* app);
void xideLanguageViewPoll(xide_app* app);
int xideLanguageViewCanCheck(xide_app* app);
int xideLanguageViewCanCheckCurrent(xide_app* app);
int xideLanguageViewCanDefine(xide_app* app);
int xideLanguageViewCanHover(xide_app* app);
int xideLanguageViewCanFindReferences(xide_app* app);
size_t xideLanguageViewReferenceCount(xide_app* app);
int xideLanguageViewReferenceJump(xide_app* app,size_t index);
int xideLanguageViewBindEditor(xide_app* app,xui_widget editor);
int xideLanguageViewCanCancel(xide_app* app);
int xideLanguageViewBusy(xide_app* app);
int xideLanguageViewAction(xide_app* app,int action);
void xideLanguageViewSaved(xide_app* app,xide_document* document,int saved);
int xideLanguageViewJump(xide_app* app,size_t index);
size_t xideLanguageViewDiagnosticCount(xide_app* app);
const char* xideLanguageViewMessage(xide_app* app);
#endif
