#ifndef XLANG_IDE_MIGRATION_JOURNAL_H
#define XLANG_IDE_MIGRATION_JOURNAL_H
#include <windows.h>
#include "services/rename_journal.h"
#include "services/project_migration.h"
/* Shared immutable-body store; migration markers live outside moved roots. */
typedef struct xide_rename_journal xide_migration_journal;
xide_migration_journal* xideMigrationJournalPrepare(xide_app* app,const char* globalDirectory,const char* directory,const char* base,const xvalue* checkpoint,const xvalue* layout,const xide_project_migration* plan,xide_model_error* error);
int xideMigrationJournalCheckpoint(xide_migration_journal* journal,xide_app* app,const char* directory,const char* base,const xvalue* checkpoint,const xvalue* layout,xide_model_error* error);
int xideMigrationJournalCancel(xide_migration_journal* journal,xide_model_error* error);
/* Validates everything into detached outputs before changing a live workspace.
   Both the original owner and the migrated owner locate an unfinished record. */
int xideMigrationJournalLoad(const char* globalDirectory,const char* path,int projectOnly,xide_workspace* model,char** projectPath,xide_migration_journal** journal,xvalue** checkpoint,xide_model_error* error);
int xideMigrationJournalStage(xide_migration_journal* journal,const char* directory,const char* base,const xvalue* currentCheckpoint,xvalue** checkpoint,xide_model_error* error);
const xvalue* xideMigrationJournalDock(const xide_migration_journal* journal,const char* generation);
/* Background maintenance only retires proven owned bodies after seven days.
   Pending, unknown, replaced, busy and damaged stores remain untouched. */
int xideMigrationJournalMaintain(const char* globalDirectory,uint64_t nowFileTime,volatile LONG* cancelled,xide_recovery_stats* stats,xide_model_error* error);
#endif
