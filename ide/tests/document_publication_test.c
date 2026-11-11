#include "editor/documents.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;static int allocationProbe,allocationCalls,allocationFail;
static int referenceProbe,retainCalls,retainResult;static volatile LONG releaseResult;static volatile int32* trackedCounter;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"document-publication:%d: %s\n",__LINE__,#e);exit(1);}}while(0)
extern void* (*__real___imp_xrtMalloc)(size_t);
extern void* (*__real___imp_xrtCalloc)(size_t,size_t);
static void* probeMalloc(size_t size){if(allocationProbe && ++allocationCalls==allocationFail)return NULL;return __real___imp_xrtMalloc(size);}
static void* probeCalloc(size_t count,size_t size){if(allocationProbe && ++allocationCalls==allocationFail)return NULL;return __real___imp_xrtCalloc(count,size);}
void* (*__wrap___imp_xrtMalloc)(size_t)=probeMalloc;
void* (*__wrap___imp_xrtCalloc)(size_t,size_t)=probeCalloc;
extern int32 (*__real___imp_xrtRefRetain)(volatile int32*);
extern int32 (*__real___imp_xrtRefRelease)(volatile int32*);
static int32 probeRetain(volatile int32* count){int32 result=__real___imp_xrtRefRetain(count);if(referenceProbe){++retainCalls;retainResult=result;trackedCounter=count;}return result;}
static int32 probeRelease(volatile int32* count){int32 result=__real___imp_xrtRefRelease(count);if(count==trackedCounter)InterlockedExchange(&releaseResult,result);return result;}
int32 (*__wrap___imp_xrtRefRetain)(volatile int32*)=probeRetain;
int32 (*__wrap___imp_xrtRefRelease)(volatile int32*)=probeRelease;
#if !defined(XUI_CODE_DOCUMENT_BATCH) || XUI_CODE_DOCUMENT_BATCH<2
int main(void){(void)checks;fputs("document_publication_test requires the qualified XUI batch-v2 SDK\n",stderr);return 77;}
#else
typedef struct baseline_job {xide_document_baseline* baseline;const char *raw,*text;const void *rawPointer,*textPointer;HANDLE ready,go;int valid;} baseline_job;
static DWORD WINAPI readBaseline(void* user)
{
 baseline_job* job=user;SetEvent(job->ready);if(WaitForSingleObject(job->go,30000)!=WAIT_OBJECT_0)return 1;
 const xide_text_file* view=xideDocumentBaselineRead(job->baseline);
 job->valid=view && view->raw==job->rawPointer && view->text==job->textPointer && view->rawLength==strlen(job->raw) && !memcmp(view->raw,job->raw,view->rawLength) && !strcmp(view->text,job->text);
 xideDocumentBaselineRelease(job->baseline);job->baseline=NULL;return 0;
}
typedef struct payload_job {
 xui_code_document_batch_layout_t layout;xui_code_document_batch_source_t sources[2];xui_code_document_batch_text_t texts[2];
 xide_document_baseline* baselines[2];xui_code_document_batch_payload payload;int dirty[2],status;DWORD thread;
} payload_job;
static DWORD WINAPI stagePayload(void* user)
{
 payload_job* job=user;job->thread=GetCurrentThreadId();
 for(unsigned i=0;i<2;++i){const xide_text_file* baseline=xideDocumentBaselineRead(job->baselines[i]);if(!baseline)return 1;job->dirty[i]=strcmp(baseline->text,job->texts[i].After.Data)!=0;}
 job->status=xuiCodeDocumentBatchStage(&job->layout,job->sources,job->texts,0,NULL,NULL,&job->payload);return 0;
}
typedef struct publication {xide_document* docs[2];payload_job* job;uint64_t ids[2],revisions[2];int inspect,callbacks,error,published,guardCalls,closeStatus;xui_context closeContext;bool closeConsumed;} publication;
static void modelChanged(xide_document* document,void* user)
{
 publication* p=user;(void)document;++p->callbacks;if(!p->inspect)return;
 for(unsigned i=0;i<2;++i)if(p->docs[i]->revision!=p->revisions[i]+1 || p->docs[i]->dirty!=p->job->dirty[i] || xuiCodeDocumentGetDirty(xuiCodeEditGetDocument(p->docs[i]->editor))!=p->job->dirty[i])p->error=1;
 if(p->published!=1)p->error=1;
 if(p->closeContext && p->callbacks==1)p->closeStatus=xuiDestroyOwnerChecked(p->closeContext,&p->closeConsumed);
}
static int beforePublish(void* user)
{
 publication* p=user;++p->guardCalls;
 for(unsigned i=0;i<2;++i)if(!p->docs[i]->open || p->docs[i]->id!=p->ids[i] || p->docs[i]->revision!=p->revisions[i] || !p->docs[i]->revision || p->docs[i]->revision==UINT64_MAX || p->docs[i]->baselineLease!=p->job->baselines[i])return XUI_ERROR_INVALID_STATE;
 return XUI_OK;
}
static void publishModel(void* user)
{
 publication* p=user;allocationCalls=0;allocationFail=0;allocationProbe=1;
 for(unsigned i=0;i<2;++i)xideDocumentPublishPrepared(p->docs[i],xuiCodeEditGetDocument(p->docs[i]->editor),p->job->dirty[i]);
 allocationProbe=0;if(allocationCalls)p->error=1;++p->published;
}
static xui_code_document_batch capture(publication* p,payload_job* job,const char* const before[2],const char* const after[2])
{
 memset(job,0,sizeof(*job));p->job=job;xui_code_document_batch_item_t items[2];xui_code_document_batch batch=NULL;
 for(unsigned i=0;i<2;++i){p->ids[i]=p->docs[i]->id;p->revisions[i]=p->docs[i]->revision;items[i]=(xui_code_document_batch_item_t){.pDocument=xuiCodeEditGetDocument(p->docs[i]->editor),.iChangeVersion=xuiCodeDocumentGetChangeVersion(xuiCodeEditGetDocument(p->docs[i]->editor))};job->baselines[i]=xideDocumentRetainBaseline(p->docs[i]);CHECK(job->baselines[i]);job->texts[i]=(xui_code_document_batch_text_t){sizeof(job->texts[i]),{before[i],strlen(before[i])},{after[i],strlen(after[i])}};}
 CHECK(xuiCodeDocumentBatchCapture(items,2,0,&batch)==XUI_OK && xuiCodeDocumentBatchDescribe(batch,&job->layout,job->sources,2)==XUI_OK);HANDLE thread=CreateThread(NULL,0,stagePayload,job,0,NULL);CHECK(thread && WaitForSingleObject(thread,30000)==WAIT_OBJECT_0);DWORD exitCode=1;CHECK(GetExitCodeThread(thread,&exitCode) && !exitCode && job->thread!=GetCurrentThreadId() && job->status==XUI_OK && job->payload);CloseHandle(thread);return batch;
}
static void finish(publication* p,payload_job* job,xui_code_document_batch batch)
{
 for(unsigned i=0;i<2;++i)xideDocumentFinishPrepared(p->docs[i]);
 CHECK(xuiCodeDocumentBatchDestroy(batch)==XUI_OK);xuiCodeDocumentBatchPayloadDestroy(job->payload);job->payload=NULL;
 for(unsigned i=0;i<2;++i){xideDocumentBaselineRelease(job->baselines[i]);job->baselines[i]=NULL;}
}
int main(int argc,char** argv)
{
 CHECK(argc==2 && xrtDirCreateAll(argv[1]));const char* raw="\xef\xbb\xbf" "func main(...) as int\r\n return 0\r\nend\r\n";const char* normalized="func main(...) as int\n return 0\nend\n";const char* saved="// saved\nfunc main(...) as int\n return 0\nend\n";const char* edited="// edited\nfunc main(...) as int\n return 1\nend\n";const char* c1="int n=1;\n";const char* c2="int n=2;\n";
 char* path=xidePathResolveLexical(argv[1],"publication.xl");char* cpath=xidePathResolveLexical(argv[1],"publication.c");CHECK(path && cpath && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)raw,strlen(raw)}) && xrtFileWriteAtomic(cpath,(xbytesview){(const unsigned char*)c1,strlen(c1)}));xui_context context=NULL;CHECK(xuiCreate(&context)==XUI_OK);xide_document_store store={0};xide_model_error error={0};publication model={0};xideDocumentsInit(&store,context,NULL,modelChanged,&model);model.docs[0]=xideDocumentOpen(&store,path,&error);model.docs[1]=xideDocumentOpen(&store,cpath,&error);CHECK(model.docs[0] && model.docs[1]);xide_document* d=model.docs[0];
 const void *originalRaw=d->baseline.raw,*originalText=d->baseline.text;uint64_t originalRevision=d->revision;
 allocationCalls=0;allocationFail=1;allocationProbe=1;xide_document_baseline* refused=xideDocumentRetainBaseline(d);allocationProbe=0;CHECK(!refused && !d->baselineLease && allocationCalls==1 && d->baseline.raw==originalRaw && d->baseline.text==originalText && d->revision==originalRevision);xrtClearError();
 referenceProbe=1;retainCalls=0;baseline_job reader={.baseline=xideDocumentRetainBaseline(d),.raw=raw,.text=normalized,.rawPointer=originalRaw,.textPointer=originalText};referenceProbe=0;CHECK(reader.baseline && retainCalls==1 && retainResult==2 && xideDocumentBaselineStorage(reader.baseline)>=2*(d->baseline.rawLength+1));referenceProbe=1;retainCalls=0;xide_document_baseline* same=xideDocumentRetainBaseline(d);referenceProbe=0;CHECK(same==reader.baseline && retainCalls==1 && retainResult==3 && xideDocumentBaselineRead(same)->text==originalText);xideDocumentBaselineRelease(same);
 reader.ready=CreateEventW(NULL,TRUE,FALSE,NULL);reader.go=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(reader.ready && reader.go);HANDLE worker=CreateThread(NULL,0,readBaseline,&reader,0,NULL);CHECK(worker && WaitForSingleObject(reader.ready,30000)==WAIT_OBJECT_0);
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(d->editor),0,"// saved\n")==XUI_OK && xideDocumentSave(d,NULL,0,&error) && !d->baselineLease && !strcmp(d->baseline.text,saved) && releaseResult==1);uint64_t id=d->id;CHECK(xideDocumentClose(d,1,&error) && !d->baselineLease && xideDocumentOpen(&store,path,&error)==d && d->id!=id);SetEvent(reader.go);CHECK(WaitForSingleObject(worker,30000)==WAIT_OBJECT_0 && reader.valid && !reader.baseline && releaseResult==0);trackedCounter=NULL;CloseHandle(worker);CloseHandle(reader.ready);CloseHandle(reader.go);
 xide_document_baseline* closedLease=xideDocumentRetainBaseline(model.docs[1]);CHECK(closedLease);uint64_t cId=model.docs[1]->id;CHECK(xideDocumentClose(model.docs[1],1,&error) && !model.docs[1]->baseline.raw && !model.docs[1]->baseline.text && !model.docs[1]->baselineLease && !strcmp(xideDocumentBaselineRead(closedLease)->text,c1));CHECK(xideDocumentOpen(&store,cpath,&error)==model.docs[1] && model.docs[1]->id!=cId);xideDocumentBaselineRelease(closedLease);
 CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(model.docs[1]->editor),0,(int)strlen(c1),c2)==XUI_OK && model.docs[1]->dirty);
 const char* before[2]={saved,c2};const char* after[2]={edited,c1};payload_job job;xui_code_document_batch batch=capture(&model,&job,before,after);CHECK(job.dirty[0] && !job.dirty[1]);bool taken=false;int status=xuiCodeDocumentBatchAccept(batch,job.payload,&taken);if(taken)job.payload=NULL;CHECK(status==XUI_OK && taken);xui_code_document_batch_callbacks_t hooks={sizeof(hooks),beforePublish,publishModel,&model};bool committed=false;model.callbacks=0;model.inspect=1;CHECK(xuiCodeDocumentBatchCommitChecked(batch,&hooks,&committed)==XUI_OK && committed && model.callbacks==2 && model.published==1 && model.guardCalls==1 && !model.error);CHECK(!model.docs[0]->preparedChange && !model.docs[1]->preparedChange);model.inspect=0;finish(&model,&job,batch);
 uint64_t revision=model.docs[1]->revision;CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(model.docs[1]->editor))==XUI_OK && model.docs[1]->dirty && model.docs[1]->revision==revision+1);CHECK(xuiCodeDocumentRedo(xuiCodeEditGetDocument(model.docs[1]->editor))==XUI_OK && !model.docs[1]->dirty && model.docs[1]->revision==revision+2);
 /* Saving a dirty document changes its baseline without changing the body
  * clock. XUI alone accepts the payload; the client baseline fence refuses. */
 const char* before2[2]={edited,c1};const char* after2[2]={saved,c2};batch=capture(&model,&job,before2,after2);CHECK(!job.dirty[0]);revision=model.docs[0]->revision;CHECK(xideDocumentSave(model.docs[0],NULL,0,&error) && model.docs[0]->revision==revision && model.docs[0]->baselineLease!=job.baselines[0]);taken=false;status=xuiCodeDocumentBatchAccept(batch,job.payload,&taken);if(taken)job.payload=NULL;CHECK(status==XUI_OK && taken);int notices=model.callbacks,published=model.published;committed=true;CHECK(xuiCodeDocumentBatchCommitChecked(batch,&hooks,&committed)==XUI_ERROR_INVALID_STATE && !committed && model.callbacks==notices && model.published==published && !strcmp(xuiCodeEditGetText(model.docs[0]->editor),edited) && !strcmp(xuiCodeEditGetText(model.docs[1]->editor),c1));finish(&model,&job,batch);xrtClearError();
 CHECK(!xideDocumentRetainBaseline(NULL) && !xideDocumentBaselineRead(NULL) && !xideDocumentBaselineStorage(NULL));xideDocumentBaselineRelease(NULL);xideDocumentFinishPrepared(NULL);
 xide_document_baseline* final=xideDocumentRetainBaseline(model.docs[0]);CHECK(final);const xide_text_file* retained=xideDocumentBaselineRead(final);CHECK(!strcmp(retained->text,edited));
 batch=capture(&model,&job,before2,after2);taken=false;status=xuiCodeDocumentBatchAccept(batch,job.payload,&taken);if(taken)job.payload=NULL;CHECK(status==XUI_OK && taken);model.inspect=1;model.callbacks=model.published=model.guardCalls=model.error=0;model.closeContext=context;committed=false;status=xuiCodeDocumentBatchCommitChecked(batch,&hooks,&committed);CHECK(status==XUI_OK && committed && !model.error && model.callbacks==1 && model.closeStatus==XUI_STATUS_DEFERRED && !model.closeConsumed && model.docs[1]->preparedChange);model.inspect=0;
 for(unsigned i=0;i<2;++i){xideDocumentFinishPrepared(model.docs[i]);CHECK(!model.docs[i]->preparedChange);}xideDocumentsClear(&store);CHECK(!strcmp(retained->text,edited));bool consumed=false;CHECK(xuiDestroyOwnerChecked(context,&consumed)==XUI_OK && consumed);context=NULL;CHECK(xuiCodeDocumentBatchDestroy(batch)==XUI_OK);for(unsigned i=0;i<2;++i)xideDocumentBaselineRelease(job.baselines[i]);xideDocumentBaselineRelease(final);free(path);free(cpath);printf("document_publication_test passed: %u checks; zero-copy saved baselines across save/close/worker retirement, real XUI worker batch, atomic dirty/revisions, one notice/advance, undo, baseline-only refusal and skipped-notice close cleanup\n",checks);return 0;
}
#endif
