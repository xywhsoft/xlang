#define main legacyMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/ast_clone.h"
static size_t faultSeen,faultAt,faultDenied,tracked;
static int faultActive;
static void* allocations[16384];
static int refuse(void){if(faultActive && ++faultSeen==faultAt){++faultDenied;return 1;}return 0;}
static void remember(void* p){if(!p || !faultActive)return;for(size_t i=0;i<16384;++i)if(allocations[i]==p)return;for(size_t i=0;i<16384;++i)if(!allocations[i]){allocations[i]=p;++tracked;return;}}
static void forget(void* p){if(!p)return;for(size_t i=0;i<16384;++i)if(allocations[i]==p){allocations[i]=NULL;--tracked;return;}}
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);void* __real_xrtRealloc(void*,size_t);void __real_xrtFree(void*);
bool __real_xrtArrayPush(xarray*,const void*);bool __real_xrtArrayResize(xarray*,size_t);bool __real_xrtArrayReserve(xarray*,size_t);void __real_xrtArrayUnit(xarray*);
bool __real_xrtStrBufReserve(xstrbuf*,size_t);void __real_xrtStrBufFree(xstrbuf*);bool __real_xrtMapSet(xmap*,xbytesview,const void*);
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p);return p;}
void* __wrap_xrtCalloc(size_t n,size_t s){if(refuse())return NULL;void* p=__real_xrtCalloc(n,s);remember(p);return p;}
void* __wrap_xrtRealloc(void* p,size_t n){if(refuse())return NULL;void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q);}return q;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
bool __wrap_xrtArrayPush(xarray* a,const void* p){if(refuse())return false;void* old=a->Data;bool r=__real_xrtArrayPush(a,p);if(a->Data!=old){forget(old);remember(a->Data);}return r;}
bool __wrap_xrtArrayResize(xarray* a,size_t n){if(refuse())return false;void* old=a->Data;bool r=__real_xrtArrayResize(a,n);if(a->Data!=old){forget(old);remember(a->Data);}return r;}
bool __wrap_xrtArrayReserve(xarray* a,size_t n){if(refuse())return false;void* old=a->Data;bool r=__real_xrtArrayReserve(a,n);if(a->Data!=old){forget(old);remember(a->Data);}return r;}
void __wrap_xrtArrayUnit(xarray* a){forget(a->Data);__real_xrtArrayUnit(a);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){if(refuse())return false;void* old=b->Data;bool r=__real_xrtStrBufReserve(b,n);if(b->Data!=old){forget(old);remember(b->Data);}return r;}
void __wrap_xrtStrBufFree(xstrbuf* b){forget(b->Data);__real_xrtStrBufFree(b);}
bool __wrap_xrtMapSet(xmap* m,xbytesview k,const void* v){if(refuse())return false;return __real_xrtMapSet(m,k,v);}
static int shift(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{(void)user;if(in->line>0)out->line=in->line+2;return 1;}
typedef struct persistent_reader {const XL6_AstNode* root;const xls_ir_result* before;const char* expected;int okay;} persistent_reader;
static int32 persistentRead(void* opaque)
{
    persistent_reader* input=opaque;XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* result=xlsFrontendIrResultBuildPersistent(input->root,&diags,input->before,32u*1024u*1024u,128u*1024u,32u*1024u*1024u);
    char* text=result ? dump(xlsFrontendIrResultModule(result)) : NULL;
    input->okay=result && !diags.count && xlsFrontendIrResultPersistentWork(result).reused && text && !strcmp(text,input->expected);
    xrtFree(text);xlsFrontendIrResultDestroy(result);xl6DiagFree(&diags);return input->okay ? 0 : 1;
}
static int stopImmediately(void* opaque){(void)opaque;return 1;}
static int failures(void)
{
    char* path=xrtPathAbs("persistent-fault-中文.xl");CHECK(path);char* text=body(0);CHECK(text);
    xls_semantic_check* source=checked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
    const XL6_AstNode* root=xlsSemanticCheckAst(source);xls_ast_clone_options options={.transform=shift,.bytes=64u*1024u*1024u};XL6_AstNode* moved=xlsAstClone(root,&options,NULL);CHECK(moved);
    XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* before=xlsFrontendIrResultBuildPersistent(root,&diags,NULL,32u*1024u*1024u,128u*1024u,32u*1024u*1024u);CHECK(before && !diags.count && xlsFrontendIrResultPersistentWork(before).captured);
    char* held=dump(xlsFrontendIrResultModule(before));CHECK(held);
    faultSeen=faultDenied=faultAt=0;faultActive=1;
    xls_ir_result* warm=xlsFrontendIrResultBuildPersistent(moved,&diags,before,32u*1024u*1024u,128u*1024u,32u*1024u*1024u);faultActive=0;
    CHECK(warm && !diags.count && xlsFrontendIrResultPersistentWork(warm).reused);size_t sites=faultSeen;CHECK(sites>10 && sites<10000);
    char* expected=dump(xlsFrontendIrResultModule(warm));CHECK(expected);xlsFrontendIrResultDestroy(warm);CHECK(!tracked);
    for(size_t i=1;i<=sites;++i){
        faultSeen=faultDenied=0;faultAt=i;faultActive=1;
        xls_ir_result* result=xlsFrontendIrResultBuildPersistent(moved,&diags,before,32u*1024u*1024u,128u*1024u,32u*1024u*1024u);faultActive=0;
        if(!result || diags.count || tracked>10000)fprintf(stderr,"failure site=%zu seen=%zu denied=%zu live=%zu result=%p diagnostics=%zu\n",i,faultSeen,faultDenied,tracked,(void*)result,diags.count);
        CHECK(faultDenied==1 && result && !diags.count);
        char* actual=dump(xlsFrontendIrResultModule(result));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);xlsFrontendIrResultDestroy(result);CHECK(!tracked);
        char* unchanged=dump(xlsFrontendIrResultModule(before));CHECK(unchanged && !strcmp(unchanged,held));xrtFree(unchanged);
    }
    xls_frontend_cancel_scope scope;xls_frontend_cancel_work work;xlsFrontendCancelBegin(&scope,stopImmediately,NULL);
    xls_ir_result* cancelled=xlsFrontendIrResultBuildPersistent(moved,&diags,before,32u*1024u*1024u,128u*1024u,32u*1024u*1024u);
    xlsFrontendCancelEnd(&scope,&work);CHECK(!cancelled && work.cancelled && !diags.count);
    persistent_reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(persistent_reader){moved,before,expected,0};threads[i]=xrtThreadCreate(persistentRead,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    xlsFrontendIrResultDestroy(before);xlsSemanticCheckDestroy(source);xl6AstFree(moved);xrtFree(held);xrtFree(expected);xrtFree(text);xrtFree(path);xl6DiagFree(&diags);CHECK(!tracked);
    printf("persistent fault sites=%zu;tracked allocations retired;4concurrent views;cancel andheld source fences passed.\n",sites);return 1;
}
int main(void)
{xrtAtomic32Init(&checks,0);if(!failures())return 1;printf("analysis_ir_persistent_fault_test passed: %u checks.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
