#ifndef XLANG_IDE_TASK_VIEW_H
#define XLANG_IDE_TASK_VIEW_H
#include "app/xide_app.h"
int xideTaskViewCreate(xide_app* app);
void xideTaskViewDestroy(xide_app* app);
void xideTaskViewPoll(xide_app* app);
int xideTaskViewSettled(xide_app* app);
int xideTaskViewAction(xide_app* app,int action);
int xideTaskViewCanBuildFile(xide_app* app);
int xideTaskViewTheme(xide_app* app);
xide_task* xideTaskViewSelected(xide_app* app);
int xideTaskViewSelect(xide_app* app,xide_task* task);
int xideTaskViewJump(xide_app* app,size_t diagnostic);
const char* xideTaskViewCompiler(xide_app* app);
int xideTaskViewShowLanguageProblems(xide_app* app,const char* const* titles,size_t count,int activate);
int xideTaskViewShowLanguageProblemsScoped(xide_app* app,const char* scope,const char* const* titles,size_t count,int activate);
void xideTaskViewClearLanguageProblems(xide_app* app);
int xideTaskViewShowLanguageProblemsRouted(xide_app* app,const char* scope,const char* const* titles,size_t count,int activate,void* owner,int (*jump)(xide_app*,size_t,void*));
void xideTaskViewClearLanguageProblemsOwned(xide_app* app,void* owner);
void* xideTaskViewLanguageProblemsOwner(xide_app* app);
#endif
