/* Compile the actual completion owner privately. Only new operation boundaries
 * are refused while TryWordView runs; all language/checker code remains shared.
 * Track owned labels/report arrays and successful AST clone roots. This does
 * not claim exhaustive failure at every allocator inside the original clone. */
#define xlsSemanticCompletionAst faultCompletionAst
#define xlsSemanticCompletionCallWork faultCompletionCallWork
#define xlsSemanticCompletionContext faultCompletionContext
#define xlsSemanticCompletionCreate faultCompletionCreate
#define xlsSemanticCompletionCreateBatch faultCompletionCreateBatch
#define xlsSemanticCompletionCreateCachedInContext faultCompletionCreateCachedInContext
#define xlsSemanticCompletionCreateCachedParsedInContext faultCompletionCreateCachedParsedInContext
#define xlsSemanticCompletionCreateInContext faultCompletionCreateInContext
#define xlsSemanticCompletionCreateIncrementalInContext faultCompletionCreateIncrementalInContext
#define xlsSemanticCompletionCreateParsed faultCompletionCreateParsed
#define xlsSemanticCompletionCreateParsedInContext faultCompletionCreateParsedInContext
#define xlsSemanticCompletionCursorSyntaxApplied faultCompletionCursorSyntaxApplied
#define xlsSemanticCompletionDestroy faultCompletionDestroy
#define xlsSemanticCompletionEntryWork faultCompletionEntryWork
#define xlsSemanticCompletionInfo faultCompletionInfo
#define xlsSemanticCompletionInputUsage faultCompletionInputUsage
#define xlsSemanticCompletionInputs faultCompletionInputs
#define xlsSemanticCompletionItem faultCompletionItem
#define xlsSemanticCompletionJson faultCompletionJson
#define xlsSemanticCompletionMaterializationWork faultCompletionMaterializationWork
#define xlsSemanticCompletionModuleWork faultCompletionModuleWork
#define xlsSemanticCompletionModules faultCompletionModules
#define xlsSemanticCompletionProbe faultCompletionProbe
#define xlsSemanticCompletionRetain faultCompletionRetain
#define xlsSemanticCompletionSemWork faultCompletionSemWork
#define xlsSemanticCompletionSource faultCompletionSource
#define xlsSemanticCompletionStorageWork faultCompletionStorageWork
#define xlsSemanticCompletionTailWork faultCompletionTailWork
#define xlsSemanticCompletionTopology faultCompletionTopology
#define xlsSemanticCompletionValidate faultCompletionValidate
#define xlsSemanticSignatureCreateBatch faultSignatureCreateBatch
#define xlsSemanticSignatureCreateInContext faultSignatureCreateInContext
#define xlsSemanticSignatureData faultSignatureData
#define xlsSemanticSignatureInputJson faultSignatureInputJson
#define xlsSemanticStringFree faultStringFree
#define xlsSemanticCompletionTryWordView faultCompletionTryWordView
#define xlsSemanticCompletionUnitWork faultCompletionUnitWork
#define xlsCompletionUnitBegin faultUnitBegin
#define xlsCompletionUnitEnd faultUnitEnd
#include "analysis/frontend_internal.h"
#include "analysis/signature_internal.h"
#include "analysis/completion_unit.h"
#include "analysis/cursor_word_view.h"
#include "analysis/input_batch.h"
#include "analysis/input_topology.h"
#include "analysis/ast_clone.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static size_t attempts,deny,live,cloneLive,topologyLive;static int active,trackingError;
static void* pointers[16384];static const XL6_AstNode* roots[8192];
static xls_input_topology* topologyRoots[1024];
static int refusal(void){if(!active)return 0;++attempts;return deny && attempts==deny;}
static void remember(void* pointer)
{
    if(!pointer || !active)return;
    for(size_t i=0;i<16384;++i)if(!pointers[i]){pointers[i]=pointer;++live;return;}
    trackingError=1;
}
static void forget(void* pointer)
{if(pointer)for(size_t i=0;i<16384;++i)if(pointers[i]==pointer){pointers[i]=NULL;--live;return;}}
static void* checkedCalloc(size_t n,size_t size){if(refusal())return NULL;void* p=xrtCalloc(n,size);remember(p);return p;}
static char* checkedDup(const char* s){if(refusal())return NULL;char* p=xrtStrDup(s);remember(p);return p;}
static char* checkedDupN(const char* s,size_t n){if(refusal())return NULL;char* p=xrtStrDupN(s,n);remember(p);return p;}
static void checkedFree(void* p){forget(p);xrtFree(p);}
static bool checkedMutex(xmutex* p){return !refusal() && xrtMutexInit(p);}
static bool checkedHash(const void* p,size_t n,void* digest){return !refusal() && xrtSha256(p,n,digest);}
static XL6_AstNode* checkedClone(const XL6_AstNode* in,const xls_ast_clone_options* options,size_t* bytes)
{
    if(refusal())return NULL;
    XL6_AstNode* root=xlsAstClone(in,options,bytes);
    if(root && active){for(size_t i=0;i<8192;++i)if(!roots[i]){roots[i]=root;++cloneLive;return root;}trackingError=1;}
    return root;
}
static void checkedAstFree(XL6_AstNode* root)
{if(root)for(size_t i=0;i<8192;++i)if(roots[i]==root){roots[i]=NULL;--cloneLive;break;}xl6AstFree(root);}
static xls_input_topology* checkedTopologyClone(const xls_input_topology* source,size_t limit,int (*cancelled)(void*),void* user)
{
    if(refusal())return NULL;
    xls_input_topology* copy=xlsInputTopologyClone(source,limit,cancelled,user);
    if(copy && active){for(size_t i=0;i<1024;++i)if(!topologyRoots[i]){topologyRoots[i]=copy;++topologyLive;return copy;}trackingError=1;}
    return copy;
}
static void checkedTopologyFree(xls_input_topology* topology)
{if(topology)for(size_t i=0;i<1024;++i)if(topologyRoots[i]==topology){topologyRoots[i]=NULL;--topologyLive;break;}xlsInputTopologyDestroy(topology);}
static void checkedQueryFree(XL6_CompletionQuery* q)
{
    if(q){for(size_t i=0;i<q->count;++i){forget(q->items[i].name);forget(q->items[i].canonical);}forget(q->prefix);forget(q->items);}
    xl6CompletionFree(q);
}
static void checkedReportFree(XL6_SourceReport* r)
{
    if(r){for(size_t i=0;i<r->count;++i){forget(r->items[i].path);forget(r->items[i].text);}
        for(size_t i=0;i<r->probeCount;++i)forget(r->probes[i].path);
        forget(r->items);forget(r->probes);forget(r->packagePath);forget(r->errorPath);}
    xl6SourceReportFree(r);
}
#define xrtCalloc checkedCalloc
#define xrtStrDup checkedDup
#define xrtStrDupN checkedDupN
#define xrtFree checkedFree
#define xrtMutexInit checkedMutex
#define xrtSha256 checkedHash
#define xlsAstClone checkedClone
#define xl6AstFree checkedAstFree
#define xl6CompletionFree checkedQueryFree
#define xl6SourceReportFree checkedReportFree
#define xlsInputTopologyClone checkedTopologyClone
#define xlsInputTopologyDestroy checkedTopologyFree
#include "../src/analysis/semantic.c"
#undef xrtCalloc
#undef xrtStrDup
#undef xrtStrDupN
#undef xrtFree
#undef xrtMutexInit
#undef xrtSha256
#undef xlsAstClone
#undef xl6AstFree
#undef xl6CompletionFree
#undef xl6SourceReportFree
#undef xlsInputTopologyClone
#undef xlsInputTopologyDestroy
extern xls_semantic_completion* originalCompletionCreateBatch(xls_entry_cache*,const xls_input_batch*,const char*,size_t,
    const xls_syntax*,xls_semantic_completion*,int (*)(void*),void*,char*,size_t) __asm__("xlsSemanticCompletionCreateBatch");
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_completion_unit_fault_test:%d: %s live=%zu roots=%zu topology=%zu attempt=%zu deny=%zu\n",__LINE__,#v,live,cloneLive,topologyLive,attempts,deny);return 0;}}while(0)
static int runFaults(void)
{
    const char* oldSource="import support\nfunc edited() as int\n p\n return 0\nend\nfunc prefixOne(int value) as int\n return value\nend\n";
    const char* newSource="import support\nfunc edited() as int\n pr\n return 0\nend\nfunc prefixOne(int value) as int\n return value\nend\n";
    char relative[160];snprintf(relative,sizeof(relative),"build/word-unit-fault-%llu",(unsigned long long)xrtClock());
    char* directory=xrtPathAbs(relative);char* library=xrtPathJoin(directory,"library");char* path=xrtPathJoin(directory,"main.xl");char* package=xrtPathJoin(library,"support");char* dependency=xrtPathJoin(package,"api.xl");char* manifest=xrtPathJoin(package,"xlang.package.json");CHECK(directory && library && path && package && dependency && manifest && xrtDirCreateAll(package));
    const char* helper="export func prefixValue() as int\n return 7\nend\n";CHECK(xrtFileWriteAtomic(dependency,(xbytesview){(const unsigned char*)helper,strlen(helper)}));
    const char* json="{\"format\":\"xlang-package-1\",\"name\":\"support\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";CHECK(xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)json,strlen(json)}));
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=path,.directory=directory,.configuration="debug",.analysisProfile="fixture-native",.targetKind="exe",.packagePath=library};
    xls_context* context=xlsContextCreate(&spec,1024u*1024u,NULL);CHECK(context);
    xls_text* a=xlsTextCreate(oldSource,strlen(oldSource),8u*1024u*1024u,NULL);xls_text* b=xlsTextCreate(newSource,strlen(newSource),8u*1024u*1024u,NULL);CHECK(a && b);
    xls_analysis_document old={"entry",path,a,1,1},next={"entry",path,b,1,2};
    xls_input_batch* before=xlsInputBatchCreateInContext(context,NULL,NULL,&old,1,"entry",NULL,NULL,NULL,0);
    xls_input_batch* after=xlsInputBatchCreateInContext(context,NULL,before,&next,1,"entry",NULL,NULL,NULL,0);CHECK(before && after);
    size_t at=(size_t)(strstr(oldSource,"\n p")-oldSource)+3,now=at+1;
    XL6_CompletionQuery aq={.path=path,.offset=at},bq={.path=path,.offset=now};xls_parse_work aw={0},bw={0};
    xls_syntax* ap=xlsSyntaxBuildCursor(path,a,NULL,&aq,&aw);xls_syntax* bp=xlsSyntaxBuildCursor(path,b,ap,&bq,&bw);CHECK(ap && bp);
    xl6CompletionFree(&aq);xl6CompletionFree(&bq);
    xls_semantic_completion* previous=originalCompletionCreateBatch(NULL,before,"entry",at,ap,NULL,NULL,NULL,NULL,0);CHECK(previous && xlsSemanticCompletionInfo(previous).available);
    CHECK(previous->report.count>=2 && previous->report.probeCount && previous->report.packagePathObserved && previous->report.packagePath && !strcmp(previous->report.packagePath,library));
    active=1;attempts=deny=0;
    xls_semantic_completion* complete=xlsSemanticCompletionTryWordView(previous,after,"entry",ap,bp,NULL,NULL);
    size_t operations=attempts;CHECK(complete && xlsSemanticCompletionUnitWork(complete).projected && operations>15);
    size_t exact=xlsSemanticCompletionUnitWork(complete).knownBytes;xlsSemanticCompletionDestroy(complete);CHECK(!live && !cloneLive && !topologyLive && !trackingError);
    for(size_t n=1;n<=operations;++n){
        attempts=0;deny=n;xls_semantic_completion* rejected=xlsSemanticCompletionTryWordView(previous,after,"entry",ap,bp,NULL,NULL);
        CHECK(!rejected && attempts>=n && !live && !cloneLive && !topologyLive && !trackingError);
        CHECK(xlsSemanticCompletionInfo(previous).available && xlsSemanticCompletionValidate(previous));
    }
    deny=0;
    for(size_t n=0;n<3;++n){
        xls_completion_unit_control control;xlsCompletionUnitBegin(&control,n==0 ? 0 : n==1 ? exact-1 : exact);
        complete=xlsSemanticCompletionTryWordView(previous,after,"entry",ap,bp,NULL,NULL);
        CHECK((complete!=NULL)==(n==2));xlsCompletionUnitEnd(&control);xlsSemanticCompletionDestroy(complete);CHECK(!live && !cloneLive && !topologyLive);
    }
    active=0;complete=xlsSemanticCompletionTryWordView(previous,after,"entry",ap,bp,NULL,NULL);CHECK(complete);
    xlsSemanticCompletionDestroy(previous);xlsInputBatchDestroy(before);xlsInputBatchDestroy(after);xlsSyntaxRelease(ap);xlsSyntaxRelease(bp);xlsTextRelease(a);xlsTextRelease(b);xlsContextRelease(context);xrtFree(path);xrtFree(directory);xrtFree(library);xrtFree(dependency);xrtFree(package);xrtFree(manifest);
    CHECK(xlsSemanticCompletionInfo(complete).available && xlsSemanticCompletionValidate(complete));xlsSemanticCompletionDestroy(complete);
    printf("word unit operation refusal positions: %zu\n",operations);return 1;
}
int main(void){if(!runFaults())return 1;printf("analysis_completion_unit_fault_test passed: %u checks\n",checks);return 0;}
