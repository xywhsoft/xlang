#include "analysis/frontend_internal.h"
#include <stdio.h>
static void pipelineScopeProbe(void);
static xatomic32 scopeCalls,scopeErrors;
static _Thread_local void *expectedLookup,*expectedResource;
#define XLS_IR_PIPELINE_SCOPE_TEST pipelineScopeProbe
#include "xl6_ir.c"
#define XLS_IR_PIPELINE_LIBRARY 1
#include "analysis_ir_pipeline_test.c"
static void pipelineScopeProbe(void)
{
 xrtAtomic32FetchAdd(&scopeCalls,1,XMEMORY_RELAXED);
 if(xlsActiveIrMemo||xlsActiveIrOwner||!xlsActiveTypeLookup||
    xlsActiveTypeLookup->previous!=expectedLookup||!xlsActiveTypeResource||
    xlsActiveTypeResource->previous!=expectedResource||xlsActiveTypeResource->failed)
  xrtAtomic32FetchAdd(&scopeErrors,1,XMEMORY_RELAXED);
}
static int contractPair(const XL6_AstNode* a,const XL6_AstNode* b,const char* expectedA,const char* expectedB)
{
 xls_ir_memo_build memo={0};xls_ir_result owner={0};xls_ir_type_lookup lookup={0};xls_ir_type_resource_scope resource={.failed=123};
 CHECK(!xlsActiveIrMemo&&!xlsActiveIrOwner&&!xlsActiveTypeLookup&&!xlsActiveTypeResource);
 xlsActiveIrMemo=&memo;xlsActiveIrOwner=&owner;xlsActiveTypeLookup=&lookup;xlsActiveTypeResource=&resource;expectedLookup=&lookup;expectedResource=&resource;
 XL6_DiagList diags[2];xl6DiagInit(&diags[0]);xl6DiagInit(&diags[1]);xls_ir_pipeline_status status;
 xls_ir_native_pipeline* state[2]={xlsIrNativePipelineCreate(&status),xlsIrNativePipelineCreate(&status)};CHECK(state[0]&&state[1]);
 const XL6_AstNode* roots[]={a,b};const char* expected[]={expectedA,expectedB};
 for(unsigned generation=0;generation<4;++generation){
  for(unsigned i=0;i<2;++i)CHECK(xlsIrNativePipelinePrepare(state[i],roots[i],&diags[i],i?SIZE_MAX:0,&status));
  for(size_t phase=0;phase<xlsIrNativePipelinePhaseCount();++phase){
   int ranges=xlsIrNativePipelinePhaseHasNodeRanges(phase);
   for(unsigned pass=0;pass<(ranges?2u:1u);++pass){
    for(unsigned j=0;j<2;++j){unsigned i=(j+phase+pass)%2;
     if(ranges&&!pass){size_t half=xlsIrNativePipelineModule(state[i])->nodeCount/2;CHECK(half&&xlsIrNativePipelineAdvanceNodes(state[i],half,&diags[i],&status));CHECK(xlsIrNativePipelinePhase(state[i])==phase&&xlsIrNativePipelineNodeCursor(state[i])==half);}
     else {CHECK(xlsIrNativePipelineAdvance(state[i],&diags[i],&status));if(ranges)CHECK(xlsIrNativePipelinePhase(state[i])==phase+1&&!xlsIrNativePipelineNodeCursor(state[i]));}
     CHECK(xlsActiveIrMemo==&memo&&xlsActiveIrOwner==&owner&&xlsActiveTypeLookup==&lookup&&xlsActiveTypeResource==&resource&&resource.failed==123);
    }
   }
  }
  for(unsigned i=0;i<2;++i){
#if defined(XLS_NATIVE_IR_PIPELINE_COMPONENT) && XLS_NATIVE_IR_PIPELINE_COMPONENT && \
    defined(XLS_NATIVE_IR_PIPELINE_SOURCE) && defined(XLS_NATIVE_IR_TYPE_RESOURCE_GUARD)
   CHECK(!state[i]->root&&xlsIrNativePipelineState(state[i])==XLS_PIPELINE_READY&&!diags[i].count);
#else
   CHECK(xlsIrNativePipelineState(state[i])==XLS_PIPELINE_READY&&!diags[i].count);
#endif
   char* actual=dump(xlsIrNativePipelineModule(state[i]));CHECK(actual&&!strcmp(actual,expected[i]));xrtFree(actual);
   if(generation&1){XL6_IrModule moved;xl6IrModuleInit(&moved);CHECK(xlsIrNativePipelineTake(state[i],&moved));xlsIrNativePipelineReset(state[i]);CHECK(xl6IrVerifyModule(&moved,&diags[i]));xl6IrModuleFree(&moved);}
   else xlsIrNativePipelineReset(state[i]);
  }
 }
 for(unsigned i=0;i<2;++i){xlsIrNativePipelineDestroy(state[i]);xl6DiagFree(&diags[i]);}
 CHECK(!memo.memo&&!memo.capture&&!owner.memo&&!owner.module.nodes&&resource.failed==123);
 CHECK(xlsActiveIrMemo==&memo&&xlsActiveIrOwner==&owner&&xlsActiveTypeLookup==&lookup&&xlsActiveTypeResource==&resource);
 xlsActiveIrMemo=NULL;xlsActiveIrOwner=NULL;xlsActiveTypeLookup=NULL;xlsActiveTypeResource=NULL;expectedLookup=expectedResource=NULL;return 1;
}
typedef struct pipeline_reader {const XL6_AstNode *a,*b;const char *expectedA,*expectedB;int passed;} pipeline_reader;
static int32 pipelineRead(void* opaque)
{pipeline_reader* r=opaque;r->passed=contractPair(r->a,r->b,r->expectedA,r->expectedB);return r->passed?0:1;}
static int contracts(void)
{
 const char* a="class Box\n string item=\"owned\"\n func value() as string\n string local=self.item\n return local\n end\nend\nfunc read(Box item) as string\n return item.value()\nend\n";
 const char* b="enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n";
 char* path=xrtPathAbs("pipeline-contract-中文.xl");CHECK(path);++probePaused;xls_semantic_check *one=checked(path,a),*two=checked(path,b);--probePaused;
 CHECK(one&&two&&xlsSemanticCheckInfo(one).accepted&&xlsSemanticCheckInfo(two).accepted);
 char *first=dump(xlsSemanticCheckIr(one)),*second=dump(xlsSemanticCheckIr(two));CHECK(first&&second);
 CHECK(contractPair(xlsSemanticCheckAst(one),xlsSemanticCheckAst(two),first,second));
 pipeline_reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(pipeline_reader){xlsSemanticCheckAst(one),xlsSemanticCheckAst(two),first,second,0};threads[i]=xrtThreadCreate(pipelineRead,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].passed);xrtThreadDestroy(threads[i]);}
 size_t ranges=0;for(size_t i=0;i<xlsIrNativePipelinePhaseCount();++i)ranges+=xlsIrNativePipelinePhaseHasNodeRanges(i);
 CHECK(ranges==15&&xrtAtomic32Load(&scopeCalls,XMEMORY_RELAXED)==5u*4u*2u*(xlsIrNativePipelinePhaseCount()+1+ranges)&&!xrtAtomic32Load(&scopeErrors,XMEMORY_RELAXED));
 CHECK(xlsSemanticCheckValidate(one)&&xlsSemanticCheckValidate(two));xlsSemanticCheckDestroy(one);xlsSemanticCheckDestroy(two);xrtFree(first);xrtFree(second);xrtFree(path);return 1;
}
int main(void)
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_pipeline_contract_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&scopeCalls,0);xrtAtomic32Init(&scopeErrors,0);
 if(!contracts())return 1;
 printf("analysis_ir_pipeline_contract_test passed: %u checks;%u original production scopes;four threads,interleaved class/enum contexts,outer memo/owner/type/resource restoration\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&scopeCalls,XMEMORY_RELAXED));return 0;
}
