/* Actual phase18 input proofs and independent immutable scalar outputs. */
typedef struct xls_name_value {
 XL6_IrImplicitNameAbi abi;XL6_IrTypeId receiver;XL6_IrNodeId declaration;
} xls_name_value;
typedef struct xls_name_unit {
 xatomic32 refs;size_t first,end,reads,keyBytes,bytes;
 xls_ir_input_read* ids;unsigned char* key;xls_name_value* values;
} xls_name_unit;
struct xls_ir_name_cache {xstrbuf context;xarray units;size_t bytes;};
struct xls_name_build {
 const xls_ir_name_cache* previous;xls_ir_name_cache* next;
 size_t limit;xls_ir_name_cache_work work;
};
typedef struct xls_name_trace {
 xls_ir_input_trace input;uint64_t *nodes,*types;
 size_t first,end,covered;
} xls_name_trace;
static _Thread_local xls_name_trace* xlsNamesActiveTrace;
static void xlsNameUnitRelease(xls_name_unit* u)
{
 if(u&&xrtAtomic32FetchSub(&u->refs,1,XMEMORY_ACQ_REL)==1){
#ifdef XLS_NAME_CACHE_UNIT_FREE_TEST
  XLS_NAME_CACHE_UNIT_FREE_TEST(u,u->bytes);
#endif
  xrtFree(u);
 }
}
static xls_name_unit* xlsNameUnitRetain(xls_name_unit* u)
{if(u)xrtAtomic32FetchAdd(&u->refs,1,XMEMORY_RELAXED);return u;}
void xlsIrNameCacheDestroy(xls_ir_name_cache* c)
{
 if(!c)return;
 for(size_t i=0;i<c->units.Count;++i)xlsNameUnitRelease(*(xls_name_unit**)xrtArrayGet(&c->units,i));
 xrtArrayUnit(&c->units);xrtStrBufFree(&c->context);xrtFree(c);
}
size_t xlsIrNameCacheBytes(const xls_ir_name_cache* c){return c?c->bytes:0;}
static int xlsNameNodeKey(xls_ir_input_trace* t,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(t->module,id);if(!n)return 0;
#define NAME_VALUE(f) if(!xlsIrInputPut(t,&n->f,sizeof(n->f)))return 0;
#define NAME_TEXT(f) if(!xlsIrInputText(t,n->f))return 0;
 XLS_NAME_CACHE_NODE_FIELDS(NAME_VALUE,NAME_TEXT)
#undef NAME_VALUE
#undef NAME_TEXT
 return 1;
}
static int xlsNameTypeKey(xls_ir_input_trace* t,XL6_IrTypeId id)
{
 const XL6_IrType* v=xl6IrTypeGet(&t->module->types,id);unsigned present=v!=NULL;
 return xlsIrInputPut(t,&present,sizeof(present))&&(!v||
  (xlsIrInputPut(t,&v->kind,sizeof(v->kind))&&xlsIrInputPut(t,&v->nominalSymbol,sizeof(v->nominalSymbol))));
}
static int xlsNameEntryKey(xls_ir_input_trace* t,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(t->module,id);if(!n||!xlsIrInputPut(t,&n->kind,sizeof(n->kind)))return 0;
 if(n->kind!=XL6_IR_NODE_NAME)return 1;
#define NAME_ENTRY(f) if(!xlsIrInputPut(t,&n->f,sizeof(n->f)))return 0;
 NAME_ENTRY(symbol);NAME_ENTRY(targetSymbol);NAME_ENTRY(typeValueTarget);NAME_ENTRY(valueCategory);NAME_ENTRY(storageType);
#undef NAME_ENTRY
 unsigned nonempty=n->text&&n->text[0];return xlsIrInputPut(t,&nonempty,sizeof(nonempty));
}
/* The phase never writes these inputs. Log unique IDs now;materialize their
 * exact values after native production. A full read upgrades an entry record
 * in place,including one first observed through an ancestor/field lookup. */
static void xlsNameRecord(xls_name_trace* trace,uint32_t kind,uint32_t id)
{
 xls_ir_input_trace* t=&trace->input;if(t->failed)return;
 uint64_t* slot=kind==1?(id&&id<=t->module->types.count?trace->types+id-1:NULL):trace->nodes+id-1;
 if(slot&&(*slot>>32)==t->epoch){
  size_t at=(uint32_t)*slot-1;if(kind==0)t->ids[at].kind=0;return;
 }
 if(t->reads>=t->readCapacity||t->reads>=UINT32_MAX){t->failed=1;return;}
 t->ids[t->reads++]=(xls_ir_input_read){kind,id};
 if(slot)*slot=((uint64_t)t->epoch<<32)|(uint32_t)t->reads;
 if(kind!=1&&id>trace->first&&id<=trace->end)++trace->covered;
}
static const XL6_IrNode* xlsNamesNodeGet(const XL6_IrModule* m,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,id);xls_name_trace* t=xlsNamesActiveTrace;
 if(n&&t&&m==t->input.module)xlsNameRecord(t,0,id);
 return n;
}
static const XL6_IrNode* xlsNamesEntryGet(const XL6_IrModule* m,XL6_IrNodeId id)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,id);xls_name_trace* t=xlsNamesActiveTrace;
 if(n&&t&&m==t->input.module)xlsNameRecord(t,2,id);
 return n;
}
static const XL6_IrType* xlsNamesTypeGet(const XL6_IrTypePool* pool,XL6_IrTypeId id)
{
 const XL6_IrType* v=xl6IrTypeGet(pool,id);xls_name_trace* t=xlsNamesActiveTrace;
 if(t){if(pool!=&t->input.module->types)t->input.failed=1;else xlsNameRecord(t,1,id);}
 return v;
}
static int xlsNameContext(xstrbuf* key,const XL6_IrModule* m,size_t limit)
{
#define NAME_KEY(v) do{if(!xlsIrMemoData(key,&(v),sizeof(v),limit))return 0;}while(0)
#define NAME_VECTOR(p,n) do{if((n)>SIZE_MAX/sizeof(*(p))||((n)&&!(p))||!xlsIrMemoData(key,(p),(n)*sizeof(*(p)),limit))return 0;}while(0)
 NAME_KEY(m->root);NAME_KEY(m->nodeCount);NAME_KEY(m->symbolNodeCapacity);NAME_VECTOR(m->symbolNodes,m->symbolNodeCapacity);
 int original=xlsActiveIrScan&&xlsActiveIrScan->originalClassifiers;NAME_KEY(original);
 NAME_KEY(m->classLayoutCount);
 for(size_t i=0;i<m->classLayoutCount;++i){const XL6_IrClassLayout* c=m->classLayouts+i;
  NAME_KEY(c->declaration);NAME_KEY(c->type);NAME_KEY(c->baseType);NAME_KEY(c->firstField);NAME_KEY(c->fieldCount);}
 NAME_KEY(m->classFieldCount);
 for(size_t i=0;i<m->classFieldCount;++i){const XL6_IrClassField* f=m->classFields+i;
  NAME_KEY(f->ownerType);NAME_KEY(f->type);NAME_KEY(f->declaration);
  size_t n=f->name?strlen(f->name)+1:0;NAME_KEY(n);if(n&&!xlsIrMemoData(key,f->name,n,limit))return 0;}
#undef NAME_VECTOR
#undef NAME_KEY
 return 1;
}
static xls_name_unit* xlsNameCapture(const XL6_IrModule* m,xls_name_trace* trace,size_t first,size_t end,size_t limit)
{
 xls_ir_input_trace* t=&trace->input;
 if(t->failed||trace->covered!=end-first)return NULL;
 t->used=0;
 for(size_t i=0;i<t->reads;++i){
  if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return NULL;
  xls_ir_input_read v=t->ids[i];if(!(v.kind==0?xlsNameNodeKey(t,v.id):v.kind==1?xlsNameTypeKey(t,v.id):xlsNameEntryKey(t,v.id)))return NULL;
 }
 size_t bytes=sizeof(xls_name_unit),ids,key,values;
 if(t->failed||!xlsIrInputRegion(&bytes,t->reads,sizeof(*t->ids),_Alignof(xls_ir_input_read),&ids)||
    !xlsIrInputRegion(&bytes,t->used,1,1,&key)||!xlsIrInputRegion(&bytes,end-first,sizeof(xls_name_value),_Alignof(xls_name_value),&values)||bytes>limit)return NULL;
 xls_name_unit* u=xrtMalloc(bytes);if(!u)return NULL;
 *u=(xls_name_unit){.first=first,.end=end,.reads=t->reads,.keyBytes=t->used,.bytes=bytes};xrtAtomic32Init(&u->refs,1);
 unsigned char* p=(unsigned char*)u;u->ids=(xls_ir_input_read*)(p+ids);u->key=p+key;u->values=(xls_name_value*)(p+values);
 if(t->reads)memcpy(u->ids,t->ids,t->reads*sizeof(*u->ids));
 if(t->used)memcpy(u->key,t->key,t->used);
 for(size_t i=first;i<end;++i){const XL6_IrNode* n=m->nodes+i;u->values[i-first]=(xls_name_value){n->implicitNameAbi,n->implicitReceiverType,n->implicitFieldDeclaration};}
 return u;
}
static int xlsNameMatchesApply(xls_ir_input_trace* t,const xls_name_unit* u,size_t first,size_t end,XL6_IrModule* destination)
{
 if(!u||u->first!=first||u->end!=end||u->keyBytes>t->capacity)return 0;
 t->used=0;t->failed=0;
 for(size_t i=0;i<u->reads;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;
  xls_ir_input_read v=u->ids[i];if(!(v.kind==0?xlsNameNodeKey(t,v.id):v.kind==1?xlsNameTypeKey(t,v.id):xlsNameEntryKey(t,v.id)))return 0;
  if(destination&&v.kind!=1&&v.id>first&&v.id<=end){
   XL6_IrNode* n=destination->nodes+v.id-1;const xls_name_value* value=u->values+v.id-1-first;
   n->implicitNameAbi=value->abi;n->implicitReceiverType=value->receiver;n->implicitFieldDeclaration=value->declaration;
  }
 }
 return !t->failed&&t->used==u->keyBytes&&!memcmp(t->key,u->key,t->used);
}
#ifdef XLS_NAME_CACHE_UNIT_FREE_TEST
static int xlsNameMatches(xls_ir_input_trace* t,const xls_name_unit* u,size_t first,size_t end)
{return xlsNameMatchesApply(t,u,first,end,NULL);}
#endif
static int xlsNamePhase(XL6_IrModule* m,xls_name_build* build)
{
 /* The reviewed classifier returns default outputs when no class layout
  * exists. Keep that native path and allocate no optional cache metadata. */
 if(!m->classLayoutCount){++build->work.declined;build->work.nativeNodes+=m->nodeCount;return xl6IrBuildImplicitNameAbis(m);}
 size_t count=1;for(size_t i=1;i<m->nodeCount;++i)count+=m->nodes[i].parent==m->root;
 xls_ir_name_cache* next=xrtCalloc(1,sizeof(*next));void* scratch=NULL;
 if(!next||build->limit<sizeof(*next)||count>(build->limit-sizeof(*next))/sizeof(xls_name_unit*)||
    !xrtArrayInit(&next->units,sizeof(xls_name_unit*))||!xrtArrayResize(&next->units,count))goto decline;
 if(next->units.Capacity>(build->limit-sizeof(*next))/sizeof(xls_name_unit*))goto decline;
 next->bytes=sizeof(*next)+next->units.Capacity*sizeof(xls_name_unit*);
 if(!xlsNameContext(&next->context,m,build->limit-next->bytes))goto decline;
 size_t contextBytes=next->context.Data?next->context.Capacity+1:0;if(contextBytes>build->limit-next->bytes)goto decline;
 next->bytes+=contextBytes;
 if(m->nodeCount>build->limit/sizeof(uint64_t)||m->types.count>build->limit/sizeof(uint64_t)-m->nodeCount)goto decline;
 size_t slots=m->nodeCount+m->types.count,seenBytes=slots*sizeof(uint64_t);
 /* Shape-sized optional scratch,never more than the caller's cap. Very long
  * identifiers can decline capture;native production is still complete. */
 size_t scratchBytes=slots>build->limit/256?build->limit:slots*256;
 if(scratchBytes<256u*1024u)scratchBytes=256u*1024u;
 if(scratchBytes>build->limit)scratchBytes=build->limit;
 if(seenBytes>=scratchBytes)goto decline;
 size_t readCapacity=(scratchBytes-seenBytes)/(2*sizeof(xls_ir_input_read));
 size_t base=seenBytes+readCapacity*sizeof(xls_ir_input_read);if(base>=scratchBytes||!readCapacity)goto decline;
 scratch=xrtMalloc(scratchBytes);if(!scratch)goto decline;memset(scratch,0,seenBytes);
 xls_name_trace trace={.input={.module=m,.ids=(xls_ir_input_read*)((unsigned char*)scratch+seenBytes),.readCapacity=readCapacity,.key=(unsigned char*)scratch+base,.capacity=scratchBytes-base},.nodes=scratch,.types=(uint64_t*)scratch+m->nodeCount};
 int same=build->previous&&xlsIrMemoEqual(&next->context,&build->previous->context);size_t first=0,slot=0;
 while(first<m->nodeCount){
  if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))goto failed;
  size_t end=first+1;while(end<m->nodeCount&&m->nodes[end].parent!=m->root)++end;
  const xls_name_unit* prior=same&&slot<build->previous->units.Count?*(xls_name_unit*const*)xrtArrayConstGet(&build->previous->units,slot):NULL;
  xls_name_unit* unit=NULL;
  if(prior&&prior->bytes<=build->limit-next->bytes&&xlsNameMatchesApply(&trace.input,prior,first,end,m)){
   unit=xlsNameUnitRetain((xls_name_unit*)prior);++build->work.reused;build->work.restoredNodes+=end-first;
  }else{
   /* A late mismatch may have restored some values. The native miss writes
    * all three outputs for every node;none of them are producer inputs. */
   ++build->work.checked;trace.input.reads=trace.input.used=0;trace.input.failed=0;trace.first=first;trace.end=end;trace.covered=0;
   if(!++trace.input.epoch){memset(scratch,0,seenBytes);trace.input.epoch=1;}
   xls_name_trace* outer=xlsNamesActiveTrace;xlsNamesActiveTrace=&trace;
   int okay=xlsNamesNativexl6IrBuildImplicitNameAbis(m,first,end,&build->work.nativeNodes);xlsNamesActiveTrace=outer;
   if(!okay||xlsFrontendCancelObserved()||xlsActiveTypeResource->failed)goto failed;
   unit=xlsNameCapture(m,&trace,first,end,build->limit-next->bytes);if(unit)++build->work.captured;else ++build->work.declined;
   if(xlsFrontendCancelObserved()){xlsNameUnitRelease(unit);goto failed;}
  }
  *(xls_name_unit**)xrtArrayGet(&next->units,slot++)=unit;if(unit)next->bytes+=unit->bytes;first=end;
 }
 xrtFree(scratch);build->next=next;return 1;
failed:
 xrtFree(scratch);xlsIrNameCacheDestroy(next);return 0;
decline:
 ++build->work.declined;xrtFree(scratch);xlsIrNameCacheDestroy(next);build->work.nativeNodes+=m->nodeCount;return xl6IrBuildImplicitNameAbis(m);
}
