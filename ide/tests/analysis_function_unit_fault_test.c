#include "analysis/function_unit_internal.h"
#include "analysis/check.h"
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
static size_t calls,failAt,live,roots;
static void* allocations[512];
static int badFree;
static int permitted(void){return ++calls!=failAt;}
static void* remember(void* pointer)
{
    if(!pointer)return NULL;
    for(size_t i=0;i<512;++i)if(!allocations[i]){allocations[i]=pointer;++live;return pointer;}
    badFree=1;return pointer;
}
static void* allocate(size_t count,size_t size){return permitted()?remember(xrtCalloc(count,size)):NULL;}
static char* textCopy(const char* text){return permitted()?remember(xrtStrDup(text)):NULL;}
static void release(void* pointer)
{
    if(!pointer)return;
    int found=0;for(size_t i=0;i<512;++i)if(allocations[i]==pointer){allocations[i]=NULL;--live;found=1;break;}
    if(!found)badFree=1;
    xrtFree(pointer);
}
static bool arrayInit(xarray* a,size_t size){return permitted() && xrtArrayInit(a,size);}
static bool arrayResize(xarray* a,size_t count){return permitted() && xrtArrayResize(a,count);}
static bool arrayPush(xarray* a,const void* item){return permitted() && xrtArrayPush(a,item);}
static XL6_AstNode* cloneTree(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes)
{
    if(!permitted())return NULL;
    XL6_AstNode* node=xlsAstClone(input,options,bytes);if(node)++roots;return node;
}
static void releaseTree(XL6_AstNode* node){if(node){if(!roots)badFree=1;else --roots;}xl6AstFree(node);}
#define xrtCalloc allocate
#define xrtStrDup textCopy
#define xrtFree release
#define xrtArrayInit arrayInit
#define xrtArrayResize arrayResize
#define xrtArrayPush arrayPush
#define xlsAstClone cloneTree
#define xl6AstFree releaseTree
#define xlsFunctionOriginCapture privateOriginCapture
#define xlsFunctionOriginRelease privateOriginRelease
#define xlsFrontendFunctionUnitCapture privateCapture
#define xlsFunctionUnitRetain privateRetain
#define xlsFunctionUnitRelease privateRelease
#define xlsFunctionUnitInfo privateInfo
#define xlsFunctionUnitNode privateNode
#define xlsFunctionUnitChild privateChild
#define xlsFunctionUnitFact privateFact
#define xlsFunctionUnitInputs privateInputs
#define xlsFunctionUnitSource privateSource
#define xlsFunctionUnitProbe privateProbe
#define xlsFunctionUnitContext privateContext
#define xlsFunctionUnitEntities privateEntities
#define xlsFunctionUnitModules privateModules
#define xlsFrontendFunctionUnitRoot privateRoot
#define xlsFrontendFunctionUnitMaterialize privateMaterialize
#define xlsFrontendFunctionSetCapture privateSetCapture
#define xlsFunctionSetRetain privateSetRetain
#define xlsFunctionSetRelease privateSetRelease
#define xlsFunctionSetInfo privateSetInfo
#define xlsFunctionSetUnit privateSetUnit
#define xlsSemanticCheckFunctionCount privateCheckCount
#define xlsSemanticCheckFunctionCapture privateCheckCapture
#define xlsSemanticCheckFunctionsCapture privateCheckSetCapture
#include "analysis/function_unit.c"
#undef xrtCalloc
#undef xrtStrDup
#undef xrtFree
#undef xrtArrayInit
#undef xrtArrayResize
#undef xrtArrayPush
#undef xlsAstClone
#undef xl6AstFree
#undef xlsFunctionOriginCapture
#undef xlsFunctionOriginRelease
#undef xlsFrontendFunctionUnitCapture
#undef xlsFunctionUnitRetain
#undef xlsFunctionUnitRelease
#undef xlsFunctionUnitInfo
#undef xlsFunctionUnitNode
#undef xlsFunctionUnitChild
#undef xlsFunctionUnitFact
#undef xlsFunctionUnitInputs
#undef xlsFunctionUnitSource
#undef xlsFunctionUnitProbe
#undef xlsFunctionUnitContext
#undef xlsFunctionUnitEntities
#undef xlsFunctionUnitModules
#undef xlsFrontendFunctionUnitRoot
#undef xlsFrontendFunctionUnitMaterialize
#undef xlsFrontendFunctionSetCapture
#undef xlsFunctionSetRetain
#undef xlsFunctionSetRelease
#undef xlsFunctionSetInfo
#undef xlsFunctionSetUnit
#undef xlsSemanticCheckFunctionCount
#undef xlsSemanticCheckFunctionCapture
#undef xlsSemanticCheckFunctionsCapture
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_function_unit_fault_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int runFaults(void)
{
    const char* body="func main(...) as int\n string data=\"a\\0😀\"\n int value=7\n return value\nend\n";
    char* path=xrtPathAbs("function-fault-中文.xl");CHECK(path);xls_semantic_input in={path,body,strlen(body)};char error[256];
    xls_semantic_check* check=xlsSemanticCheckCreate(path,&in,1,1,NULL,NULL,error,sizeof(error));CHECK(check && xlsSemanticCheckInfo(check).accepted);
    const XL6_AstNode* root=xlsSemanticCheckAst(check)->children[0];CHECK(root->kind==XL6_AST_FUNC);
    XL6_SourceReportItem items[2]={{.path=path,.text=(char*)body,.size=strlen(body),.rawSize=strlen(body),.kind=XL6_SOURCE_REPORT_SOURCE,.origin="overlay"},{.path="package.json",.text="{}",.size=2,.rawSize=2,.kind=XL6_SOURCE_REPORT_MANIFEST,.origin="disk"}};
    XL6_ResolutionProbe probes[2]={{"missing.xl",0},{"existing.xl",1}};XL6_SourceReport report={.items=items,.count=2,.complete=1,.probes=probes,.probeCount=2,.packagePath="package-root",.packagePathObserved=1};
    size_t points=0;
    for(size_t failure=0;failure<=points;++failure){
        calls=0;failAt=failure;xls_function_origin* origin=privateOriginCapture(&report,NULL,NULL,NULL,NULL,NULL);
        xls_function_unit* unit=origin?privateCapture(root,xlsSemanticCheckTypedIndex(check),origin,1024*1024,NULL,NULL):NULL;
        if(!failure){points=calls;CHECK(unit && points>20 && privateInfo(unit).facts && privateInfo(unit).nodes);}
        else CHECK(!unit);
        privateRelease(unit);privateOriginRelease(origin);CHECK(!live && !roots && !badFree && xlsSemanticCheckInfo(check).accepted);
    }
    size_t setPoints=0;XL6_AstNode* definitions[]={(XL6_AstNode*)root,(XL6_AstNode*)root};XL6_AstNode module={.kind=XL6_AST_MODULE,.children=definitions,.childCount=2};
    for(size_t failure=0;failure<=setPoints;++failure){
        calls=0;failAt=failure;xls_function_origin* stamp=privateOriginCapture(&report,NULL,NULL,NULL,NULL,NULL);
        xls_function_set* set=stamp?privateSetCapture(&module,xlsSemanticCheckTypedIndex(check),stamp,1024*1024,NULL,NULL):NULL;
        if(!failure){setPoints=calls;CHECK(set && privateSetInfo(set).functions==2 && setPoints>points);}
        else CHECK(!set);
        privateSetRelease(set);privateOriginRelease(stamp);CHECK(!live && !roots && !badFree);
    }
    failAt=calls=0;xls_function_origin* origin=privateOriginCapture(&report,NULL,NULL,NULL,NULL,NULL);CHECK(origin);size_t baseline=live;
    for(size_t bytes=0;bytes<512;++bytes){CHECK(!privateCapture(root,NULL,origin,bytes,NULL,NULL) && live==baseline && !roots && !badFree);}
    xls_function_unit* unit=privateCapture(root,xlsSemanticCheckTypedIndex(check),origin,1024*1024,NULL,NULL);CHECK(unit);
    xls_function_info info=privateInfo(unit);xls_function_unit* exact=privateCapture(root,xlsSemanticCheckTypedIndex(check),origin,info.ownedBytes,NULL,NULL);CHECK(exact && privateInfo(exact).ownedBytes==info.ownedBytes);privateRelease(exact);
    CHECK(!privateCapture(root,xlsSemanticCheckTypedIndex(check),origin,info.ownedBytes-1,NULL,NULL));
    xls_function_set* set=privateSetCapture(&module,xlsSemanticCheckTypedIndex(check),origin,1024*1024,NULL,NULL);CHECK(set);xls_function_set_info setInfo=privateSetInfo(set);
    xls_function_set* exactSet=privateSetCapture(&module,xlsSemanticCheckTypedIndex(check),origin,setInfo.ownedBytes,NULL,NULL);CHECK(exactSet && privateSetInfo(exactSet).ownedBytes==setInfo.ownedBytes);privateSetRelease(exactSet);
    CHECK(!privateSetCapture(&module,xlsSemanticCheckTypedIndex(check),origin,setInfo.ownedBytes-1,NULL,NULL));
    calls=0;failAt=1;
    for(uint32_t id=1;id<=info.nodes;++id){xls_function_node node;CHECK(privateNode(unit,id,&node));for(size_t i=0;i<node.children;++i){uint32_t child;CHECK(privateChild(unit,id,i,&child));}}
    for(size_t i=0;i<info.facts;++i){xls_index_fact fact;CHECK(privateFact(unit,i,&fact));}
    xls_inputs inputs;CHECK(privateInputs(unit,&inputs));for(size_t i=0;i<inputs.sources;++i){xls_function_source source;CHECK(privateSource(unit,i,&source));}for(size_t i=0;i<inputs.probes;++i){xls_input_probe probe;CHECK(privateProbe(unit,i,&probe));}
    for(size_t i=0;i<2;++i){xls_function_unit* borrowed=privateSetUnit(set,i);CHECK(borrowed && privateInfo(borrowed).nodes==info.nodes);privateRelease(borrowed);}
    CHECK(privateInfo(unit).nodes==info.nodes && !privateContext(unit) && !privateEntities(unit) && !privateModules(unit) && privateSetInfo(set).functions==2 && !privateSetUnit(set,2) && !calls);
    failAt=0;xls_function_unit* independent=privateSetUnit(set,1);privateSetRelease(set);CHECK(independent && privateInfo(independent).nodes==info.nodes);privateRelease(independent);privateRelease(unit);privateOriginRelease(origin);CHECK(!live && !roots && !badFree);
    xlsSemanticCheckDestroy(check);xrtFree(path);printf("new function/origin allocation/refusal points=%zu;setprefix rollback points=%zu;read-only queries allocate0\n",points,setPoints);return 1;
}
int main(void){if(!runFaults())return 1;printf("analysis_function_unit_fault_test passed:%u checks\n",checks);return 0;}
