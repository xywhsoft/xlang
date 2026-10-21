#ifndef XLANG_IDE_EDITOR_VIEW_H
#define XLANG_IDE_EDITOR_VIEW_H
#include "app/xide_app.h"
int xideEditorPreparePathChange(xide_app* app,const char* source,const char* target,xide_model_error* error);
void xideEditorFinishPathChange(xide_app* app,const char* target,int committed);
int xideEditorViewInit(xide_app* app);
int xideEditorOpen(xide_app* app,const char* path,xide_model_error* error);
int xideEditorShow(xide_app* app,xide_document* document);
int xideEditorClose(xide_app* app,xide_document* document,int discardChanges,xide_model_error* error);
int xideEditorConfirmAll(xide_app* app);
int xideEditorSaveForProject(xide_app* app,const xide_project* project);
int xideEditorSaveForTask(xide_app* app,xide_document* document);
int xideEditorAction(xide_app* app,int action);
int xideEditorReset(xide_app* app);
int xideEditorApplyTheme(xide_app* app);
void xideEditorPoll(xide_app* app);
#endif
