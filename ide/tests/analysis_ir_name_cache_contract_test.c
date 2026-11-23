#include "analysis/frontend_internal.h"
#include "analysis/ir_native_name_cache.h"
static void namePoison(void*,size_t);
#define XLS_NAME_CACHE_UNIT_FREE_TEST namePoison
#include "xl6_ir.c"
#define XLS_IR_NAME_CACHE_LIBRARY 1
#include "analysis_ir_name_cache_test.c"
static xatomic32 poisoned;
static void namePoison(void* p,size_t bytes)
{xrtAtomic32FetchAdd(&poisoned,1,XMEMORY_RELAXED);memset(p,0,bytes);}
#if defined(XLS_NATIVE_IR_PIPELINE_COMPONENT) && XLS_NATIVE_IR_PIPELINE_COMPONENT && \
    defined(XLS_NATIVE_IR_PIPELINE_SOURCE) && defined(XLS_NATIVE_IR_TYPE_RESOURCE_GUARD)
static int privateContexts(const char* path)
{
 const char* source="class ImplicitBox\n string item=\"owned\"\n string other=\"other\"\n func value() as string\n return item\n end\nend\n";
 ++probePaused;xls_semantic_check* model=checked(path,source);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);xls_ir_name_cache* previous=NULL;CHECK(b);
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineReset(b);
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));while(xlsIrNativePipelinePhase(b)<18)CHECK(xlsIrNativePipelineAdvance(b,&d,&status));
 unsigned char* key=xrtMalloc(16u*1024u*1024u);CHECK(key);xls_ir_input_trace trace={.module=&b->module,.key=key,.capacity=16u*1024u*1024u};int queried=0;
 for(size_t i=0;i<previous->units.Count&&!queried;++i){xls_name_unit* unit=*(xls_name_unit**)xrtArrayGet(&previous->units,i);if(!unit)continue;
  for(size_t j=0;j<unit->reads;++j)if(unit->ids[j].kind==1){
   XL6_IrTypeId id=unit->ids[j].id;if(!id||id>b->module.types.count)continue;CHECK(xlsNameMatches(&trace,unit,unit->first,unit->end));
   XL6_SymbolId saved=b->module.types.items[id-1].nominalSymbol;b->module.types.items[id-1].nominalSymbol=saved^1u;
   CHECK(!xlsNameMatches(&trace,unit,unit->first,unit->end));b->module.types.items[id-1].nominalSymbol=saved;CHECK(xlsNameMatches(&trace,unit,unit->first,unit->end));queried=1;break;
  }
 }
 CHECK(queried);xrtFree(key);
 size_t capacity=b->module.symbolNodeCapacity;CHECK(capacity<SIZE_MAX/sizeof(*b->module.symbolNodes)-1);
 XL6_IrNodeId* nodes=xrtRealloc(b->module.symbolNodes,(capacity+1)*sizeof(*nodes));CHECK(nodes);b->module.symbolNodes=nodes;nodes[capacity]=0;b->module.symbolNodeCapacity=capacity+1;
 xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;
 CHECK(xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&next&&!work.reused&&work.checked);
 xlsIrNameCacheDestroy(previous);xlsIrNameCacheDestroy(next);CHECK(xl6IrVerifyModule(&b->module,&d));xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);xlsSemanticCheckDestroy(model);return 1;
}
typedef struct name_reader {const XL6_AstNode* root;const char* expected;const xls_ir_name_cache* previous;int passed;} name_reader;
static int nameReaderCheck(name_reader* reader)
{
 for(unsigned pass=0;pass<3;++pass){
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;
  CHECK(!xlsNamesActiveTrace&&xlsIrNativePipelinePrepare(b,reader->root,&d,0,&status));
  xls_name_trace outer={.input={.failed=1,.reads=123}};xlsNamesActiveTrace=&outer;
  int okay=xlsIrNativePipelineFinishCachedNames(b,&d,reader->previous,16u*1024u*1024u,&next,&work,&status);
  CHECK(xlsNamesActiveTrace==&outer&&outer.input.failed==1&&outer.input.reads==123&&!b->names);xlsNamesActiveTrace=NULL;
  CHECK(okay&&next&&work.reused&&!d.count);char* actual=dump(&b->module);CHECK(actual&&!strcmp(actual,reader->expected));xrtFree(actual);xlsIrNameCacheDestroy(next);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 return 1;
}
static int32 nameReaderThread(void* p){name_reader* r=p;r->passed=nameReaderCheck(r);return r->passed?0:1;}
static int concurrent(const char* path)
{
 xls_semantic_check* model[2];char* expected[2];
 for(unsigned i=0;i<2;++i){char* source=nameSource(i);CHECK(source);++probePaused;model[i]=checked(path,source);--probePaused;xrtFree(source);CHECK(model[i]&&xlsSemanticCheckInfo(model[i]).accepted);expected[i]=dump(xlsSemanticCheckIr(model[i]));CHECK(expected[i]);}
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_name_cache* previous=NULL;
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model[0]),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 name_reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(name_reader){xlsSemanticCheckAst(model[i%2]),expected[i%2],previous,0};threads[i]=xrtThreadCreate(nameReaderThread,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].passed);xrtThreadDestroy(threads[i]);}
 xlsIrNameCacheDestroy(previous);
 for(unsigned i=0;i<2;++i){CHECK(xlsSemanticCheckValidate(model[i]));xlsSemanticCheckDestroy(model[i]);xrtFree(expected[i]);}
 return 1;
}
static int layoutInputs(const char* path)
{
 const char* text="class ImplicitBox\n string item=\"owned\"\n string other=\"other\"\n func value() as string\n return item\n end\nend\n";
 ++probePaused;xls_semantic_check* model=checked(path,text);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b[2];xls_ir_name_cache* previous=NULL;
 b[0]=xlsIrNativePipelineCreate(&status);CHECK(b[0]&&xlsIrNativePipelinePrepare(b[0],xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b[0],&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineReset(b[0]);
 b[1]=xlsIrNativePipelineCreate(&status);CHECK(b[1]);
 for(unsigned k=0;k<2;++k){CHECK(xlsIrNativePipelinePrepare(b[k],xlsSemanticCheckAst(model),&d,0,&status));while(xlsIrNativePipelinePhase(b[k])<18)CHECK(xlsIrNativePipelineAdvance(b[k],&d,&status));}
 XL6_IrNodeId declaration=0;const XL6_IrModule* reference=xlsSemanticCheckIr(model);
 for(size_t i=0;i<reference->nodeCount&&!declaration;++i)if(reference->nodes[i].implicitNameAbi==XL6_IR_IMPLICIT_NAME_CLASS_FIELD)declaration=reference->nodes[i].implicitFieldDeclaration;
 CHECK(declaration);size_t field=SIZE_MAX;
 for(size_t i=0;i<b[0]->module.classFieldCount;++i)if(b[0]->module.classFields[i].declaration==declaration){field=i;break;}
 CHECK(field!=SIZE_MAX&&field<b[1]->module.classFieldCount);char* saved[2];
 for(unsigned k=0;k<2;++k){saved[k]=b[k]->module.classFields[field].name;b[k]->module.classFields[field].name="__changed_native_field_name__";}
 xls_name_build cached={.previous=previous,.limit=16u*1024u*1024u};xls_ir_type_resource_scope resource={.previous=xlsActiveTypeResource};xlsActiveTypeResource=&resource;
 int okay=xlsNamePhase(&b[0]->module,&cached);xlsActiveTypeResource=resource.previous;CHECK(okay&&!resource.failed&&cached.next&&cached.work.checked&&!cached.work.reused);
 CHECK(xl6IrBuildImplicitNameAbis(&b[1]->module));size_t changed=0;CHECK(b[0]->module.nodeCount==b[1]->module.nodeCount);
 for(size_t i=0;i<b[0]->module.nodeCount;++i){const XL6_IrNode* a=b[0]->module.nodes+i;const XL6_IrNode* c=b[1]->module.nodes+i;
  CHECK(a->implicitNameAbi==c->implicitNameAbi&&a->implicitReceiverType==c->implicitReceiverType&&a->implicitFieldDeclaration==c->implicitFieldDeclaration);
  changed+=a->implicitNameAbi!=reference->nodes[i].implicitNameAbi||a->implicitReceiverType!=reference->nodes[i].implicitReceiverType||a->implicitFieldDeclaration!=reference->nodes[i].implicitFieldDeclaration;
 }
 CHECK(changed);xlsIrNameCacheDestroy(cached.next);
 for(unsigned k=0;k<2;++k){b[k]->module.classFields[field].name=saved[k];xlsIrNativePipelineDestroy(b[k]);}
 /* Existing phase outputs are not inputs. Seed every slot with invalid old
  * values,then require hits to overwrite them before full native verification. */
 b[0]=xlsIrNativePipelineCreate(&status);CHECK(b[0]&&xlsIrNativePipelinePrepare(b[0],xlsSemanticCheckAst(model),&d,0,&status));while(xlsIrNativePipelinePhase(b[0])<18)CHECK(xlsIrNativePipelineAdvance(b[0],&d,&status));
 for(size_t i=0;i<b[0]->module.nodeCount;++i){b[0]->module.nodes[i].implicitNameAbi=(XL6_IrImplicitNameAbi)UINT32_MAX;b[0]->module.nodes[i].implicitReceiverType=UINT32_MAX;b[0]->module.nodes[i].implicitFieldDeclaration=UINT32_MAX;}
 xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;CHECK(xlsIrNativePipelineFinishCachedNames(b[0],&d,previous,16u*1024u*1024u,&next,&work,&status)&&next&&!work.checked&&!work.nativeNodes&&work.reused&&!d.count);
 char* expected=dump(reference);xlsSemanticCheckDestroy(model);xlsIrNameCacheDestroy(previous);xlsIrNameCacheDestroy(next);char* actual=dump(&b[0]->module);CHECK(expected&&actual&&!strcmp(expected,actual));
 xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b[0]);xl6DiagFree(&d);return 1;
}

static int lateMismatch(const char* path)
{
 ++probePaused;xls_semantic_check* model=checked(path,nameResourceSource);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b[2];xls_ir_name_cache* previous=NULL;
 b[0]=xlsIrNativePipelineCreate(&status);CHECK(b[0]&&xlsIrNativePipelinePrepare(b[0],xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b[0],&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineReset(b[0]);
 const XL6_IrModule* reference=xlsSemanticCheckIr(model);XL6_IrNodeId id=0;
 for(size_t i=0;i<reference->nodeCount;++i)if(reference->nodes[i].implicitNameAbi==XL6_IR_IMPLICIT_NAME_CLASS_FIELD&&reference->nodes[i].text&&!strcmp(reference->nodes[i].text,"item"))id=(XL6_IrNodeId)(i+1);
 CHECK(id);const xls_name_unit* selected=NULL;size_t typeReads=0;
 for(size_t i=0;i<previous->units.Count;++i){
  const xls_name_unit* u=*(xls_name_unit*const*)xrtArrayConstGet(&previous->units,i);CHECK(u);
  unsigned char* nodes=xrtCalloc(reference->nodeCount+1,1);unsigned char* types=xrtCalloc(reference->types.count+1,1);CHECK(nodes&&types);size_t covered=0;
  for(size_t j=0;j<u->reads;++j){xls_ir_input_read v=u->ids[j];
   if(v.kind==1){if(v.id&&v.id<=reference->types.count){CHECK(!types[v.id]);types[v.id]=1;++typeReads;}}
   else{CHECK(v.id&&v.id<=reference->nodeCount&&!nodes[v.id]);nodes[v.id]=1;if(v.id>u->first&&v.id<=u->end)++covered;}
  }
  CHECK(covered==u->end-u->first);xrtFree(nodes);xrtFree(types);
  if(id>u->first&&id<=u->end)selected=u;
 }
 CHECK(selected&&selected->reads>64&&typeReads);
 b[1]=xlsIrNativePipelineCreate(&status);CHECK(b[1]);
 for(unsigned k=0;k<2;++k){CHECK(xlsIrNativePipelinePrepare(b[k],xlsSemanticCheckAst(model),&d,0,&status));while(xlsIrNativePipelinePhase(b[k])<18)CHECK(xlsIrNativePipelineAdvance(b[k],&d,&status));}
 char* saved[2];for(unsigned k=0;k<2;++k){saved[k]=b[k]->module.nodes[id-1].text;b[k]->module.nodes[id-1].text="faux";}
 for(size_t i=selected->first;i<selected->end;++i){b[0]->module.nodes[i].implicitNameAbi=(XL6_IrImplicitNameAbi)UINT32_MAX;b[0]->module.nodes[i].implicitReceiverType=UINT32_MAX;b[0]->module.nodes[i].implicitFieldDeclaration=UINT32_MAX;}
 unsigned char* key=xrtMalloc(16u*1024u*1024u);CHECK(key);xls_ir_input_trace trace={.module=&b[0]->module,.key=key,.capacity=16u*1024u*1024u};
 /* Same-sized changed spelling is rejected by the final input comparison,
  * after restored values have actually overwritten the sentinel slots. */
 CHECK(!xlsNameMatchesApply(&trace,selected,selected->first,selected->end,&b[0]->module));size_t touched=0;
 for(size_t i=selected->first;i<selected->end;++i)touched+=b[0]->module.nodes[i].implicitNameAbi!=(XL6_IrImplicitNameAbi)UINT32_MAX;
 CHECK(touched&&b[0]->module.nodes[id-1].implicitNameAbi==reference->nodes[id-1].implicitNameAbi);
 xls_name_build cached={.previous=previous,.limit=16u*1024u*1024u};xls_ir_type_resource_scope resources={.previous=xlsActiveTypeResource};xlsActiveTypeResource=&resources;
 int okay=xlsNamePhase(&b[0]->module,&cached);xlsActiveTypeResource=resources.previous;CHECK(okay&&!resources.failed&&cached.next&&cached.work.checked&&cached.work.reused);
 CHECK(xl6IrBuildImplicitNameAbis(&b[1]->module));size_t changed=0;
 for(size_t i=0;i<reference->nodeCount;++i){const XL6_IrNode* a=b[0]->module.nodes+i;const XL6_IrNode* c=b[1]->module.nodes+i;
  CHECK(a->implicitNameAbi==c->implicitNameAbi&&a->implicitReceiverType==c->implicitReceiverType&&a->implicitFieldDeclaration==c->implicitFieldDeclaration);
  changed+=a->implicitNameAbi!=reference->nodes[i].implicitNameAbi||a->implicitReceiverType!=reference->nodes[i].implicitReceiverType||a->implicitFieldDeclaration!=reference->nodes[i].implicitFieldDeclaration;
 }
 CHECK(changed);xlsIrNameCacheDestroy(cached.next);xrtFree(key);
 for(unsigned k=0;k<2;++k){b[k]->module.nodes[id-1].text=saved[k];xlsIrNativePipelineDestroy(b[k]);}
 /* The borrowed cache remains usable after a failed fused match and miss. */
 b[0]=xlsIrNativePipelineCreate(&status);CHECK(b[0]&&xlsIrNativePipelinePrepare(b[0],xlsSemanticCheckAst(model),&d,0,&status));xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;
 CHECK(xlsIrNativePipelineFinishCachedNames(b[0],&d,previous,16u*1024u*1024u,&next,&work,&status)&&next&&work.reused&&!work.checked&&!d.count);
 char* expected=dump(reference);xlsSemanticCheckDestroy(model);xlsIrNameCacheDestroy(previous);xlsIrNameCacheDestroy(next);char* actual=dump(&b[0]->module);CHECK(expected&&actual&&!strcmp(expected,actual));
 xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b[0]);xl6DiagFree(&d);return 1;
}

static int contractTests(void)
{
 CHECK(!xlsIrNameCacheFixtureMain());char* path=xrtPathAbs("name-cache-contract-中文.xl");CHECK(path);CHECK(privateContexts(path));CHECK(layoutInputs(path));CHECK(lateMismatch(path));CHECK(concurrent(path));xrtFree(path);CHECK(xrtAtomic32Load(&poisoned,XMEMORY_RELAXED)>0);return 1;
}
#else
static int contractTests(void){return 1;}
#endif
int main(void)
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_name_cache_contract_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&poisoned,0);if(!contractTests())return 1;
 printf("analysis_ir_name_cache_contract_test passed: %u checks;%u retired units poisoned;queried nominal types,changed native class fields,seeded outputs,common context,four concurrent readers,trace restoration\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&poisoned,XMEMORY_RELAXED));return 0;
}
