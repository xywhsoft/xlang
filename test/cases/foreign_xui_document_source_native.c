/* Independent native source-patch and allocator conformance, not a wrapper. */
#include "tests/xui/xui_code_margin_host.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct PatchBudget {long remaining;size_t live,bytes,failures;} PatchBudget;
typedef union PatchBlock {max_align_t alignment;size_t bytes;} PatchBlock;
static void* patchAllocate(void* raw,size_t bytes)
{
    PatchBudget* b=raw;
    if(!b->remaining){++b->failures;return NULL;}
    if(b->remaining>0)--b->remaining;
    assert(bytes<=SIZE_MAX-sizeof(PatchBlock));
    PatchBlock* block=malloc(sizeof(*block)+bytes);if(!block)return NULL;
    block->bytes=bytes;++b->live;b->bytes+=bytes;return block+1;
}
static void patchFree(void* raw,void* value)
{
    if(!value)return;
    PatchBudget* b=raw;PatchBlock* block=(PatchBlock*)value-1;
    assert(b->live && b->bytes>=block->bytes);--b->live;b->bytes-=block->bytes;free(block);
}
static xui_document patchDocument(PatchBudget* budget)
{
    xui_doc_desc_t desc={.iSize=sizeof(desc),.iProfile=XUI_DOCUMENT_MARKDOWN};
    xui_document result=NULL;
    if(budget){desc.onAlloc=patchAllocate;desc.onFree=patchFree;desc.pAllocatorUser=budget;
        assert(xuiDocumentCreate(&desc,&result)==XUI_OK);}
    else assert(xuiDocumentCreateOwned(&desc,&result)==XUI_OK);
    assert(xuiDocumentLoadMarkdown(result,"alpha beta\n",11)==XUI_OK);return result;
}
static xui_doc_source_patch_t patch(uint64_t from,uint64_t to,const char* text,uint64_t size)
{return (xui_doc_source_patch_t){.iSize=sizeof(xui_doc_source_patch_t),.iStart=from,.iEnd=to,.sText=text,.iTextBytes=size};}
static xui_doc_prepare_info_t patchInfo(xui_document_prepare p)
{xui_doc_prepare_info_t info={.iSize=sizeof(info)};assert(xuiDocumentPrepareGetInfo(p,&info)==XUI_OK);return info;}
static void patchSource(xui_document d,const char* expected,size_t size)
{
    xui_document_snapshot snapshot=NULL;char bytes[64];memset(bytes,0x7e,sizeof(bytes));assert(size<sizeof(bytes));
    assert(xuiDocumentAcquireSnapshot(d,&snapshot)==XUI_OK);
    assert(xuiDocumentSnapshotReadSource(snapshot,0,bytes,size)==XUI_OK);
    assert(!memcmp(bytes,expected,size) && bytes[size]==0x7e);xuiDocumentSnapshotRelease(snapshot);
}
static void patchPublish(xui_document d,xui_document_prepare p,const char* expected,size_t bytes,uint64_t operations)
{
    xui_document_change_set changes=NULL;assert(xuiDocumentPrepareRun(p)==XUI_OK);
    assert(xuiDocumentPreparePublish(d,p,&changes)==XUI_OK);
    xui_doc_change_info_t info={.iSize=sizeof(info)};
    assert(xuiDocumentChangeSetGetInfo(changes,&info)==XUI_OK && info.iOrigin==73 && info.iOperationCount==operations);
    xuiDocumentChangeSetRelease(changes);patchSource(d,expected,bytes);
    assert(xuiDocumentUndo(d,&changes)==XUI_OK);xuiDocumentChangeSetRelease(changes);patchSource(d,"alpha beta\n",11);
    assert(xuiDocumentRedo(d,&changes)==XUI_OK);xuiDocumentChangeSetRelease(changes);patchSource(d,expected,bytes);
}
static void owned(void)
{
    xui_document d=patchDocument(NULL);xui_document_prepare first=NULL,next=NULL,refused=NULL;
    xui_doc_txn_desc_t options={.iSize=sizeof(options),.iDomain=XUI_DOC_SOURCE,.iOrigin=73,.iGroup=19};
    char firstText[]="ALPHA";xui_doc_source_patch_t rows[2]={patch(0,5,firstText,5),patch(6,10,"BETA",4)};
    assert(xuiDocumentPrepareSource(d,&options,rows,2,&first)==XUI_OK);
    memset(firstText,'?',5);assert(patchInfo(first).iPatchCount==2);
    xui_doc_source_patch_t invalid=patch(9,2,"bad",3);
    assert(xuiDocumentPrepareContinueSource(d,first,&invalid,1,&refused)==XUI_ERROR_INVALID_ARGUMENT && !refused);
    assert(!patchInfo(first).bCancellationRequested && patchInfo(first).iPatchCount==2);
    xui_doc_source_patch_t last=patch(6,10,"\xe6\xb1\x89\xe5\xad\x97",6);
    assert(xuiDocumentPrepareContinueSource(d,first,&last,1,&next)==XUI_OK && patchInfo(next).iPatchCount==3);
    assert(xuiDocumentPrepareContinueSource(d,first,&last,1,&refused)==XUI_DOC_ERROR_STALE && !refused);
    xuiDocumentPrepareRelease(first);patchPublish(d,next,"ALPHA \xe6\xb1\x89\xe5\xad\x97\n",13,3);
    xuiDocumentPrepareRelease(next);assert(xuiDocumentReleaseChecked(d)==XUI_OK);
}
static void budgeted(void)
{
    unsigned prefixes=0,failures=0;bool succeeded=false;
    for(int remaining=0;remaining<256;++remaining){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);PatchBudget budget={.remaining=-1};
        xui_document d=patchDocument(&budget);xui_document_prepare first=NULL,next=NULL;
        xui_doc_txn_desc_t options={.iSize=sizeof(options),.iDomain=XUI_DOC_SOURCE,.iOrigin=73,.iGroup=19};
        xui_doc_source_patch_t begin=patch(0,1,"A",1);
        assert(xuiDocumentPrepareSource(d,&options,&begin,1,&first)==XUI_OK);xui_doc_prepare_info_t prior=patchInfo(first);
        xui_doc_source_patch_t rows[2]={patch(1,5,"LPHA",4),patch(6,10,"BETA",4)};
        budget.remaining=remaining;int status=xuiDocumentPrepareContinueSource(d,first,rows,2,&next);
        bool failed=budget.failures!=0;budget.remaining=-1;xrtClearError();++prefixes;
        if(status!=XUI_OK){
            ++failures;assert(status==XUI_ERROR_OUT_OF_MEMORY && !next);
            xui_doc_prepare_info_t current=patchInfo(first);
            assert(current.iGeneration==prior.iGeneration && current.iPatchCount==1 && !current.bCancellationRequested);
            assert(xuiDocumentPrepareContinueSource(d,first,rows,2,&next)==XUI_OK);
        }
        assert(next && patchInfo(next).iPatchCount==3);patchPublish(d,next,"ALPHA BETA\n",11,3);
        xuiDocumentPrepareRelease(next);xuiDocumentPrepareRelease(first);xuiDocumentRelease(d);
        assert(!budget.live && !budget.bytes);xrtMemDebugSnapshot(&after);
        assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
        assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
        if(!failed){assert(status==XUI_OK);succeeded=true;break;}
    }
    assert(succeeded && failures>0);
    printf("Document native patches allocation prefixes=%u, failures=%u, diagnostic allocator payloads balanced\n",prefixes,failures);
}
int main(void)
{
    marginBind();owned();budgeted();
    puts("Document native patches: source copy, candidate retention, retry, metadata and one-step undo passed");return 0;
}
