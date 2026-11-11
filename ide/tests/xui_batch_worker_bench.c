/* Preparation-cost screen, not an IDE input/latency or whole LSP benchmark. */
#include "xui.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static LARGE_INTEGER frequency;
static double now(void){LARGE_INTEGER value;QueryPerformanceCounter(&value);return (double)value.QuadPart*1000.0/(double)frequency.QuadPart;}
typedef struct bench_job {
    xui_code_document_batch_layout_t layout;
    xui_code_document_batch_source_t source;
    xui_code_document_batch_text_t text;
    xui_code_document_batch_payload payload;
    DWORD thread;
    int status;
    double milliseconds;
} bench_job;
static DWORD WINAPI build(void* user)
{
    bench_job* job=user;job->thread=GetCurrentThreadId();double start=now();
    job->status=xuiCodeDocumentBatchStage(&job->layout,&job->source,&job->text,256u*1024u*1024u,NULL,NULL,&job->payload);
    job->milliseconds=now()-start;return 0;
}
int main(int argc,char** argv)
{
    if(argc!=6 || !QueryPerformanceFrequency(&frequency))return 2;
    size_t size=(size_t)strtoull(argv[1],NULL,10);int dense=atoi(argv[2]),split=atoi(argv[3]),commit=atoi(argv[4]),sample=atoi(argv[5]);
    if(!size || size>8u*1024u*1024u)return 2;
    char *before=malloc(size+1),*after=malloc(size+1);xui_code_document doc=NULL;xui_code_document_batch batch=NULL;bench_job job={0};int status=XUI_OK,exitCode=1;
    if(!before || !after)goto cleanup;
    for(size_t i=0;i<size;++i){before[i]=i%80==79?'\n':'a';}before[size]=0;
    if(dense)memset(after,'\n',size);else{memcpy(after,before,size);after[0]='X';}after[size]=0;
    status=xuiCodeDocumentCreate(&doc);if(status==XUI_OK)status=xuiCodeDocumentSetTextLength(doc,before,(int)size);if(status!=XUI_OK)goto cleanup;
    xui_code_document_batch_item_t item={doc,xuiCodeDocumentGetChangeVersion(doc),{before,size},{after,size}};
    double start=now(),prepare=0,capture=0,accept=0,publish=0,retire=0;bool applied=false;
    if(split){
        double a=now();status=xuiCodeDocumentBatchCapture(&item,1,256u*1024u*1024u,&batch);
        if(status==XUI_OK){status=xuiCodeDocumentBatchDescribe(batch,&job.layout,&job.source,1);}capture=now()-a;
        if(status!=XUI_OK)goto cleanup;
        job.text=(xui_code_document_batch_text_t){sizeof(job.text),item.Before,item.After};HANDLE thread=CreateThread(NULL,0,build,&job,0,NULL);
        if(!thread){goto cleanup;}DWORD waited=WaitForSingleObject(thread,30000);if(waited!=WAIT_OBJECT_0)abort();CloseHandle(thread);
        if(job.thread==GetCurrentThreadId() || job.status!=XUI_OK){status=job.status?job.status:XUI_ERROR;goto cleanup;}
        a=now();bool taken=false;status=xuiCodeDocumentBatchAccept(batch,job.payload,&taken);if(taken)job.payload=NULL;accept=now()-a;
    }else{double a=now();status=xuiCodeDocumentBatchPrepare(&item,1,256u*1024u*1024u,&batch);prepare=now()-a;}
    if(status!=XUI_OK)goto cleanup;
    if(commit){double a=now();status=xuiCodeDocumentBatchCommit(batch,&applied);publish=now()-a;if(status!=XUI_OK || !applied)goto cleanup;}
    {double a=now();status=xuiCodeDocumentBatchDestroy(batch);batch=NULL;retire=now()-a;}
    double wall=now()-start;if(status!=XUI_OK)goto cleanup;
    const char* expected=commit?after:before;const char* actual=xuiCodeDocumentGetText(doc);if(!actual || memcmp(actual,expected,size))goto cleanup;
    printf("{\"mode\":\"%s\",\"bytes\":%zu,\"denseLines\":%d,\"commit\":%d,\"sample\":%d,\"prepareMs\":%.6f,\"captureMs\":%.6f,\"workerStageMs\":%.6f,\"acceptMs\":%.6f,\"commitMs\":%.6f,\"destroyMs\":%.6f,\"ownerSdkWorkMs\":%.6f,\"wallMsIncludingTestJoin\":%.6f,\"ownerThread\":%lu,\"stageThread\":%lu}\n",
        split?"split":"synchronous",size,dense,commit,sample,prepare,capture,job.milliseconds,accept,publish,retire,prepare+capture+accept+publish+retire,wall,GetCurrentThreadId(),job.thread);
    exitCode=0;
cleanup:
    if(exitCode)fprintf(stderr,"batch cost screen failed: status=%d bytes=%zu split=%d\n",status,size,split);
    xuiCodeDocumentBatchPayloadDestroy(job.payload);if(batch)xuiCodeDocumentBatchDestroy(batch);if(doc)xuiCodeDocumentDestroy(doc);free(before);free(after);return exitCode;
}
