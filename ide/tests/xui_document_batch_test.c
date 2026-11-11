#include "xui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#ifndef XUI_BATCH_DOCUMENT_SOURCE
#error "Pass the actual matched Document producer source for deterministic allocation testing"
#endif
static unsigned checks,allocationCalls,failAllocation;static int tracking;
static void* allocations[8192];static size_t active;
static void* batchMalloc(size_t bytes)
{
    if(tracking && ++allocationCalls==failAllocation)return NULL;
    void* p=xrtMalloc(bytes);if(p){if(active==8192)abort();allocations[active++]=p;}return p;
}
static void batchFree(void* p)
{
    if(p){size_t i=0;while(i<active && allocations[i]!=p)++i;if(i==active)abort();allocations[i]=allocations[--active];}xrtFree(p);
}
static void* batchRealloc(void* p,size_t bytes)
{
    if(tracking && ++allocationCalls==failAllocation)return NULL;
    size_t i=0;if(p){while(i<active && allocations[i]!=p)++i;if(i==active)abort();}
    void* result=xrtRealloc(p,bytes);if(result){if(p)allocations[i]=result;else{if(active==8192)abort();allocations[active++]=result;}}return result;
}
#define xrtMalloc batchMalloc
#define xrtRealloc batchRealloc
#define xrtFree batchFree
#include XUI_BATCH_DOCUMENT_SOURCE
#undef xrtMalloc
#undef xrtRealloc
#undef xrtFree
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"xui_document_batch_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xui_code_document docs[2];static xui_code_document_batch current;
static int callbackError,noticeCount,prepareCount,mode;
static const char *before[]={"ab","xyz"},*after[]={"A\n中文😀\n","Y\n"};
static xui_code_document_batch_item_t item(xui_code_document doc,const char* previous,const char* next)
{return (xui_code_document_batch_item_t){doc,xuiCodeDocumentGetChangeVersion(doc),{previous,strlen(previous)},{next,strlen(next)}};}
static int createDocs(void)
{
    CHECK(xuiCodeDocumentCreate(docs)==XUI_OK && xuiCodeDocumentCreate(docs+1)==XUI_OK);
    CHECK(xuiCodeDocumentSetText(docs[0],"a")==XUI_OK && xuiCodeDocumentInsert(docs[0],1,"b")==XUI_OK);
    CHECK(xuiCodeDocumentBeginEdit(docs[0])==XUI_OK && xuiCodeDocumentInsert(docs[0],2,"c")==XUI_OK && xuiCodeDocumentEndEdit(docs[0])==XUI_OK && xuiCodeDocumentUndo(docs[0])==XUI_OK);
    CHECK(xuiCodeDocumentSetText(docs[1],before[1])==XUI_OK);
    CHECK(!strcmp(xuiCodeDocumentGetText(docs[0]),before[0]) && xuiCodeDocumentCanUndo(docs[0]) && xuiCodeDocumentCanRedo(docs[0]));
    CHECK(xuiCodeDocumentGetText(docs[1]));return 1;
}
static void clearDocs(void){for(size_t i=0;i<2;++i)if(docs[i]){xuiCodeDocumentDestroy(docs[i]);docs[i]=NULL;}}
static int unchanged(void)
{
    CHECK(!strcmp(xuiCodeDocumentGetText(docs[0]),before[0]) && !strcmp(xuiCodeDocumentGetText(docs[1]),before[1]));
    CHECK(xuiCodeDocumentCanUndo(docs[0]) && xuiCodeDocumentCanRedo(docs[0]) && !xuiCodeDocumentGetDirty(docs[1]));return 1;
}
static int preflight(xui_code_document doc,int start,int end,const char* text,int bytes,bool reset,void* user)
{
    (void)doc;(void)start;(void)end;(void)text;(void)bytes;(void)reset;(void)user;++prepareCount;
    if(xuiCodeDocumentInsert(docs[1],0,"forbidden")!=XUI_ERROR_BUSY)callbackError=1;
    if(xuiCodeDocumentBatchDestroy(current)!=XUI_ERROR_BUSY)callbackError=1;
    bool applied=true;if(xuiCodeDocumentBatchCommit(current,&applied)!=XUI_ERROR_BUSY || applied)callbackError=1;
    if(mode==1)return XUI_ERROR_OUT_OF_MEMORY;
    if(mode==2 && docs[1]){bool consumed=false;if(xuiCodeDocumentDestroyOwnerChecked(docs[1],&consumed)!=XUI_OK || !consumed)callbackError=1;docs[1]=NULL;}
    if(mode==4 && docs[0]){bool consumed=false;if(xuiCodeDocumentDestroyOwnerChecked(docs[0],&consumed)!=XUI_OK || !consumed)callbackError=1;docs[0]=NULL;}
    return XUI_OK;
}
static void changed(xui_code_document doc,xui_code_range_t range,uint32_t version,void* user)
{
    (void)doc;(void)range;(void)version;(void)user;++noticeCount;
    for(size_t i=0;i<2;++i)if(docs[i] && strcmp(xuiCodeDocumentGetText(docs[i]),after[i]))callbackError=1;
    if(xuiCodeDocumentBatchDestroy(current)!=XUI_ERROR_BUSY)callbackError=1;
    if(docs[1] && xuiCodeDocumentInsert(docs[1],0,"forbidden")!=XUI_ERROR_BUSY)callbackError=1;
    if(mode==3 && docs[1]){bool consumed=false;if(xuiCodeDocumentDestroyOwnerChecked(docs[1],&consumed)!=XUI_OK || !consumed)callbackError=1;docs[1]=NULL;}
}
static int preparePair(void)
{xui_code_document_batch_item_t rows[]={item(docs[0],before[0],after[0]),item(docs[1],before[1],after[1])};return xuiCodeDocumentBatchPrepare(rows,2,0,&current);}
static int coldFault(unsigned gate)
{
    CHECK(createDocs());size_t baseline=active;allocationCalls=0;failAllocation=gate;tracking=1;int status=preparePair();tracking=0;
    if(gate){CHECK(allocationCalls>=gate && status==XUI_ERROR_OUT_OF_MEMORY && !current && active==baseline);CHECK(unchanged());}
    else{
        CHECK(status==XUI_OK && current);unsigned observed=allocationCalls;size_t exact=current->used;
        CHECK(unchanged());CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;CHECK(active==baseline);
        xui_code_document_batch_item_t rows[]={item(docs[0],before[0],after[0]),item(docs[1],before[1],after[1])};
        CHECK(xuiCodeDocumentBatchPrepare(rows,2,exact-1,&current)==XUI_ERROR_OUT_OF_MEMORY && !current && active==baseline);
        CHECK(xuiCodeDocumentBatchPrepare(rows,2,exact,&current)==XUI_OK && current);CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
        printf("batch allocation coverage: %u cold gates; exact known storage %zu bytes\n",observed,exact);
    }
    clearDocs();CHECK(!active);return 1;
}
static int publication(void)
{
    CHECK(createDocs());noticeCount=callbackError=prepareCount=mode=0;CHECK(preparePair()==XUI_OK);
    CHECK(xuiInternalCodeDocumentSetEditObserverChecked(docs[0],NULL,preflight,NULL,NULL)==XUI_OK);
    CHECK(xuiCodeDocumentSetChangeCallback(docs[0],changed,NULL)==XUI_OK && xuiCodeDocumentSetChangeCallback(docs[1],changed,NULL)==XUI_OK);
    allocationCalls=0;failAllocation=1;tracking=1;bool applied=false;int status=xuiCodeDocumentBatchCommit(current,&applied);tracking=0;
    CHECK(status==XUI_OK && applied && !allocationCalls && !callbackError && prepareCount==1 && noticeCount==2);
    CHECK(xuiCodeDocumentGetLineCount(docs[0])==3 && xuiCodeDocumentGetLineCount(docs[1])==2);
    CHECK(!xuiCodeDocumentCanRedo(docs[0]) && xuiCodeDocumentGetDirty(docs[0]) && xuiCodeDocumentGetDirty(docs[1]));
    applied=true;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_ERROR_INVALID_STATE && !applied);
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
    CHECK(xuiInternalCodeDocumentSetEditObserverChecked(docs[0],NULL,NULL,NULL,NULL)==XUI_OK);
    CHECK(xuiCodeDocumentSetChangeCallback(docs[0],NULL,NULL)==XUI_OK && xuiCodeDocumentSetChangeCallback(docs[1],NULL,NULL)==XUI_OK);
    CHECK(xuiCodeDocumentUndo(docs[0])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[0]),before[0]));
    CHECK(xuiCodeDocumentUndo(docs[0])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[0]),"a") && !xuiCodeDocumentGetDirty(docs[0]));
    CHECK(xuiCodeDocumentRedo(docs[0])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[0]),before[0]));
    CHECK(xuiCodeDocumentRedo(docs[0])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[0]),after[0]));
    CHECK(xuiCodeDocumentUndo(docs[1])==XUI_OK && !strcmp(xuiCodeDocumentGetText(docs[1]),before[1]) && !xuiCodeDocumentGetDirty(docs[1]));
    clearDocs();CHECK(!active);return 1;
}
static int callbackFailures(void)
{
    for(int scenario=1;scenario<=4;++scenario){
        CHECK(createDocs());mode=scenario;noticeCount=callbackError=prepareCount=0;CHECK(preparePair()==XUI_OK);
        CHECK(xuiInternalCodeDocumentSetEditObserverChecked(docs[scenario==4?1:0],NULL,preflight,NULL,NULL)==XUI_OK);
        CHECK(xuiCodeDocumentSetChangeCallback(docs[0],changed,NULL)==XUI_OK);
        bool applied=true;int status=xuiCodeDocumentBatchCommit(current,&applied);CHECK(status!=XUI_OK && !callbackError);
        if(scenario==4){CHECK(!applied && !docs[0] && !strcmp(xuiCodeDocumentGetText(docs[1]),before[1]) && !noticeCount);}
        else if(scenario==3){CHECK(applied && !strcmp(xuiCodeDocumentGetText(docs[0]),after[0]) && !docs[1] && noticeCount==1);}
        else{CHECK(!applied && !strcmp(xuiCodeDocumentGetText(docs[0]),before[0]) && !noticeCount && xuiCodeDocumentCanRedo(docs[0]));if(scenario==1)CHECK(unchanged());}
        CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);
    }
    mode=0;return 1;
}
typedef struct wrong_thread {xui_code_document_batch batch;int commit,destroy;bool applied;} wrong_thread;
static DWORD WINAPI otherThread(void* data)
{wrong_thread* s=data;s->applied=true;s->commit=xuiCodeDocumentBatchCommit(s->batch,&s->applied);s->destroy=xuiCodeDocumentBatchDestroy(s->batch);return 0;}
static int staleAndLifetime(void)
{
    CHECK(createDocs());CHECK(preparePair()==XUI_OK);wrong_thread other={.batch=current};HANDLE thread=CreateThread(NULL,0,otherThread,&other,0,NULL);CHECK(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread));
    CHECK(other.commit==XUI_ERROR_BUSY && other.destroy==XUI_ERROR_BUSY && !other.applied && unchanged());
    CHECK(xuiCodeDocumentReplace(docs[1],0,1,"Z")==XUI_OK);bool applied=true;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_ERROR_INVALID_STATE && !applied && !strcmp(xuiCodeDocumentGetText(docs[0]),before[0]));
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);
    CHECK(createDocs());CHECK(preparePair()==XUI_OK);xuiCodeDocumentDestroy(docs[1]);docs[1]=NULL;applied=true;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_ERROR_INVALID_STATE && !applied && !strcmp(xuiCodeDocumentGetText(docs[0]),before[0]));
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);return 1;
}
static int shapesAndCapacity(void)
{
    CHECK(createDocs());xui_code_document_batch_item_t rows[]={item(docs[0],before[0],after[0]),item(docs[0],before[0],after[0])};
    CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)==XUI_ERROR_INVALID_ARGUMENT && !current && unchanged());
    rows[1]=item(docs[1],before[1],after[1]);rows[1].Before=xrtStrView("different");CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)==XUI_ERROR_INVALID_STATE && !current && unchanged());
    const char* invalid[]={"\xff","a\rb"};for(size_t i=0;i<2;++i){rows[1]=item(docs[1],before[1],invalid[i]);CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)!=XUI_OK && !current && unchanged());}
    rows[1]=item(docs[1],before[1],after[1]);const char zero[]={'x',0,'y'};rows[1].After=(xstrview){zero,3};CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)==XUI_ERROR_INVALID_ARGUMENT && !current && unchanged());
    CHECK(xuiCodeDocumentBeginEdit(docs[1])==XUI_OK);CHECK(preparePair()==XUI_ERROR_INVALID_STATE && !current);CHECK(xuiCodeDocumentEndEdit(docs[1])==XUI_OK && unchanged());
    CHECK(xuiCodeDocumentBatchPrepare(NULL,0,0,&current)==XUI_OK);bool applied=false;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_OK && applied);CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
    rows[0]=item(docs[0],before[0],"");rows[1]=item(docs[1],before[1],before[1]);uint32_t version=xuiCodeDocumentGetChangeVersion(docs[1]);CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)==XUI_OK);CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_OK && applied && !*xuiCodeDocumentGetText(docs[0]) && xuiCodeDocumentGetLineCount(docs[0])==1 && xuiCodeDocumentGetChangeVersion(docs[1])==version);CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;clearDocs();CHECK(!active);
    xui_code_document_batch_item_t many[513]={0};for(size_t i=0;i<512;++i){CHECK(xuiCodeDocumentCreate(&many[i].pDocument)==XUI_OK);many[i].Before=many[i].After=(xstrview){"",0};}
    CHECK(xuiCodeDocumentBatchPrepare(many,513,0,&current)==XUI_ERROR_INVALID_ARGUMENT && !current);CHECK(xuiCodeDocumentBatchPrepare(many,512,0,&current)==XUI_OK);CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_OK && applied);CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
    for(size_t i=0;i<512;++i){xuiCodeDocumentDestroy(many[i].pDocument);}CHECK(!active);return 1;
}
int main(int argc,char** argv)
{
    unsigned gate=argc>1?(unsigned)strtoul(argv[1],NULL,10):0;
    if(!coldFault(gate))return 1;
    if(!gate && (!publication() || !callbackFailures() || !staleAndLifetime() || !shapesAndCapacity()))return 1;
    printf("xui_document_batch_test passed: %u checks; atomic observer publication, zero core commit allocations, prior undo/redo, cold refusal, versions/lifetime/reentry/owner-close, budget, Unicode/line indexes and512 rows\n",checks);return 0;
}
