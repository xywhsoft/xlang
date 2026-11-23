/* Shared input proof for actual native phases28/29,with original global order. */
typedef struct xls_hg_unit {
 xatomic32 refs;size_t first,end,reads,keyBytes,binds,projects,bytes;
 xls_ir_input_read* ids;unsigned char* key;
 XL6_IrNativeHandleBind* bind;XL6_IrNativeHandleProject* project;
} xls_hg_unit;
struct xls_ir_handle_group {xstrbuf context;xarray units;size_t bytes;};
typedef struct xls_hg_pending {
 size_t first,end,bindStart,bindCount,reads;size_t idBytes;
 xls_ir_input_read* ids;const xls_hg_unit* prior;int capture;
} xls_hg_pending;
typedef struct xls_handle_build {
 const xls_ir_handle_group* previous;xls_ir_handle_group* next;
 size_t limit,workingBytes,scratchBytes;void* scratch;xarray pending;
 xls_ir_read_log trace;xls_ir_handle_group_work work;int ready;
} xls_handle_build;
static _Thread_local xls_ir_read_log* xlsHGActiveTrace;
static void xlsHGRelease(xls_hg_unit* u)
{
 if(u&&xrtAtomic32FetchSub(&u->refs,1,XMEMORY_ACQ_REL)==1){
#ifdef XLS_HANDLE_GROUP_UNIT_FREE_TEST
  XLS_HANDLE_GROUP_UNIT_FREE_TEST(u,u->bytes);
#endif
  xrtFree(u);
 }
}
static xls_hg_unit* xlsHGRetain(xls_hg_unit* u)
{if(u)xrtAtomic32FetchAdd(&u->refs,1,XMEMORY_RELAXED);return u;}
void xlsIrHandleGroupDestroy(xls_ir_handle_group* c)
{
 if(!c)return;
 for(size_t i=0;i<c->units.Count;++i)xlsHGRelease(*(xls_hg_unit**)xrtArrayGet(&c->units,i));
 xrtArrayUnit(&c->units);xrtStrBufFree(&c->context);xrtFree(c);
}
size_t xlsIrHandleGroupBytes(const xls_ir_handle_group* c){return c?c->bytes:0;}
static void xlsHGWorkingClear(xls_handle_build* b)
{
 for(size_t i=0;i<b->pending.Count;++i){xls_hg_pending* p=xrtArrayGet(&b->pending,i);xrtFree(p->ids);}
 xrtArrayUnit(&b->pending);xrtFree(b->scratch);b->scratch=NULL;b->workingBytes=b->scratchBytes=0;b->ready=0;
}
static const XL6_IrNode* xlsHGNodeGet(const XL6_IrModule* m,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,id);xls_ir_read_log* t=xlsHGActiveTrace;
 if(n&&t&&t->input.module==m)xlsIrInputRecord(t,0,id);
 return n;
}
static const XL6_IrNode* xlsHGEntryGet(const XL6_IrModule* m,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,id);xls_ir_read_log* t=xlsHGActiveTrace;
 if(n&&t&&t->input.module==m)xlsIrInputRecord(t,n->kind==XL6_IR_NODE_CALL||n->kind==XL6_IR_NODE_MEMBER?0:2,id);
 return n;
}
static const XL6_IrType* xlsHGTypeGet(const XL6_IrTypePool* pool,XL6_IrTypeId id)
{
 const XL6_IrType* v=xl6IrTypeGet(pool,id);xls_ir_read_log* t=xlsHGActiveTrace;
 if(t){if(pool!=&t->input.module->types)t->input.failed=1;else xlsIrInputRecord(t,1,id);}
 return v;
}
static int xlsHGNodeKey(xls_ir_input_trace* t,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(t->module,id);if(!n)return 0;
#define HG_VALUE(f) if(!xlsIrInputPut(t,&n->f,sizeof(n->f)))return 0;
#define HG_TEXT(f) if(!xlsIrInputText(t,n->f))return 0;
 XLS_HG_NODE_FIELDS(HG_VALUE,HG_TEXT)
#undef HG_VALUE
#undef HG_TEXT
 if(n->firstChild>t->module->childCount||n->childCount>t->module->childCount-n->firstChild||n->childCount>SIZE_MAX/sizeof(*t->module->children))return 0;
 return xlsIrInputPut(t,n->childCount?t->module->children+n->firstChild:NULL,n->childCount*sizeof(*t->module->children));
}
static int xlsHGTypeKey(xls_ir_input_trace* t,XL6_IrTypeId id)
{
 const XL6_IrType* v=xl6IrTypeGet(&t->module->types,id);unsigned present=v!=NULL;
 if(!xlsIrInputPut(t,&present,sizeof(present)))return 0;
 if(!v)return 1;
#define HG_TYPE(f) if(!xlsIrInputPut(t,&v->f,sizeof(v->f)))return 0;
 HG_TYPE(kind);HG_TYPE(cAbi);HG_TYPE(nominalSymbol);HG_TYPE(ownershipAbi);HG_TYPE(paramCount);
#undef HG_TYPE
 present=v->params!=NULL;
 return xlsIrInputPut(t,&present,sizeof(present))&&(!present||(v->paramCount<=SIZE_MAX/sizeof(*v->params)&&xlsIrInputPut(t,v->params,v->paramCount*sizeof(*v->params))));
}
static int xlsHGReadKey(xls_ir_input_trace* t,xls_ir_input_read r)
{
 if(r.kind==0)return xlsHGNodeKey(t,r.id);
 if(r.kind==1)return xlsHGTypeKey(t,r.id);
 const XL6_IrNode* n=xl6IrNodeGet(t->module,r.id);return n&&xlsIrInputPut(t,&n->kind,sizeof(n->kind));
}
static int xlsHGContext(xstrbuf* key,const XL6_IrModule* m,size_t limit)
{
 if(m->specializedSymbolCount>m->specializedSymbolCapacity||
    m->genericSpecializationCount>m->genericSpecializationCapacity||
    (m->specializedSymbolCount&&!m->specializedSymbols)||
    (m->genericSpecializationCount&&!m->genericSpecializations)||
    (m->classLayoutCount&&!m->classLayouts)||(m->classFieldCount&&!m->classFields)||
    (m->nodeCount&&!m->nodes)||(m->childCount&&!m->children))return 0;
#define HG_KEY(v) do{if(!xlsIrMemoData(key,&(v),sizeof(v),limit))return 0;}while(0)
#define HG_VECTOR(p,n) do{if((n)>SIZE_MAX/sizeof(*(p))||((n)&&!(p))||!xlsIrMemoData(key,(p),(n)*sizeof(*(p)),limit))return 0;}while(0)
#define HG_STRING(p) do{size_t len=(p)?strlen(p)+1:0;HG_KEY(len);if(len&&!xlsIrMemoData(key,p,len,limit))return 0;}while(0)
 HG_KEY(m->root);HG_KEY(m->nodeCount);HG_KEY(m->childCount);HG_KEY(m->symbolNodeCapacity);HG_VECTOR(m->symbolNodes,m->symbolNodeCapacity);
 HG_KEY(m->specializedSymbolCount);HG_KEY(m->specializedSymbolCapacity);
 for(size_t i=0;i<m->specializedSymbolCount;++i){HG_KEY(m->specializedSymbols[i].symbol);HG_KEY(m->specializedSymbols[i].declaration);}
 HG_KEY(m->genericSpecializationCount);HG_KEY(m->genericSpecializationCapacity);
 for(size_t i=0;i<m->genericSpecializationCount;++i){const XL6_IrGenericSpecialization* s=m->genericSpecializations+i;
  HG_KEY(s->symbol);HG_KEY(s->declaration);HG_KEY(s->sourceDeclaration);HG_KEY(s->typeArgCount);unsigned has=s->typeArgs!=NULL;HG_KEY(has);if(has)HG_VECTOR(s->typeArgs,s->typeArgCount);}
 HG_KEY(m->classLayoutCount);
 for(size_t i=0;i<m->classLayoutCount;++i){const XL6_IrClassLayout* l=m->classLayouts+i;HG_KEY(l->type);HG_KEY(l->baseType);HG_KEY(l->firstField);HG_KEY(l->fieldCount);}
 HG_KEY(m->classFieldCount);
 for(size_t i=0;i<m->classFieldCount;++i){const XL6_IrClassField* f=m->classFields+i;
  HG_KEY(f->ownerType);HG_KEY(f->type);HG_KEY(f->declaration);HG_KEY(f->nativeDestroySymbol);HG_STRING(f->name);}
 /* Negative and ordered positive proof for the original whole-node lifecycle
  * scan. Other node values are read through the traced NodeGet gateway. */
 size_t candidates=0;
 for(size_t i=0;i<m->nodeCount;++i){const XL6_IrNode* n=m->nodes+i;
  if(n->kind!=XL6_IR_NODE_FUNC_DECL||n->sourceKind!=XL6_AST_NATIVE_EXTERN_SPECIAL||!n->symbol||!n->childCount)continue;
  ++candidates;HG_KEY(i);HG_KEY(n->symbol);HG_KEY(n->parent);HG_KEY(n->firstChild);HG_KEY(n->childCount);
  if(n->firstChild>=m->childCount)return 0;
  XL6_IrNodeId child=m->children[n->firstChild];HG_KEY(child);const XL6_IrNode* name=xl6IrNodeGet(m,child);unsigned has=name!=NULL;HG_KEY(has);if(has)HG_STRING(name->text);
 }
 HG_KEY(candidates);
 HG_KEY(m->nativeHandleBindCount);HG_VECTOR(m->nativeHandleBinds,m->nativeHandleBindCount);
 HG_KEY(m->nativeHandleProjectCount);HG_VECTOR(m->nativeHandleProjects,m->nativeHandleProjectCount);
#undef HG_STRING
#undef HG_VECTOR
#undef HG_KEY
 return 1;
}
static int xlsHGMatches(xls_ir_input_trace* t,const xls_hg_unit* u,size_t first,size_t end)
{
 if(!u||u->first!=first||u->end!=end||u->keyBytes>t->capacity)return 0;
 t->used=0;t->failed=0;
 for(size_t i=0;i<u->reads;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!xlsHGReadKey(t,u->ids[i]))return 0;}
 return !t->failed&&t->used==u->keyBytes&&!memcmp(t->key,u->key,t->used);
}
static int xlsHGAppendBinds(XL6_IrModule* m,const xls_hg_unit* u)
{
 if(u->binds>SIZE_MAX-m->nativeHandleBindCount)return 0;
 size_t end=m->nativeHandleBindCount+u->binds;
 if(end>m->nativeHandleBindCapacity&&!xlsIrTypeGrow((void**)&m->nativeHandleBinds,&m->nativeHandleBindCapacity,end-1,sizeof(*m->nativeHandleBinds)))return 0;
 if(u->binds)memcpy(m->nativeHandleBinds+m->nativeHandleBindCount,u->bind,u->binds*sizeof(*u->bind));
 m->nativeHandleBindCount=end;return 1;
}
static int xlsHGAppendProjects(XL6_IrModule* m,const xls_hg_unit* u)
{
 if(u->projects>SIZE_MAX-m->nativeHandleProjectCount)return 0;
 size_t end=m->nativeHandleProjectCount+u->projects;
 if(end>m->nativeHandleProjectCapacity&&!xlsIrTypeGrow((void**)&m->nativeHandleProjects,&m->nativeHandleProjectCapacity,end-1,sizeof(*m->nativeHandleProjects)))return 0;
 if(u->projects)memcpy(m->nativeHandleProjects+m->nativeHandleProjectCount,u->project,u->projects*sizeof(*u->project));
 m->nativeHandleProjectCount=end;return 1;
}
static void xlsHGBegin(xls_handle_build* b,size_t first,size_t end)
{
 xls_ir_read_log* t=&b->trace;t->input.reads=t->input.used=0;t->input.failed=0;t->first=first;t->end=end;t->covered=0;
 if(!++t->input.epoch){memset(b->scratch,0,(t->input.module->nodeCount+t->input.module->types.count)*sizeof(uint64_t));t->input.epoch=1;}
}
static xls_hg_unit* xlsHGCapture(xls_handle_build* b,const XL6_IrModule* m,const xls_hg_pending* p,size_t projectStart)
{
 xls_ir_input_trace* t=&b->trace.input;if(t->failed||b->trace.covered!=p->end-p->first)return NULL;
 t->used=0;
 for(size_t i=0;i<t->reads;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return NULL;if(!xlsHGReadKey(t,t->ids[i]))return NULL;}
 size_t bytes=sizeof(xls_hg_unit),ids,key,binds,projects,count=m->nativeHandleProjectCount-projectStart;
 if(!xlsIrInputRegion(&bytes,t->reads,sizeof(*t->ids),_Alignof(xls_ir_input_read),&ids)||
    !xlsIrInputRegion(&bytes,t->used,1,1,&key)||
    !xlsIrInputRegion(&bytes,p->bindCount,sizeof(XL6_IrNativeHandleBind),_Alignof(XL6_IrNativeHandleBind),&binds)||
    !xlsIrInputRegion(&bytes,count,sizeof(XL6_IrNativeHandleProject),_Alignof(XL6_IrNativeHandleProject),&projects)||bytes>b->limit-b->next->bytes)return NULL;
 xls_hg_unit* u=xrtMalloc(bytes);if(!u)return NULL;
 *u=(xls_hg_unit){.first=p->first,.end=p->end,.reads=t->reads,.keyBytes=t->used,.binds=p->bindCount,.projects=count,.bytes=bytes};xrtAtomic32Init(&u->refs,1);
 unsigned char* data=(unsigned char*)u;u->ids=(xls_ir_input_read*)(data+ids);u->key=data+key;u->bind=(XL6_IrNativeHandleBind*)(data+binds);u->project=(XL6_IrNativeHandleProject*)(data+projects);
 if(t->reads)memcpy(u->ids,t->ids,t->reads*sizeof(*u->ids));
 if(t->used)memcpy(u->key,t->key,t->used);
 if(p->bindCount)memcpy(u->bind,m->nativeHandleBinds+p->bindStart,p->bindCount*sizeof(*u->bind));
 if(count)memcpy(u->project,m->nativeHandleProjects+projectStart,count*sizeof(*u->project));
 return u;
}
static int xlsHGInitialize(XL6_IrModule* m,xls_handle_build* b,size_t count)
{
 b->next=xrtCalloc(1,sizeof(*b->next));if(!b->next||b->limit<sizeof(*b->next)||
  count>(b->limit-sizeof(*b->next))/sizeof(xls_hg_unit*)||!xrtArrayInit(&b->next->units,sizeof(xls_hg_unit*))||!xrtArrayResize(&b->next->units,count))return 0;
 if(b->next->units.Capacity>(b->limit-sizeof(*b->next))/sizeof(xls_hg_unit*))return 0;
 b->next->bytes=sizeof(*b->next)+b->next->units.Capacity*sizeof(xls_hg_unit*);
 if(!xlsHGContext(&b->next->context,m,b->limit-b->next->bytes))return 0;
 size_t bytes=b->next->context.Data?b->next->context.Capacity+1:0;if(bytes>b->limit-b->next->bytes)return 0;b->next->bytes+=bytes;
 if(count>b->limit/sizeof(xls_hg_pending)||!xrtArrayInit(&b->pending,sizeof(xls_hg_pending))||!xrtArrayResize(&b->pending,count))return 0;
 if(b->pending.Capacity>b->limit/sizeof(xls_hg_pending))return 0;
 b->workingBytes=b->pending.Capacity*sizeof(xls_hg_pending);size_t available=(b->limit-b->workingBytes)/2;
 if(m->nodeCount>available/sizeof(uint64_t)||m->types.count>available/sizeof(uint64_t)-m->nodeCount)return 0;
 size_t slots=m->nodeCount+m->types.count,seen=slots*sizeof(uint64_t);
 b->scratchBytes=slots>available/256?available:slots*256;if(b->scratchBytes<65536)b->scratchBytes=65536;if(b->scratchBytes>available)b->scratchBytes=available;
 if(seen>=b->scratchBytes)return 0;
 size_t reads=(b->scratchBytes-seen)/(2*sizeof(xls_ir_input_read)),base=seen+reads*sizeof(xls_ir_input_read);if(!reads||base>=b->scratchBytes)return 0;
 b->scratch=xrtMalloc(b->scratchBytes);if(!b->scratch)return 0;memset(b->scratch,0,seen);b->workingBytes+=b->scratchBytes;
 b->trace=(xls_ir_read_log){.input={.module=m,.ids=(xls_ir_input_read*)((unsigned char*)b->scratch+seen),.readCapacity=reads,.key=(unsigned char*)b->scratch+base,.capacity=b->scratchBytes-base},.nodes=b->scratch,.types=(uint64_t*)b->scratch+m->nodeCount};
 b->ready=1;return 1;
}
static int xlsHGBinds(XL6_IrModule* m,xls_handle_build* b)
{
 size_t count=1;for(size_t i=1;i<m->nodeCount;++i)count+=m->nodes[i].parent==m->root;
 if(!xlsHGInitialize(m,b,count)){
  ++b->work.declined;xlsHGWorkingClear(b);xlsIrHandleGroupDestroy(b->next);b->next=NULL;b->work.bindNodes+=m->nodeCount;return xl6IrBuildNativeHandleBinds(m);
 }
 int same=b->previous&&xlsIrMemoEqual(&b->next->context,&b->previous->context);size_t slot=0,first=0;
 while(first<m->nodeCount){
  if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;
  size_t end=first+1;while(end<m->nodeCount&&m->nodes[end].parent!=m->root)++end;
  xls_hg_pending* p=xrtArrayGet(&b->pending,slot);*p=(xls_hg_pending){.first=first,.end=end,.bindStart=m->nativeHandleBindCount};
  const xls_hg_unit* prior=same&&slot<b->previous->units.Count?*(xls_hg_unit*const*)xrtArrayConstGet(&b->previous->units,slot):NULL;
  if(prior&&prior->bytes<=b->limit-b->next->bytes){++b->work.proofChecks;if(xlsHGMatches(&b->trace.input,prior,first,end))p->prior=prior;}
  if(p->prior){
   if(!xlsHGAppendBinds(m,p->prior))return 0;
   b->next->bytes+=p->prior->bytes;/* Reserve before considering the next range. */
   b->work.restoredBinds+=p->prior->binds;
  }
  else{
   ++b->work.checked;xlsHGBegin(b,first,end);xls_ir_read_log* outer=xlsHGActiveTrace;xlsHGActiveTrace=&b->trace;
   int okay=xlsHGNativexl6IrBuildNativeHandleBinds(m,first,end,&b->work.bindNodes);xlsHGActiveTrace=outer;
   if(!okay||xlsFrontendCancelObserved()||xlsActiveTypeResource->failed)return 0;
   p->reads=b->trace.input.reads;
   if(!b->trace.input.failed&&p->reads<=SIZE_MAX/sizeof(*p->ids)&&p->reads*sizeof(*p->ids)<=b->limit-b->workingBytes){
    p->idBytes=p->reads*sizeof(*p->ids);p->ids=p->idBytes?xrtMalloc(p->idBytes):NULL;
    if(!p->idBytes||p->ids){if(p->idBytes)memcpy(p->ids,b->trace.input.ids,p->idBytes);b->workingBytes+=p->idBytes;p->capture=1;}
   }
   if(!p->capture)++b->work.declined;
  }
  p->bindCount=m->nativeHandleBindCount-p->bindStart;first=end;++slot;
 }
 return 1;
}
static int xlsHGProjects(XL6_IrModule* m,xls_handle_build* b)
{
 if(!b->ready){b->work.projectNodes+=m->nodeCount;return xl6IrBuildNativeHandleProjects(m);}
 for(size_t slot=0;slot<b->pending.Count;++slot){
  if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;
  xls_hg_pending* p=xrtArrayGet(&b->pending,slot);xls_hg_unit* u=NULL;
  if(p->prior){
   /* The group input has not changed and is NOT revalidated here. */
   if(!xlsHGAppendProjects(m,p->prior))return 0;
   u=xlsHGRetain((xls_hg_unit*)p->prior);++b->work.reused;b->work.restoredProjects+=p->prior->projects;
  }else{
   size_t start=m->nativeHandleProjectCount;xlsHGBegin(b,p->first,p->end);
   if(p->capture)for(size_t i=0;i<p->reads;++i)xlsIrInputRecord(&b->trace,p->ids[i].kind,p->ids[i].id);
   if(p->ids){xrtFree(p->ids);p->ids=NULL;b->workingBytes-=p->idBytes;p->idBytes=0;}
   xls_ir_read_log* outer=xlsHGActiveTrace;xlsHGActiveTrace=p->capture?&b->trace:NULL;
   int okay=xlsHGNativexl6IrBuildNativeHandleProjects(m,p->first,p->end,&b->work.projectNodes);xlsHGActiveTrace=outer;
   if(!okay||xlsFrontendCancelObserved()||xlsActiveTypeResource->failed)return 0;
   if(p->capture){u=xlsHGCapture(b,m,p,start);if(u){++b->work.captured;b->next->bytes+=u->bytes;}else ++b->work.declined;}
   if(xlsFrontendCancelObserved()){xlsHGRelease(u);return 0;}
  }
  *(xls_hg_unit**)xrtArrayGet(&b->next->units,slot)=u;
 }
 xlsHGWorkingClear(b);return 1;
}
