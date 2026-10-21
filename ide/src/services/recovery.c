#define _WIN32_WINNT 0x0601
#include "services/recovery.h"
#include "services/session.h"
#include "services/migration_journal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct xide_recovery_service { char* directory; HANDLE thread; volatile LONG cancel; xide_recovery_result result; };
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int fail(xide_model_error* e,const char* s) { if(e) { e->code=XIDE_MODEL_IO; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static DWORD WINAPI worker(void* data)
{
    xide_recovery_service* s=data; xide_recovery_result* r=&s->result; xide_recovery_guard guard={0};
    FILETIME time; GetSystemTimeAsFileTime(&time); uint64_t now=((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime;
    if(r->job==XIDE_RECOVERY_GLOBAL) r->okay=xideStateRepairPrepare(r->reference,&s->cancel,&r->global,&r->error);
    else if(r->job==XIDE_RECOVERY_MIGRATIONS) r->okay=xideMigrationJournalMaintain(r->reference,now,&s->cancel,&r->migrationStats,&r->error);
    else if(r->job==XIDE_RECOVERY_REBUILD) r->okay=xideRecoveryBackupCreate(s->directory,&s->cancel,&r->backup,&r->error);
    else if(xideRecoveryLock(&guard,s->directory,&r->error)) {
        if(r->job==XIDE_RECOVERY_SCAN) r->okay=xideRecoveryListRead(&guard,&r->list,&s->cancel,&r->error);
        else if(r->job==XIDE_RECOVERY_PREVIEW) r->okay=xideRecoveryRead(&guard,r->reference,&r->preview,&r->error);
        else if(r->job==XIDE_RECOVERY_MAINTAIN) {
            r->okay=xideRecoveryCollect(&guard,now,xideSessionValidate,&r->stats,&r->error);
        }
    }
    xideRecoveryUnlock(&guard);
    if(r->job==XIDE_RECOVERY_MAINTAIN && r->okay && r->reference && !InterlockedCompareExchange(&s->cancel,0,0))
        r->okay=xideMigrationJournalMaintain(r->reference,now,&s->cancel,&r->migrationStats,&r->error);
    if(InterlockedCompareExchange(&s->cancel,0,0)) {
        if(r->backup.retired && !xideRecoveryBackupUndo(&r->backup,&r->error)) r->okay=0;
        else fail(&r->error,"Recovery operation cancelled; original metadata is preserved.");
        r->okay=0;
    }
    return 0;
}
xide_recovery_service* xideRecoveryServiceCreate(const char* directory)
{ xide_recovery_service* s=calloc(1,sizeof(*s)); if(!s) return NULL; s->directory=directory ? copy(directory) : NULL; if(directory && !s->directory) { free(s); return NULL; } return s; }
int xideRecoveryServiceStart(xide_recovery_service* s,int job,const char* ref,xide_model_error* e)
{
    if(!s || s->thread || job<XIDE_RECOVERY_SCAN || job>XIDE_RECOVERY_MIGRATIONS || (job!=XIDE_RECOVERY_GLOBAL && job!=XIDE_RECOVERY_MIGRATIONS && !s->directory) || ((job==XIDE_RECOVERY_PREVIEW || job==XIDE_RECOVERY_GLOBAL || job==XIDE_RECOVERY_MIGRATIONS) && !ref)) return fail(e,"Recovery service is unavailable or already working.");
    xideRecoveryResultClear(&s->result,1); s->result.job=job; s->result.reference=ref ? copy(ref) : NULL;
    if(ref && !s->result.reference) return fail(e,"Cannot retain the preview identity.");
    InterlockedExchange(&s->cancel,0); s->thread=CreateThread(NULL,0,worker,s,0,NULL);
    return s->thread ? 1 : fail(e,"Cannot start the recovery worker.");
}
int xideRecoveryServiceBusy(xide_recovery_service* s) { return s && s->thread; }
void xideRecoveryServiceCancel(xide_recovery_service* s) { if(s && s->thread) { InterlockedExchange(&s->cancel,1); CancelSynchronousIo(s->thread); } }
int xideRecoveryServiceTake(xide_recovery_service* s,xide_recovery_result* out)
{
    if(!s || !s->thread || WaitForSingleObject(s->thread,0)!=WAIT_OBJECT_0) return 0;
    CloseHandle(s->thread); s->thread=NULL; *out=s->result; memset(&s->result,0,sizeof(s->result)); return 1;
}
void xideRecoveryResultClear(xide_recovery_result* r,int undo)
{
    if(!r) return;
    if(undo && r->backup.retired) { xide_model_error e={0}; if(!xideRecoveryBackupUndo(&r->backup,&e)) fprintf(stderr,"recovery undo: %s\n",e.message); }
    free(r->reference); xideRecoveryListClear(&r->list); xideTextFileClear(&r->preview); xideRecoveryBackupClear(&r->backup); xideStateRepairClear(&r->global); memset(r,0,sizeof(*r));
}
void xideRecoveryServiceDestroy(xide_recovery_service* s)
{ if(!s) return; if(s->thread) { xideRecoveryServiceCancel(s); WaitForSingleObject(s->thread,INFINITE); CloseHandle(s->thread); } xideRecoveryResultClear(&s->result,1); free(s->directory); free(s); }
