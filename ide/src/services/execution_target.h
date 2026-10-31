#ifndef XLANG_IDE_EXECUTION_TARGET_H
#define XLANG_IDE_EXECUTION_TARGET_H
#include "app/actions.h"
#include "model/workspace.h"
#include "editor/documents.h"
enum xide_target_mode { XIDE_TARGET_AUTO, XIDE_TARGET_STARTUP, XIDE_TARGET_PROJECT, XIDE_TARGET_FILE };
typedef struct xide_execution_context {
    xide_project* project;
    xide_configuration* configuration;
    xide_document* document;
    const char *projectId,*configurationId,*reason;
    int file,commandLineConfiguration;
    char target[512],config[256];
} xide_execution_context;
/* Borrowed pointers are valid until the next document/model mutation. Task
   submission must copy identities before any save or modal continuation. */
void xideExecutionResolve(xide_app* app,int forceFile,xide_execution_context* out);
xide_action_state xideExecutionQuery(xide_app* app,int action);
int xideExecutionMode(xide_app* app);
const char* xideExecutionProject(xide_app* app);
const char* xideExecutionConfiguration(xide_app* app,const xide_project* project);
xvalue* xideExecutionAnalysisPacket(xide_app* app,const char* package,int64_t epoch,xide_model_error* error);
int xideExecutionSelect(xide_app* app,int mode,const char* project);
int xideExecutionSelectConfiguration(xide_app* app,const char* project,const char* configuration);
int xideExecutionValid(const xvalue* state);
xvalue* xideExecutionCapture(xide_app* app);
int xideExecutionRestore(xide_app* app,const xvalue* state);
void xideExecutionReset(xide_app* app);
#endif
