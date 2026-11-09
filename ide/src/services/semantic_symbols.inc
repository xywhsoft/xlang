/* Compact immutable declarations over an already verified original index.
 * The private node-shaped view is used ONLY by the existing wire serializer;
 * it is never exposed as a general navigation/binding table. */
#define SYMBOL_PROJECTION_BYTES (64u*1024u*1024u)
#define SYMBOL_TEXT_BYTES (4u*1024u*1024u)
struct xide_semantic_symbols {xatomic32 references;xide_semantic_definitions view;size_t bytes;};
typedef struct symbol_build_row {node value;size_t ordinal;enum xls_declaration_kind role;xls_index_range fullCompiler,nameCompiler;} symbol_build_row;
typedef struct symbol_scope {unsigned id,depth;size_t nearest;int hidden,callable;} symbol_scope;

void xideSemanticSymbolsDestroy(xide_semantic_symbols* s)
{
 if(!s || xrtAtomic32FetchSub(&s->references,1,XMEMORY_ACQ_REL)!=1)return;
 if(s->view.nodes)for(size_t i=0;i<s->view.count;++i){xrtFree(s->view.nodes[i].name);xrtFree(s->view.nodes[i].header);}
 for(size_t i=0;i<s->view.sourceCount;++i)xrtFree(s->view.paths ? s->view.paths[i] : NULL);
 xrtFree(s->view.nodes);xrtFree(s->view.paths);xrtFree(s);
}
xide_semantic_symbols* xideSemanticSymbolsRetain(xide_semantic_symbols* s)
{if(s)xrtAtomic32FetchAdd(&s->references,1,XMEMORY_RELAXED);return s;}
size_t xideSemanticSymbolsBytes(const xide_semantic_symbols* s){return s ? s->bytes : 0;}
size_t xideSemanticSymbolsCount(const xide_semantic_symbols* s){return s ? s->view.count : 0;}
int xideSemanticSymbolAt(const xide_semantic_symbols* s,size_t at,xide_semantic_symbol_view* out)
{
 if(!s || !out || at>=s->view.count)return 0;
 const node* n=s->view.nodes+at;
 *out=(xide_semantic_symbol_view){.path=s->view.paths[n->full.source-1],.name=n->name,.detail=n->header,.kind=n->kind,
  .parent=n->parent ? (size_t)n->parent-1 : SIZE_MAX,
  .start={n->full.start.line,n->full.start.character,0},.end={n->full.end.line,n->full.end.character,0},
  .selectionStart={n->selection.start.line,n->selection.start.character,0},.selectionEnd={n->selection.end.line,n->selection.end.character,0}};
 return 1;
}
xvalue* xideSemanticSymbolsDocument(xide_semantic_symbols* s,const char* path,const char* uri)
{return s ? xideSemanticDocumentSymbolsForClient(&s->view,path,uri) : NULL;}
static int symbolWireKind(enum xls_declaration_kind role)
{
 switch(role){
 case XLS_DECL_FUNCTION:return 12;case XLS_DECL_CLASS:return 5;case XLS_DECL_INTERFACE:return 11;
 case XLS_DECL_ENUM:return 10;case XLS_DECL_ENUM_MEMBER:return 22;case XLS_DECL_RECORD:return 23;
 case XLS_DECL_ALIAS:case XLS_DECL_TYPE_PARAMETER:return 26;case XLS_DECL_VARIABLE:return 13;
 case XLS_DECL_CONSTANT:return 14;case XLS_DECL_FIELD:return 8;case XLS_DECL_PROPERTY:return 7;
 case XLS_DECL_SPECIAL:return 6;default:return 0;
 }
}
static int symbolStopped(int (*stopped)(void*),void* user){return stopped && stopped(user);}
static int compareSymbolRows(const void* av,const void* bv)
{
 const symbol_build_row *a=av,*b=bv;const span *x=&a->value.full,*y=&b->value.full;
 if(x->source!=y->source)return (x->source>y->source)-(x->source<y->source);
 if(x->startOffset!=y->startOffset)return (x->startOffset>y->startOffset)-(x->startOffset<y->startOffset);
 if(x->endOffset!=y->endOffset)return (x->endOffset<y->endOffset)-(x->endOffset>y->endOffset);
 int order=comparePoint(a->value.selection.start,b->value.selection.start);
 return order ? order : (a->ordinal>b->ordinal)-(a->ordinal<b->ordinal);
}
static char* symbolCopyText(xide_semantic_symbols* s,xstrview text,size_t limit)
{
 if(text.Size>limit || text.Size+1>SYMBOL_TEXT_BYTES-s->view.textBytes)return NULL;
 char* copy=xrtMalloc(text.Size+1);if(!copy)return NULL;
 memcpy(copy,text.Data,text.Size);copy[text.Size]=0;s->view.textBytes+=text.Size+1;return copy;
}
static int symbolReserve(xarray* rows,size_t fixed,size_t* peak)
{
 size_t wanted=rows->Count+1,capacity=rows->Capacity;
 if(wanted>UINT_MAX || wanted>(SIZE_MAX-8)/2)return 0;
 if(wanted>capacity)capacity=wanted*2+8; /* Conservative pinned XRT growth bound. */
 if(fixed>SYMBOL_PROJECTION_BYTES || capacity>(SYMBOL_PROJECTION_BYTES-fixed)/sizeof(symbol_build_row))return 0;
 size_t bytes=fixed+capacity*sizeof(symbol_build_row);
 if(wanted>(SYMBOL_PROJECTION_BYTES-bytes)/(sizeof(node)+sizeof(unsigned)+4*sizeof(endpoint)))return 0;
 bytes+=wanted*(sizeof(node)+sizeof(unsigned)+4*sizeof(endpoint));if(bytes>*peak)*peak=bytes;
 if(wanted>rows->Capacity && !xrtArrayReserve(rows,wanted))return 0;
 return rows->Capacity<=capacity;
}
#ifdef XLS_COMPILER_XRT
static int sameCompilerRange(xls_index_range a,xls_index_range b)
{
 return a.known==b.known && (!a.known || (a.startOffset==b.startOffset && a.endOffset==b.endOffset &&
  a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn));
}
static int symbolNativeName(const xls_semantic_index* index,const xls_declaration_fact* f,xls_index_fact* token)
{
 if(!f->nameNode || !xlsSemanticIndexFact(index,(size_t)f->nameNode-1,token) ||
    token->source!=f->nameSource || !sameCompilerRange(token->tokenRange,f->nameRange))return 0;
 if(f->shape.name==XLS_DECL_NAME_TOKEN)return f->nameNode==f->fact.id;
 return f->shape.name==XLS_DECL_NAME_CHILD && f->nameNode==f->fact.id+1 &&
  token->parent==f->fact.id && token->kind && !strcmp(token->kind,"name");
}
#endif
static xide_semantic_symbols* symbolsFromVerified(
#ifdef XLS_COMPILER_XRT
 const xls_semantic_index* native,
#endif
 const xvalue* legacy,const xide_semantic_definitions* verified,const xide_semantic_source* sources,size_t count,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error)
{
 if(error)*error=(xide_model_error){0};
 if(work)*work=(xide_symbol_work){0};
 if(!verified || !sources || !count || count>512 || verified->sourceCount!=count || symbolStopped(stopped,user))return NULL;
 size_t total=verified->count,fixed=sizeof(xide_semantic_symbols)+count*sizeof(char*)+SYMBOL_TEXT_BYTES+sizeof(endpoint),peak=0;
 const xvalue* legacyRows=get(legacy,"nodes");
#ifdef XLS_COMPILER_XRT
 int batch=0;
 if(native && xlsSemanticIndexCount(native)!=total)return NULL;
 batch=native && verified->native;
#endif
 xide_semantic_symbols* s=xrtCalloc(1,sizeof(*s));xarray rows={0};unsigned* mapping=NULL;endpoint* ends=NULL;size_t endpointCount=0,coordinateBudget=0;int initialized=0,invalid=0,okay=0,dense=0;
 if(!s){if(error){error->code=XIDE_MODEL_IO;snprintf(error->message,sizeof(error->message),"Declaration projection storage is unavailable.");}return NULL;}
 xrtAtomic32Init(&s->references,1);
 s->view.sourceCount=count;s->view.paths=xrtCalloc(count,sizeof(char*));if(!s->view.paths)goto done;
 for(size_t i=0;i<count;++i){
  if(symbolStopped(stopped,user) || !sources[i].path || !sources[i].text)goto done;
  size_t n=strlen(sources[i].path)+1;if(n>SYMBOL_PROJECTION_BYTES-fixed)goto done;fixed+=n;
  s->view.paths[i]=xrtStrDup(sources[i].path);if(!s->view.paths[i])goto done;
 }
 peak=fixed;if(!xrtArrayInit(&rows,sizeof(symbol_build_row)))goto done;initialized=1;
 symbol_scope stack[DEPTH_LIMIT+1]={0};size_t depth=0;
 for(size_t i=0;i<total;++i){
  if(!(i%64) && symbolStopped(stopped,user))goto done;
  xls_declaration_shape shape={0};unsigned parent=0,rawDepth=0,nameNode=0,fullNode=(unsigned)i+1;span full={0},selection={0};int existingName=0;
#ifdef XLS_COMPILER_XRT
  xls_declaration_fact f={0};
  if(native){
   if(!xlsSemanticIndexDeclarationFact(native,i,&f) || f.fact.id!=i+1 || f.fact.parent>i ||
      f.shape.kind<XLS_DECL_NONE || f.shape.kind>XLS_DECL_TYPE_PARAMETER || f.shape.name>XLS_DECL_NAME_CHILD || f.shape.callable>1){invalid=1;goto done;}
   shape=f.shape;parent=f.fact.parent;nameNode=f.nameRange.known ? f.nameNode : 0;fullNode=f.declarationNode;
   rawDepth=verified->native ? verified->native->shapes[i]&511u : verified->nodes[i].depth;
  }else
#endif
  {
   const node* raw=verified->nodes+i;const xvalue* value=xrtValueArrayGet(legacyRows,i);
   shape=xlsDeclarationShapeFromText(string(get(value,"kind")));parent=raw->parent;rawDepth=raw->depth;
   if(shape.kind==XLS_DECL_TYPE_PARAMETER && (!raw->nameSyntax || !parent || !verified->nodes[parent-1].typeParams)){
    unsigned owner=parent ? verified->nodes[parent-1].parent : 0;
    const char* parentKind=parent ? string(get(xrtValueArrayGet(legacyRows,parent-1),"kind")) : NULL;
    const char* ownerKind=owner ? string(get(xrtValueArrayGet(legacyRows,owner-1),"kind")) : NULL;
    if(raw->nameSyntax && owner && parent==owner+1 && parentKind && !strcmp(parentKind,"list") && ownerKind && !strcmp(ownerKind,"multi_var_decl")){
     shape=(xls_declaration_shape){XLS_DECL_VARIABLE,XLS_DECL_NAME_TOKEN,0};fullNode=owner;
    }else shape=(xls_declaration_shape){0};
   }
   const xvalue* text=get(value,"text");
   if((shape.kind==XLS_DECL_RECORD || shape.kind==XLS_DECL_FIELD) && text && xrtValueType(text)==XVALUE_NULL)shape.name=XLS_DECL_NAME_NONE;
   if(shape.name!=XLS_DECL_NAME_NONE && raw->selection.source && raw->selection.endOffset>raw->selection.startOffset){nameNode=(unsigned)i+1;existingName=1;}
   else if(shape.name==XLS_DECL_NAME_TOKEN)nameNode=(unsigned)i+1;
   else if(shape.name==XLS_DECL_NAME_CHILD && i+1<total && verified->nodes[i+1].parent==i+1 && verified->nodes[i+1].nameSyntax)nameNode=(unsigned)i+2;
  }
  if(parent){while(depth && stack[depth-1].id!=parent)--depth;if(!depth){invalid=1;goto done;}}
  else depth=0;
  if(rawDepth!=depth+1 || depth>=DEPTH_LIMIT){invalid=1;goto done;}
  symbol_scope frame={.id=(unsigned)i+1,.depth=rawDepth,.nearest=depth ? stack[depth-1].nearest : 0,
   .hidden=depth && (stack[depth-1].hidden || stack[depth-1].callable),.callable=(int)shape.callable};
  if(!frame.hidden && shape.kind!=XLS_DECL_NONE && nameNode){
#ifdef XLS_COMPILER_XRT
   if(native){
    xls_index_fact token={0},declaration={0};
    if(!symbolNativeName(native,&f,&token) || !fullNode || fullNode>i+1 ||
       !xlsSemanticIndexFact(native,(size_t)fullNode-1,&declaration) || declaration.source!=f.declarationSource ||
       !sameCompilerRange(declaration.range,f.declarationRange)){invalid=1;goto done;}
    if(batch){
     if(f.declarationRange.known)full=(span){.source=(int)f.declarationSource,.startOffset=(int)f.declarationRange.startOffset,.endOffset=(int)f.declarationRange.endOffset};
     if(f.nameRange.known)selection=(span){.source=(int)f.nameSource,.startOffset=(int)f.nameRange.startOffset,.endOffset=(int)f.nameRange.endOffset};
    }else{full=verified->nodes[fullNode-1].full;selection=verified->nodes[nameNode-1].token;}
   }else
#endif
   {full=verified->nodes[fullNode-1].full;selection=existingName ? verified->nodes[i].selection : verified->nodes[nameNode-1].token;}
   if(full.source && full.source==selection.source && selection.endOffset>selection.startOffset){
    symbol_build_row row={.value={.parent=(unsigned)frame.nearest,.kind=symbolWireKind(shape.kind),.full=full,.selection=selection},.ordinal=rows.Count,.role=shape.kind};
#ifdef XLS_COMPILER_XRT
    if(batch){row.fullCompiler=f.declarationRange;row.nameCompiler=f.nameRange;}
#endif
    if(!symbolReserve(&rows,fixed,&peak))goto done;
    if(!xrtArrayPush(&rows,&row))goto done;
    frame.nearest=rows.Count;
   }
  }
  stack[depth++]=frame;
 }
 if(symbolStopped(stopped,user))goto done;
 symbol_build_row* data=(symbol_build_row*)rows.Data;
#ifdef XLS_COMPILER_XRT
 if(batch && rows.Count){
  /* Batch only declaration endpoints. Repeated per-offset line scans would
     otherwise become quadratic for a long/minified source line. Every raw
     fact/range was already checked by the retained immutable point view. */
  size_t reserved=fixed+rows.Capacity*sizeof(symbol_build_row)+rows.Count*(sizeof(node)+sizeof(unsigned)+4*sizeof(endpoint));
  if(reserved>SYMBOL_PROJECTION_BYTES)goto done;
  coordinateBudget=SYMBOL_PROJECTION_BYTES-reserved;if(coordinateBudget>8u*1024u*1024u)coordinateBudget=8u*1024u*1024u;
  if(reserved+coordinateBudget>peak)peak=reserved+coordinateBudget;
  ends=xrtCalloc(rows.Count*4+1,sizeof(*ends));if(!ends)goto done;
  for(size_t i=0;i<rows.Count;++i){
   if(!(i%64) && symbolStopped(stopped,user))goto done;
   node* n=&data[i].value;
   if(!typedRange(data[i].fullCompiler,n->full.source,sources,count,&n->full,ends,&endpointCount) ||
      !typedRange(data[i].nameCompiler,n->selection.source,sources,count,&n->selection,ends,&endpointCount)){invalid=1;goto done;}
  }
  int fast=coordinateBudget ? convertDense(ends,endpointCount,sources,count,coordinateBudget) : -1;dense=fast==1;
  if(!(fast<0 ? convert(ends,endpointCount,sources) : fast)){invalid=1;goto done;}
 }
#endif
 for(size_t i=0;i<rows.Count;++i){
  if(!(i%64) && symbolStopped(stopped,user))goto done;
  xstrview name,header;node* n=&data[i].value;
  if(!nodeTextSlices(n,sources,&name,&header) || !name.Data){invalid=1;goto done;}
  n->name=symbolCopyText(s,name,512);if(!n->name)goto done;
  if(header.Data){n->header=symbolCopyText(s,header,2048);if(!n->header)goto done;}
 }
 if(symbolStopped(stopped,user))goto done;
 if(!xrtArraySort(&rows,compareSymbolRows))goto done;
 s->view.count=rows.Count;
 if(rows.Count){s->view.nodes=xrtCalloc(rows.Count,sizeof(node));mapping=xrtCalloc(rows.Count,sizeof(unsigned));if(!s->view.nodes || !mapping)goto done;}
 data=(symbol_build_row*)rows.Data;
 for(size_t i=0;i<rows.Count;++i)mapping[data[i].ordinal]=(unsigned)i+1;
 for(size_t i=0;i<rows.Count;++i){
  if(!(i%64) && symbolStopped(stopped,user))goto done;
  node* n=&data[i].value;
  if(n->parent>rows.Count){invalid=1;goto done;}
  n->parent=n->parent ? mapping[n->parent-1] : 0;
  if(data[i].role==XLS_DECL_SPECIAL)n->kind=!strcmp(n->name,"create") ? 9 : 6;
  s->view.nodes[i]=*n;n->name=n->header=NULL;
 }
 for(size_t i=0;i<rows.Count;++i){
  node* n=s->view.nodes+i;unsigned parent=n->parent;
  for(unsigned steps=0;parent;){
   if(++steps>DEPTH_LIMIT || parent>rows.Count || parent==i+1){invalid=1;goto done;}
   node* p=s->view.nodes+parent-1;
   if(p->full.source==n->full.source){
    if(parent>=i+1 || n->full.startOffset<p->full.startOffset || n->full.endOffset>p->full.endOffset){invalid=1;goto done;}
    if(n->kind==12 && (p->kind==5 || p->kind==11))n->kind=6;
    break;
   }
   parent=p->parent;
  }
 }
 if(symbolStopped(stopped,user))goto done;
 s->bytes=sizeof(*s)+count*sizeof(char*)+rows.Count*sizeof(node)+s->view.textBytes;
 for(size_t i=0;i<count;++i)s->bytes+=strlen(s->view.paths[i])+1;
 if(work)*work=(xide_symbol_work){.facts=total,.declarations=rows.Count,.ownedBytes=s->bytes,.constructionAllowanceBytes=peak,.borrowedValidationBytes=navigationBytes(verified),.coordinateEndpoints=endpointCount,.coordinateScratchAllowanceBytes=coordinateBudget,.denseCoordinates=dense};
 okay=1;
done:
 if(initialized){symbol_build_row* data=(symbol_build_row*)rows.Data;for(size_t i=0;i<rows.Count;++i){xrtFree(data[i].value.name);xrtFree(data[i].value.header);}xrtArrayUnit(&rows);}
 xrtFree(mapping);xrtFree(ends);
 if(!okay){xideSemanticSymbolsDestroy(s);s=NULL;if(error){error->code=invalid ? XIDE_MODEL_INVALID : XIDE_MODEL_IO;snprintf(error->message,sizeof(error->message),"%s",symbolStopped(stopped,user) ? "Declaration projection cancelled." : invalid ? "Declaration projection facts or ranges are invalid." : "Declaration projection storage is unavailable.");}}
 return s;
}
xide_semantic_symbols* xideSemanticSymbolsCreate(const xvalue* index,const xide_semantic_source* sources,size_t count,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error)
{
 if(work)*work=(xide_symbol_work){0};
 if(error)*error=(xide_model_error){0};
 if(symbolStopped(stopped,user))return NULL;
 xide_semantic_definitions* verified=xideSemanticDefinitionsCreate(index,sources,count,error);if(!verified)return NULL;
 xide_semantic_symbols* out=symbolsFromVerified(
#ifdef XLS_COMPILER_XRT
  NULL,
#endif
  index,verified,sources,count,stopped,user,work,error);
 xideSemanticDefinitionsDestroy(verified);if(out && work)work->fullValidationFallback=1;return out;
}
#ifdef XLS_COMPILER_XRT
xide_semantic_symbols* xideSemanticSymbolsCreateTyped(const xls_semantic_index* index,const xide_semantic_source* sources,size_t count,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error)
{
 if(work)*work=(xide_symbol_work){0};
 if(error)*error=(xide_model_error){0};
 if(symbolStopped(stopped,user))return NULL;
 xide_semantic_definitions* verified=xideSemanticDefinitionsCreateTyped(index,sources,count,error);if(!verified)return NULL;
 xide_semantic_symbols* out=symbolsFromVerified(index,NULL,verified,sources,count,stopped,user,work,error);
 xideSemanticDefinitionsDestroy(verified);if(out && work)work->fullValidationFallback=1;return out;
}
xide_semantic_symbols* xideSemanticSymbolsFromNavigation(const xide_navigation_snapshot* snapshot,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error)
{
 if(work)*work=(xide_symbol_work){0};
 if(error)*error=(xide_model_error){0};
 if(!snapshot || symbolStopped(stopped,user))return NULL;
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(snapshot->owner);
 if(!index){if(error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Declaration owner is invalid.");}return NULL;}
 if(!xideSemanticNavigationPointsReady(snapshot))return xideSemanticSymbolsCreateTyped(index,snapshot->sources,snapshot->count,stopped,user,work,error);
 const xide_semantic_definitions* verified=snapshot->points;
 if(!verified || !verified->native || verified->native->index!=index)return NULL;
 xide_semantic_symbols* out=symbolsFromVerified(index,NULL,verified,snapshot->sources,snapshot->count,stopped,user,work,error);
 if(out && work)work->reusedValidation=1;
 return out;
}
#endif
