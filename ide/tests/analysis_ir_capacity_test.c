#include "analysis/frontend_internal.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static void capacityEvent(int,size_t,int);
#define XLS_IR_CAPACITY_EVENT(begin,bytes,okay) capacityEvent(begin,bytes,okay)
#include "xl6_ir.c"
#define main capacityWorkerBaseMain
#include "analysis_worker_cache_test.c"
#undef main

static _Thread_local int capacityDepth;
static xatomic32 capacityRecording,capacityRefuse;
static xatomic64 capacityAttempts,capacityDenied,capacityMaximum;
static void capacityEvent(int begin,size_t bytes,int okay)
{
 (void)okay;
 if(!xrtAtomic32Load(&capacityRecording,XMEMORY_ACQUIRE))return;
 if(begin){
  capacityDepth=1;xrtAtomic64FetchAdd(&capacityAttempts,1,XMEMORY_RELAXED);
  uint64 old=xrtAtomic64Load(&capacityMaximum,XMEMORY_RELAXED);
  while(old<bytes && !xrtAtomic64CompareExchange(&capacityMaximum,&old,bytes,XMEMORY_RELAXED,XMEMORY_RELAXED)){}
 }else capacityDepth=0;
}
void* __real_xrtRealloc(void*,size_t);
void* __wrap_xrtRealloc(void* pointer,size_t bytes)
{
 if(capacityDepth && xrtAtomic32Load(&capacityRefuse,XMEMORY_ACQUIRE)){
  xrtAtomic64FetchAdd(&capacityDenied,1,XMEMORY_RELAXED);return NULL;
 }
 return __real_xrtRealloc(pointer,bytes);
}
static void capacityReset(int refuse)
{
 xrtAtomic64Store(&capacityAttempts,0,XMEMORY_RELAXED);xrtAtomic64Store(&capacityDenied,0,XMEMORY_RELAXED);
 xrtAtomic64Store(&capacityMaximum,0,XMEMORY_RELAXED);xrtAtomic32Store(&capacityRefuse,refuse,XMEMORY_RELEASE);
}
static uint64 capacityCount(void){return xrtAtomic64Load(&capacityAttempts,XMEMORY_RELAXED);}
static int capacityVectors(void)
{
 const size_t counts[]={0,1,63,64,65,127,128,129};
 for(unsigned fail=0;fail<2;++fail)for(size_t i=0;i<sizeof(counts)/sizeof(*counts);++i){
  size_t allocated=64;unsigned char* data=xrtMalloc(allocated);CHECK(data);memset(data,0xa5,allocated);void* original=data;
  capacityReset(fail);xlsIrCapacityReserve((void**)&data,&allocated,counts[i],1);
  if(counts[i]<=64)CHECK(!capacityCount() && data==original && allocated==64);
  else if(fail)CHECK(capacityCount()==1 && data==original && allocated==64 && xrtAtomic64Load(&capacityDenied,XMEMORY_RELAXED)==1);
  else CHECK(capacityCount()==1 && allocated>=counts[i] && allocated<2*counts[i]);
  for(unsigned j=0;j<64;++j)CHECK(data[j]==0xa5);
  xrtFree(data);
 }
 capacityReset(0);size_t count=0;void* value=NULL;
 xlsIrCapacityReserve(&value,&count,1,SIZE_MAX);xlsIrCapacityReserve(&value,&count,SIZE_MAX,2);
 CHECK(!value && !count && !capacityCount());xlsIrCapacityReserve(&value,&count,64,0);CHECK(!value && !count && !capacityCount());
 return 1;
}
static int capacityProofs(void)
{
 xls_typed_input_identity *id=xlsTypedInputIdentityCreate(),*other=xlsTypedInputIdentityCreate();CHECK(id && other);
 xls_ir_input_proof proof={.identity=id};xls_ir_unit unit={.nodes=100,.children=99};size_t nodes=1,children=3;
 CHECK(!xlsIrCapacityAdd(NULL,&proof,id,&nodes,&children));CHECK(!xlsIrCapacityAdd(&unit,NULL,id,&nodes,&children));
 CHECK(!xlsIrCapacityAdd(&unit,&proof,NULL,&nodes,&children));CHECK(!xlsIrCapacityAdd(&unit,&proof,other,&nodes,&children));CHECK(nodes==1 && children==3);
 CHECK(xlsIrCapacityAdd(&unit,&proof,id,&nodes,&children) && nodes==101 && children==102);
 unit.nodes=0;CHECK(!xlsIrCapacityAdd(&unit,&proof,id,&nodes,&children));unit.nodes=UINT32_MAX;
 CHECK(!xlsIrCapacityAdd(&unit,&proof,id,&nodes,&children) && nodes==101 && children==102);
 unit.nodes=100;unit.children=SIZE_MAX;CHECK(!xlsIrCapacityAdd(&unit,&proof,id,&nodes,&children) && nodes==101 && children==102);
 xlsTypedInputIdentityRelease(id);xlsTypedInputIdentityRelease(other);return 1;
}
static int capacityCancellation(void* user){return *(int*)user;}
static int capacityCancel(void)
{
 XL6_AstNode other={.kind=XL6_AST_IMPORT};XL6_AstNode* children[65];for(unsigned i=0;i<65;++i)children[i]=&other;
 XL6_AstNode root={.kind=XL6_AST_MODULE,.children=children,.childCount=65};XL6_IrModule module={0};xls_ir_memo memo={0},previous={0};memo.entries.Count=65;
 xls_ir_memo_build build={.root=&root,.module=&module,.memo=&memo,.baseline=&previous};
 /* No function is visited: these opaque sentinels are never dereferenced.
  * The real cancellation scope observes the bounded root-array scan. */
 xls_ir_input_scope scope={xlsActiveIrInput,(xls_sem_memo*)1,(xls_typed_module*)1,&root};xlsActiveIrInput=&scope;
 for(unsigned mode=0;mode<3;++mode){
  int cancelled=mode!=0;xls_frontend_cancel_scope cancel;xls_frontend_cancel_work work;xlsFrontendCancelBegin(&cancel,capacityCancellation,&cancelled);capacityReset(0);
  if(mode==2)CHECK(xlsFrontendCancelPoll());
  xlsIrCapacityForest(&build,&module,&root,NULL);xlsFrontendCancelEnd(&cancel,&work);
  CHECK(!capacityCount() && !module.nodes && !module.children);
  CHECK(work.cancelled==(int)(mode!=0) && work.predicateCalls==(mode ? 1u:2u));
 }
 xlsActiveIrInput=scope.previous;return 1;
}
static char* capacityProgram(unsigned version,int imported)
{
 xstrbuf text={0};int okay=xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf// 中文😀\r\n"));
 for(unsigned i=0;okay && i<64;++i){
  okay=xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local=value+%u\r\n",i,i);
  unsigned count=version==3 || (version==2 && !i) ? 1 : 24;
  for(unsigned j=0;okay && j<count;++j)okay=xrtStrBufAppendFormat(&text," local=local+%u\r\n",j);
  okay=okay && xrtStrBufAppend(&text,xrtStrView(" return local\r\nend\r\n"));
 }
 okay=okay && xrtStrBufAppendFormat(&text,"export func compute(int value) as int\r\n return work_0(value)+%u\r\nend\r\n",version);
 if(!imported)okay=okay && xrtStrBufAppendFormat(&text,"func main(...) as int\r\n int unique_A=compute(3)\r\n return unique_A+%u\r\nend\r\n",version==3 ? 3:0);
 char* result=okay ? xrtStrDup(text.Data):NULL;xrtStrBufFree(&text);return result;
}
static char* capacityDump(const XL6_IrModule* module)
{
 XL6_Output output;if(!xl6OutputOpenMemory(&output))return NULL;xl6IrDumpModuleJson(&output,module);
 char* result=xl6OutputHasFailed(&output) ? NULL:xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);return result;
}
static int capacityPipeline(int imported,int fail)
{
 char *directory=xrtPathAbs("."),*entry=xrtPathJoin(directory,"capacity-中文😀.xl"),*dep=xrtPathJoin(directory,"dep.xl"),*project=xrtPathJoin(directory,"capacity.xlp");CHECK(directory && entry && dep && project);
 const char* members[]={entry,dep};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="ir-capacity",.targetKind="exe"};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,imported ? 2:1};xls_context* context=xlsContextCreate(&spec,1024u*1024u,NULL);CHECK(context);
 xls_worker* worker=xlsWorkerCreate(NULL,4);CHECK(worker);xls_analysis_document docs[2]={{"entry",entry,NULL,1,1},{"dep",dep,NULL,2,1}};fixture_result held={0};char* heldDump=NULL;
 for(unsigned version=0;version<4;++version){
  char* program=capacityProgram(version,imported);CHECK(program);char main[256];snprintf(main,sizeof(main),"import dep\r\nfunc main(...) as int\r\n int unique_A=dep.compute(3)\r\n return unique_A+%u\r\nend\r\n",version==3 ? 3:0);
  xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);docs[0].text=xlsTextCreate(imported ? main:program,strlen(imported ? main:program),1024u*1024u,NULL);
  docs[1].text=imported ? xlsTextCreate(program,strlen(program),1024u*1024u,NULL):NULL;docs[0].revision=docs[1].revision=version+1;CHECK(docs[0].text && (!imported || docs[1].text));
  capacityReset(fail);fixture_result result=query(worker,CHECK_KIND,context,docs,imported ? 2:1,"entry","entry");CHECK(result.state==XLS_QUERY_READY && xlsSemanticCheckInfo(result.check).accepted);
  uint64 attempts=capacityCount(),denied=xrtAtomic64Load(&capacityDenied,XMEMORY_RELAXED),maximum=xrtAtomic64Load(&capacityMaximum,XMEMORY_RELAXED);
  if(!version)CHECK(!attempts && !denied);
  if(version==1)CHECK(attempts==2 && (fail ? denied==2:!denied));
  if(version==3)CHECK(maximum<1024u*sizeof(XL6_IrNode));
  CHECK(parity(&result,CHECK_KIND,context,docs,imported ? 2:1,"entry","entry"));
  xls_semantic_input inputs[2];for(unsigned i=0;i<(imported ? 2u:1u);++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
  xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,entry,inputs,imported ? 2:1,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
  char *a=capacityDump(xlsSemanticCheckIr(result.check)),*b=capacityDump(xlsSemanticCheckIr(fresh));CHECK(a && b && !strcmp(a,b));xrtFree(b);xlsSemanticCheckDestroy(fresh);
  printf("capacity-import%d-fail%d-v%u attempts=%llu refused=%llu maximum=%llu\n",imported,fail,version,(unsigned long long)attempts,(unsigned long long)denied,(unsigned long long)maximum);
  if(!version){held=result;heldDump=a;}else{releaseResult(&result);xrtFree(a);}xrtFree(program);
 }
 xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xlsWorkerDestroy(worker);xlsContextRelease(context);
 char* old=capacityDump(xlsSemanticCheckIr(held.check));CHECK(old && !strcmp(old,heldDump));xrtFree(old);xrtFree(heldDump);releaseResult(&held);
 xrtFree(project);xrtFree(dep);xrtFree(entry);xrtFree(directory);return 1;
}
int main(void)
{
 if(capacityWorkerBaseMain())return 1;
 unsigned before=checks;xrtAtomic32Init(&capacityRecording,1);xrtAtomic32Init(&capacityRefuse,0);xrtAtomic64Init(&capacityAttempts,0);xrtAtomic64Init(&capacityDenied,0);xrtAtomic64Init(&capacityMaximum,0);
 if(!capacityVectors() || !capacityProofs() || !capacityCancel())return 1;
 for(int imported=0;imported<2;++imported)for(int fail=0;fail<2;++fail)if(!capacityPipeline(imported,fail))return 1;
 printf("analysis_ir_capacity_test passed: %u new checks; identity-bounded reservations, exact full IR/AST parity, shrinking edits, real allocation refusals and held lifetime\n",checks-before);return 0;
}
#else
int main(void){puts("analysis_ir_capacity_test skipped: original native cohort retained");return 0;}
#endif
