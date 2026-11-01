/* Own only this collector's public allocation and XRT buffer entries. This
 * deliberately does not claim visibility into XRT pools or the global heap. */
#include "analysis/frontend_index.h"
#include "analysis/frontend_type_arena.h"
#include "src/type/xl6_type_model.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"arena_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int gate,tracking,badTracker;static size_t attempts,failAt,reserveCalls;
static void* ownedBlocks[10000];static size_t ownedBlockCount;
typedef struct owned_buffer {xstrbuf* object;char* data;} owned_buffer;
static owned_buffer ownedBuffers[16];static size_t ownedBufferCount;
static size_t blockSlot(void* p){for(size_t i=0;i<ownedBlockCount;++i)if(ownedBlocks[i]==p)return i;return SIZE_MAX;}
static void blockAdd(void* p){if(p && tracking){if(ownedBlockCount==10000 || blockSlot(p)!=SIZE_MAX)badTracker=1;else ownedBlocks[ownedBlockCount++]=p;}}
static void blockRemove(void* p){size_t i=blockSlot(p);if(i!=SIZE_MAX)ownedBlocks[i]=ownedBlocks[--ownedBlockCount];}
static void bufferUpdate(xstrbuf* b)
{
    if(!tracking || !b->Data)return;
    for(size_t i=0;i<ownedBufferCount;++i)if(ownedBuffers[i].object==b){if(ownedBuffers[i].data!=b->Data)blockRemove(ownedBuffers[i].data);ownedBuffers[i].data=b->Data;return;}
    if(ownedBufferCount==16)badTracker=1;else ownedBuffers[ownedBufferCount++]=(owned_buffer){b,b->Data};
}
static int refuse(void){return gate && ++attempts==failAt;}
void* __real_xrtCalloc(size_t,size_t);void* __real_xrtRealloc(void*,size_t);
bool __real_xrtStrBufAppend(xstrbuf*,xstrview);bool __real_xrtStrBufReserve(xstrbuf*,size_t);void __real_xrtStrBufFree(xstrbuf*);void __real_xrtFree(void*);
void* __wrap_xrtCalloc(size_t n,size_t size){if(refuse())return NULL;void* p=__real_xrtCalloc(n,size);blockAdd(p);return p;}
void* __wrap_xrtRealloc(void* p,size_t size)
{
    if(refuse())return NULL;
    size_t slot=blockSlot(p);void* next=__real_xrtRealloc(p,size);
    if(next && tracking){if(slot!=SIZE_MAX)ownedBlocks[slot]=next;else blockAdd(next);}
    return next;
}
void __wrap_xrtFree(void* p){if(p && tracking)blockRemove(p);__real_xrtFree(p);}
bool __wrap_xrtStrBufAppend(xstrbuf* b,xstrview v)
{if(gate==1 && refuse())return false;bool okay=__real_xrtStrBufAppend(b,v);bufferUpdate(b);return okay;}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t size)
{if(gate)++reserveCalls;if(gate==1 && refuse())return false;bool okay=__real_xrtStrBufReserve(b,size);bufferUpdate(b);return okay;}
void __wrap_xrtStrBufFree(xstrbuf* b)
{
    if(b && tracking){
        for(size_t i=0;i<ownedBufferCount;++i)if(ownedBuffers[i].object==b){if(ownedBuffers[i].data!=b->Data)badTracker=1;ownedBuffers[i]=ownedBuffers[--ownedBufferCount];break;}
        blockRemove(b->Data);
    }
    __real_xrtStrBufFree(b);
}
static int appendCases(void)
{
    const char* names[]={"int64","float64","Queue","类型😀",""};
    const size_t limits[]={1,2,3,8,8191,8192};
    for(size_t n=0;n<sizeof(names)/sizeof(*names);++n){
        XL6_AstNode* type=xl6AstNewText(XL6_AST_TYPE,names[n],strlen(names[n]),NULL);CHECK(type);
        for(size_t j=0;j<sizeof(limits)/sizeof(*limits);++j){
            xstrbuf arena={0};CHECK(xrtStrBufAppend(&arena,xrtStrView("seed")));size_t offset=SIZE_MAX,length=SIZE_MAX;
            char* normal=xl6TypeToCanonicalStringLimited(NULL,type,limits[j],128,8192);
            int okay=xlsFrontendTypeCanonicalAppend(NULL,type,limits[j],128,8192,&arena,65536,&offset,&length);
            CHECK(okay==(normal!=NULL) && !memcmp(arena.Data,"seed",4));
            if(normal)CHECK(offset==4 && length==strlen(normal) && arena.Size==offset+length+1 && !strcmp(arena.Data+offset,normal));
            else CHECK(arena.Size==4 && arena.Data[4]==0 && !offset && !length);
            xrtFree(normal);xrtStrBufFree(&arena);
        }
        if(!*names[n]){xstrbuf empty={0};size_t offset,length;CHECK(xlsFrontendTypeCanonicalAppend(NULL,type,8192,128,8192,&empty,65536,&offset,&length) && !offset && !length && empty.Size==1 && empty.Data[0]==0);xrtStrBufFree(&empty);}
        xl6AstFree(type);
    }
    XL6_AstNode* type=xl6AstNewText(XL6_AST_TYPE,"int64",5,NULL);CHECK(type);xstrbuf arena={0};size_t offsets[3000];
    for(size_t i=0;i<3000;++i){size_t length;CHECK(xlsFrontendTypeCanonicalAppend(NULL,type,8192,128,8192,&arena,65536,&offsets[i],&length) && length==3);}
    CHECK(arena.Capacity>64);
    for(size_t i=0;i<3000;++i)CHECK(!strcmp(arena.Data+offsets[i],"int") && (i==0 || offsets[i]>offsets[i-1]));
    size_t size=arena.Size,offset,length;CHECK(!xlsFrontendTypeCanonicalAppend(NULL,type,2,128,8192,&arena,65536,&offset,&length) && arena.Size==size);
    for(size_t i=0;i<3000;++i)CHECK(!strcmp(arena.Data+offsets[i],"int"));
    CHECK(!xlsFrontendTypeCanonicalAppend(NULL,type,8192,0,8192,&arena,65536,&offset,&length));
    CHECK(!xlsFrontendTypeCanonicalAppend(NULL,type,8192,128,0,&arena,65536,&offset,&length));
    CHECK(!xlsFrontendTypeCanonicalAppend(NULL,type,8192,128,8192,&arena,size-1,&offset,&length));
    xrtStrBufFree(&arena);xl6AstFree(type);
    char longName[8195];memset(longName,'x',sizeof(longName)-1);longName[sizeof(longName)-1]=0;
    for(size_t length=8191;length<=8193;++length){
        XL6_AstNode* large=xl6AstNewText(XL6_AST_TYPE,longName,length,NULL);CHECK(large);
        char* normal=xl6TypeToCanonicalStringLimited(NULL,large,8192,128,8192);xstrbuf sink={0};size_t offset,size;
        int okay=xlsFrontendTypeCanonicalAppend(NULL,large,8192,128,8192,&sink,65536,&offset,&size);
        CHECK(okay==(normal!=NULL));if(okay)CHECK(size==length && !strcmp(sink.Data+offset,normal));else CHECK(!sink.Size);
        xrtStrBufFree(&sink);xrtFree(normal);xl6AstFree(large);
    }
    XL6_AstNode* fn=xl6AstNewText(XL6_AST_TYPE,"function",8,NULL),*args=xl6AstNew(XL6_AST_LIST,NULL),*returns=xl6AstNew(XL6_AST_LIST,NULL);CHECK(fn && args && returns);
    for(unsigned i=0;i<12;++i){XL6_AstNode* arg=xl6AstNewText(XL6_AST_TYPE,"long_argument_name",18,NULL);CHECK(arg);xl6AstAddChild(args,arg);}
    XL6_AstNode* result=xl6AstNewText(XL6_AST_TYPE,"int64",5,NULL);CHECK(result);xl6AstAddChild(returns,result);xl6AstAddChild(fn,args);xl6AstAddChild(fn,returns);
    const size_t dimensions[][3]={{8,128,8192},{8192,1,8192},{8192,128,1},{8192,128,8192}};
    for(size_t i=0;i<4;++i){
        xstrbuf sink={0};CHECK(xrtStrBufAppend(&sink,xrtStrView("seed")));size_t offset,size;
        char* normal=xl6TypeToCanonicalStringLimited(NULL,fn,dimensions[i][0],dimensions[i][1],dimensions[i][2]);
        int okay=xlsFrontendTypeCanonicalAppend(NULL,fn,dimensions[i][0],dimensions[i][1],dimensions[i][2],&sink,65536,&offset,&size);
        CHECK(okay==(normal!=NULL) && !memcmp(sink.Data,"seed",4));
        if(okay)CHECK(size==strlen(normal) && !strcmp(sink.Data+offset,normal));else CHECK(sink.Size==4 && sink.Data[4]==0);
        xrtFree(normal);xrtStrBufFree(&sink);
    }
    xstrbuf sink={0};CHECK(xrtStrBufAppend(&sink,xrtStrView("seed")));size_t finalOffset,finalSize;gate=1;attempts=reserveCalls=0;failAt=1;
    int okay=xlsFrontendTypeCanonicalAppend(NULL,fn,8192,128,8192,&sink,65536,&finalOffset,&finalSize);gate=0;
    CHECK(!okay && reserveCalls && sink.Size==4 && !strcmp(sink.Data,"seed"));
    CHECK(xlsFrontendTypeCanonicalAppend(NULL,fn,8192,128,8192,&sink,65536,&finalOffset,&finalSize));xrtStrBufFree(&sink);xl6AstFree(fn);return 1;
}
static XL6_AstNode* typedRoot(size_t count,const char* path)
{
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return NULL;
    for(size_t i=0;i<count;++i){XL6_AstNode* n=xl6AstNewText(XL6_AST_NAME,"value",5,NULL);if(!n){xl6AstFree(root);return NULL;}
        n->inferredType=xl6AstNewText(XL6_AST_TYPE,"int64",5,NULL);if(!n->inferredType){xl6AstFree(n);xl6AstFree(root);return NULL;}
        xl6AstAddChild(root,n);if(root->childCount!=i+1){xl6AstFree(n);xl6AstFree(root);return NULL;}}
    if(!xl6AstSetSourceTree(root,path)){xl6AstFree(root);return NULL;}return root;
}
static int verifyIndex(xls_semantic_index* index,size_t count)
{
    CHECK(index && xlsSemanticIndexCount(index)==count+1);
    const char* first=NULL;
    for(size_t i=1;i<=count;++i){xls_index_fact f;CHECK(xlsSemanticIndexFact(index,i,&f) && f.canonical && !strcmp(f.canonical,"int"));if(i==1)first=f.canonical;}
    for(size_t i=1;i<=count;++i){xls_index_fact f;CHECK(xlsSemanticIndexFact(index,i,&f) && !strcmp(f.canonical,"int"));}
    xls_index_fact f;CHECK(xlsSemanticIndexFact(index,1,&f) && f.canonical==first);return 1;
}
static int indexCases(void)
{
    char* path=xrtPathAbs("arena-owned-index.xl");CHECK(path);XL6_SourceReport report;xl6SourceReportInit(&report);CHECK(xl6SourceReportText(&report,path,"",0,"memory"));
    XL6_AstNode* root=typedRoot(3000,path);CHECK(root);CHECK(!ownedBlockCount && !ownedBufferCount);tracking=1;
    gate=2;attempts=reserveCalls=0;failAt=SIZE_MAX;char error[256];xls_semantic_index* index=xlsFrontendIndexCreate(root,&report,error,sizeof(error));gate=0;
    printf("typed index: public allocator entries=%zu,arena growth calls=%zu,owned public blocks=%zu,owned XRT buffers=%zu,nodes=3000\n",attempts,reserveCalls,ownedBlockCount,ownedBufferCount);
    size_t creation=attempts+reserveCalls;gate=2;attempts=reserveCalls=0;CHECK(verifyIndex(index,3000));gate=0;
    CHECK(!attempts && !reserveCalls && creation<3000/4 && !badTracker && ownedBufferCount==1);
    xlsFrontendIndexDestroy(index);CHECK(!ownedBlockCount && !ownedBufferCount && !badTracker);tracking=0;xl6AstFree(root);
    root=typedRoot(32,path);CHECK(root);tracking=1;gate=1;attempts=reserveCalls=0;failAt=SIZE_MAX;
    index=xlsFrontendIndexCreate(root,&report,error,sizeof(error));gate=0;size_t sites=attempts;CHECK(verifyIndex(index,32) && sites && reserveCalls>=2);xlsFrontendIndexDestroy(index);CHECK(!ownedBlockCount && !ownedBufferCount && !badTracker);
    for(size_t site=1;site<=sites;++site){
        gate=1;attempts=0;failAt=site;index=xlsFrontendIndexCreate(root,&report,error,sizeof(error));gate=0;
        CHECK(!index && *error && !ownedBlockCount && !ownedBufferCount && !badTracker && root->childCount==32);
        index=xlsFrontendIndexCreate(root,&report,error,sizeof(error));CHECK(verifyIndex(index,32));xlsFrontendIndexDestroy(index);CHECK(!ownedBlockCount && !ownedBufferCount && !badTracker);
    }
    printf("owned index: %zu actual refusal sites,all clean and retryable\n",sites);
    tracking=0;xl6AstFree(root);xl6SourceReportFree(&report);xrtFree(path);return 1;
}
int main(void)
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#endif
    int okay=appendCases() && indexCases();printf("%s: analysis_index_arena_test,%u checks\n",okay ? "passed" : "failed",checks);return okay ? 0 : 1;
}
