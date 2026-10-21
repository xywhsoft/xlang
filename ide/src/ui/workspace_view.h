#ifndef XLANG_IDE_WORKSPACE_VIEW_H
#define XLANG_IDE_WORKSPACE_VIEW_H
#include "app/xide_app.h"
int xideWorkspaceViewCreate(xide_app* app);
char* xideWorkspaceTreeKey(xide_app* app,int node);
int xideWorkspaceTreeSelect(xide_app* app,int node);
void xideWorkspaceViewDestroy(xide_app* app);
xvalue* xideWorkspaceViewDraftSnapshot(xide_app* app);
int xideWorkspaceViewHasDraft(xide_app* app);
const xide_file* xideWorkspaceViewSelectedFile(xide_app* app);
int xideWorkspaceViewCanRenameFile(xide_app* app);
int xideWorkspaceViewCanRemoveFile(xide_app* app);
int xideWorkspaceViewRefresh(xide_app* app);
void xideWorkspaceViewLayout(xide_app* app);
int xideWorkspaceViewSelectProject(xide_app* app,const char* id);
int xideWorkspaceViewSelectFile(xide_app* app,const char* project,const char* file);
int xideWorkspaceOpenFile(xide_app* app,const char* path);
int xideWorkspaceViewApplyProperties(xide_app* app,const char* name,const char* description,const char* kind,const char* entry,xide_model_error* error);
int xideWorkspaceOpen(xide_app* app,const char* path,int projectOnly,xide_model_error* error);
int xideWorkspaceSaveAll(xide_app* app,const char* path,xide_model_error* error);
int xideWorkspaceAction(xide_app* app,int action);
int xideWorkspaceQuitRequest(void* user);
int xideWorkspaceViewApplyConfigurations(xide_app* app,const char* projectId,const xvalue* configs,const char* active,xide_model_error* error);
int xideWorkspaceViewApplyReferences(xide_app* app,const char* projectId,const xvalue* references,xide_model_error* error);
int xideWorkspaceViewSelectConfiguration(xide_app* app,const char* id,xide_model_error* error);
#endif