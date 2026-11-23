/* Included only by the qualified native pipeline component. */
typedef struct xls_call_read {uint32_t kind,id;} xls_call_read;
typedef struct xls_call_unit {
 xatomic32 refs;size_t first,end,reads,keyBytes,sites,args,bytes;
 xls_call_read* ids;unsigned char* key;XL6_IrCallSite* site;XL6_IrCallArgument* arg;
} xls_call_unit;
struct xls_ir_call_cache {xstrbuf context;xarray units;size_t bytes;};
typedef struct xls_call_build {
 const xls_ir_call_cache* previous;xls_ir_call_cache* next;
 size_t limit;xls_ir_call_cache_work work;
} xls_call_build;
typedef struct xls_call_trace {
 const XL6_IrModule* module;uint32_t *seen,epoch;xls_call_read* ids;
 size_t reads,readCapacity,used,capacity;unsigned char* key;int failed;
} xls_call_trace;
static _Thread_local xls_call_trace* xlsCallsActiveTrace;
static void xlsCallUnitRelease(xls_call_unit* u)
{
 if(u&&xrtAtomic32FetchSub(&u->refs,1,XMEMORY_ACQ_REL)==1){
#ifdef XLS_CALL_CACHE_UNIT_FREE_TEST
  XLS_CALL_CACHE_UNIT_FREE_TEST(u,u->bytes);
#endif
  xrtFree(u);
 }
}
static xls_call_unit* xlsCallUnitRetain(xls_call_unit* u)
{if(u)xrtAtomic32FetchAdd(&u->refs,1,XMEMORY_RELAXED);return u;}
void xlsIrCallCacheDestroy(xls_ir_call_cache* c)
{
 if(!c)return;
 for(size_t i=0;i<c->units.Count;++i)xlsCallUnitRelease(*(xls_call_unit**)xrtArrayGet(&c->units,i));
 xrtArrayUnit(&c->units);xrtStrBufFree(&c->context);xrtFree(c);
}
size_t xlsIrCallCacheBytes(const xls_ir_call_cache* c){return c?c->bytes:0;}
static int xlsCallPut(xls_call_trace* t,const void* p,size_t n)
{
 if(t->failed||n>t->capacity-t->used||(n&&!p)){t->failed=1;return 0;}
 if(n)memcpy(t->key+t->used,p,n);
 t->used+=n;return 1;
}
static int xlsCallText(xls_call_trace* t,const char* p)
{
 uint64_t n=0;
 if(p){size_t room=t->capacity-t->used;for(;n<room&&p[n];++n){}if(n==room){t->failed=1;return 0;}++n;}
 return xlsCallPut(t,&n,sizeof(n))&&xlsCallPut(t,p,(size_t)n);
}
static int xlsCallNodeKey(xls_call_trace* t,XL6_IrNodeId id)
{
 const XL6_IrModule* m=t->module;const XL6_IrNode* n=xl6IrNodeGet(m,id);if(!n)return 0;
 if(n->firstChild>m->childCount||n->childCount>m->childCount-n->firstChild)return 0;
 XL6_IrNode value;memcpy(&value,n,sizeof(value));value.text=NULL;value.literalData=NULL;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
 value.compiledPackageLink=NULL;
#endif
 /* Exact in-process values,not hashes. Padding can only cause a miss. The
  * complete pinned native schema has exactly these owned pointer fields. */
 if(!xlsCallPut(t,&value,sizeof(value))||!xlsCallText(t,n->text))return 0;
 unsigned present=n->literalData!=NULL;
 if(!xlsCallPut(t,&present,sizeof(present)))return 0;
 if(present&&(n->literalSize==SIZE_MAX||!xlsCallPut(t,n->literalData,n->literalSize+1)))return 0;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
 if(!xlsCallText(t,n->compiledPackageLink))return 0;
#endif
 return n->childCount<=SIZE_MAX/sizeof(*m->children)&&xlsCallPut(t,
  n->childCount?m->children+n->firstChild:NULL,n->childCount*sizeof(*m->children));
}
static int xlsCallEntryKey(xls_call_trace* t,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(t->module,id);if(!n)return 0;
 /* Exact read set of the entry kind tests and the two pinned syntax-call
  * predicates. Actual call construction goes through the full-node gateway. */
 return xlsCallPut(t,&n->kind,sizeof(n->kind))&&xlsCallPut(t,&n->targetSymbol,sizeof(n->targetSymbol))&&
  xlsCallPut(t,&n->callKind,sizeof(n->callKind))&&xlsCallPut(t,&n->opcode,sizeof(n->opcode))&&xlsCallPut(t,&n->lvalueKind,sizeof(n->lvalueKind));
}
static int xlsCallTypeKey(xls_call_trace* t,XL6_IrTypeId id)
{
 const XL6_IrType* v=xl6IrTypeGet(&t->module->types,id);unsigned present=v!=NULL;
 if(!xlsCallPut(t,&present,sizeof(present)))return 0;
 if(!v)return 1;
#define TYPE_KEY(f) do{if(!xlsCallPut(t,&v->f,sizeof(v->f)))return 0;}while(0)
#define TYPE_VECTOR(f,p) do{unsigned has=v->f!=NULL;if(!xlsCallPut(t,&has,sizeof(has)))return 0; \
 if(has&&(v->paramCount>SIZE_MAX/sizeof(*v->f)||!xlsCallPut(t,v->f,v->paramCount*sizeof(*v->f))))return 0;}while(0)
 TYPE_KEY(kind);TYPE_KEY(cAbi);TYPE_KEY(callableErasureAbi);TYPE_KEY(paramCount);
 TYPE_VECTOR(params,0);TYPE_VECTOR(parameterModes,0);TYPE_VECTOR(parameterAbis,0);
#undef TYPE_VECTOR
#undef TYPE_KEY
 return 1;
}
static int xlsCallRead(xls_call_trace* t,uint32_t kind,uint32_t id)
{
 if(t->reads>=t->readCapacity){t->failed=1;return 0;}
 t->ids[t->reads++]=(xls_call_read){kind,id};
 return kind==0?xlsCallNodeKey(t,id):kind==1?xlsCallTypeKey(t,id):xlsCallEntryKey(t,id);
}
static const XL6_IrNode* xlsCallsNodeGet(const XL6_IrModule* m,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,id);xls_call_trace* t=xlsCallsActiveTrace;
 if(n&&t&&m==t->module&&!t->failed&&t->seen[id-1]!=t->epoch){
  t->seen[id-1]=t->epoch;
  if(!xlsCallRead(t,0,id))t->failed=1;
 }
 return n;
}
static const XL6_IrNode* xlsCallsEntryGet(const XL6_IrModule* m,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,id);xls_call_trace* t=xlsCallsActiveTrace;
 if(n&&t&&m==t->module&&!t->failed&&!xlsCallRead(t,2,id))t->failed=1;
 return n;
}
static const XL6_IrType* xlsCallsTypeGet(const XL6_IrTypePool* pool,XL6_IrTypeId id)
{
 const XL6_IrType* v=xl6IrTypeGet(pool,id);xls_call_trace* t=xlsCallsActiveTrace;
 if(t&&!t->failed){if(pool!=&t->module->types||!xlsCallRead(t,1,id))t->failed=1;}
 return v;
}
static int xlsCallContext(xstrbuf* key,const XL6_IrModule* m,size_t limit)
{
#define CALL_KEY(v) do{if(!xlsIrMemoData(key,&(v),sizeof(v),limit))return 0;}while(0)
#define CALL_VECTOR(p,n) do{if((n)>SIZE_MAX/sizeof(*(p))||((n)&&!(p))||!xlsIrMemoData(key,(p),(n)*sizeof(*(p)),limit))return 0;}while(0)
 CALL_KEY(m->root);CALL_KEY(m->nodeCount);CALL_KEY(m->childCount);
 CALL_KEY(m->symbolNodeCapacity);CALL_VECTOR(m->symbolNodes,m->symbolNodeCapacity);
 CALL_KEY(m->intrinsicCallCount);
 for(size_t i=0;i<m->intrinsicCallCount;++i)CALL_KEY(m->intrinsicCalls[i].origin);
 CALL_KEY(m->classDispatchSlotCount);
 for(size_t i=0;i<m->classDispatchSlotCount;++i)CALL_KEY(m->classDispatchSlots[i].declarationSymbol);
 CALL_KEY(m->classDispatchImplCount);
 for(size_t i=0;i<m->classDispatchImplCount;++i){CALL_KEY(m->classDispatchImpls[i].implementationSymbol);CALL_KEY(m->classDispatchImpls[i].slot);}
 CALL_KEY(m->protocolDispatchSlotCount);
 for(size_t i=0;i<m->protocolDispatchSlotCount;++i){CALL_KEY(m->protocolDispatchSlots[i].declarationSymbol);CALL_KEY(m->protocolDispatchSlots[i].protocolType);}
 /* Existing rows precede all new range production. The cloned builder only
  * appends its output;the original finalizer runs outside captured ranges. */
 CALL_KEY(m->callSiteCount);CALL_VECTOR(m->callSites,m->callSiteCount);CALL_KEY(m->callArgumentCount);
 for(size_t i=0;i<m->callArgumentCount;++i){
  XL6_IrCallArgument a=m->callArguments[i];const char* name=a.name;a.name=NULL;CALL_KEY(a);
  size_t n=name?strlen(name)+1:0;CALL_KEY(n);if(n&&!xlsIrMemoData(key,name,n,limit))return 0;
 }
#undef CALL_VECTOR
#undef CALL_KEY
 return 1;
}
static int xlsCallRegion(size_t* bytes,size_t count,size_t width,size_t align,size_t* at)
{
 size_t pad=(align-*bytes%align)%align;if(pad>SIZE_MAX-*bytes)return 0;*bytes+=pad;*at=*bytes;
 if(count>(SIZE_MAX-*bytes)/width)return 0;
 *bytes+=count*width;return 1;
}
static xls_call_unit* xlsCallCapture(const XL6_IrModule* m,const xls_call_trace* t,
 size_t first,size_t end,size_t siteStart,size_t argStart,size_t limit)
{
 size_t sites=m->callSiteCount-siteStart,args=m->callArgumentCount-argStart;
 size_t bytes=sizeof(xls_call_unit),ids,key,ss,aa,names;
 if(t->failed||!xlsCallRegion(&bytes,t->reads,sizeof(*t->ids),_Alignof(xls_call_read),&ids)||
    !xlsCallRegion(&bytes,t->used,1,1,&key)||!xlsCallRegion(&bytes,sites,sizeof(XL6_IrCallSite),_Alignof(XL6_IrCallSite),&ss)||
    !xlsCallRegion(&bytes,args,sizeof(XL6_IrCallArgument),_Alignof(XL6_IrCallArgument),&aa))return NULL;
 names=bytes;
 for(size_t i=argStart;i<m->callArgumentCount;++i){const char* p=m->callArguments[i].name;
  if(p){size_t n=strlen(p)+1;if(n>SIZE_MAX-bytes)return NULL;bytes+=n;}}
 if(bytes>limit)return NULL;
 xls_call_unit* u=xrtMalloc(bytes);if(!u)return NULL;
 *u=(xls_call_unit){.first=first,.end=end,.reads=t->reads,.keyBytes=t->used,.sites=sites,.args=args,.bytes=bytes};xrtAtomic32Init(&u->refs,1);
 unsigned char* data=(unsigned char*)u;u->ids=(xls_call_read*)(data+ids);u->key=data+key;u->site=(XL6_IrCallSite*)(data+ss);u->arg=(XL6_IrCallArgument*)(data+aa);
 if(t->reads)memcpy(u->ids,t->ids,t->reads*sizeof(*u->ids));
 if(t->used)memcpy(u->key,t->key,t->used);
 for(size_t i=0;i<sites;++i){u->site[i]=m->callSites[siteStart+i];
  if(u->site[i].firstArgument<argStart||u->site[i].firstArgument-argStart>args||u->site[i].argumentCount>args-(u->site[i].firstArgument-argStart)){xrtFree(u);return NULL;}
  u->site[i].firstArgument-=argStart;
 }
 for(size_t i=0;i<args;++i){u->arg[i]=m->callArguments[argStart+i];const char* p=u->arg[i].name;
  if(p){size_t n=strlen(p)+1;u->arg[i].name=(char*)data+names;memcpy(data+names,p,n);names+=n;}}
 return u;
}
static int xlsCallMatches(xls_call_trace* t,const xls_call_unit* u,size_t first,size_t end)
{
 if(!u||u->first!=first||u->end!=end||u->keyBytes>t->capacity)return 0;
 t->used=0;t->failed=0;
 for(size_t i=0;i<u->reads;++i){
  if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;
  xls_call_read v=u->ids[i];if(!(v.kind==0?xlsCallNodeKey(t,v.id):v.kind==1?xlsCallTypeKey(t,v.id):xlsCallEntryKey(t,v.id)))return 0;
 }
 return !t->failed&&t->used==u->keyBytes&&!memcmp(t->key,u->key,t->used);
}
static int xlsCallApply(XL6_IrModule* m,const xls_call_unit* u)
{
 if(u->sites>SIZE_MAX-m->callSiteCount||u->args>SIZE_MAX-m->callArgumentCount)return 0;
 size_t siteEnd=m->callSiteCount+u->sites,argEnd=m->callArgumentCount+u->args,base=m->callArgumentCount;
 while(siteEnd>m->callSiteCapacity)if(!xlsIrTypeGrow((void**)&m->callSites,&m->callSiteCapacity,siteEnd-1,sizeof(*m->callSites)))return 0;
 while(argEnd>m->callArgumentCapacity)if(!xlsIrTypeGrow((void**)&m->callArguments,&m->callArgumentCapacity,argEnd-1,sizeof(*m->callArguments)))return 0;
 for(size_t i=0;i<u->args;++i){XL6_IrCallArgument a=u->arg[i];
  if(a.name){a.name=xlsIrTypeStrDupN(a.name,strlen(a.name));if(!a.name)return 0;}
  m->callArguments[m->callArgumentCount++]=a;
 }
 for(size_t i=0;i<u->sites;++i){XL6_IrCallSite s=u->site[i];s.firstArgument+=base;m->callSites[m->callSiteCount++]=s;}
 return 1;
}
static int xlsCallPhase(XL6_IrModule* m,xls_call_build* build)
{
 /* All allocation here is optional metadata except xlsCallApply's native
  * arrays/names. Every miss executes the qualified native loop body. */
 size_t count=1;for(size_t i=1;i<m->nodeCount;++i)count+=m->nodes[i].parent==m->root;
 xls_ir_call_cache* next=xrtCalloc(1,sizeof(*next));void* scratch=NULL;
 if(!next||build->limit<sizeof(*next)||count>(build->limit-sizeof(*next))/sizeof(xls_call_unit*)||
    !xrtArrayInit(&next->units,sizeof(xls_call_unit*))||
    !xrtArrayResize(&next->units,count))goto decline;
 if(next->units.Capacity>(build->limit-sizeof(*next))/sizeof(xls_call_unit*))goto decline;
 next->bytes=sizeof(*next)+next->units.Capacity*sizeof(xls_call_unit*);
 if(!xlsCallContext(&next->context,m,build->limit-next->bytes))goto decline;
 size_t contextBytes=next->context.Data?next->context.Capacity+1:0;
 if(contextBytes>build->limit-next->bytes)goto decline;
 next->bytes+=contextBytes;
 if(m->nodeCount>build->limit/sizeof(uint32_t))goto decline;
 size_t seenBytes=m->nodeCount*sizeof(uint32_t),readCapacity=(build->limit-seenBytes)/(2*sizeof(xls_call_read));
 size_t base=seenBytes+readCapacity*sizeof(xls_call_read);
 if(base>=build->limit||!readCapacity)goto decline;
 scratch=xrtMalloc(build->limit);if(!scratch)goto decline;
 memset(scratch,0,seenBytes);
 xls_call_trace trace={.module=m,.seen=scratch,.ids=(xls_call_read*)((unsigned char*)scratch+seenBytes),.readCapacity=readCapacity,.key=(unsigned char*)scratch+base,.capacity=build->limit-base};
 _Static_assert(sizeof(XL6_IrNodeId)==sizeof(uint32_t)&&sizeof(XL6_IrTypeId)==sizeof(uint32_t),"call trace input IDs");
 int same=build->previous&&xlsIrMemoEqual(&next->context,&build->previous->context);
 size_t first=0,slot=0;
 while(first<m->nodeCount){
  if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))goto failed;
  size_t end=first+1;while(end<m->nodeCount&&m->nodes[end].parent!=m->root)++end;
  const xls_call_unit* prior=same&&slot<build->previous->units.Count?*(xls_call_unit*const*)xrtArrayConstGet(&build->previous->units,slot):NULL;
  xls_call_unit* unit=NULL;
  if(prior&&prior->bytes<=build->limit-next->bytes&&xlsCallMatches(&trace,prior,first,end)){
   if(!xlsCallApply(m,prior))goto failed;
   unit=xlsCallUnitRetain((xls_call_unit*)prior);++build->work.reused;
   build->work.restoredSites+=prior->sites;build->work.restoredArguments+=prior->args;
  }else{
   ++build->work.checked;trace.reads=trace.used=0;trace.failed=0;
   if(!++trace.epoch){memset(trace.seen,0,seenBytes);++trace.epoch;}
   size_t site=m->callSiteCount,arg=m->callArgumentCount;
   xls_call_trace* outer=xlsCallsActiveTrace;xlsCallsActiveTrace=&trace;
   int okay=xlsCallsNativeBuildCallSites(m,first,end,&build->work.nativeNodes);xlsCallsActiveTrace=outer;
   if(!okay||xlsFrontendCancelObserved()||xlsActiveTypeResource->failed)goto failed;
   unit=xlsCallCapture(m,&trace,first,end,site,arg,build->limit-next->bytes);
   if(unit)++build->work.captured;else ++build->work.declined;
  }
  *(xls_call_unit**)xrtArrayGet(&next->units,slot++)=unit;if(unit)next->bytes+=unit->bytes;first=end;
 }
 xrtFree(scratch);
 if(!xl6IrBuildCallSiteContracts(m)){xlsIrCallCacheDestroy(next);return 0;}
 build->next=next;return 1;
failed:
 xrtFree(scratch);xlsIrCallCacheDestroy(next);return 0;
decline:
 ++build->work.declined;xrtFree(scratch);xlsIrCallCacheDestroy(next);
 build->work.nativeNodes+=m->nodeCount;return xl6IrBuildCallSites(m);
}
