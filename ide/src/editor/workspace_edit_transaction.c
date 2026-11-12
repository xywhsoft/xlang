#include "editor/workspace_edit_transaction.h"
#include "platform/workspace_edit_spool_internal.h"
#include "platform/workspace_edit_files_internal.h"
#include "platform/path.h"
#include "platform/workspace_edit_aliases.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define GATE_RESERVE (16u*1024u*1024u)
#define ALIAS_RESERVE (16u*1024u*1024u)
#define JOURNAL_RESERVE (1024u*1024u)
struct xide_edit_transaction {
 xide_workspace_edit *full,*selected;xide_open_edit_work* editors;
 xide_edit_aliases* aliases;xide_edit_alias_model* models;size_t modelCount;int modelsCaptured;
 xide_edit_alias_exclusion* excluded;size_t exclusions;
 xide_edit_spool* spool;xide_edit_journal* journal;xide_edit_file_write* files[512];
 size_t openRows[512],closedRows[512];wchar_t* keys[512];char* directory;
 size_t limit,metadata;int attached,busy,readers;xatomic32 cancel;
 xide_edit_transaction_status status;
};
struct xide_edit_transaction_ticket {uint64_t thread;xide_document_store* store;xui_context context;xui_lifetime_link_t life;xide_open_edit_ticket* editors;xide_edit_transaction* work;int busy,attempted;int(*guard)(void*);void* user;};
static int fail(xide_model_error* e,int code,const char* s){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",s);}return 0;}
static int keyOrder(const void* a,const void* b){return xidePathNativeKeyCompare(*(wchar_t*const*)a,*(wchar_t*const*)b);}
static int charge(xide_edit_transaction* w,size_t n,xide_model_error* e){if(n>w->limit-w->status.knownBytes)return fail(e,XIDE_MODEL_MEMORY,"Mixed edit known-storage admission exceeded.");w->status.knownBytes+=n;return 1;}
void xideEditTransactionCancel(xide_edit_transaction* w){if(w)xrtAtomic32Store(&w->cancel,1,XMEMORY_RELEASE);}
static int stopped(xide_edit_transaction* w,int(*fn)(void*),void* user){return xrtAtomic32Load(&w->cancel,XMEMORY_ACQUIRE)||(fn&&fn(user));}
typedef struct cancellation {xide_edit_transaction* work;int(*callback)(void*);void* user;} cancellation;
static int cancelled(void* user){cancellation* c=user;return stopped(c->work,c->callback,c->user);}
static void gone(void* user){xide_edit_transaction_ticket* t=user;t->context=NULL;xideEditTransactionCancel(t->work);}
int xideEditTransactionStatus(const xide_edit_transaction* w,xide_edit_transaction_status* s){if(s)memset(s,0,sizeof(*s));if(!w||!s||w->busy)return 0;*s=w->status;return 1;}
static int unlock(xide_edit_transaction* w,xide_model_error* e){if(!w->readers)return 1;if(!xideWorkspaceEditFilesUnlockReaders(xideEditSpoolSources(w->spool),e))return 0;w->readers=0;return 1;}
static int settleReaders(xide_edit_transaction* w,xide_model_error* e)
{if(unlock(w,e))return 1;w->status.phase=XIDE_MIX_IN_DOUBT;w->status.needsRecovery=1;return 0;}
int xideEditTransactionDestroy(xide_edit_transaction* w)
{
 if(!w)return XUI_OK;
 if(w->attached||w->busy||w->status.phase==XIDE_MIX_READY||w->status.phase==XIDE_MIX_UI_COMMITTED||w->status.phase==XIDE_MIX_UI_REJECTED)return XUI_ERROR_BUSY;
 (void)unlock(w,NULL);xideEditAliasesDestroy(w->aliases);for(size_t i=0;i<w->modelCount;++i)xrtFree((void*)w->models[i].path);xrtFree(w->models);for(size_t i=1;i<w->exclusions;++i)xrtFree((void*)w->excluded[i].path);xrtFree(w->excluded);xideEditJournalDestroy(w->journal);for(size_t i=0;i<w->status.closed;++i)xideEditFileWriteDestroy(w->files[i]);xideEditSpoolDestroy(w->spool);xideOpenEditWorkDestroy(w->editors);xideWorkspaceEditRelease(w->full);xideWorkspaceEditRelease(w->selected);for(size_t i=0;i<w->status.closed;++i)free(w->keys[i]);free(w->directory);xrtFree(w);return XUI_OK;
}
xide_edit_transaction* xideEditTransactionPrepare(xide_workspace_edit* plan,const char* directory,size_t budget,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 xide_workspace_edit_info info;size_t limit=budget?budget:256u*1024u*1024u;if(!xideWorkspaceEditInfo(plan,&info)||!directory||!*directory||strlen(directory)>32768||info.documents>512||limit>512u*1024u*1024u){fail(e,XIDE_MODEL_INVALID,"Invalid mixed-edit plan, scope or aggregate budget.");return NULL;}
 xide_edit_transaction* w=xrtCalloc(1,sizeof(*w));if(!w){fail(e,XIDE_MODEL_MEMORY,"Cannot allocate mixed-edit work.");return NULL;}w->limit=limit;w->status.phase=XIDE_MIX_PREPARED;xrtAtomic32Init(&w->cancel,0);int okay=charge(w,sizeof(*w),e);if(okay){w->full=xideWorkspaceEditRetain(plan);w->directory=xidePathResolveLexical(NULL,directory);okay=w->full&&w->directory&&charge(w,strlen(w->directory)+1,e);}
 if(okay){w->excluded=xrtCalloc(256,sizeof(*w->excluded));okay=w->excluded&&charge(w,256*sizeof(*w->excluded),e);if(okay){w->excluded[0]=(xide_edit_alias_exclusion){w->directory,1};w->exclusions=1;}}
 for(size_t i=0;okay&&i<info.documents;++i){if(stopped(w,cancel,user)){okay=fail(e,XIDE_MODEL_CONFLICT,"Mixed-edit preparation cancelled.");break;}xide_workspace_edit_document d;okay=xideWorkspaceEditDocumentAt(plan,i,&d);if(!okay)break;if(d.hasVersion)w->openRows[w->status.open++]=i;else{size_t at=w->status.closed++;w->closedRows[at]=i;w->keys[at]=xidePathLexicalWide(d.path);okay=w->keys[at]&&charge(w,(wcslen(w->keys[at])+1)*sizeof(wchar_t),e);}}
 if(okay){qsort(w->keys,w->status.closed,sizeof(*w->keys),keyOrder);enum xide_workspace_edit_error error;w->selected=xideWorkspaceEditSelect(plan,w->openRows,w->status.open,limit>256u*1024u*1024u?256u*1024u*1024u:limit,cancel,user,&error);okay=w->selected!=NULL;if(!okay)fail(e,error==XIDE_WORKSPACE_EDIT_CANCELLED?XIDE_MODEL_CONFLICT:XIDE_MODEL_MEMORY,"Cannot project the open edit targets.");}
 if(okay){w->metadata=w->status.knownBytes;xide_workspace_edit_info selected;xideWorkspaceEditInfo(w->selected,&selected);okay=charge(w,info.bytes,e)&&charge(w,selected.bytes,e);}
 if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot prepare mixed-edit metadata.");xideEditTransactionDestroy(w);return NULL;}return w;
}
int xideEditTransactionProtect(xide_edit_transaction* w,const xide_edit_alias_exclusion* paths,size_t count,xide_model_error* e)
{
 if(e){*e=(xide_model_error){0};}
 if(!w||w->busy||w->attached||w->status.phase!=XIDE_MIX_PREPARED||count>256-w->exclusions||(count&&!paths))return fail(e,XIDE_MODEL_INVALID,"Protected paths require an unattached prepared transaction.");
 char* copies[256]={0};size_t bytes=0;int okay=1;
 for(size_t i=0;okay&&i<count;++i){const char* path=paths[i].path;size_t n=path?strlen(path):0;if(!n||n>65535||(paths[i].directory!=0&&paths[i].directory!=1)||n+1>1024u*1024u-bytes){okay=0;break;}copies[i]=xrtMalloc(n+1);if(!copies[i]){okay=0;break;}memcpy(copies[i],path,n+1);bytes+=n+1;}
 if(okay)okay=charge(w,bytes,e);
 if(okay){w->metadata+=bytes;for(size_t i=0;i<count;++i)w->excluded[w->exclusions++]=(xide_edit_alias_exclusion){copies[i],paths[i].directory};}
 else{for(size_t i=0;i<count;++i)xrtFree(copies[i]);if(e&&!e->code)fail(e,XIDE_MODEL_MEMORY,"Cannot retain complete protected path set.");}return okay;
}
static void modelsClear(xide_edit_transaction* w)
{for(size_t i=0;i<w->modelCount;++i)xrtFree((void*)w->models[i].path);xrtFree(w->models);w->models=NULL;w->modelCount=0;w->modelsCaptured=0;}
static int modelsCapture(xide_edit_transaction* w,const xide_document_store* store)
{
 if(store->count>4096)return 0;
 size_t count=0,bytes=0;for(size_t i=0;i<store->count;++i){const xide_document* d=store->items[i];if(d->open&&d->path&&*d->path)++count;}
 if(count){w->models=xrtCalloc(count,sizeof(*w->models));if(!w->models)return 0;}bytes=count*sizeof(*w->models);
 for(size_t i=0;i<store->count;++i){const xide_document* d=store->items[i];if(!d->open||!d->path||!*d->path)continue;size_t n=strlen(d->path);if(!d->id||n>65535||n+1>1024u*1024u-bytes)return 0;char* path=xrtMalloc(n+1);if(!path)return 0;memcpy(path,d->path,n+1);w->models[w->modelCount++]=(xide_edit_alias_model){path,d->id};bytes+=n+1;}
 if(!charge(w,bytes,NULL))return 0;
 w->metadata+=bytes;w->modelsCaptured=1;return 1;
}
static int modelsCurrent(const xide_edit_transaction* w,const xide_document_store* store)
{
 if(!w->modelsCaptured)return 1;
 size_t at=0;for(size_t i=0;i<store->count;++i){const xide_document* d=store->items[i];if(!d->open||!d->path||!*d->path)continue;if(at>=w->modelCount||d->id!=w->models[at].identity||strcmp(d->path,w->models[at].path))return 0;++at;}return at==w->modelCount;
}
static int local(xide_edit_transaction_ticket* t)
{
 xide_edit_transaction* w=t->work;if(!t->context||t->context!=t->store->context||t->store->count>4096||stopped(w,NULL,NULL)||!modelsCurrent(w,t->store))return 0;
 if(!w->status.closed)return 1;
 for(size_t i=0;i<t->store->count;++i){xide_document* d=t->store->items[i];if(!d->open||!d->path)continue;wchar_t* key=xidePathLexicalWide(d->path);if(!key)return 0;size_t low=0,high=w->status.closed;while(low<high){size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(w->keys[mid],key)<0)low=mid+1;else high=mid;}int found=low<w->status.closed&&!xidePathNativeKeyCompare(w->keys[low],key);free(key);if(found)return 0;}
 return 1;
}
static int uiGuard(void* user){xide_edit_transaction_ticket* t=user;if(!local(t))return XUI_ERROR_INVALID_STATE;int status=t->guard(t->user);return status==XUI_OK&&!local(t)?XUI_ERROR_INVALID_STATE:status;}
int xideEditTransactionCapture(xide_document_store* store,xide_edit_transaction* w,xide_edit_transaction_ticket** out)
{
 if(out)*out=NULL;
 if(!out||!store||!store->context||!w||w->busy||w->attached||w->status.phase!=XIDE_MIX_PREPARED)return XUI_ERROR_INVALID_ARGUMENT;
 xide_edit_transaction_ticket* t=xrtCalloc(1,sizeof(*t));if(!t)return XUI_ERROR_OUT_OF_MEMORY;t->thread=xrtThreadCurrentId();t->store=store;t->context=store->context;t->work=w;int status=XUI_ERROR_INVALID_STATE;size_t before=w->status.knownBytes,metadataBefore=w->metadata;
 /* Partition first: the SDK exposes an admission quota, not exact payload
  * bytes. Keep half the remaining quota for disk/recovery staging, and count
  * the entire editor partition once Stage attempts to consume it. */
 if(local(t)&&modelsCapture(w,store)){status=xuiContextLifetimeWatch(t->context,&t->life,gone,t);if(status==XUI_OK&&w->status.open){size_t available=(w->limit-w->status.knownBytes)/2;status=available?xideOpenEditCapture(store,w->selected,available,&t->editors,&w->editors):XUI_ERROR_OUT_OF_MEMORY;}}
 if(status==XUI_OK){size_t bytes=xideOpenEditReservation(w->editors);if(bytes>w->limit-w->status.knownBytes)status=XUI_ERROR_OUT_OF_MEMORY;else{w->status.knownBytes+=bytes;w->status.phase=XIDE_MIX_CAPTURED;w->attached=1;*out=t;return XUI_OK;}}
 modelsClear(w);w->status.knownBytes=before;w->metadata=metadataBefore;(void)xideOpenEditTicketDestroy(t->editors);xuiLifetimeUnwatch(&t->life);xideOpenEditWorkDestroy(w->editors);w->editors=NULL;xrtFree(t);return status;
}
int xideEditTransactionStage(xide_edit_transaction* w,int(*cancel)(void*),void* user,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!w||w->busy||w->status.phase!=XIDE_MIX_CAPTURED)return fail(e,XIDE_MODEL_INVALID,"Mixed-edit capture is not ready for staging.");
 w->busy=1;cancellation c={w,cancel,user};int okay=!cancelled(&c);if(okay&&w->status.open)okay=xideOpenEditStage(w->editors,cancelled,&c)==XUI_OK;
 xide_workspace_edit_info selected;xideWorkspaceEditInfo(w->selected,&selected);size_t outside=w->metadata+selected.bytes+xideOpenEditReservation(w->editors)+GATE_RESERVE+ALIAS_RESERVE+JOURNAL_RESERVE;
 if(okay&&outside>=w->limit)okay=fail(e,XIDE_MODEL_MEMORY,"Insufficient aggregate room for recovery staging.");
 xide_edit_spool_baseline views[512];for(size_t i=0;okay&&i<w->status.open;++i){views[i]=(xide_edit_spool_baseline){w->openRows[i],xideOpenEditBaselineAt(w->editors,i)};if(!views[i].file)okay=0;}
 if(okay){size_t available=w->limit-outside;w->spool=xideEditSpoolPrepare(w->full,views,w->status.open,w->directory,NULL,available>256u*1024u*1024u?256u*1024u*1024u:available,cancelled,&c,e);okay=w->spool!=NULL;}
 if(okay){w->status.knownBytes=outside;if(!charge(w,xideEditSpoolReservation(w->spool),e))okay=0;if(okay){w->aliases=xideEditAliasesPin(w->full,w->models,w->modelCount,w->excluded,w->exclusions,cancelled,&c,e);okay=w->aliases!=NULL;}xideWorkspaceEditRelease(w->full);w->full=NULL;}
 for(size_t i=0;okay&&i<w->status.closed;++i){if(stopped(w,cancel,user)){okay=0;break;}size_t available=w->limit-w->status.knownBytes;w->files[i]=available?xideEditFileWritePrepare(w->spool,w->closedRows[i],available>256u*1024u*1024u?256u*1024u*1024u:available,e):NULL;okay=w->files[i]&&charge(w,xideEditFileWriteStorage(w->files[i]),e);}
 if(okay){w->readers=1;okay=xideWorkspaceEditFilesLockReaders(xideEditSpoolSources(w->spool),e);}
 if(okay){w->journal=xideEditJournalCreate(w->spool,e);okay=w->journal!=NULL;}
 w->status.phase=okay?XIDE_MIX_STAGED:XIDE_MIX_FAILED;w->busy=0;if(!okay&&e&&!e->code)fail(e,XIDE_MODEL_CONFLICT,"Mixed-edit staging failed or was cancelled; source files were not written.");return okay;
}
int xideEditTransactionArm(xide_edit_transaction_ticket* t,int(*guard)(void*),void* user)
{if(!t||!guard||t->thread!=xrtThreadCurrentId()||t->busy||t->work->busy||t->work->status.phase!=XIDE_MIX_STAGED)return XUI_ERROR_INVALID_ARGUMENT;t->busy=1;t->guard=guard;t->user=user;int status=uiGuard(t);if(status==XUI_OK&&t->editors)status=xideOpenEditValidate(t->editors,t->work->editors);t->guard=NULL;t->user=NULL;if(status==XUI_OK)t->work->status.phase=XIDE_MIX_ARMED;t->busy=0;return status;}
typedef struct file_fence {xide_edit_transaction* work;size_t document;int(*guard)(void*,size_t,int);void* user;xide_model_error error;} file_fence;
static int fileGuard(void* user,int action){file_fence* f=user;if(action==XIDE_EDIT_FILE_APPLY&&stopped(f->work,NULL,NULL))return 0;if(!f->guard(f->user,f->document,action))return 0;if(action==XIDE_EDIT_FILE_APPLY&&stopped(f->work,NULL,NULL))return 0;return xideEditJournalFileIntent(f->work->journal,f->document,action,&f->error);}
static int rollback(xide_edit_transaction* w,int noGui,int(*guard)(void*,size_t,int),void* user,xide_model_error* e)
{
 xide_edit_journal_status j;if(!xideEditJournalStatus(w->journal,&j)||j.frozen||!xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_ROLLING_BACK,noGui,e))goto doubtful;
 int okay=1;for(size_t i=w->status.closed;i;--i){file_fence f={w,w->closedRows[i-1],guard,user,{0}};xide_model_error error={0};int restored=xideEditFileWriteRestore(w->files[i-1],fileGuard,&f,&error);if(restored)++w->status.restored;else okay=0;if(!xideEditJournalRecordFile(w->journal,w->files[i-1],&error))okay=0;if(!okay&&e&&!e->code)*e=error;if(!xideEditJournalStatus(w->journal,&j)||j.frozen)goto doubtful;}
 if(okay&&xideWorkspaceEditFilesValidate(xideEditSpoolSources(w->spool),NULL,NULL,e)&&xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_ROLLED_BACK,0,e)){w->status.phase=XIDE_MIX_ROLLED_BACK;w->status.rolledBack=1;return settleReaders(w,e);}
 (void)xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_IN_DOUBT,0,NULL);
doubtful:
 w->status.phase=XIDE_MIX_IN_DOUBT;w->status.needsRecovery=1;if(e&&!e->code)fail(e,XIDE_MODEL_IO,"Mixed-edit rollback is uncertain; preserve recovery data and coordinate readers.");return 0;
}
int xideEditTransactionApply(xide_edit_transaction* w,int(*guard)(void*,size_t,int),void* user,int(*cancel)(void*),void* cancelUser,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!w||!guard||w->busy||w->status.phase!=XIDE_MIX_ARMED)return fail(e,XIDE_MODEL_INVALID,"Mixed-edit apply requires owner arming and detached policy.");
 w->busy=1;cancellation c={w,cancel,cancelUser};int okay=!cancelled(&c)&&xideEditAliasesValidate(w->aliases,cancelled,&c,e)&&guard(user,SIZE_MAX,XIDE_EDIT_FILE_APPLY)&&!cancelled(&c)&&xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_APPLYING,0,e);
 for(size_t i=0;okay&&i<w->status.closed;++i){file_fence f={w,w->closedRows[i],guard,user,{0}};okay=!cancelled(&c)&&xideEditFileWriteApply(w->files[i],fileGuard,&f,cancelled,&c,e);int recorded=xideEditJournalRecordFile(w->journal,w->files[i],&f.error);if(!recorded&&e&&!e->code)*e=f.error;if(okay)++w->status.applied;okay=okay&&recorded;}
 for(size_t i=0;okay&&i<w->status.closed;++i)okay=xideEditFileWriteVerify(w->files[i],e);
 if(okay)okay=!stopped(w,cancel,cancelUser)&&xideEditAliasesValidate(w->aliases,cancelled,&c,e)&&guard(user,SIZE_MAX,XIDE_EDIT_FILE_APPLY)&&!stopped(w,cancel,cancelUser)&&xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_READY,0,e)&&xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_PUBLISHING,0,e);
 if(okay)w->status.phase=XIDE_MIX_READY;else{if(e&&!e->code)fail(e,XIDE_MODEL_CONFLICT,"Mixed-edit apply cancelled or refused.");xide_model_error cleanup={0};(void)rollback(w,1,guard,user,&cleanup);if(w->status.needsRecovery&&e)*e=cleanup;}w->busy=0;return okay;
}
int xideEditTransactionPublish(xide_edit_transaction_ticket* t,int(*guard)(void*),void* user,xide_open_edit_outcome* out)
{
 if(out)*out=(xide_open_edit_outcome){0};
 if(!t||!guard||!out||t->thread!=xrtThreadCurrentId()||t->busy||t->attempted||t->work->busy||t->work->status.phase!=XIDE_MIX_READY)return XUI_ERROR_INVALID_ARGUMENT;
 t->busy=t->attempted=1;t->guard=guard;t->user=user;int status;if(t->editors)status=xideOpenEditCommit(t->editors,t->work->editors,uiGuard,t,out);else{status=uiGuard(t);out->committed=status==XUI_OK;}
 t->work->status.editor=*out;t->work->status.committed=out->committed;t->work->status.phase=out->committed?XIDE_MIX_UI_COMMITTED:XIDE_MIX_UI_REJECTED;t->guard=NULL;t->user=NULL;t->busy=0;return status;
}
int xideEditTransactionFinish(xide_edit_transaction* w,int(*guard)(void*,size_t,int),void* user,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(!w||!guard||w->busy||(w->status.phase!=XIDE_MIX_UI_COMMITTED&&w->status.phase!=XIDE_MIX_UI_REJECTED))return fail(e,XIDE_MODEL_INVALID,"A definite owner publication outcome is required.");
 w->busy=1;int okay;
 if(w->status.committed){okay=xideEditJournalSetPhase(w->journal,XIDE_EDIT_TX_COMMITTED,0,e);w->status.phase=okay?XIDE_MIX_COMMITTED:XIDE_MIX_IN_DOUBT;w->status.needsRecovery=!okay;if(okay)okay=settleReaders(w,e);}else okay=rollback(w,1,guard,user,e);w->busy=0;return okay;
}
int xideEditTransactionTicketDestroy(xide_edit_transaction_ticket* t)
{
 if(!t)return XUI_OK;
 if(t->thread!=xrtThreadCurrentId()||t->busy||t->work->busy)return XUI_ERROR_BUSY;
 int status=xideOpenEditTicketDestroy(t->editors);if(status!=XUI_OK)return status;xuiLifetimeUnwatch(&t->life);xide_edit_transaction* w=t->work;xideEditTransactionCancel(w);if(w->status.phase==XIDE_MIX_READY)w->status.phase=XIDE_MIX_UI_REJECTED;w->attached=0;xrtFree(t);return XUI_OK;
}
