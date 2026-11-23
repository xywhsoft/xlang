/* Construction-bound experimental selective native exit production. */
#include "analysis/ir_function_exit.h"
#include "analysis/ir_exit_range.h"
static _Thread_local size_t xlsFnLimit=8u*1024u*1024u;
void xlsIrFunctionExitLimit(size_t bytes){xlsFnLimit=bytes<8u*1024u*1024u?bytes:8u*1024u*1024u;}
#if XLS_EXPERIMENTAL_IR_FUNCTION_EXITS && defined(XLS_NATIVE_IR_RANGE_PRODUCTION)
typedef struct xls_fn_key {xstrbuf data;size_t limit;} xls_fn_key;
static int xlsFnAppend(xls_fn_key* p,const void* data,size_t bytes)
{return xlsIrMemoData(&p->data,data,bytes,p->limit);}
static int xlsFnNumber(xls_fn_key* p,uint64_t value)
{unsigned char bytes[10];size_t n=xlsIrMemoNumberBytes(bytes,value);return xlsFnAppend(p,bytes,n);}
static int xlsFnText(xls_fn_key* p,const char* text)
{size_t n=strlen(text)+1;return n&&xlsFnNumber(p,n)&&xlsFnAppend(p,text,n);}
static int xlsFnVector(xls_fn_key* p,const void* data,size_t count,size_t extra,size_t width)
{if(count>SIZE_MAX-extra)return 0;count+=extra;return count<=SIZE_MAX/width&&xlsFnNumber(p,count)&&xlsFnAppend(p,data,count*width);}
#include "ir_function_exit_projection.inc"
typedef struct xls_fn_binding {uint32_t native,kind;uint64_t value;} xls_fn_binding;
typedef struct xls_fn_unit {
 xatomic32 refs;xstrbuf key;size_t bytes,count;
 XL6_IrGeneratorClosePointAbi* close;
 XL6_IrExitPlan* plans;size_t planCount;
 XL6_IrCleanupAction* cleanups;size_t cleanupCount;
 XL6_IrExitStep* steps;size_t stepCount;
 xls_fn_binding* bindings;size_t bindingCount;
} xls_fn_unit;
typedef struct xls_fn_context {
 xatomic32 refs;xls_fn_key key;size_t bytes;
 xls_entity_store* store;uint64_t domain;
} xls_fn_context;
typedef struct xls_fn_entry {
 size_t first,count;uint64_t identity;
 xls_lexical_symbols lexical;xls_fn_unit* unit;
} xls_fn_entry;
struct xls_function_exit_cache {
 xarray entries;size_t bytes;xls_fn_context* context;
 xls_ir_function_exit_work work;
};
typedef struct xls_fn_input {
 xls_fn_key key;xarray regions,bindings;xmap regionIndex,bindingIndex;
 unsigned maps;unsigned char* seen;
} xls_fn_input;
typedef struct xls_fn_outputs {XL6_IrModule module;XL6_IrGeneratorClosePointAbi* close;} xls_fn_outputs;
static int xlsFnCancelled(void* unused)
{(void)unused;return xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1);}
static void xlsFnUnitRelease(xls_fn_unit* p)
{if(p&&xrtAtomic32FetchSub(&p->refs,1,XMEMORY_ACQ_REL)==1){xrtStrBufFree(&p->key);xrtFree(p);}}
static xls_fn_unit* xlsFnUnitRetain(xls_fn_unit* p)
{if(p)xrtAtomic32FetchAdd(&p->refs,1,XMEMORY_RELAXED);return p;}
static void xlsFnContextRelease(xls_fn_context* p)
{if(p&&xrtAtomic32FetchSub(&p->refs,1,XMEMORY_ACQ_REL)==1){xlsEntityStoreRelease(p->store);xrtStrBufFree(&p->key.data);xrtFree(p);}}
static void xlsFnExitDestroy(xls_function_exit_cache* cache)
{
 if(!cache)return;
 for(size_t i=0;i<cache->entries.Count;++i){xls_fn_entry* e=xrtArrayGet(&cache->entries,i);xlsLexicalSymbolsClear(&e->lexical);xlsFnUnitRelease(e->unit);}
 xrtArrayUnit(&cache->entries);xlsFnContextRelease(cache->context);xrtFree(cache);
}
static int xlsFnLexicalCopy(xls_lexical_symbols* out,const xls_lexical_symbols* from)
{
 if(!from->ready||!xrtArrayInit(&out->order,sizeof(xls_lexical_symbol))||!xrtArrayInit(&out->sorted,sizeof(xls_lexical_symbol))||
    !xrtArrayResize(&out->order,from->order.Count)||!xrtArrayResize(&out->sorted,from->sorted.Count))return 0;
 if(from->order.Count)memcpy(out->order.Data,from->order.Data,from->order.Count*sizeof(xls_lexical_symbol));
 if(from->sorted.Count)memcpy(out->sorted.Data,from->sorted.Data,from->sorted.Count*sizeof(xls_lexical_symbol));
 out->bytes=(out->order.Capacity+out->sorted.Capacity)*sizeof(xls_lexical_symbol);out->ready=1;return 1;
}
static void xlsFnExitRecord(xls_ir_memo_build* b,size_t slot,size_t first,const xls_lexical_symbols* lexical)
{
 if(!xlsFnLimit||!b||!lexical->ready||first>=b->module->nodeCount||slot>=b->memo->entries.Count)return;
 xls_function_exit_cache* c=b->memo->functionExits;
 if(!c){
  c=xrtCalloc(1,sizeof(*c));if(!c)return;
  if(!xrtArrayInit(&c->entries,sizeof(xls_fn_entry))||!xrtArrayResize(&c->entries,b->memo->entries.Count)||
     c->entries.Capacity>(xlsFnLimit>sizeof(*c)?xlsFnLimit-sizeof(*c):0)/sizeof(xls_fn_entry)){xlsFnExitDestroy(c);return;}
  c->bytes=sizeof(*c)+c->entries.Capacity*sizeof(xls_fn_entry);b->memo->functionExits=c;
 }
 if(c->work.attempted)return;
 xls_fn_entry* e=xrtArrayGet(&c->entries,slot);if(!e||e->count)return;
 if(!xlsFnLexicalCopy(&e->lexical,lexical)||e->lexical.bytes>xlsFnLimit-c->bytes){xlsLexicalSymbolsClear(&e->lexical);return;}
 c->bytes+=e->lexical.bytes;e->first=first;e->count=b->module->nodeCount-first;
 xls_ir_unit* const* raw=xrtArrayConstGet(&b->memo->entries,slot);e->identity=raw&&*raw?(*raw)->identity:0;++c->work.recorded;
}
static void xlsFnInputClear(xls_fn_input* p)
{
 xrtStrBufFree(&p->key.data);xrtArrayUnit(&p->regions);xrtArrayUnit(&p->bindings);
 xrtFree(p->seen);
 if(p->maps&1)xrtMapUnit(&p->regionIndex);
 if(p->maps&2)xrtMapUnit(&p->bindingIndex);
 *p=(xls_fn_input){0};
}
static int xlsFnRegion(xls_fn_input* p,uint32_t value,int create,uint32_t* out)
{
 *out=0;if(!value)return 1;xbytesview key={(const unsigned char*)&value,sizeof(value)};
 const uint32_t* found=xrtMapGet(&p->regionIndex,key);if(found){*out=*found;return 1;}
 if(!create||p->regions.Count>=UINT32_MAX)return 0;
 *out=(uint32_t)p->regions.Count+1;
 return xrtArrayPush(&p->regions,&value)&&xrtMapSet(&p->regionIndex,key,out);
}
static xls_fn_binding xlsFnSymbol(const xls_fn_entry* e,const xls_entity_view* entities,XL6_SymbolId native)
{
 int local=0;XL6_SymbolId slot=xlsLexicalSymbolKey(&e->lexical,native,&local);
 if(local)return (xls_fn_binding){native,1,slot};
 uint64_t identity=xlsEntityViewSymbol(entities,native);
 return (xls_fn_binding){native,identity?2:0,identity?identity:native};
}
static int xlsFnKeySymbol(xls_fn_input* p,const xls_fn_entry* e,const xls_entity_view* entities,XL6_SymbolId native)
{xls_fn_binding v=xlsFnSymbol(e,entities,native);return xlsFnNumber(&p->key,v.kind)&&xlsFnNumber(&p->key,v.value);}
static int xlsFnToken(xls_fn_input* p,const xls_fn_entry* e,const xls_entity_view* entities,XL6_SymbolId native,uint32_t* out)
{
 *out=0;if(!native)return 1;xbytesview key={(const unsigned char*)&native,sizeof(native)};
 const uint32_t* found=xrtMapGet(&p->bindingIndex,key);if(found){*out=*found;return 1;}
 if(p->bindings.Count>=UINT32_MAX)return 0;
 xls_fn_binding value=xlsFnSymbol(e,entities,native);*out=(uint32_t)p->bindings.Count+1;
 return xrtArrayPush(&p->bindings,&value)&&xrtMapSet(&p->bindingIndex,key,out);
}
static int xlsFnBuildInput(xls_fn_input* p,const XL6_IrModule* m,const xls_fn_entry* e,const xls_entity_view* entities,const unsigned char* initializers,size_t limit)
{
 p->key.limit=limit;
 if(!e->lexical.ready||!e->count||e->count>limit/8||e->first>=m->nodeCount||e->count>m->nodeCount-e->first||e->count>UINT32_MAX||
    m->nodes[e->first].kind!=XL6_IR_NODE_FUNC_DECL||m->nodes[e->first].parent!=m->root)return 0;
 if(!xrtArrayInit(&p->regions,sizeof(uint32_t))||!xrtArrayInit(&p->bindings,sizeof(xls_fn_binding)))return 0;
 if(!xrtMapInit(&p->regionIndex,sizeof(uint32_t)))return 0;
 p->maps|=1;
 if(!xrtMapInit(&p->bindingIndex,sizeof(uint32_t)))return 0;
 p->maps|=2;
 p->seen=xrtCalloc(e->count/8+(e->count%8!=0),1);if(!p->seen)return 0;p->seen[0]=1;
 if(!xlsFnNumber(&p->key,e->identity)||!xlsFnNumber(&p->key,e->count))return 0;
 size_t childTotal=0;
 for(size_t i=0;i<e->count;++i){
  if(!(i%64)&&xlsFnCancelled(NULL))return 0;
  const XL6_IrNode* n=&m->nodes[e->first+i];XL6_IrNode normalized=*n;
  if(n->firstChild>m->childCount||n->childCount>m->childCount-n->firstChild||
     (!i?n->parent!=m->root:n->parent<=e->first||n->parent>=e->first+i+1))return 0;
  normalized.parent=i?(XL6_IrNodeId)(n->parent-e->first):0;normalized.firstChild=0;
  if(!xlsFnRegion(p,n->region,1,&normalized.region)||!xlsFnRegion(p,n->targetRegion,1,&normalized.targetRegion)||
     !xlsFnKeySymbol(p,e,entities,n->symbol)||!xlsFnKeySymbol(p,e,entities,n->targetSymbol)||!xlsFnKeySymbol(p,e,entities,n->compoundOperatorSymbol))return 0;
  normalized.symbol=normalized.targetSymbol=normalized.compoundOperatorSymbol=0;
  if(!xlsFnNodeKey(&p->key,&normalized)||!xlsFnNumber(&p->key,initializers[e->first+i]==2))return 0;
  if(n->childCount>e->count-1-childTotal)return 0;
  childTotal+=n->childCount;
  for(size_t j=0;j<n->childCount;++j){XL6_IrNodeId child=m->children[n->firstChild+j];
   if(child<=e->first+i+1||child-e->first>e->count||m->nodes[child-1].parent!=e->first+i+1||!xlsFnNumber(&p->key,child-e->first))return 0;
   size_t at=child-e->first-1;if(p->seen[at/8]&(1u<<(at%8)))return 0;p->seen[at/8]|=(unsigned char)(1u<<(at%8));
  }
 }
 /* Unique edges, parent backlinks, increasing IDs and n-1 edges prove this
  * complete local tree. The producer still requires a valid native module;
  * the full original verifier also validates the final complete module. */
 return childTotal==e->count-1;
}
static xls_fn_context* xlsFnContextBuild(const XL6_IrModule* m,const xls_ir_memo_build* b,size_t limit)
{
 if(limit<=sizeof(xls_fn_context)||m->root==0||m->root>m->nodeCount)return NULL;
 xls_fn_context* c=xrtCalloc(1,sizeof(*c));if(!c)return NULL;xrtAtomic32Init(&c->refs,1);
 c->store=xlsEntityStoreRetain((xls_entity_store*)xlsEntityViewStore(b->memo->entities));c->domain=xlsEntityViewInfo(b->memo->entities).domain;
 xls_fn_key* p=&c->key;p->limit=limit-sizeof(*c);
 if(!xlsFnNumber(p,b->memo->context.Size)||!xlsFnAppend(p,b->memo->context.Data,b->memo->context.Size)||
    !xlsFnNumber(p,m->root)||!xlsFnNumber(p,m->nodes[m->root-1].kind)||!xlsFnBook_XL6_IrRuntimeTypeMap(p,&m->runtimeTypeMap))goto failed;
#define FN_CONTEXT_TABLE(ptr,count,capacity,book) \
 if(m->count>m->capacity||(m->count&&!m->ptr)||!xlsFnNumber(p,m->count))goto failed; \
 for(size_t i=0;i<m->count;++i){if(!(i%64)&&xlsFnCancelled(NULL))goto failed;if(!book(p,&m->ptr[i]))goto failed;}
 if(m->types.count>m->types.capacity||(m->types.count&&!m->types.items)||!xlsFnNumber(p,m->types.count))goto failed;
 for(size_t i=0;i<m->types.count;++i){if(!(i%64)&&xlsFnCancelled(NULL))goto failed;if(!xlsFnBook_XL6_IrType(p,&m->types.items[i]))goto failed;}
 FN_CONTEXT_TABLE(enumLayouts,enumLayoutCount,enumLayoutCapacity,xlsFnBook_XL6_IrEnumLayout)
 FN_CONTEXT_TABLE(enumVariants,enumVariantCount,enumVariantCapacity,xlsFnBook_XL6_IrEnumVariant)
 FN_CONTEXT_TABLE(enumPayloads,enumPayloadCount,enumPayloadCapacity,xlsFnBook_XL6_IrEnumPayload)
 FN_CONTEXT_TABLE(functions,functionCount,functionCapacity,xlsFnBook_XL6_IrFunction)
 FN_CONTEXT_TABLE(genericSpecializations,genericSpecializationCount,genericSpecializationCapacity,xlsFnBook_XL6_IrGenericSpecialization)
#undef FN_CONTEXT_TABLE
 /* The only cross-callable node scan in the pinned producer resolves native
  * destructors. Preserve its ordered candidate set, names and complete parent
  * chain. Other callable bodies cannot contribute lexical cleanup in a valid
  * closed function subtree: routed exits compare the enclosing callable, and
  * AFTER_SCOPE visits only the selected block's direct binding children. */
 for(size_t i=0;i<m->nodeCount;++i){const XL6_IrNode* n=&m->nodes[i];
  if(!(i%64)&&xlsFnCancelled(NULL))goto failed;
  if(n->kind!=XL6_IR_NODE_FUNC_DECL||n->sourceKind!=XL6_AST_NATIVE_EXTERN_SPECIAL||!n->symbol||!n->childCount)continue;
  if(!xlsFnNumber(p,i+1)||!xlsFnNodeKey(p,n)||n->firstChild>=m->childCount)goto failed;
  XL6_IrNodeId id=m->children[n->firstChild];if(!id||id>m->nodeCount||!xlsFnNodeKey(p,&m->nodes[id-1]))goto failed;
  id=n->parent;size_t remaining=m->nodeCount;
  while(id&&remaining--){if(id>m->nodeCount||!xlsFnNumber(p,id)||!xlsFnNodeKey(p,&m->nodes[id-1]))goto failed;id=m->nodes[id-1].parent;}
  if(id||!xlsFnNumber(p,0))goto failed;
 }
 if(!xlsFnNumber(p,0))goto failed;
 c->bytes=sizeof(*c)+(c->key.data.Data?c->key.data.Capacity+1:0);if(c->bytes>limit)goto failed;return c;
failed:xlsFnContextRelease(c);return NULL;
}
static int xlsFnSpace(size_t* bytes,size_t count,size_t width,size_t align,size_t* offset)
{size_t pad=(align-*bytes%align)%align;if(pad>SIZE_MAX-*bytes)return 0;*bytes+=pad;*offset=*bytes;if(count>(SIZE_MAX-*bytes)/width)return 0;*bytes+=count*width;return 1;}
static int xlsFnEncodeNode(XL6_IrNodeId* value,const xls_fn_entry* e)
{if(!*value)return 1;if(*value<=e->first||*value-e->first>e->count)return 0;*value=(XL6_IrNodeId)(*value-e->first);return 1;}
static xls_fn_unit* xlsFnCapture(xls_fn_input* in,const xls_fn_entry* e,const xls_entity_view* entities,xls_ir_exit_range_view v,size_t limit)
{
 uint32_t ignored;
 for(size_t i=0;i<v.cleanupCount;++i){if(!(i%64)&&xlsFnCancelled(NULL))return NULL;if(!xlsFnToken(in,e,entities,v.cleanups[i].targetSymbol,&ignored)||!xlsFnToken(in,e,entities,v.cleanups[i].calleeSymbol,&ignored))return NULL;}
 size_t bytes=sizeof(xls_fn_unit),close,plans,cleanups,steps,bindings;
#define FN_SPACE(count,type,offset) xlsFnSpace(&bytes,count,sizeof(type),_Alignof(type),&offset)
 if(!FN_SPACE(v.count,XL6_IrGeneratorClosePointAbi,close)||!FN_SPACE(v.planCount,XL6_IrExitPlan,plans)||
    !FN_SPACE(v.cleanupCount,XL6_IrCleanupAction,cleanups)||!FN_SPACE(v.stepCount,XL6_IrExitStep,steps)||
    !FN_SPACE(in->bindings.Count,xls_fn_binding,bindings)||bytes>limit||in->key.data.Capacity>=limit-bytes)return NULL;
#undef FN_SPACE
 xls_fn_unit* u=xrtCalloc(1,bytes);if(!u)return NULL;xrtAtomic32Init(&u->refs,1);
 unsigned char* mem=(void*)u;u->count=v.count;u->close=(void*)(mem+close);u->plans=(void*)(mem+plans);u->cleanups=(void*)(mem+cleanups);u->steps=(void*)(mem+steps);u->bindings=(void*)(mem+bindings);
 u->planCount=v.planCount;u->cleanupCount=v.cleanupCount;u->stepCount=v.stepCount;u->bindingCount=in->bindings.Count;
 if(v.count)memcpy(u->close,v.close,v.count*sizeof(*v.close));
 if(v.planCount)memcpy(u->plans,v.plans,v.planCount*sizeof(*v.plans));
 if(v.cleanupCount)memcpy(u->cleanups,v.cleanups,v.cleanupCount*sizeof(*v.cleanups));
 if(v.stepCount)memcpy(u->steps,v.steps,v.stepCount*sizeof(*v.steps));
 if(u->bindingCount)memcpy(u->bindings,in->bindings.Data,u->bindingCount*sizeof(*u->bindings));
 for(size_t i=0;i<u->planCount;++i){if(!(i%64)&&xlsFnCancelled(NULL))goto failed;if(!xlsFnEncodeNode(&u->plans[i].origin,e)||!xlsFnRegion(in,u->plans[i].targetRegion,0,&u->plans[i].targetRegion))goto failed;}
 for(size_t i=0;i<u->cleanupCount;++i){XL6_IrCleanupAction* a=&u->cleanups[i];
  if(!(i%64)&&xlsFnCancelled(NULL))goto failed;
  if(!xlsFnEncodeNode(&a->origin,e)||!xlsFnEncodeNode(&a->ownerDeclaration,e)||!xlsFnToken(in,e,entities,a->targetSymbol,&a->targetSymbol)||!xlsFnToken(in,e,entities,a->calleeSymbol,&a->calleeSymbol))goto failed;}
 for(size_t i=0;i<u->stepCount;++i){XL6_IrExitStep* s=&u->steps[i];if(!(i%64)&&xlsFnCancelled(NULL))goto failed;if(!xlsFnEncodeNode(&s->origin,e)||!xlsFnEncodeNode(&s->failureTarget,e)||!xlsFnRegion(in,s->failureRegion,0,&s->failureRegion))goto failed;}
 u->key=in->key.data;in->key.data=(xstrbuf){0};u->bytes=bytes+u->key.Capacity+1;return u;
failed:xlsFnUnitRelease(u);return NULL;
}
static int xlsFnRouted(const XL6_IrExitPlan* p)
{return xl6IrExitPlanHasRoute(p);}
static int xlsFnPushPlan(xls_fn_outputs* out,const XL6_IrExitPlan* value)
{XL6_IrModule* m=&out->module;if(!xl6IrGrow((void**)&m->exitPlans,&m->exitPlanCapacity,m->exitPlanCount,sizeof(*m->exitPlans)))return 0;m->exitPlans[m->exitPlanCount++]=*value;return 1;}
static int xlsFnPushCleanup(xls_fn_outputs* out,const XL6_IrCleanupAction* value)
{XL6_IrModule* m=&out->module;if(!xl6IrGrow((void**)&m->cleanupActions,&m->cleanupActionCapacity,m->cleanupActionCount,sizeof(*m->cleanupActions)))return 0;m->cleanupActions[m->cleanupActionCount++]=*value;return 1;}
static int xlsFnPushStep(xls_fn_outputs* out,const XL6_IrExitStep* value)
{XL6_IrModule* m=&out->module;if(!xl6IrGrow((void**)&m->exitSteps,&m->exitStepCapacity,m->exitStepCount,sizeof(*m->exitSteps)))return 0;m->exitSteps[m->exitStepCount++]=*value;return 1;}
static int xlsFnAppendFresh(xls_fn_outputs* out,xls_ir_exit_range_view v)
{
 size_t clean=out->module.cleanupActionCount,steps=out->module.exitStepCount;
 for(size_t i=0;i<v.planCount;++i){XL6_IrExitPlan p=v.plans[i];if(p.firstCleanup>SIZE_MAX-clean||p.firstStep>SIZE_MAX-steps)return 0;p.firstCleanup+=clean;if(xlsFnRouted(&p))p.firstStep+=steps;if(!xlsFnPushPlan(out,&p))return 0;}
 for(size_t i=0;i<v.cleanupCount;++i)if(!xlsFnPushCleanup(out,&v.cleanups[i]))return 0;
 for(size_t i=0;i<v.stepCount;++i){XL6_IrExitStep s=v.steps[i];if(s.kind==XL6_IR_EXIT_STEP_SCOPE){if(s.firstCleanup>SIZE_MAX-clean)return 0;s.firstCleanup+=clean;}if(!xlsFnPushStep(out,&s))return 0;}
 if(v.count)memcpy(out->close+v.first,v.close,v.count*sizeof(*v.close));
 return 1;
}
static int xlsFnDecodeNode(XL6_IrNodeId* value,const xls_fn_entry* e)
{if(!*value)return 1;if(*value>e->count||e->first>UINT32_MAX-*value)return 0;*value+=(XL6_IrNodeId)e->first;return 1;}
static int xlsFnDecodeRegion(uint32_t* value,const xls_fn_input* in)
{if(!*value)return 1;const uint32_t* r=xrtArrayConstGet(&in->regions,*value-1);if(!r)return 0;*value=*r;return 1;}
static int xlsFnResolve(const xls_fn_binding* b,const xls_fn_entry* e,const xls_entity_view* entities,XL6_SymbolId* out)
{
 if(b->kind==2){*out=xlsEntityViewNativeSymbol(entities,b->value);return *out!=0;}
 if(b->value>UINT32_MAX)return 0;
 return xlsLexicalSymbolResolve(&e->lexical,(XL6_SymbolId)b->value,(int)b->kind,out);
}
static int xlsFnDecodeSymbol(XL6_SymbolId* value,const xls_fn_unit* u,const xls_fn_entry* e,const xls_entity_view* entities)
{if(!*value)return 1;if(*value>u->bindingCount)return 0;return xlsFnResolve(&u->bindings[*value-1],e,entities,value);}
static int xlsFnAppendUnit(xls_fn_outputs* out,const xls_fn_unit* u,const xls_fn_entry* e,const xls_fn_input* in,const xls_entity_view* entities)
{
 size_t clean=out->module.cleanupActionCount,steps=out->module.exitStepCount;
 for(size_t i=0;i<u->planCount;++i){XL6_IrExitPlan p=u->plans[i];if(!(i%64)&&xlsFnCancelled(NULL))return 0;if(!xlsFnDecodeNode(&p.origin,e)||!xlsFnDecodeRegion(&p.targetRegion,in)||p.firstCleanup>SIZE_MAX-clean||p.firstStep>SIZE_MAX-steps)return 0;p.firstCleanup+=clean;if(xlsFnRouted(&p))p.firstStep+=steps;if(!xlsFnPushPlan(out,&p))return 0;}
 for(size_t i=0;i<u->cleanupCount;++i){XL6_IrCleanupAction a=u->cleanups[i];if(!(i%64)&&xlsFnCancelled(NULL))return 0;if(!xlsFnDecodeNode(&a.origin,e)||!xlsFnDecodeNode(&a.ownerDeclaration,e)||!xlsFnDecodeSymbol(&a.targetSymbol,u,e,entities)||!xlsFnDecodeSymbol(&a.calleeSymbol,u,e,entities)||!xlsFnPushCleanup(out,&a))return 0;}
 for(size_t i=0;i<u->stepCount;++i){XL6_IrExitStep s=u->steps[i];if(!(i%64)&&xlsFnCancelled(NULL))return 0;if(!xlsFnDecodeNode(&s.origin,e)||!xlsFnDecodeNode(&s.failureTarget,e)||!xlsFnDecodeRegion(&s.failureRegion,in))return 0;if(s.kind==XL6_IR_EXIT_STEP_SCOPE){if(s.firstCleanup>SIZE_MAX-clean)return 0;s.firstCleanup+=clean;}if(!xlsFnPushStep(out,&s))return 0;}
 memcpy(out->close+e->first,u->close,e->count*sizeof(*u->close));return 1;
}
static int xlsFnFresh(xls_fn_outputs* out,const XL6_IrModule* m,size_t first,size_t count,xls_function_exit_cache* c,xls_ir_exit_range** result)
{
 xls_ir_exit_range_work work;*result=xlsIrExitRangeProduce(m,first,count,SIZE_MAX,xlsFnCancelled,NULL,NULL,&work);
 c->work.nativeNodes+=work.planNodes;return *result&&xlsFnAppendFresh(out,xlsIrExitRangeView(*result));
}
static xls_fn_unit* xlsFnPrevious(const xls_ir_memo_build* b,const xls_fn_entry* current,size_t slot)
{
 const xls_function_exit_cache* old=b->baseline?b->baseline->functionExits:NULL;if(!old)return NULL;
 if(current->identity&&b->functionsReady){size_t low=0,high=b->functions.Count;
  while(low<high){size_t mid=low+(high-low)/2;const xls_ir_function* f=xrtArrayConstGet(&b->functions,mid);if(f->identity<current->identity)low=mid+1;else high=mid;}
  const xls_ir_function* f=xrtArrayConstGet(&b->functions,low);if(!f||f->identity!=current->identity)return NULL;slot=f->slot;
 }
 const xls_fn_entry* e=xrtArrayConstGet(&old->entries,slot);return e?e->unit:NULL;
}
static int xlsFnExitTry(XL6_IrModule* m)
{
 xls_ir_memo_build* b=xlsActiveIrMemo;if(!b||b->module!=m||!b->memo||!xlsFnLimit||!xlsIrExitRangeAvailable())return 0;
 xls_function_exit_cache* c=b->memo->functionExits;if(!c||c->work.attempted)return 0;
 c->work.attempted=1;c->work.moduleNodes=m->nodeCount;
 xls_fn_outputs output={0};unsigned char* initializers=NULL;xls_ir_exit_range* fresh=NULL;int okay=0;size_t cursor=0;
 if(m->nodeCount>UINT32_MAX||m->nodeCount>m->nodeCapacity||m->childCount>m->childCapacity||(m->childCount&&!m->children)||c->bytes>xlsFnLimit)goto done;
 c->context=xlsFnContextBuild(m,b,xlsFnLimit-c->bytes);if(!c->context)goto done;c->bytes+=c->context->bytes;
 const xls_function_exit_cache* old=b->baseline?b->baseline->functionExits:NULL;
 int same=old&&old->context&&old->context->store==c->context->store&&old->context->domain==c->context->domain&&xlsIrMemoEqual(&old->context->key.data,&c->context->key.data);
 if(same&&old->context->bytes<=xlsFnLimit-(c->bytes-c->context->bytes)){
  c->bytes-=c->context->bytes;xlsFnContextRelease(c->context);c->context=old->context;
  xrtAtomic32FetchAdd(&c->context->refs,1,XMEMORY_RELAXED);c->bytes+=c->context->bytes;
 }
 if(m->nodeCount>SIZE_MAX/sizeof(*output.close))goto done;
 output.close=xrtMalloc(m->nodeCount*sizeof(*output.close));initializers=xrtCalloc(m->nodeCount,1);if(!output.close||!initializers)goto done;
 if(m->valueInitializerCount>m->valueInitializerCapacity||(m->valueInitializerCount&&!m->valueInitializers))goto done;
 for(size_t i=0;i<m->valueInitializerCount;++i){if(!(i%64)&&xlsFnCancelled(NULL))goto done;const XL6_IrValueInitializer* v=&m->valueInitializers[i];if(v->declaration&&v->declaration<=m->nodeCount&&!initializers[v->declaration-1])initializers[v->declaration-1]=v->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE?2:1;}
 for(size_t slot=0;slot<c->entries.Count;++slot){
  xls_fn_entry* e=xrtArrayGet(&c->entries,slot);if(!e->count)continue;
  if(xlsFnCancelled(NULL)||e->first<cursor||e->first>m->nodeCount||e->count>m->nodeCount-e->first)goto done;
  if(e->first>cursor){if(!xlsFnFresh(&output,m,cursor,e->first-cursor,c,&fresh))goto done;xlsIrExitRangeDestroy(fresh);fresh=NULL;}
  xls_fn_input in={0};int keyed=xlsFnBuildInput(&in,m,e,b->memo->entities,initializers,xlsFnLimit);
  xls_fn_unit* unit=same&&keyed?xlsFnPrevious(b,e,slot):NULL;
  if(unit&&unit->count==e->count&&xlsIrMemoEqual(&unit->key,&in.key.data)&&unit->bytes<=xlsFnLimit-c->bytes){
   if(!xlsFnAppendUnit(&output,unit,e,&in,b->memo->entities)){xlsFnInputClear(&in);goto done;}
   e->unit=xlsFnUnitRetain(unit);c->bytes+=unit->bytes;++c->work.reused;
  }else{
   if(!xlsFnFresh(&output,m,e->first,e->count,c,&fresh)){xlsFnInputClear(&in);goto done;}
   ++c->work.fresh;
   if(keyed){e->unit=xlsFnCapture(&in,e,b->memo->entities,xlsIrExitRangeView(fresh),xlsFnLimit-c->bytes);if(e->unit)c->bytes+=e->unit->bytes;}
   xlsIrExitRangeDestroy(fresh);fresh=NULL;
  }
  xlsFnInputClear(&in);cursor=e->first+e->count;
 }
 if(cursor<m->nodeCount){if(!xlsFnFresh(&output,m,cursor,m->nodeCount-cursor,c,&fresh))goto done;xlsIrExitRangeDestroy(fresh);fresh=NULL;}
 if(xlsFnCancelled(NULL))goto done;
 xrtFree(m->exitPlans);xrtFree(m->cleanupActions);xrtFree(m->exitSteps);
 m->exitPlans=output.module.exitPlans;m->exitPlanCount=output.module.exitPlanCount;m->exitPlanCapacity=output.module.exitPlanCapacity;output.module.exitPlans=NULL;
 m->cleanupActions=output.module.cleanupActions;m->cleanupActionCount=output.module.cleanupActionCount;m->cleanupActionCapacity=output.module.cleanupActionCapacity;output.module.cleanupActions=NULL;
 m->exitSteps=output.module.exitSteps;m->exitStepCount=output.module.exitStepCount;m->exitStepCapacity=output.module.exitStepCapacity;output.module.exitSteps=NULL;
 for(size_t i=0;i<m->nodeCount;++i)m->nodes[i].generatorClosePointAbi=output.close[i];
 c->work.assembled=1;okay=1;
done:
 xlsIrExitRangeDestroy(fresh);xrtFree(output.module.exitPlans);xrtFree(output.module.cleanupActions);xrtFree(output.module.exitSteps);xrtFree(output.close);xrtFree(initializers);
 for(size_t i=0;i<c->entries.Count;++i){xls_fn_entry* e=xrtArrayGet(&c->entries,i);c->bytes-=e->lexical.bytes;xlsLexicalSymbolsClear(&e->lexical);}
 c->work.bytes=c->bytes;if(!okay)c->work.fallbackNodes=m->nodeCount;return okay;
}
xls_ir_function_exit_work xlsIrFunctionExitWork(const xls_ir_memo* m)
{return m&&m->functionExits?m->functionExits->work:(xls_ir_function_exit_work){0};}
int xlsIrFunctionExitAvailable(void){return 1;}
#else
#if XLS_EXPERIMENTAL_IR_FUNCTION_EXITS
static void xlsFnExitDestroy(xls_function_exit_cache* p){(void)p;}
static void xlsFnExitRecord(xls_ir_memo_build* b,size_t slot,size_t first,const xls_lexical_symbols* s){(void)b;(void)slot;(void)first;(void)s;}
static int xlsFnExitTry(XL6_IrModule* m){(void)m;return 0;}
#endif
xls_ir_function_exit_work xlsIrFunctionExitWork(const xls_ir_memo* m){(void)m;return(xls_ir_function_exit_work){0};}
int xlsIrFunctionExitAvailable(void){return 0;}
#endif
