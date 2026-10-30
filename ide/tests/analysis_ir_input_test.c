#include "analysis/frontend_internal.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Actual generated IR TU; this private object never enters the product. */
#include "xl6_ir.c"
#include "analysis/module_store.h"
#include "analysis/worker_modules.h"
#define main inputWorkerBaseMain
#include "analysis_worker_cache_test.c"
#undef main

static size_t faultAt,faultCalls,faultDenied,liveAllocations;
static int faultEnabled,allocationOverflow;
static void* trackedAllocations[64];
static int refuse(void)
{if(faultEnabled && ++faultCalls==faultAt){++faultDenied;return 1;}return 0;}
static void remember(void* p)
{if(!p || !faultEnabled)return;for(size_t i=0;i<64;++i)if(trackedAllocations[i]==p)return;for(size_t i=0;i<64;++i)if(!trackedAllocations[i]){trackedAllocations[i]=p;++liveAllocations;return;}allocationOverflow=1;}
static void forget(void* p)
{if(!p)return;for(size_t i=0;i<64;++i)if(trackedAllocations[i]==p){trackedAllocations[i]=NULL;--liveAllocations;return;}}
void* __real_xrtCalloc(size_t,size_t);void __real_xrtFree(void*);
bool __real_xrtArrayResize(xarray*,size_t);void __real_xrtArrayUnit(xarray*);
bool __real_xrtStrBufReserve(xstrbuf*,size_t);void __real_xrtStrBufFree(xstrbuf*);
bool __real_xrtMapSet(xmap*,xbytesview,const void*);
void* __wrap_xrtCalloc(size_t n,size_t s){if(refuse())return NULL;void* p=__real_xrtCalloc(n,s);remember(p);return p;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
bool __wrap_xrtArrayResize(xarray* a,size_t n){if(refuse())return false;void* old=a->Data;bool r=__real_xrtArrayResize(a,n);if(a->Data!=old){forget(old);remember(a->Data);}return r;}
void __wrap_xrtArrayUnit(xarray* a){forget(a->Data);__real_xrtArrayUnit(a);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){if(refuse())return false;void* old=b->Data;bool r=__real_xrtStrBufReserve(b,n);if(b->Data!=old){forget(old);remember(b->Data);}return r;}
void __wrap_xrtStrBufFree(xstrbuf* b){forget(b->Data);__real_xrtStrBufFree(b);}
bool __wrap_xrtMapSet(xmap* m,xbytesview k,const void* v){if(refuse())return false;return __real_xrtMapSet(m,k,v);}

static xls_ir_input_work lastInput;
static unsigned inputBuilds,keyOracles,badKeys;
xls_ir_result* __real_xlsFrontendIrResultBuildTypedChecked(xls_typed_module*,XL6_DiagList*,
 const xls_ir_result*,size_t,const xls_entity_view*,size_t,xls_sem_memo*);
xls_ir_result* __wrap_xlsFrontendIrResultBuildTypedChecked(xls_typed_module* input,XL6_DiagList* diags,
 const xls_ir_result* previous,size_t bytes,const xls_entity_view* entities,size_t persistent,xls_sem_memo* memo)
{
 xls_ir_result* result=__real_xlsFrontendIrResultBuildTypedChecked(input,diags,previous,bytes,entities,persistent,memo);
 ++inputBuilds;lastInput=xlsFrontendIrResultInputWork(result);
 const XL6_AstNode* root=xlsTypedModuleRoot(input);
 if(result && result->memo)for(size_t i=0;i<result->memo->entries.Count;++i){
  const xls_ir_unit* unit=*(xls_ir_unit* const*)xrtArrayConstGet(&result->memo->entries,i);
  if(!unit)continue;
  xls_lexical_symbols symbols={0};xstrbuf key={0};
  int okay=xlsLexicalSymbolsBuild(&symbols,root->children[i],XLS_LEXICAL_SYMBOL_BYTES) &&
   xlsFrontendIrAstKeyEntities(&key,root->children[i],&symbols,entities) && xlsIrMemoEqual(&key,&unit->key);
  ++keyOracles;if(!okay)++badKeys;
  xlsLexicalSymbolsClear(&symbols);xrtStrBufFree(&key);
 }
 if(xlsActiveIrInput)++badKeys;
 return result;
}
static char* inputProgram(unsigned version)
{
 xstrbuf s={0};int okay=xrtStrBufAppend(&s,xrtStrView("\xef\xbb\xbfimport dep\r\n"));
 if(version==2)okay=okay && xrtStrBufAppend(&s,xrtStrView("// moved 中文😀\r\n"));
 if(version==3)okay=okay && xrtStrBufAppend(&s,xrtStrView("func inserted(int value) as int\r\n return value\r\nend\r\n"));
 for(unsigned i=0;okay && i<24;++i){
  if(version==4 && i==4)okay=xrtStrBufAppend(&s,xrtStrView("#doc\r\ndesc: modified 中文😀\r\n#end\r\n"));
  const char* name=version==5 && i==5 ? "other" : "value";
  okay=okay && xrtStrBufAppendFormat(&s,"func helper_%u(int %s) as int\r\n",i,name);
  if(version==6 && i==6)okay=okay && xrtStrBufAppend(&s,xrtStrView(" int additional=1\r\n"));
  okay=okay && xrtStrBufAppendFormat(&s," if %s>0 then\r\n return %s+%u\r\n end\r\n return 0\r\nend\r\n",name,name,i+(i==0 ? version+1 : 0));
 }
 okay=okay && xrtStrBufAppend(&s,xrtStrView("func main(...) as int\r\n int unique_main=dep.compute(helper_0(7))\r\n return unique_main\r\nend\r\n"));
 char* result=okay ? xrtStrDup(s.Data) : NULL;xrtStrBufFree(&s);return result;
}
static char* inputDump(const XL6_IrModule* ir)
{
 XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,ir);
 char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;
}
static int pipeline(void)
{
 char* directory=xrtPathAbs(".");char *entry=xrtPathJoin(directory,"input-main.xl"),*dep=xrtPathJoin(directory,"dep.xl"),*project=xrtPathJoin(directory,"input-test.xlp");
 CHECK(directory && entry && dep && project);
 const char* members[]={entry,dep};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="ir-final-input",.targetKind="exe"};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};
 xls_context* context=xlsContextCreate(&spec,1024u*1024u,NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);
 xls_worker* worker=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(context && store && worker);
 xls_analysis_document docs[2]={{"entry",entry,NULL,1,0},{"dep",dep,NULL,2,0}};
 uint64_t reused=0,avoided=0;fixture_result held={0};char* heldDump=NULL;
 for(unsigned v=0;v<9;++v){
  char* source=inputProgram(v);const char* dependency=v==7 ? "export func compute(int value) as int\r\n return value+2\r\nend\r\n" : "export func compute(int value) as int\r\n return value+1\r\nend\r\n";CHECK(source);
  xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);
  docs[0].text=xlsTextCreate(source,strlen(source),1024u*1024u,NULL);docs[1].text=xlsTextCreate(dependency,strlen(dependency),1024u*1024u,NULL);
  docs[0].revision=docs[1].revision=v+1;CHECK(docs[0].text && docs[1].text);
  unsigned before=inputBuilds;fixture_result r=query(worker,CHECK_KIND,context,docs,2,"entry","entry");
  CHECK(r.state==XLS_QUERY_READY && xlsSemanticCheckInfo(r.check).accepted && inputBuilds==before+1 && !badKeys);
  xls_ir_input_work work=lastInput;reused+=work.reused;avoided+=work.avoidedBytes;
  fprintf(stdout,"input-v%u encoded=%llu reused=%llu captured=%llu avoided=%llu proof=%llu\n",v,(unsigned long long)work.encoded,(unsigned long long)work.reused,(unsigned long long)work.captured,(unsigned long long)work.avoidedBytes,(unsigned long long)work.bytes);
  /* Original raw capture declines the first interning/type-changing function
   * and main's new vararg prefix. Only the24 eligible fragments get proofs. */
  if(!v)CHECK(work.encoded==26 && work.captured==24 && !work.reused && work.bytes);
  if(v==1)CHECK(work.reused>=23 && work.encoded && work.avoidedBytes && !work.copiedBytes);
  CHECK(parity(&r,CHECK_KIND,context,docs,2,"entry","entry"));
  xls_semantic_input inputs[2];for(size_t i=0;i<2;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
  xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,entry,inputs,2,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
  char *a=inputDump(xlsSemanticCheckIr(r.check)),*b=inputDump(xlsSemanticCheckIr(fresh));CHECK(a && b && !strcmp(a,b));xrtFree(b);xlsSemanticCheckDestroy(fresh);
  if(!v){held=r;heldDump=a;}else{releaseResult(&r);xrtFree(a);}
  xrtFree(source);
 }
 CHECK(reused>=23 && avoided && keyOracles>=9*24 && !badKeys);
 xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xlsWorkerDestroy(worker);xlsModuleStoreRelease(store);xlsContextRelease(context);
 char* old=inputDump(xlsSemanticCheckIr(held.check));CHECK(old && !strcmp(old,heldDump));xrtFree(old);xrtFree(heldDump);releaseResult(&held);
 xrtFree(directory);xrtFree(entry);xrtFree(dep);xrtFree(project);return 1;
}
static int projections(void)
{
 xls_typed_input_identity *id=xlsTypedInputIdentityCreate(),*other=xlsTypedInputIdentityCreate();CHECK(id && other);
 xls_lexical_symbols symbols={0};CHECK(xrtArrayInit(&symbols.order,sizeof(xls_lexical_symbol)));
 xls_lexical_symbol row={3,1};CHECK(xrtArrayPush(&symbols.order,&row));symbols.ready=1;
 xls_ir_input_key key={.identity=id};key.builder.initialized=key.builder.okay=1;
 CHECK(xrtMapInit(&key.builder.seen,sizeof(size_t)) && xrtArrayInit(&key.builder.entities,sizeof(xls_ir_entity_projection)));
 xlsIrInputObserve(&key.builder,0,0);xlsIrInputObserve(&key.builder,UINT32_MAX,0);xlsIrInputObserve(&key.builder,UINT32_MAX,0);
 CHECK(key.builder.okay && key.builder.entities.Count==1);
 CHECK(!xlsIrInputCreate(&key,&symbols,NULL,sizeof(xls_ir_input_proof)));
 xls_ir_input_proof* proof=xlsIrInputCreate(&key,&symbols,NULL,XLS_IR_INPUT_BYTES);CHECK(proof);
 CHECK(xlsIrInputMatch(proof,id,&symbols,NULL));CHECK(!xlsIrInputMatch(proof,other,&symbols,NULL));
 ((xls_ir_entity_projection*)proof->entities.Data)[0].identity=1;CHECK(!xlsIrInputMatch(proof,id,&symbols,NULL));
 ((xls_ir_entity_projection*)proof->entities.Data)[0].identity=0;
 ((xls_lexical_symbol*)symbols.order.Data)[0].symbol=4;CHECK(!xlsIrInputMatch(proof,id,&symbols,NULL));
 ((xls_lexical_symbol*)symbols.order.Data)[0].symbol=3;proof->domain=1;CHECK(!xlsIrInputMatch(proof,id,&symbols,NULL));proof->domain=0;
 symbols.ready=0;CHECK(!xlsIrInputMatch(proof,id,&symbols,NULL));symbols.ready=1;
 xlsIrInputObserve(&key.builder,UINT32_MAX,1);CHECK(!key.builder.okay);
 xlsIrInputProofRelease(proof);xlsIrInputBuilderClear(&key.builder);xlsLexicalSymbolsClear(&symbols);
 xlsTypedInputIdentityRelease(id);xlsTypedInputIdentityRelease(other);return 1;
}
static int borrowedKeys(void)
{
 xstrbuf original={0};CHECK(xrtStrBufAppend(&original,xrtStrView("borrowed canonical key")));
 xls_ir_memo memo={0};xls_ir_memo_build build={.memo=&memo};
 xls_ir_input_key state={.borrowed=1};xstrbuf key=original;
 CHECK(!xlsIrInputKeyOwn(&state,&build,&key,original.Size-1));
 CHECK(state.borrowed && key.Data==original.Data && !memo.inputWork.copiedBytes);
 xlsIrInputKeyEnd(&state,&build,&key);CHECK(!key.Data && !strcmp(original.Data,"borrowed canonical key"));
 state=(xls_ir_input_key){.borrowed=1};key=original;
 CHECK(xlsIrInputKeyOwn(&state,&build,&key,1024));
 CHECK(!state.borrowed && key.Data!=original.Data && xlsIrMemoEqual(&key,&original) && memo.inputWork.copiedBytes==original.Size);
 xlsIrInputKeyEnd(&state,&build,&key);xrtStrBufFree(&key);CHECK(!strcmp(original.Data,"borrowed canonical key"));
 xrtStrBufFree(&original);
 xls_ir_input_builder builder={.initialized=1,.okay=1};CHECK(xrtMapInit(&builder.seen,sizeof(size_t)) && xrtArrayInit(&builder.entities,sizeof(xls_ir_entity_projection)));
 for(unsigned i=1;i<=XLS_IR_INPUT_QUERIES;++i)xlsIrInputObserve(&builder,i,0);
 CHECK(builder.okay && builder.entities.Count==XLS_IR_INPUT_QUERIES);
 xlsIrInputObserve(&builder,XLS_IR_INPUT_QUERIES+1,0);CHECK(!builder.okay && builder.entities.Count==XLS_IR_INPUT_QUERIES);
 xlsIrInputBuilderClear(&builder);return 1;
}
static int inputFaults(void)
{
 xls_typed_input_identity* id=xlsTypedInputIdentityCreate();CHECK(id);
 xls_lexical_symbols symbols={0};CHECK(xrtArrayInit(&symbols.order,sizeof(xls_lexical_symbol)));
 xls_lexical_symbol row={3,1};CHECK(xrtArrayPush(&symbols.order,&row));symbols.ready=1;
 size_t sites=0;
 for(size_t site=0;site<=sites;++site){
  xls_ir_input_key key={.identity=id};key.builder.initialized=key.builder.okay=1;
  CHECK(xrtMapInit(&key.builder.seen,sizeof(size_t)) && xrtArrayInit(&key.builder.entities,sizeof(xls_ir_entity_projection)));
  xlsIrInputObserve(&key.builder,UINT32_MAX,0);CHECK(key.builder.okay);
  faultCalls=faultDenied=0;faultAt=site;faultEnabled=1;
  xls_ir_input_proof* proof=xlsIrInputCreate(&key,&symbols,NULL,XLS_IR_INPUT_BYTES);faultEnabled=0;
  if(!site){sites=faultCalls;CHECK(proof && sites>=2 && sites<20);}else CHECK(!proof && faultDenied==1);
  xlsIrInputProofRelease(proof);xlsIrInputBuilderClear(&key.builder);CHECK(!liveAllocations && !allocationOverflow);
 }
 xls_ir_input_builder builder={.initialized=1,.okay=1};CHECK(xrtMapInit(&builder.seen,sizeof(size_t)) && xrtArrayInit(&builder.entities,sizeof(xls_ir_entity_projection)));
 faultCalls=faultDenied=0;faultAt=1;faultEnabled=1;xlsIrInputObserve(&builder,UINT32_MAX,0);faultEnabled=0;
 CHECK(faultDenied==1 && !builder.okay);xlsIrInputBuilderClear(&builder);
 xstrbuf original={0};CHECK(xrtStrBufAppend(&original,xrtStrView("allocation failure preserves borrowed input")));
 xls_ir_memo memo={0};xls_ir_memo_build build={.memo=&memo};xls_ir_input_key key={.borrowed=1};xstrbuf copy=original;
 faultCalls=faultDenied=0;faultAt=1;faultEnabled=1;int owned=xlsIrInputKeyOwn(&key,&build,&copy,1024);faultEnabled=0;
 CHECK(!owned && faultDenied==1 && key.borrowed && copy.Data==original.Data && !memo.inputWork.copiedBytes);
 xlsIrInputKeyEnd(&key,&build,&copy);CHECK(!copy.Data && !liveAllocations);xrtStrBufFree(&original);
 xlsLexicalSymbolsClear(&symbols);xlsTypedInputIdentityRelease(id);return 1;
}
int main(void)
{if(!projections() || !borrowedKeys() || !inputFaults() || !pipeline())return 1;printf("analysis_ir_input_test passed: %u checks,%u complete canonical key oracles;actual checked pipeline and fresh full IR/AST/diagnostics,negative entity and lexical/domain/identity guards,bounded queries,real allocation failures and borrowed key ownership,held owners\n",checks,keyOracles);return 0;}
#else
int main(void){puts("analysis_ir_input_test passed: legacy retains original full keys");return 0;}
#endif
