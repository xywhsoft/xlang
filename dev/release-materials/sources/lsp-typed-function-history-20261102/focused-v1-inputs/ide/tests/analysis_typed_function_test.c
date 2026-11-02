#include "analysis/function_unit_internal.h"
#include "analysis_test_clock.h"
#include "analysis/check_session.h"
#include "analysis/ast_package_fields.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_typed_function_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xatomicptr watched[3];static xatomic32 drops[3],cloneCalls;
static int countClones,faultActive,failMutex;
static size_t faultAt,faultCalls,faultDenied,tracked;
static void* allocations[4096];
static xatomic32 probe,attempts,inside,overlap,allowFree;
static int refuse(void){if(faultActive && ++faultCalls==faultAt){++faultDenied;return 1;}return 0;}
static void remember(void* p)
{
    if(!p || !faultActive)return;
    for(size_t i=0;i<4096;++i)if(allocations[i]==p)return;
    for(size_t i=0;i<4096;++i)if(!allocations[i]){allocations[i]=p;++tracked;return;}
    abort();
}
static void forget(void* p)
{if(p)for(size_t i=0;i<4096;++i)if(allocations[i]==p){allocations[i]=NULL;--tracked;return;}}
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);void* __real_xrtRealloc(void*,size_t);void __real_xrtFree(void*);
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p);return p;}
void* __wrap_xrtCalloc(size_t n,size_t s){if(refuse())return NULL;void* p=__real_xrtCalloc(n,s);remember(p);return p;}
void* __wrap_xrtRealloc(void* p,size_t n){if(refuse())return NULL;void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q);}return q;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
char* __real_xrtStrDup(const char*);
char* __wrap_xrtStrDup(const char* text){if(refuse())return NULL;char* p=__real_xrtStrDup(text);remember(p);return p;}
bool __real_xrtArrayPush(xarray*,const void*);bool __real_xrtArrayResize(xarray*,size_t);bool __real_xrtArrayReserve(xarray*,size_t);void __real_xrtArrayUnit(xarray*);
bool __real_xrtMapSet(xmap*,xbytesview,const void*);
bool __wrap_xrtArrayPush(xarray* a,const void* value)
{if(refuse())return false;void* old=a->Data;bool okay=__real_xrtArrayPush(a,value);if(a->Data!=old){forget(old);remember(a->Data);}return okay;}
bool __wrap_xrtArrayResize(xarray* a,size_t count)
{if(refuse())return false;void* old=a->Data;bool okay=__real_xrtArrayResize(a,count);if(a->Data!=old){forget(old);remember(a->Data);}return okay;}
bool __wrap_xrtArrayReserve(xarray* a,size_t count)
{if(refuse())return false;void* old=a->Data;bool okay=__real_xrtArrayReserve(a,count);if(a->Data!=old){forget(old);remember(a->Data);}return okay;}
void __wrap_xrtArrayUnit(xarray* a){forget(a->Data);__real_xrtArrayUnit(a);}
bool __wrap_xrtMapSet(xmap* map,xbytesview key,const void* value){return refuse() ? false : __real_xrtMapSet(map,key,value);}
bool __real_xrtMutexInit(xmutex*);bool __real_xrtMutexLock(xmutex*);
bool __wrap_xrtMutexInit(xmutex* lock){return failMutex ? false : __real_xrtMutexInit(lock);}
bool __wrap_xrtMutexLock(xmutex* lock)
{if(xrtAtomic32Load(&probe,XMEMORY_ACQUIRE))xrtAtomic32FetchAdd(&attempts,1,XMEMORY_RELEASE);return __real_xrtMutexLock(lock);}
XL6_AstNode* __real_xlsAstClone(const XL6_AstNode*,const xls_ast_clone_options*,size_t*);
XL6_AstNode* __wrap_xlsAstClone(const XL6_AstNode* node,const xls_ast_clone_options* options,size_t* bytes)
{if(countClones)xrtAtomic32FetchAdd(&cloneCalls,1,XMEMORY_RELAXED);return __real_xlsAstClone(node,options,bytes);}
void __real_xl6AstFree(XL6_AstNode*);
void __wrap_xl6AstFree(XL6_AstNode* node)
{
    int observed=0;
    if(node)for(unsigned i=0;i<3;++i){ptr expected=node;
        if(xrtAtomicPtrCompareExchange(&watched[i],&expected,NULL,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
            xrtAtomic32FetchAdd(&drops[i],1,XMEMORY_RELAXED);observed=i!=0;break;
        }
    }
    int active=observed && xrtAtomic32Load(&probe,XMEMORY_ACQUIRE);
    if(active){
        if(xrtAtomic32FetchAdd(&inside,1,XMEMORY_ACQ_REL))xrtAtomic32Store(&overlap,1,XMEMORY_RELEASE);
        while(!xrtAtomic32Load(&allowFree,XMEMORY_ACQUIRE))xrtThreadYield();
    }
    __real_xl6AstFree(node);
    if(active)xrtAtomic32FetchSub(&inside,1,XMEMORY_ACQ_REL);
}
static void watch(unsigned i,const XL6_AstNode* node)
{xrtAtomicPtrStore(&watched[i],(ptr)node,XMEMORY_RELEASE);xrtAtomic32Store(&drops[i],0,XMEMORY_RELAXED);}
static int sameText(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    XL6_AstNode left=*a,right=*b;
    left.text=right.text=left.doc=right.doc=NULL;left.literalData=right.literalData=NULL;
    left.source=right.source=NULL;left.children=right.children=NULL;left.childCapacity=right.childCapacity=0;
    left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;
    left.xlsStorage=right.xlsStorage=NULL;left.xlsLeafReferences=right.xlsLeafReferences=0;
    xlsAstPackageClearOwned(&left);xlsAstPackageClearOwned(&right);
    if(memcmp(&left,&right,sizeof(left)) || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) ||
       !xlsAstPackageLinkEqual(a,b) || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)))return 0;
    if(!!a->source!=!!b->source)return 0;
    if(a->source && (!sameText(a->source->path,b->source->path) || a->source->completionQuery!=b->source->completionQuery ||
       a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!sameAst(a->children[i],b->children[i]))return 0;
    return sameAst(a->inferredType,b->inferredType) && sameAst(a->semanticDeclaredType,b->semanticDeclaredType);
}
static XL6_AstNode* fixture(void)
{
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return NULL;
    for(unsigned i=0;i<3;++i){
        XL6_AstNode* child=xl6AstNew(i==2 ? XL6_AST_VAR_DECL : XL6_AST_FUNC,NULL);
        if(!child){xl6AstFree(root);return NULL;}
        xl6AstAddChild(root,child);
        if(root->childCount!=i+1){xl6AstFree(child);xl6AstFree(root);return NULL;}
        child->text=xrtStrDup(i ? "second" : "first");
        child->inferredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);
        child->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);
        if(!child->text || !child->inferredType || !child->semanticDeclaredType){xl6AstFree(root);return NULL;}
    }
    if(!xl6AstSetSourceTree(root,"typed-owner-中文😀.xl")){xl6AstFree(root);return NULL;}return root;
}
static int metadataFaults(void)
{
    for(size_t i=1;i<=3;++i){
        XL6_AstNode* root=fixture();CHECK(root);faultAt=i;faultCalls=faultDenied=0;faultActive=1;
        xls_typed_module* module=xlsTypedModuleTake(root);faultActive=0;CHECK(faultDenied==1);
        if(i==1){CHECK(!module && !tracked && root->childCount==3);xl6AstFree(root);}
        else{
            CHECK(module && xlsTypedModuleRoot(module)==root);
            xls_typed_function* f=xlsTypedModuleFunction(module,0);CHECK((i==2)==(f!=NULL));
            CHECK((i==3)==(xlsTypedModuleIdentity(module)!=NULL));
            xlsTypedFunctionRelease(f);xlsTypedModuleRelease(module);CHECK(!tracked);
        }
    }
    XL6_AstNode* root=fixture();CHECK(root);failMutex=1;xls_typed_module* module=xlsTypedModuleTake(root);failMutex=0;
    CHECK(module && !xlsTypedModuleFunction(module,0) && xlsTypedModuleRoot(module)==root);xlsTypedModuleRelease(module);
    CHECK(!xlsTypedModuleFunction(NULL,0) && !xlsTypedFunctionRoot(NULL) && !xlsTypedFunctionMetadataBytes(NULL));
    xlsTypedFunctionRelease(NULL);return 1;
}
static int32 releaseFunction(void* user){xlsTypedFunctionRelease(user);return 0;}
static int serializedTeardown(void)
{
    XL6_AstNode* original=fixture();CHECK(original);xls_ast_pool_work work={0};
    XL6_AstNode* root=xlsAstClonePooled(original,NULL,NULL,NULL,8u*1024u*1024u,&work,NULL);xl6AstFree(original);
    CHECK(root && work.nodes && root->xlsStorage && root->children[0]->xlsStorage==root->children[1]->xlsStorage);
    CHECK(root->source==root->children[0]->source && root->source==root->children[1]->source);
    const XL6_AstNode* first=root->children[0];const XL6_AstNode* second=root->children[1];
    xls_typed_module* module=xlsTypedModuleTake(root);CHECK(module);
    xls_typed_function* a=xlsTypedModuleFunction(module,0);xls_typed_function* b=xlsTypedModuleFunction(module,1);
    CHECK(a && b && !xlsTypedModuleFunction(module,2) && !xlsTypedModuleFunction(module,3));
    CHECK(xlsTypedFunctionRoot(a)==first && xlsTypedFunctionRoot(b)==second && xlsTypedFunctionMetadataBytes(a));
    watch(0,root);watch(1,first);watch(2,second);xlsTypedModuleRelease(module);
    CHECK(xrtAtomic32Load(&drops[0],XMEMORY_RELAXED)==1 && !xrtAtomic32Load(&drops[1],XMEMORY_RELAXED));
    CHECK(sameText(xl6AstSourcePath(xlsTypedFunctionRoot(a)),"typed-owner-中文😀.xl"));
    xrtAtomic32Store(&attempts,0,XMEMORY_RELAXED);xrtAtomic32Store(&inside,0,XMEMORY_RELAXED);
    xrtAtomic32Store(&overlap,0,XMEMORY_RELAXED);xrtAtomic32Store(&allowFree,0,XMEMORY_RELAXED);xrtAtomic32Store(&probe,1,XMEMORY_RELEASE);
    xthread* ta=xrtThreadCreate(releaseFunction,a,0);CHECK(ta);
    uint64 end=xlsTestClockMicros()+5000000;
    while(!xrtAtomic32Load(&inside,XMEMORY_ACQUIRE) && xlsTestClockMicros()<end)xrtThreadYield();
    xthread* tb=xrtThreadCreate(releaseFunction,b,0);CHECK(tb);
    while(xrtAtomic32Load(&attempts,XMEMORY_ACQUIRE)<2 && !xrtAtomic32Load(&overlap,XMEMORY_ACQUIRE) && xlsTestClockMicros()<end)xrtThreadYield();
    int entered=xrtAtomic32Load(&attempts,XMEMORY_ACQUIRE),held=xrtAtomic32Load(&inside,XMEMORY_ACQUIRE);
    xrtAtomic32Store(&allowFree,1,XMEMORY_RELEASE);
    CHECK(xrtThreadWait(ta)==XWAIT_OK && xrtThreadWait(tb)==XWAIT_OK);xrtThreadDestroy(ta);xrtThreadDestroy(tb);xrtAtomic32Store(&probe,0,XMEMORY_RELEASE);
    CHECK(entered==2 && held==1 && !xrtAtomic32Load(&overlap,XMEMORY_ACQUIRE));
    CHECK(xrtAtomic32Load(&drops[1],XMEMORY_RELAXED)==1 && xrtAtomic32Load(&drops[2],XMEMORY_RELAXED)==1);return 1;
}
typedef struct reader {xls_function_unit* unit;int okay;} reader;
static int32 readFunction(void* user)
{
    reader* r=user;r->okay=1;
    for(unsigned i=0;i<24;++i){
        XL6_AstNode* copy=xlsFrontendFunctionUnitMaterialize(r->unit,64u*1024u*1024u,NULL,NULL);
        if(!copy || !sameAst(copy,xlsFrontendFunctionUnitRoot(r->unit)))r->okay=0;
        xl6AstFree(copy);
    }
    xlsFunctionUnitRelease(r->unit);return 0;
}
static int captures(void)
{
    char* path=xrtPathAbs("typed-function-actual-中文😀.xl");CHECK(path);
    const char* text="// 中文😀\nfunc helper(int value) as int\n int total=value+1\n return total\nend\nfunc main(...) as int\n return helper(7)\nend\n";
    xls_text* content=xlsTextCreate(text,strlen(text),8u*1024u*1024u,NULL);CHECK(content);
    xls_analysis_document document={"entry",path,content,31,1};xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);char error[256];
    xls_semantic_check* check=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
    if(!check)fprintf(stderr,"actual check:%s\n",error);
    CHECK(check && xlsSemanticCheckInfo(check).accepted);
    const xls_typed_module* module=xlsFrontendCheckFunctionModule(check);const XL6_AstNode* root=xlsSemanticCheckAst(check);CHECK(module && root==xlsTypedModuleRoot(module));
    CHECK(root->childCount==2 && root->children[0]->xlsStorage);
    const XL6_AstNode* first=root->children[0];const XL6_AstNode* second=root->children[1];
    xrtAtomic32Store(&cloneCalls,0,XMEMORY_RELAXED);countClones=1;
    xls_function_unit* unit=xlsSemanticCheckFunctionCapture(check,0,64u*1024u*1024u,NULL,NULL,error,sizeof(error));countClones=0;
    CHECK(unit && !xrtAtomic32Load(&cloneCalls,XMEMORY_RELAXED) && xlsFrontendFunctionUnitRoot(unit)==first);
    xls_function_info info=xlsFunctionUnitInfo(unit);CHECK(info.nodes && info.facts && info.treeBytes && info.ownedBytes);
    xls_function_unit* exact=xlsSemanticCheckFunctionCapture(check,0,info.ownedBytes,NULL,NULL,error,sizeof(error));CHECK(exact);xlsFunctionUnitRelease(exact);
    CHECK(!xlsSemanticCheckFunctionCapture(check,0,info.ownedBytes-1,NULL,NULL,error,sizeof(error)));
    xrtAtomic32Store(&cloneCalls,0,XMEMORY_RELAXED);countClones=1;
    xls_function_set* set=xlsSemanticCheckFunctionsCapture(check,64u*1024u*1024u,NULL,NULL,error,sizeof(error));countClones=0;
    CHECK(set && !xrtAtomic32Load(&cloneCalls,XMEMORY_RELAXED));xls_function_set_info setInfo=xlsFunctionSetInfo(set);CHECK(setInfo.functions==2);
    xls_function_set* exactSet=xlsSemanticCheckFunctionsCapture(check,setInfo.ownedBytes,NULL,NULL,error,sizeof(error));CHECK(exactSet);xlsFunctionSetRelease(exactSet);
    CHECK(!xlsSemanticCheckFunctionsCapture(check,setInfo.ownedBytes-1,NULL,NULL,error,sizeof(error)));
    xls_function_unit* other=xlsFunctionSetUnit(set,1);CHECK(other && xlsFrontendFunctionUnitRoot(other)==second);
    xlsFunctionUnitRelease(other);xlsFunctionSetRelease(set);
    /* Compare complete native fields with the original clone constructor. */
    XL6_SourceReport report;CHECK(xlsFrontendCheckFunctionInputs(check,&report));
    xls_function_origin* origin=xlsFunctionOriginCapture(&report,(xls_context*)xlsSemanticCheckContext(check),(xls_module_graph*)xlsSemanticCheckModules(check),xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(check)),NULL,NULL);CHECK(origin);
    xls_function_unit* cloned=xlsFrontendFunctionUnitCapture(first,xlsSemanticCheckTypedIndex(check),origin,64u*1024u*1024u,NULL,NULL);
    CHECK(cloned && xlsFrontendFunctionUnitRoot(cloned)!=first && sameAst(first,xlsFrontendFunctionUnitRoot(cloned)));xlsFunctionUnitRelease(cloned);
    xls_typed_function* f=xlsTypedModuleFunction(module,0);CHECK(f);size_t sites=0;
    for(size_t i=0;i<=sites;++i){
        faultAt=i;faultCalls=faultDenied=0;faultActive=1;
        xls_function_unit* captured=xlsFrontendFunctionUnitCaptureOwned(f,xlsSemanticCheckTypedIndex(check),origin,64u*1024u*1024u,NULL,NULL);faultActive=0;
        if(!i){sites=faultCalls;CHECK(captured && sites>5 && sites<1024);}
        else CHECK(faultDenied==1);
        if(captured){CHECK(xlsFrontendFunctionUnitRoot(captured)==first);xlsFunctionUnitRelease(captured);}
        CHECK(!tracked && xlsTypedFunctionRoot(f)==first);
    }
    xlsTypedFunctionRelease(f);xlsFunctionOriginRelease(origin);
    watch(0,root);watch(1,first);watch(2,second);
    xlsCheckSessionDestroy(session);xlsSemanticCheckDestroy(check);xlsTextRelease(content);xrtFree(path);
    CHECK(xrtAtomic32Load(&drops[0],XMEMORY_RELAXED)==1 && !xrtAtomic32Load(&drops[1],XMEMORY_RELAXED) && xrtAtomic32Load(&drops[2],XMEMORY_RELAXED)==1);
    CHECK(xlsFunctionUnitInfo(unit).facts==info.facts && xlsFrontendFunctionUnitRoot(unit)==first);
    reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(reader){xlsFunctionUnitRetain(unit),0};threads[i]=xrtThreadCreate(readFunction,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    xlsFunctionUnitRelease(unit);CHECK(xrtAtomic32Load(&drops[1],XMEMORY_RELAXED)==1);
    printf("owned function capture:%zu allocation refusal sites;no tree clone;root and unretained sibling retired;96 independent materializations\n",sites);return 1;
}
int main(void)
{
    for(unsigned i=0;i<3;++i){xrtAtomicPtrInit(&watched[i],NULL);xrtAtomic32Init(&drops[i],0);}
    xrtAtomic32Init(&cloneCalls,0);xrtAtomic32Init(&probe,0);xrtAtomic32Init(&attempts,0);xrtAtomic32Init(&inside,0);xrtAtomic32Init(&overlap,0);xrtAtomic32Init(&allowFree,0);
    if(!metadataFaults() || !serializedTeardown() || !captures())return 1;
    printf("analysis_typed_function_test passed:%u checks;native transferred functions,exact budgets,optional failures,serialized sibling teardown\n",checks);return 0;
}
