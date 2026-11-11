/* Reuse the exact producer/allocator observer from the existing batch test. */
#define main xuiBatchLegacyTestMain
#include "xui_document_batch_test.c"
#undef main
typedef struct worker_job {
    xui_code_document_batch_layout_t layout;
    xui_code_document_batch_source_t sources[2];
    xui_code_document_batch_text_t texts[2];
    xui_code_document_batch_payload payload;
    HANDLE entered,resume;
    DWORD thread;
    unsigned polls,stopAt;
    int status,timedOut;
} worker_job;
static int workerStop(void* user)
{
    worker_job* job=user;++job->polls;
    if(job->entered && job->polls==1){SetEvent(job->entered);if(WaitForSingleObject(job->resume,10000)!=WAIT_OBJECT_0){job->timedOut=1;return 1;}}
    return job->stopAt && job->polls>=job->stopAt;
}
static DWORD WINAPI stageThread(void* user)
{
    worker_job* job=user;job->thread=GetCurrentThreadId();
    job->status=xuiCodeDocumentBatchStage(&job->layout,job->sources,job->texts,0,workerStop,job,&job->payload);return 0;
}
static DWORD WINAPI retireThread(void* user)
{xuiCodeDocumentBatchPayloadDestroy((xui_code_document_batch_payload)user);return 0;}
static int joinThread(HANDLE thread)
{CHECK(thread && WaitForSingleObject(thread,20000)==WAIT_OBJECT_0 && CloseHandle(thread));return 1;}
static int runStage(worker_job* job)
{DWORD owner=GetCurrentThreadId();CHECK(joinThread(CreateThread(NULL,0,stageThread,job,0,NULL)));CHECK(job->thread!=owner && !job->timedOut);return 1;}
static int retirePayload(xui_code_document_batch_payload payload)
{CHECK(joinThread(CreateThread(NULL,0,retireThread,payload,0,NULL)));return 1;}
static int capture(worker_job* job)
{
    xui_code_document_batch_item_t rows[2];
    for(size_t i=0;i<2;++i){rows[i]=item(docs[i],before[i],after[i]);rows[i].Before=(xstrview){(const char*)1,SIZE_MAX};rows[i].After=rows[i].Before;}
    CHECK(xuiCodeDocumentBatchCapture(rows,2,0,&current)==XUI_OK && current);
    CHECK(xuiCodeDocumentBatchDescribe(current,&job->layout,job->sources,2)==XUI_OK);
    for(size_t i=0;i<2;++i)job->texts[i]=(xui_code_document_batch_text_t){sizeof(job->texts[i]),xrtStrView(before[i]),xrtStrView(after[i])};
    return 1;
}
static int coldStage(unsigned gate)
{
    CHECK(createDocs());worker_job job={0};CHECK(capture(&job));size_t baseline=active;
    bool applied=true;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_ERROR_INVALID_STATE && !applied);
    allocationCalls=0;failAllocation=gate;tracking=1;CHECK(runStage(&job));tracking=0;
    if(gate){CHECK(allocationCalls>=gate && job.status==XUI_ERROR_OUT_OF_MEMORY && !job.payload && active==baseline);}
    else{
        CHECK(job.status==XUI_OK && job.payload && job.polls>0);printf("worker stage allocation coverage: %u cold gates,%u cancellation polls\n",allocationCalls,job.polls);
        for(size_t i=0;i<job.payload->count;++i){xui_document_batch_row_t* row=job.payload->rows+i;CHECK(!row->document && !row->lifetime.pUser && !row->call.document && !row->call.context);}
        CHECK(retirePayload(job.payload));job.payload=NULL;CHECK(active==baseline);
        for(unsigned i=1;i<=job.polls;++i){worker_job stopped=job;stopped.payload=NULL;stopped.polls=0;stopped.stopAt=i;CHECK(runStage(&stopped));CHECK(stopped.status==XUI_ERROR_INVALID_STATE && !stopped.payload && stopped.polls==i && active==baseline);}
    }
    CHECK(unchanged());CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);return 1;
}
static int coldCapture(unsigned gate)
{
    CHECK(createDocs());size_t baseline=active;xui_code_document_batch_item_t rows[]={item(docs[0],before[0],after[0]),item(docs[1],before[1],after[1])};
    allocationCalls=0;failAllocation=gate;tracking=1;int status=xuiCodeDocumentBatchCapture(rows,2,0,&current);tracking=0;
    CHECK(allocationCalls>=gate && status==XUI_ERROR_OUT_OF_MEMORY && !current && active==baseline && unchanged());clearDocs();CHECK(!active);return 1;
}
static int acceptedAndQuota(void)
{
    CHECK(createDocs());worker_job job={0};CHECK(capture(&job) && runStage(&job));CHECK(job.status==XUI_OK && job.payload);size_t exact=job.payload->used;
    CHECK(retirePayload(job.payload));job.payload=NULL;
    CHECK(xuiCodeDocumentBatchStage(&job.layout,job.sources,job.texts,exact-1,NULL,NULL,&job.payload)==XUI_ERROR_OUT_OF_MEMORY && !job.payload);
    CHECK(xuiCodeDocumentBatchStage(&job.layout,job.sources,job.texts,exact,NULL,NULL,&job.payload)==XUI_OK && job.payload);
    allocationCalls=0;failAllocation=1;tracking=1;bool taken=false;int status=xuiCodeDocumentBatchAccept(current,job.payload,&taken);tracking=0;
    CHECK(status==XUI_OK && taken && !allocationCalls);job.payload=NULL;CHECK(unchanged());
    allocationCalls=0;failAllocation=1;tracking=1;bool applied=false;status=xuiCodeDocumentBatchCommit(current,&applied);tracking=0;
    CHECK(status==XUI_OK && applied && !allocationCalls && !strcmp(xuiCodeDocumentGetText(docs[0]),after[0]) && !strcmp(xuiCodeDocumentGetText(docs[1]),after[1]));
    CHECK(xuiCodeDocumentUndo(docs[0])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[0]),before[0]));CHECK(xuiCodeDocumentUndo(docs[0])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[0]),"a"));
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);return 1;
}
static int changedWhileStaging(int close)
{
    CHECK(createDocs());worker_job job={0};CHECK(capture(&job));job.entered=CreateEventW(NULL,TRUE,FALSE,NULL);job.resume=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(job.entered && job.resume);
    HANDLE worker=CreateThread(NULL,0,stageThread,&job,0,NULL);CHECK(worker && WaitForSingleObject(job.entered,10000)==WAIT_OBJECT_0);
    if(close)clearDocs();else CHECK(xuiCodeDocumentReplace(docs[1],0,1,"Z")==XUI_OK);
    CHECK(SetEvent(job.resume) && joinThread(worker) && CloseHandle(job.entered) && CloseHandle(job.resume));CHECK(job.status==XUI_OK && job.payload && !job.timedOut);
    bool taken=true;CHECK(xuiCodeDocumentBatchAccept(current,job.payload,&taken)==XUI_ERROR_INVALID_STATE && !taken);
    CHECK(retirePayload(job.payload));CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
    if(!close)CHECK(!strcmp(xuiCodeDocumentGetText(docs[0]),before[0]) && !strcmp(xuiCodeDocumentGetText(docs[1]),"Zyz"));
    clearDocs();CHECK(!active);return 1;
}
static int staleCaptureAndBody(void)
{
    CHECK(createDocs());worker_job old={0};CHECK(capture(&old));uint64_t serial=old.layout.iCapture;CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
    worker_job fresh={0};CHECK(capture(&fresh));CHECK(serial && fresh.layout.iCapture && serial!=fresh.layout.iCapture);CHECK(runStage(&old) && old.status==XUI_OK);
    bool taken=true;CHECK(xuiCodeDocumentBatchAccept(current,old.payload,&taken)==XUI_ERROR_INVALID_STATE && !taken);CHECK(retirePayload(old.payload));
    /* Put a real gap between the prefix and suffix, restoring the same bytes. */
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;CHECK(xuiCodeDocumentReplace(docs[1],1,2,"Y")==XUI_OK && xuiCodeDocumentReplace(docs[1],1,2,"y")==XUI_OK);CHECK(xuiCodeDocumentSetDirty(docs[1],0)==XUI_OK);
    worker_job job={0};CHECK(capture(&job));CHECK(docs[1]->iGapStart>0 && docs[1]->iGapStart<docs[1]->iLength);
    const char* wrong[]={"Qyz","xyQ","xyQ"};for(size_t i=0;i<3;++i){job.texts[1].Before=xrtStrView(wrong[i]);if(i==2)job.texts[1].After=job.texts[1].Before;CHECK(runStage(&job) && job.status==XUI_OK);taken=true;CHECK(xuiCodeDocumentBatchAccept(current,job.payload,&taken)==XUI_ERROR_INVALID_STATE && !taken);CHECK(retirePayload(job.payload));job.payload=NULL;}
    job.texts[1].Before=xrtStrView(before[1]);job.texts[1].After=xrtStrView(after[1]);CHECK(job.sources[1].iUndoCount>0);--job.sources[1].iUndoCount;
    CHECK(runStage(&job) && job.status==XUI_OK);taken=true;CHECK(xuiCodeDocumentBatchAccept(current,job.payload,&taken)==XUI_ERROR_INVALID_STATE && !taken);CHECK(retirePayload(job.payload));job.payload=NULL;++job.sources[1].iUndoCount;
    CHECK(runStage(&job) && job.status==XUI_OK);taken=false;CHECK(xuiCodeDocumentBatchAccept(current,job.payload,&taken)==XUI_OK && taken);job.payload=NULL;
    bool applied=false;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_OK && applied && !strcmp(xuiCodeDocumentGetText(docs[1]),after[1]));CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);return 1;
}
typedef struct model_hooks {int mode,guard,published,notices,error;} model_hooks;
static int modelGuard(void* user)
{
    model_hooks* model=user;++model->guard;
    if(xuiCodeDocumentInsert(docs[1],0,"forbidden")!=XUI_ERROR_BUSY)model->error=1;
    if(model->mode==1)return XUI_ERROR_RESOURCE_FAILED;
    if(model->mode==2){bool consumed=false;if(xuiCodeDocumentDestroyOwnerChecked(docs[0],&consumed)!=XUI_OK || !consumed)model->error=1;docs[0]=NULL;}
    return XUI_OK;
}
static void modelPublished(void* user)
{
    model_hooks* model=user;++model->published;
    for(size_t i=0;i<2;++i)if(!docs[i] || strcmp(xuiCodeDocumentGetText(docs[i]),after[i]))model->error=1;
}
static void modelNotice(xui_code_document doc,xui_code_range_t range,uint32_t version,void* user)
{
    (void)doc;(void)range;(void)version;model_hooks* model=user;++model->notices;if(model->published!=1)model->error=1;
}
static int hostHooks(void)
{
    for(int mode=0;mode<=2;++mode){
        CHECK(createDocs());worker_job job={0};CHECK(capture(&job) && runStage(&job) && job.status==XUI_OK);bool taken=false;CHECK(xuiCodeDocumentBatchAccept(current,job.payload,&taken)==XUI_OK && taken);job.payload=NULL;
        model_hooks model={.mode=mode};xui_code_document_batch_callbacks_t hooks={sizeof(hooks),modelGuard,modelPublished,&model};
        for(size_t i=0;i<2;++i){CHECK(xuiCodeDocumentSetChangeCallback(docs[i],modelNotice,&model)==XUI_OK);}
        bool applied=false;allocationCalls=0;failAllocation=1;tracking=1;int status=xuiCodeDocumentBatchCommitChecked(current,&hooks,&applied);tracking=0;
        CHECK(!allocationCalls && !model.error && model.guard==1);
        if(!mode)CHECK(status==XUI_OK && applied && model.published==1 && model.notices==2);
        else{CHECK(status!=XUI_OK && !applied && !model.published && !model.notices);if(mode==1)CHECK(unchanged());else CHECK(!docs[0] && !strcmp(xuiCodeDocumentGetText(docs[1]),before[1]));}
        CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);
    }
    return 1;
}
static int takenOnCleanupFailure(void)
{
    CHECK(createDocs());worker_job job={0};CHECK(capture(&job) && runStage(&job) && job.status==XUI_OK);
    xrtSetErrorInfo(XERR_STATE,"batch-test",99,"Expected ambient diagnostic tests taken ownership.");bool taken=false;int status=xuiCodeDocumentBatchAccept(current,job.payload,&taken);CHECK(status!=XUI_OK && taken);job.payload=NULL;xrtClearError();
    bool applied=false;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_OK && applied);CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);return 1;
}
static int exhaustedCaptureIdentity(void)
{
    CHECK(createDocs());size_t baseline=active;uint64 saved=xrtAtomic64Load(&__xuiDocumentBatchSerial,XMEMORY_RELAXED);xrtAtomic64Store(&__xuiDocumentBatchSerial,UINT64_MAX,XMEMORY_RELAXED);
    xui_code_document_batch_item_t rows[]={item(docs[0],before[0],after[0]),item(docs[1],before[1],after[1])};
    CHECK(xuiCodeDocumentBatchCapture(rows,2,0,&current)==XUI_ERROR_LIMIT_EXCEEDED && !current && active==baseline && unchanged());CHECK(xrtAtomic64Load(&__xuiDocumentBatchSerial,XMEMORY_RELAXED)==UINT64_MAX);
    xrtAtomic64Store(&__xuiDocumentBatchSerial,saved,XMEMORY_RELAXED);clearDocs();CHECK(!active);return 1;
}
int main(int argc,char** argv)
{
    if(argc>2 && !strcmp(argv[1],"capture")){if(!coldCapture((unsigned)strtoul(argv[2],NULL,10)))return 1;}
    else{
        unsigned gate=argc>1?(unsigned)strtoul(argv[1],NULL,10):0;if(!coldStage(gate))return 1;
        if(!gate && (!acceptedAndQuota() || !changedWhileStaging(0) || !changedWhileStaging(1) || !staleCaptureAndBody() || !hostHooks() || !takenOnCleanupFailure() || !exhaustedCaptureIdentity()))return 1;
    }
    printf("xui_batch_worker_test passed: %u checks; real worker without GUI links, independent payload life, cold refusal/cancellation, no-allocation accept, stale capture/body/gap/version/close, host publication gates and taken-on-cleanup ownership\n",checks);return 0;
}
