#include "analysis/frontend_internal.h"
#include "analysis/ir_native_pipeline.h"
static void nodesVisit(size_t,size_t);
#define XLS_IR_PIPELINE_NODE_VISIT nodesVisit
#include "xl6_ir.c"
static int nodesAdvance(xls_ir_native_pipeline*,XL6_DiagList*,xls_ir_pipeline_status*);
static int nodesFinish(xls_ir_native_pipeline*,XL6_DiagList*,xls_ir_pipeline_status*);
#define xlsIrNativePipelineAdvance nodesAdvance
#define xlsIrNativePipelineFinish nodesFinish
#define XLS_IR_PIPELINE_LIBRARY 1
#include "analysis_ir_pipeline_test.c"
#undef xlsIrNativePipelineAdvance
#undef xlsIrNativePipelineFinish
static _Thread_local int visiting;
static _Thread_local size_t expectedPhase,expectedNode,expectedEnd;
static xatomic64 visits,slices,visitErrors;
static void nodesVisit(size_t phase,size_t node)
{
 if(!visiting||phase!=expectedPhase||node!=expectedNode||node>=expectedEnd)
  xrtAtomic64FetchAdd(&visitErrors,1,XMEMORY_RELAXED);
 ++expectedNode;xrtAtomic64FetchAdd(&visits,1,XMEMORY_RELAXED);
}
static int nodesAdvance(xls_ir_native_pipeline* b,XL6_DiagList* d,xls_ir_pipeline_status* status)
{
 size_t phase=xlsIrNativePipelinePhase(b);
 if(!xlsIrNativePipelinePhaseHasNodeRanges(phase))return xlsIrNativePipelineAdvance(b,d,status);
 const XL6_IrModule* module=xlsIrNativePipelineModule(b);CHECK(module&&module->nodeCount);
 size_t count=module->nodeCount,step=count/3+1;
 while(xlsIrNativePipelinePhase(b)==phase){
  size_t first=xlsIrNativePipelineNodeCursor(b),end=count-first>step?first+step:count;
  CHECK(!xlsIrNativePipelineAdvanceNodes(b,first,d,status)&&*status==XLS_PIPELINE_INPUT);
  CHECK(!xlsIrNativePipelineAdvanceNodes(b,count+1,d,status)&&*status==XLS_PIPELINE_INPUT);
  expectedPhase=phase;expectedNode=first;expectedEnd=end;visiting=1;
  int okay=xlsIrNativePipelineAdvanceNodes(b,end,d,status);visiting=0;
  if(!okay)return 0;
  CHECK(expectedNode==end&&*status==XLS_PIPELINE_OK);
  xrtAtomic64FetchAdd(&slices,1,XMEMORY_RELAXED);
  if(end<count){XL6_IrModule moved;xl6IrModuleInit(&moved);
   CHECK(xlsIrNativePipelinePhase(b)==phase&&xlsIrNativePipelineNodeCursor(b)==end&&
         xlsIrNativePipelineState(b)==XLS_PIPELINE_PREPARED&&!xlsIrNativePipelineTake(b,&moved));
  }else CHECK(xlsIrNativePipelinePhase(b)==phase+1&&!xlsIrNativePipelineNodeCursor(b));
 }
 return 1;
}
static int nodesFinish(xls_ir_native_pipeline* b,XL6_DiagList* d,xls_ir_pipeline_status* status)
{
 if(!b||xlsIrNativePipelineState(b)!=XLS_PIPELINE_PREPARED)return xlsIrNativePipelineFinish(b,d,status);
 while(xlsIrNativePipelineState(b)==XLS_PIPELINE_PREPARED)if(!nodesAdvance(b,d,status))return 0;
 return xlsIrNativePipelineFinish(b,d,status);
}
static int suffixAndReset(const char* path)
{
 ++probePaused;xls_semantic_check* model=checked(path,resourceSource);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 char* expected=dump(xlsSemanticCheckIr(model));CHECK(expected);
 for(unsigned mode=0;mode<3;++mode){
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
  CHECK(!xlsIrNativePipelineAdvanceNodes(b,1,&d,&status)&&status==XLS_PIPELINE_INPUT&&!xlsIrNativePipelineNodeCursor(b));
  while(xlsIrNativePipelinePhase(b)<27)CHECK(xlsIrNativePipelineAdvance(b,&d,&status));
  size_t count=xlsIrNativePipelineModule(b)->nodeCount;CHECK(count>1);
  visiting=1;expectedPhase=27;expectedNode=0;expectedEnd=1;
  CHECK(xlsIrNativePipelineAdvanceNodes(b,1,&d,&status)&&expectedNode==1);visiting=0;
  if(mode==0){xlsIrNativePipelineReset(b);CHECK(!xlsIrNativePipelineNodeCursor(b)&&xlsIrNativePipelineState(b)==XLS_PIPELINE_EMPTY);continue;}
  visiting=1;expectedPhase=27;expectedNode=1;expectedEnd=count;
  if(mode==1)CHECK(xlsIrNativePipelineAdvance(b,&d,&status));
  else CHECK(xlsIrNativePipelineFinish(b,&d,&status));
  visiting=0;CHECK(expectedNode==count&&!xlsIrNativePipelineNodeCursor(b));
  CHECK(xlsIrNativePipelineFinish(b,&d,&status));char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected));xrtFree(actual);xlsIrNativePipelineReset(b);
 }
 xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);xlsSemanticCheckDestroy(model);xrtFree(expected);return 1;
}
static int selectedFunction(const char* path)
{
#if defined(XLS_NATIVE_IR_PIPELINE_COMPONENT) && XLS_NATIVE_IR_PIPELINE_COMPONENT && \
    defined(XLS_NATIVE_IR_PIPELINE_SOURCE) && defined(XLS_NATIVE_IR_TYPE_RESOURCE_GUARD)
 xstrbuf code={0};CHECK(xrtStrBufAppend(&code,xrtStrView("func target(int value) as int\n return value+1\nend\n")));
 for(unsigned i=0;i<256;++i)CHECK(xrtStrBufAppendFormat(&code,"func f%u(int value) as int\n int result=target(value)\n string local=\"value\"\n return result+%u\nend\n",i,i));
 ++probePaused;xls_semantic_check* model=checked(path,code.Data);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);xrtStrBufFree(&code);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
 while(xlsIrNativePipelinePhase(b)<26)CHECK(xlsIrNativePipelineAdvance(b,&d,&status));
 XL6_IrModule* module=&b->module;size_t first=SIZE_MAX,end=module->nodeCount;
 for(size_t i=0;i<module->nodeCount;++i){
  const XL6_IrNode* name=&module->nodes[i];if(!name->parent||name->parent>module->nodeCount||!name->text)continue;
  size_t parent=name->parent-1;if(module->nodes[parent].kind!=XL6_IR_NODE_FUNC_DECL)continue;
  if(!strcmp(name->text,"f128"))first=parent;
  else if(first!=SIZE_MAX&&!strcmp(name->text,"f129")){end=parent;break;}
 }
 CHECK(first!=SIZE_MAX&&end>first&&end-first<module->nodeCount/100&&!module->callSiteCount);
 size_t bytes=module->nodeCount*sizeof(*module->nodes);XL6_IrNode* before=xrtMalloc(bytes);CHECK(before);memcpy(before,module->nodes,bytes);
 visiting=1;expectedPhase=26;expectedNode=first;expectedEnd=end;
 CHECK(xlsIrPipelineNodes26(module,first,end));visiting=0;
 CHECK(expectedNode==end&&module->callSiteCount==1&&!d.count&&module->callSites[0].origin>first&&module->callSites[0].origin<=end);
 CHECK(!memcmp(before,module->nodes,first*sizeof(*before))&&!memcmp(before+end,module->nodes+end,(module->nodeCount-end)*sizeof(*before)));
 const XL6_IrModule* full=xlsSemanticCheckIr(model);size_t targetCalls=0;
 for(size_t i=0;i<full->callSiteCount;++i)targetCalls+=full->callSites[i].targetSymbol==module->callSites[0].targetSymbol;
 CHECK(targetCalls==256);
 printf("selected f128: %zu/%zu actual native call scan nodes;1/256 calls to target\n",end-first,module->nodeCount);
 xrtFree(before);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);xlsSemanticCheckDestroy(model);
#else
 (void)path;
#endif
 return 1;
}
static int nodesTests(void)
{
 CHECK(!xlsIrPipelineFixtureMain());
 char* path=xrtPathAbs("pipeline-node-suffix.xl");CHECK(path);CHECK(suffixAndReset(path));CHECK(selectedFunction(path));xrtFree(path);
 CHECK(!xrtAtomic64Load(&visitErrors,XMEMORY_RELAXED)&&xrtAtomic64Load(&slices,XMEMORY_RELAXED)>0);
 return 1;
}
int main(void)
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_pipeline_nodes_test passed:component unavailable");return 0;}
 xrtAtomic64Init(&visits,0);xrtAtomic64Init(&slices,0);xrtAtomic64Init(&visitErrors,0);
 if(!nodesTests())return 1;
 printf("analysis_ir_pipeline_nodes_test passed: %u checks;%llu actual native node visits;%llu slices;49 complete semantic inputs,partial/reset/suffix/full verifier\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),(unsigned long long)xrtAtomic64Load(&visits,XMEMORY_RELAXED),(unsigned long long)xrtAtomic64Load(&slices,XMEMORY_RELAXED));return 0;
}
