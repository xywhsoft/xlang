#ifndef XLANG_IDE_SESSION_H
#define XLANG_IDE_SESSION_H
#include "app/xide_app.h"
#include "platform/recovery_store.h"
#include "platform/state_repair.h"
#include "services/resource_rebind.h"
#include "services/project_migration.h"
typedef struct xide_session xide_session;
int xideSessionValidate(const xvalue* root);
int xideSessionRebuild(xide_app* app,xide_recovery_guard* guard,xide_model_error* error);
enum { XIDE_GLOBAL_INDEX,XIDE_GLOBAL_WINDOW,XIDE_GLOBAL_SETTINGS,XIDE_GLOBAL_COUNT };
char* xideSessionGlobalPath(xide_app* app,int kind);
int xideSessionGlobalPublish(xide_app* app,int kind,xide_state_repair* repair,xide_model_error* error);
typedef struct xide_legacy_session_result { size_t opened,unavailable; int applied,persisted; } xide_legacy_session_result;
/* Applies only to the matching imported workspace and a fresh native scope.
   Existing session state is protected. A save failure leaves applied live state. */
int xideSessionApplyLegacyHints(xide_app* app,const xvalue* hints,xide_legacy_session_result* result,xide_model_error* error);
int xideSessionStart(xide_app* app);
int xideSessionSave(xide_app* app,int cleanExit,xide_model_error* error);
/* Startup restoration may intentionally make Save a no-op. Fatal-error UI must
   not describe that no-op as a newly saved checkpoint. */
int xideSessionCanAttemptCheckpoint(xide_app* app);
int xideSessionPrepareRename(xide_app* app,const char* source,const char* target,const xide_resource_rebind* resources,xide_model_error* error);
int xideSessionFinishRename(xide_app* app,const xide_resource_rebind* resources,xide_model_error* error);
void xideSessionCancelPreparedRename(xide_app* app);
int xideSessionBeforeProjectMigration(xide_app* app,xide_model_error* error);
int xideSessionPrepareProjectMigration(xide_app* app,const xide_project_migration* plan,xide_model_error* error);
void xideSessionCancelProjectMigration(xide_app* app);
int xideSessionLoadMigration(xide_app* app,const char* path,int projectOnly,xide_workspace* model,char** projectPath,int* found,xide_model_error* error);
void xideSessionDiscardMigration(xide_app* app);
int xideSessionMaintain(xide_app* app,xide_recovery_stats* stats,xide_model_error* error);
/* Capture a background maintenance root without touching any recovery files. */
int xideSessionMaintenanceRoot(xide_app* app,char** globalDirectory,xide_model_error* error);
void xideSessionPoll(xide_app* app);
void xideSessionAfterLayout(xide_app* app);
int xideSessionBeforeWorkspace(xide_app* app,xide_model_error* error);
void xideSessionAfterWorkspace(xide_app* app);
void xideSessionRelocate(xide_app* app);
void xideSessionDestroy(xide_app* app);
const char* xideSessionDirectory(xide_app* app);
const char* xideSessionIssue(xide_app* app);
#endif
