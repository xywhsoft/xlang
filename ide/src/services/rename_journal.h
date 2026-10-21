#ifndef XLANG_IDE_RENAME_JOURNAL_H
#define XLANG_IDE_RENAME_JOURNAL_H
#include "app/xide_app.h"
#include "services/resource_rebind.h"
#include "platform/recovery_store.h"
typedef struct xide_rename_journal xide_rename_journal;
/* All publication is under the scope checkpoint lock. Payload bodies are
   immutable, separately hashed files; the <=8 MiB marker is published last. */
xide_rename_journal* xideRenameJournalPrepare(xide_app* app,const char* directory,const char* base,const xvalue* checkpoint,const char* source,const char* target,const xide_resource_rebind* resources,xide_model_error* error);
int xideRenameJournalFinish(xide_rename_journal* journal,xide_app* app,const xide_resource_rebind* resources,xide_model_error* error);
int xideRenameJournalCancelPrepared(xide_rename_journal* journal,xide_model_error* error);
int xideRenameJournalCheckpoint(xide_rename_journal* journal,xide_app* app,const xvalue* checkpoint,xide_recovery_guard* guard,xide_model_error* error);
/* Only an identity-proven move with recognized manifest/resource baselines
   changes the live model. User files are never written by recovery. */
int xideRenameJournalRecover(xide_app* app,const char* directory,const char* base,const xvalue* currentCheckpoint,xide_rename_journal** journal,xvalue** checkpoint,xide_model_error* error);
int xideRenameJournalApplyRecovered(xide_rename_journal* journal,xide_app* app,xide_model_error* error);
int xideRenameJournalPending(const xide_rename_journal* journal);
void xideRenameJournalDestroy(xide_rename_journal* journal);
#endif
