#include "editor/workspace_edit_documents.h"
#include "editor/workspace_edit_positions.h"
#include "platform/path.h"
#include <limits.h>
#include <string.h>
typedef struct work_row {xide_document_baseline* baseline;size_t first,count;int dirty;} work_row;
typedef struct ticket_row {
 uint64_t id,revision;size_t slot;uint32_t version;int dirty,external,override;
 xui_widget editor;xui_code_document core;xui_lifetime_link_t widgetLife,coreLife;
 char *path,*language;float scrollX,scrollY;
} ticket_row;
struct xide_open_edit_work {
 xide_workspace_edit* plan;xui_code_document_batch_layout_t layout;
 xui_code_document_batch_source_t* sources;work_row* rows;
 xui_code_selection_t *before,*after;size_t count,selections,used,limit;int attempted,ready;
 xide_edit_positions* positions;xui_code_document_batch_payload payload;
};
struct xide_open_edit_ticket {
 uint64_t thread,serial;size_t count;int busy,attempted;
 xide_document_store* store;xui_context context;xui_lifetime_link_t contextLife;
 ticket_row* rows;xui_code_document_batch batch;
 xide_open_edit_work* work;int (*hostGuard)(void*);void* hostUser;
};
static void contextGone(void* user){((xide_open_edit_ticket*)user)->context=NULL;}
static void widgetGone(void* user){((ticket_row*)user)->editor=NULL;}
static void coreGone(void* user){((ticket_row*)user)->core=NULL;}
static int charge(xide_open_edit_work* w,size_t bytes)
{if(bytes>w->limit-w->used)return 0;w->used+=bytes;return 1;}
static char* copyMetadata(xide_open_edit_work* w,const char* text)
{size_t bytes=strlen(text)+1;if(!charge(w,bytes))return NULL;char* p=xrtMalloc(bytes);if(p)memcpy(p,text,bytes);return p;}
static xide_document* findModel(xide_open_edit_ticket* t,const ticket_row* r)
{
 if(!t->context || !r->editor || !r->core)return NULL;
 /* The normal append-only store path is O(1) per target. Reordering does not
  * turn a stale slot into a model pointer: validate identity, then fall back. */
 if(r->slot<t->store->count){xide_document* d=t->store->items[r->slot];if(d->open && d->id==r->id && d->editor==r->editor)return d;}
 for(size_t i=0;i<t->store->count;++i){xide_document* d=t->store->items[i];if(d->open && d->id==r->id && d->editor==r->editor)return d;}
 return NULL;
}
static int selectionEqual(const xui_code_selection_t* a,const xui_code_selection_t* b)
{return a->iAnchorOffset==b->iAnchorOffset && a->iCaretOffset==b->iCaretOffset && a->iPreferredColumn==b->iPreferredColumn && a->iFlags==b->iFlags;}
static int readingCurrent(const ticket_row* r,const work_row* w,const xide_open_edit_work* work)
{
 xui_code_selection_model selections=xuiCodeEditGetSelection(r->editor);float x=0,y=0;
 if(!selections || xuiCodeSelectionGetCount(selections)!=(int)w->count || xuiCodeEditGetScroll(r->editor,&x,&y)!=XUI_OK || x!=r->scrollX || y!=r->scrollY)return 0;
 for(size_t i=0;i<w->count;++i){xui_code_selection_t s={0};if(xuiCodeSelectionGetAt(selections,(int)i,&s)!=XUI_OK || !selectionEqual(&s,work->before+w->first+i))return 0;}
 return 1;
}
static int current(xide_open_edit_ticket* t,xide_open_edit_work* w,int reading)
{
 if(!t->context || t->store->context!=t->context || t->store->count>4096)return 0;
 for(size_t i=0;i<t->count;++i){ticket_row* r=t->rows+i;xide_document* d=findModel(t,r);if(!d || !d->revision || d->revision==UINT64_MAX || d->revision!=r->revision || d->dirty!=r->dirty || d->externalState!=r->external || d->languageOverride!=r->override || d->suppressChange || d->preparedChange || d->baselineLease!=w->rows[i].baseline || !d->path || !d->language || strcmp(d->path,r->path) || strcmp(d->language,r->language) || xuiCodeEditGetDocument(d->editor)!=r->core || xuiCodeDocumentGetVersion(r->core)!=r->version || xuiCodeEditIsReadonly(d->editor) || (reading && !readingCurrent(r,w->rows+i,w)))return 0;}
 return 1;
}
void xideOpenEditWorkDestroy(xide_open_edit_work* w)
{
 if(!w)return;
 xuiCodeDocumentBatchPayloadDestroy(w->payload);xideEditPositionsDestroy(w->positions);
 for(size_t i=0;w->rows && i<w->count;++i)xideDocumentBaselineRelease(w->rows[i].baseline);
 xideWorkspaceEditRelease(w->plan);xrtFree(w->sources);xrtFree(w->rows);xrtFree(w->before);xrtFree(w->after);xrtFree(w);
}
int xideOpenEditTicketDestroy(xide_open_edit_ticket* t)
{
 if(!t)return XUI_OK;
 if(t->thread!=xrtThreadCurrentId() || t->busy)return XUI_ERROR_BUSY;
 int status=xuiCodeDocumentBatchDestroy(t->batch);if(status!=XUI_OK)return status;
 xuiLifetimeUnwatch(&t->contextLife);
 for(size_t i=0;t->rows && i<t->count;++i){xuiLifetimeUnwatch(&t->rows[i].widgetLife);xuiLifetimeUnwatch(&t->rows[i].coreLife);xrtFree(t->rows[i].path);xrtFree(t->rows[i].language);}
 xrtFree(t->rows);xrtFree(t);return XUI_OK;
}
int xideOpenEditCapture(xide_document_store* store,xide_workspace_edit* plan,size_t budget,xide_open_edit_ticket** ticket,xide_open_edit_work** work)
{
 if(!ticket || !work)return XUI_ERROR_INVALID_ARGUMENT;
 *ticket=NULL;*work=NULL;xide_workspace_edit_info info;
 if(!store || !store->context || !xideWorkspaceEditInfo(plan,&info) || info.documents>512 || store->count>4096 || budget>256u*1024u*1024u)return XUI_ERROR_INVALID_ARGUMENT;
 size_t limit=budget?budget:128u*1024u*1024u,rows=info.documents?info.documents:1;
 size_t initial=sizeof(xide_open_edit_ticket)+sizeof(xide_open_edit_work)+rows*(sizeof(ticket_row)+sizeof(work_row)+sizeof(xui_code_document_batch_source_t));
 if(info.bytes>limit || initial>limit-info.bytes)return XUI_ERROR_OUT_OF_MEMORY;
 xide_open_edit_ticket* t=xrtCalloc(1,sizeof(*t));xide_open_edit_work* w=xrtCalloc(1,sizeof(*w));int status=XUI_ERROR_OUT_OF_MEMORY;
 if(!t || !w)goto failed;
 t->thread=xrtThreadCurrentId();t->store=store;t->context=store->context;t->count=info.documents;
 w->limit=limit;w->used=initial+info.bytes;w->plan=xideWorkspaceEditRetain(plan);w->count=info.documents;if(!w->plan)goto failed;
 t->rows=xrtCalloc(rows,sizeof(*t->rows));w->rows=xrtCalloc(rows,sizeof(*w->rows));w->sources=xrtCalloc(rows,sizeof(*w->sources));if(!t->rows || !w->rows || !w->sources)goto failed;
 status=xuiContextLifetimeWatch(t->context,&t->contextLife,contextGone,t);if(status!=XUI_OK)goto failed;
 xui_code_document_batch_item_t targets[512];size_t metadata=0;
 for(size_t i=0;i<t->count;++i){
  xide_workspace_edit_document p;status=XUI_ERROR_INVALID_STATE;if(!xideWorkspaceEditDocumentAt(plan,i,&p) || !p.hasVersion || (p.proof.present && !p.proof.overlay))goto failed;
  xide_document* d=NULL;size_t slot=0;for(size_t j=0;j<store->count;++j)if(store->items[j]->open && store->items[j]->id==p.identity){d=store->items[j];slot=j;break;}
  if(!d || !d->path || !d->language || !d->revision || d->revision==UINT64_MAX || d->revision!=p.revision || d->suppressChange || d->preparedChange || d->externalState || xuiCodeEditIsReadonly(d->editor) || !xidePathLexicalEqual(d->path,p.path))goto failed;
  ticket_row* r=t->rows+i;r->id=d->id;r->revision=d->revision;r->slot=slot;r->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));r->editor=d->editor;r->core=xuiCodeEditGetDocument(d->editor);r->dirty=d->dirty;r->external=d->externalState;r->override=d->languageOverride;
  size_t strings=strlen(d->path)+strlen(d->language)+2;status=XUI_ERROR_OUT_OF_MEMORY;if(strings>1024u*1024u-metadata)goto failed;metadata+=strings;r->path=copyMetadata(w,d->path);r->language=copyMetadata(w,d->language);if(!r->path || !r->language)goto failed;
  w->rows[i].baseline=xideDocumentRetainBaseline(d);if(!w->rows[i].baseline || !charge(w,xideDocumentBaselineStorage(w->rows[i].baseline)))goto failed;
  status=xuiWidgetLifetimeWatch(r->editor,&r->widgetLife,widgetGone,r);if(status==XUI_OK)status=xuiCodeDocumentLifetimeWatch(r->core,&r->coreLife,coreGone,r);if(status!=XUI_OK)goto failed;
  int n=xuiCodeSelectionGetCount(xuiCodeEditGetSelection(r->editor));status=XUI_ERROR_INVALID_STATE;if(n<1 || (size_t)n>4096-w->selections)goto failed;w->rows[i].first=w->selections;w->rows[i].count=(size_t)n;w->selections+=(size_t)n;
  if(xuiCodeEditGetScroll(r->editor,&r->scrollX,&r->scrollY)!=XUI_OK)goto failed;
  targets[i]=(xui_code_document_batch_item_t){.pDocument=r->core,.iChangeVersion=xuiCodeDocumentGetChangeVersion(r->core)};
 }
 status=XUI_ERROR_OUT_OF_MEMORY;size_t selected=w->selections?w->selections:1;if(!charge(w,2*selected*sizeof(*w->before)))goto failed;
 w->before=xrtCalloc(selected,sizeof(*w->before));w->after=xrtCalloc(selected,sizeof(*w->after));if(!w->before || !w->after)goto failed;
 for(size_t i=0;i<t->count;++i)for(size_t j=0;j<w->rows[i].count;++j)if(xuiCodeSelectionGetAt(xuiCodeEditGetSelection(t->rows[i].editor),(int)j,w->before+w->rows[i].first+j)!=XUI_OK){status=XUI_ERROR_INVALID_STATE;goto failed;}
 status=XUI_ERROR_OUT_OF_MEMORY;if(w->used==limit)goto failed;
 status=xuiCodeDocumentBatchCapture(targets,t->count,limit-w->used,&t->batch);if(status==XUI_OK)status=xuiCodeDocumentBatchDescribe(t->batch,&w->layout,w->sources,t->count);if(status!=XUI_OK)goto failed;
 status=XUI_ERROR_OUT_OF_MEMORY;if(!charge(w,w->layout.iCaptureBytes))goto failed;
 t->serial=w->layout.iCapture;*ticket=t;*work=w;return XUI_OK;
failed:
 if(t && !t->thread)t->thread=xrtThreadCurrentId();
 (void)xideOpenEditTicketDestroy(t);xideOpenEditWorkDestroy(w);return status;
}
int xideOpenEditStage(xide_open_edit_work* w,int (*cancelled)(void*),void* user)
{
 if(!w || w->attempted)return XUI_ERROR_INVALID_STATE;
 w->attempted=1;enum xide_workspace_edit_error error;xide_workspace_edit_info info;
 if(!xideWorkspaceEditInfo(w->plan,&info))return XUI_ERROR_INVALID_STATE;
 size_t available=w->limit-(w->used-info.bytes);
 w->positions=xideEditPositionsPrepare(w->plan,available,cancelled,user,&error);if(!w->positions)return error==XIDE_WORKSPACE_EDIT_MEMORY || error==XIDE_WORKSPACE_EDIT_LIMIT?XUI_ERROR_OUT_OF_MEMORY:XUI_ERROR_INVALID_STATE;
 size_t mapping=xideEditPositionsStorage(w->positions)-info.bytes;if(!charge(w,mapping))return XUI_ERROR_OUT_OF_MEMORY;
 xui_code_document_batch_text_t texts[512];
 for(size_t i=0;i<w->count;++i){
  if(cancelled && cancelled(user))return XUI_ERROR_INVALID_STATE;
  xide_workspace_edit_document d;if(!xideWorkspaceEditDocumentAt(w->plan,i,&d))return XUI_ERROR_INVALID_STATE;
  texts[i]=(xui_code_document_batch_text_t){sizeof(texts[i]),{xlsTextData(d.before),xlsTextSize(d.before)},{xlsTextData(d.after),xlsTextSize(d.after)}};
  const xide_text_file* baseline=xideDocumentBaselineRead(w->rows[i].baseline);w->rows[i].dirty=strlen(baseline->text)!=texts[i].After.Size || memcmp(baseline->text,texts[i].After.Data,texts[i].After.Size);
  for(size_t j=0;j<w->rows[i].count;++j){size_t at=w->rows[i].first+j;xide_edit_selection selected={w->before[at].iAnchorOffset,w->before[at].iCaretOffset},mapped;if(!xideEditSelectionMap(w->positions,i,selected,&mapped) || mapped.anchor>INT_MAX || mapped.caret>INT_MAX)return XUI_ERROR_INVALID_STATE;w->after[at]=w->before[at];w->after[at].iAnchorOffset=(int)mapped.anchor;w->after[at].iCaretOffset=(int)mapped.caret;}
 }
 if(w->used==w->limit)return XUI_ERROR_OUT_OF_MEMORY;
 /* SDK Stage charges its retained Capture metadata again in its own quota.
  * Include it there, but retain it in used while admitting the position map. */
 int status=xuiCodeDocumentBatchStage(&w->layout,w->sources,texts,w->limit-w->used+w->layout.iCaptureBytes,cancelled,user,&w->payload);w->ready=status==XUI_OK;return status;
}
static int beforePublish(void* user)
{
 xide_open_edit_ticket* t=user;if(!current(t,t->work,1))return XUI_ERROR_INVALID_STATE;
 int status=t->hostGuard(t->hostUser);if(status!=XUI_OK)return status;
 return current(t,t->work,1)?XUI_OK:XUI_ERROR_INVALID_STATE;
}
static void publishModels(void* user)
{
 xide_open_edit_ticket* t=user;
 for(size_t i=0;i<t->count;++i){xide_workspace_edit_document d;xideWorkspaceEditDocumentAt(t->work->plan,i,&d);if(d.changed)xideDocumentPublishPrepared(findModel(t,t->rows+i),t->rows[i].core,t->work->rows[i].dirty);}
}
int xideOpenEditCommit(xide_open_edit_ticket* t,xide_open_edit_work* w,int (*hostGuard)(void*),void* user,xide_open_edit_outcome* outcome)
{
 if(!outcome)return XUI_ERROR_INVALID_ARGUMENT;
 *outcome=(xide_open_edit_outcome){0};if(!t || !w || !hostGuard)return XUI_ERROR_INVALID_ARGUMENT;
 if(t->thread!=xrtThreadCurrentId() || t->busy)return XUI_ERROR_BUSY;
 if(t->attempted || !w->ready || t->serial!=w->layout.iCapture || t->count!=w->count)return XUI_ERROR_INVALID_STATE;
 t->attempted=t->busy=1;t->work=w;t->hostGuard=hostGuard;t->hostUser=user;int status=XUI_ERROR_INVALID_STATE;bool taken=false;
 if(current(t,w,1)){status=xuiCodeDocumentBatchAccept(t->batch,w->payload,&taken);if(taken)w->payload=NULL;if(status==XUI_OK){xui_code_document_batch_callbacks_t hooks={sizeof(hooks),beforePublish,publishModels,t};status=xuiCodeDocumentBatchCommitChecked(t->batch,&hooks,&outcome->committed);}}
 if(outcome->committed)for(size_t i=0;i<t->count;++i){ticket_row* r=t->rows+i;xide_document* d=findModel(t,r);xide_workspace_edit_document p;xideWorkspaceEditDocumentAt(w->plan,i,&p);if(!p.changed)continue;if(d)xideDocumentFinishPrepared(d);
  if(!d || d->revision!=r->revision+1 || d->baselineLease!=w->rows[i].baseline || d->externalState!=r->external || d->languageOverride!=r->override || !d->path || !d->language || strcmp(d->path,r->path) || strcmp(d->language,r->language) || xuiCodeEditIsReadonly(d->editor) || xuiCodeEditGetDocument(d->editor)!=r->core || xuiCodeDocumentGetVersion(r->core)!=(uint32_t)(r->version+1) || !readingCurrent(r,w->rows+i,w)){++outcome->readingSkipped;continue;}
  xui_code_selection_model selection=xuiCodeEditGetSelection(r->editor);int restored=XUI_OK;
  for(size_t j=0;j<w->rows[i].count;++j){const xui_code_selection_t* state=w->after+w->rows[i].first+j;restored=j?xuiCodeSelectionAdd(selection,state):xuiCodeSelectionSetState(selection,r->core,state);if(restored!=XUI_OK)break;}
  if(restored==XUI_OK)restored=xuiCodeEditSetScroll(r->editor,r->scrollX,r->scrollY);
  if(restored==XUI_OK)++outcome->readingRestored;else{++outcome->readingSkipped;if(!outcome->readingStatus)outcome->readingStatus=restored;}
 }
 t->hostGuard=NULL;t->hostUser=NULL;t->work=NULL;t->busy=0;return status;
}
