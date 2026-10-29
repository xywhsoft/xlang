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

#include "analysis/typed_module_internal.h"
static XL6_AstNode* watchedRoot;
static unsigned rootDrops;
void __real_xl6AstFree(XL6_AstNode*);
void __wrap_xl6AstFree(XL6_AstNode* root)
{if(root && root==watchedRoot)++rootDrops;__real_xl6AstFree(root);}
static int typedFailures(void)
{
    char* path=xrtPathAbs("typed-fault-中文.xl");CHECK(path);
    const char* text="func helper(int value) as int\n int total=value+1\n if total>2 then\n total=total+value\n end\n return total\nend\nfunc main(...) as int\n return helper(7)\nend\n";
    xls_semantic_check* source=checked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
    char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);size_t sites=0;
    for(size_t i=0;i<=sites;++i){
        xls_ast_clone_options options={.bytes=8u*1024u*1024u};XL6_AstNode* root=xlsAstClone(xlsSemanticCheckAst(source),&options,NULL);CHECK(root);
        watchedRoot=root;rootDrops=0;
        faultSeen=faultDenied=0;faultAt=1;faultActive=1;xls_typed_module* input=xlsTypedModuleTake(root);faultActive=0;
        CHECK(!input && faultDenied==1 && !rootDrops && !tracked);
        input=xlsTypedModuleTake(root);CHECK(input);
        XL6_DiagList diags;xl6DiagInit(&diags);
        xls_ir_result* result=xlsFrontendIrResultBuildTyped(input,&diags,NULL,32u*1024u*1024u,NULL,0);CHECK(result && !diags.count);
        xlsTypedModuleRelease(input);CHECK(!rootDrops);
        xls_ir_persistent_work cold=xlsFrontendIrResultPersistentWork(result);
        faultSeen=faultDenied=0;faultAt=i;faultActive=1;
        int prepared=xlsFrontendIrResultPreparePersistent(result,32u*1024u*1024u);faultActive=0;
        if(!i){sites=faultSeen;CHECK(prepared==2 && sites>10 && sites<2000);}
        else CHECK(faultDenied==1 && (prepared==0 || prepared==2));
        if(!prepared){CHECK(!rootDrops && !tracked);CHECK(xlsFrontendIrResultPreparePersistent(result,32u*1024u*1024u)==2);}
        CHECK(rootDrops==1);watchedRoot=NULL;
        xls_ir_persistent_work now=xlsFrontendIrResultPersistentWork(result);CHECK(!memcmp(&cold,&now,sizeof(cold)));
        char* actual=dump(xlsFrontendIrResultModule(result));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);
        xlsFrontendIrResultDestroy(result);xl6DiagFree(&diags);CHECK(!tracked);
    }
    printf("typed prepare fault sites=%zu;all transferred allocations retired;Take failure leaves root owned;prepare retries;unchanged creation provenance.\n",sites);
    xrtFree(expected);xlsSemanticCheckDestroy(source);xrtFree(path);return 1;
}
int main(void)
{xrtAtomic32Init(&checks,0);if(!typedFailures())return 1;printf("analysis_typed_module_fault_test passed:%u checks.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
