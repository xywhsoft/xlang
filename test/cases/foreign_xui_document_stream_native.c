/* Real native stream semantics, independent of language marshalling. */
#include "tests/xui/xui_code_margin_host.h"
#include <stdio.h>
#include <stdlib.h>

static xui_document document(void)
{
    const xui_doc_desc_t desc={.iSize=sizeof(desc),.iProfile=XUI_DOCUMENT_MARKDOWN};
    xui_document d=NULL;assert(xuiDocumentCreateOwned(&desc,&d)==XUI_OK);
    assert(xuiDocumentLoadMarkdown(d,"start\n",6)==XUI_OK);return d;
}
static void source(xui_document d,const char* expected,size_t length)
{
    xui_document_snapshot snapshot=NULL;assert(xuiDocumentAcquireSnapshot(d,&snapshot)==XUI_OK);
    char buffer[256];assert(length<sizeof(buffer));memset(buffer,0x7e,sizeof(buffer));
    assert(xuiDocumentSnapshotReadSource(snapshot,0,buffer,length)==XUI_OK);
    assert(!memcmp(buffer,expected,length)&&buffer[length]==0x7e);
    assert(xuiDocumentSnapshotReleaseChecked(snapshot)==XUI_OK);
}
static xui_doc_prepare_info_t info(xui_document_prepare p)
{
    xui_doc_prepare_info_t out={.iSize=sizeof(out)};
    assert(xuiDocumentPrepareGetInfo(p,&out)==XUI_OK);return out;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    if(after.LiveCount!=before->LiveCount||after.LiveBytes!=before->LiveBytes)
        fprintf(stderr,"stream live before=%llu/%llu after=%llu/%llu\n",(unsigned long long)before->LiveCount,(unsigned long long)before->LiveBytes,(unsigned long long)after.LiveCount,(unsigned long long)after.LiveBytes);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes);
    assert(after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount);
}
static void partial(void)
{
    xui_document d=document();uint64_t revision=xuiDocumentGetRevision(d);
    xui_doc_txn_desc_t desc={.iSize=sizeof(desc),.iDomain=XUI_DOC_SOURCE,.iOrigin=73,.iGroup=19};
    xui_document_prepare first=NULL,next=NULL;xui_document_change_set changes=NULL;
    assert(xuiDocumentPrepareStreamSource(d,NULL,&desc,"\xe4\xb8",2,0,&first)==XUI_OK);
    xui_doc_prepare_info_t before=info(first);
    assert(before.iBufferedUtf8Bytes==2&&before.iSourceBytes==6);
    assert(xuiDocumentPrepareRun(first)==XUI_DOC_ERROR_BUSY);
    assert(xuiDocumentPreparePublish(d,first,&changes)==XUI_DOC_ERROR_BUSY&&!changes);
    /* This is the exact invalid call emitted by the current language binding. */
    assert(xuiDocumentPrepareStreamSource(d,first,&desc,"\xad\n",2,1,&next)==XUI_ERROR_INVALID_ARGUMENT&&!next);
    assert(info(first).iGeneration==before.iGeneration&&xuiDocumentGetRevision(d)==revision);
    assert(xuiDocumentPrepareStreamSource(d,first,NULL,"",0,1,&next)==XUI_DOC_ERROR_UTF8&&!next);
    assert(xuiDocumentPrepareStreamSource(d,first,NULL,"x",1,0,&next)==XUI_DOC_ERROR_UTF8&&!next);
    assert(info(first).iBufferedUtf8Bytes==2);
    char tail[]="\xad\n";
    assert(xuiDocumentPrepareStreamSource(d,first,NULL,tail,2,1,&next)==XUI_OK);
    memset(tail,'x',2); /* Native owns an independent source before return. */
    assert(xuiDocumentPrepareReleaseChecked(first)==XUI_OK);
    assert(info(next).iBufferedUtf8Bytes==0&&info(next).iSourceBytes==10);
    assert(xuiDocumentPrepareRun(next)==XUI_OK&&xuiDocumentPreparePublish(d,next,&changes)==XUI_OK);
    xui_doc_change_info_t change={.iSize=sizeof(change)};
    assert(xuiDocumentChangeSetGetInfo(changes,&change)==XUI_OK&&change.iOrigin==73);
    assert(xuiDocumentChangeSetReleaseChecked(changes)==XUI_OK&&xuiDocumentPrepareReleaseChecked(next)==XUI_OK);
    source(d,"start\n\xe4\xb8\xad\n",10);
    assert(xuiDocumentUndo(d,&changes)==XUI_OK&&xuiDocumentChangeSetReleaseChecked(changes)==XUI_OK);
    source(d,"start\n",6);
    assert(xuiDocumentRedo(d,&changes)==XUI_OK&&xuiDocumentChangeSetReleaseChecked(changes)==XUI_OK);
    source(d,"start\n\xe4\xb8\xad\n",10);
    assert(xuiDocumentReleaseChecked(d)==XUI_OK);
}
typedef struct Worker {xui_document_prepare p;int status;} Worker;
static int32 run(void* raw)
{
    Worker* w=raw;w->status=xuiDocumentPrepareRun(w->p);
    assert(xuiDocumentPrepareReleaseChecked(w->p)==XUI_OK);return 0;
}
static void worker(void)
{
    xui_document d=document();xui_document_prepare p=NULL;xui_document_change_set change=NULL;
    assert(xuiDocumentPrepareStreamSource(d,NULL,NULL,"worker\n",7,1,&p)==XUI_OK);
    assert(xuiDocumentPrepareAddRef(p)==XUI_OK);Worker w={p,123};
    xthread* thread=xrtThreadCreate(run,&w,0);assert(thread&&xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(w.status==XUI_OK&&xuiDocumentPreparePublish(d,p,&change)==XUI_OK);
    assert(xuiDocumentChangeSetReleaseChecked(change)==XUI_OK&&xuiDocumentPrepareReleaseChecked(p)==XUI_OK);
    source(d,"start\nworker\n",13);assert(xuiDocumentReleaseChecked(d)==XUI_OK);
}
typedef struct Budget {long remaining;size_t live,bytes,allocations,failures;} Budget;
typedef union Block {max_align_t alignment;size_t bytes;} Block;
static void* allocate(void* raw,size_t size)
{
    Budget* b=raw;
    if(!b->remaining){++b->failures;return NULL;}
    if(b->remaining>0)--b->remaining;
    if(size>SIZE_MAX-sizeof(Block))return NULL;
    Block* block=malloc(sizeof(*block)+size);if(!block)return NULL;
    block->bytes=size;++b->live;b->bytes+=size;++b->allocations;return block+1;
}
static void release(void* raw,void* value)
{
    if(!value)return;
    Budget* b=raw;Block* block=(Block*)value-1;
    assert(b->live&&b->bytes>=block->bytes);--b->live;b->bytes-=block->bytes;free(block);
}
static xui_document diagnosticDocument(Budget* b)
{
    const xui_doc_desc_t desc={.iSize=sizeof(desc),.iProfile=XUI_DOCUMENT_MARKDOWN,.onAlloc=allocate,.onFree=release,.pAllocatorUser=b};
    xui_document d=NULL;assert(xuiDocumentCreate(&desc,&d)==XUI_OK);
    assert(xuiDocumentLoadMarkdown(d,"start\n",6)==XUI_OK);return d;
}
static void allocation(void)
{
    unsigned checked=0,failures=0;bool succeeded=false;
    char tail[16386];memset(tail,'a',sizeof(tail));tail[0]=(char)0xad;tail[1]='\n';
    for(int budget=0;budget<160;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Budget diagnostic={.remaining=-1};
        xui_document d=diagnosticDocument(&diagnostic);
        xui_document_prepare first=NULL,next=NULL;
        assert(xuiDocumentPrepareStreamSource(d,NULL,NULL,"\xe4\xb8",2,0,&first)==XUI_OK);
        xui_doc_prepare_info_t previous=info(first);diagnostic.remaining=budget;
        int status=xuiDocumentPrepareStreamSource(d,first,NULL,tail,sizeof(tail),1,&next);
        bool failed=diagnostic.failures!=0;diagnostic.remaining=-1;xrtClearError();++checked;
        if(status==XUI_OK){assert(next);xuiDocumentPrepareRelease(next);}
        else {++failures;assert(status==XUI_ERROR_OUT_OF_MEMORY&&!next);xui_doc_prepare_info_t after=info(first);
            assert(after.iGeneration==previous.iGeneration&&after.iSourceBytes==6&&after.iBufferedUtf8Bytes==2&&!after.bCancellationRequested);}
        xuiDocumentPrepareRelease(first);xuiDocumentRelease(d);
        assert(!diagnostic.live&&!diagnostic.bytes);balance(&before);
        if(!failed){assert(status==XUI_OK);succeeded=true;break;}
    }
    assert(succeeded&&failures>0);printf("Document native stream allocation prefixes=%u, failures=%u, diagnostic allocator payloads balanced\n",checked,failures);
}
int main(void)
{
    marginBind();xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    partial();balance(&before);worker();balance(&before);allocation();balance(&before);
    puts("Document native stream: NULL continuation descriptor, split UTF-8, failed candidate preservation, source copy, worker Run and one-step undo passed");return 0;
}
