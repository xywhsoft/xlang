#ifndef XLANG_IDE_PROJECT_MIGRATION_H
#define XLANG_IDE_PROJECT_MIGRATION_H
#include "model/workspace.h"
typedef struct xide_project_migration xide_project_migration;
enum { XIDE_MIGRATE_MANIFEST, XIDE_MIGRATE_DIRECTORY };
/* Prepare writes nothing; target is an absolute destination file/directory.
   Caller prepares document/session bindings before Commit and refreshes UI after.
   Full model and structural baselines reject invalidated borrowed patch slots. */
xide_project_migration* xideProjectMigrationPrepare(xide_workspace* workspace,const char* projectId,const char* target,int mode,xide_model_error* error);
void xideProjectMigrationDestroy(xide_project_migration* plan);
/* Retarget an open-dialog baseline without accepting a replaced source/model. */
xide_project_migration* xideProjectMigrationRetarget(const xide_project_migration* baseline,const char* target,xide_model_error* error);
const char* xideProjectMigrationSource(const xide_project_migration* plan);
const char* xideProjectMigrationTarget(const xide_project_migration* plan);
int xideProjectMigrationMode(const xide_project_migration* plan);
/* Returned malloc string includes only explicit model path semantics. */
char* xideProjectMigrationMapPath(const xide_project_migration* plan,const char* path);
size_t xideProjectMigrationPathCount(const xide_project_migration* plan);
const char* xideProjectMigrationPathBefore(const xide_project_migration* plan,size_t index);
const char* xideProjectMigrationPathAfter(const xide_project_migration* plan,size_t index);
size_t xideProjectMigrationProjectCount(const xide_project_migration* plan);
xide_project* xideProjectMigrationProjectAt(const xide_project_migration* plan,size_t index);
int xideProjectMigrationWorkspaceMoves(const xide_project_migration* plan);
/* Apply only to a separately decoded preview model. No disk operation; the
   migration journal uses this to persist the exact model before moving. */
int xideProjectMigrationApplyPreview(xide_project_migration* plan,xide_model_error* error);
/* Same-volume no-replace move, then preallocated model patch application.
   Does not save dirty manifests or rewrite source import/include strings. */
int xideProjectMigrationCommit(xide_project_migration* plan,xide_model_error* error);
#endif
