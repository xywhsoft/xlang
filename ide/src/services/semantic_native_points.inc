/* Immutable point-query lookup over the original checked index. Only cursor
 * and declaration lookups are retained: no node,canonical,name,header or
 * reference copies. Validation still visits every original fact/range and
 * charges the original text admission before publishing this optional view. */
struct native_points {
 const xls_semantic_index* index;
 xide_semantic_source* sources;xls_text** texts;uint16_t* shapes;
 size_t sourcesCount,nodesCount;
};
typedef struct native_spans {span full,token,selection;} native_spans;
static void nativePointsDestroy(native_points* p)
{
 if(!p)return;
 for(size_t i=0;i<p->sourcesCount;++i)xlsTextRelease(p->texts ? p->texts[i] : NULL);
 xrtFree(p->texts);xrtFree(p->sources);xrtFree(p->shapes);xrtFree(p);
}
static size_t nativePointsBytes(const xide_semantic_definitions* d)
{
 const native_points* p=d->native;
 size_t bytes=sizeof(*d)+sizeof(*p)+p->sourcesCount*(sizeof(char*)+sizeof(xide_semantic_source)+sizeof(xls_text*))+
  (p->nodesCount+1)*sizeof(uint16_t)+(d->count+1)*2*sizeof(declaration)+d->cursors.Capacity*sizeof(cursor_row);
 for(size_t i=0;i<p->sourcesCount;++i)bytes+=strlen(d->paths[i])+1+xlsTextStorageBytes(p->texts[i]);
 return bytes;
}
static int nativeShape(xide_semantic_definitions* d,size_t at,xls_index_fact* f,node* n)
{
 native_points* p=d->native;node parent={0};int metadata=1;xls_index_point_fact point;
 if(!xlsSemanticIndexPointFact(p->index,at,&point) || point.fact.parent>at)return 0;
 *f=point.fact;
 if(f->parent){
  unsigned bits=p->shapes[f->parent-1];parent.depth=bits&511u;
  parent.typeParams=(bits&512u)!=0;parent.typeConstraint=(bits&1024u)!=0;parent.kind=bits&2048u ? 5 : 0;
 }
 return typedNodeShapeKind(n,f,point.kind,at,f->parent ? &parent : NULL,p->sourcesCount,&metadata);
}
static int nativePointRange(xide_semantic_definitions* d,xls_index_range r,unsigned source,span* out)
{
 if(!r.known)return 1;
 native_points* p=d->native;xls_position a,b;
 if(!source || source>p->sourcesCount || !xlsTextPosition(p->texts[source-1],r.startOffset,&a) || !xlsTextPosition(p->texts[source-1],r.endOffset,&b) ||
    a.line>INT_MAX || b.line>INT_MAX || a.character>INT_MAX-1 || b.character>INT_MAX-1)return 0;
 int prefix=p->sources[source-1].utf16Prefix;
 *out=(span){(int)source,(int)r.startOffset,(int)r.endOffset,{(int)a.line,(int)a.character+(!a.line ? prefix : 0)},{(int)b.line,(int)b.character+(!b.line ? prefix : 0)}};return 1;
}
static node* nativePointNode(xide_semantic_definitions* d,size_t at,node* out)
{
 if(at>=d->count)return NULL;
 *out=(node){0};xls_index_fact f;
 if(!nativeShape(d,at,&f,out) || !nativePointRange(d,f.range,f.source,&out->full) ||
    !nativePointRange(d,f.tokenRange,f.source,&out->token) || !nativePointRange(d,f.selectionRange,f.selectionSource,&out->selection))return NULL;
 out->type=(char*)f.canonical;return out;
}
static int nativePointText(xide_semantic_definitions* d,node* n,char* name,char* header)
{
 xstrview a,b;if(!nodeTextSlices(n,d->native->sources,&a,&b))return 0;
 if(a.Data){memcpy(name,a.Data,a.Size);name[a.Size]=0;n->name=name;}
 if(b.Data){memcpy(header,b.Data,b.Size);header[b.Size]=0;n->header=header;}
 return 1;
}
static int nativeTextCharge(size_t* bytes,size_t length,size_t limit)
{
 if(length>limit || length+1>4u*1024u*1024u-*bytes)return 0;
 *bytes+=length+1;return 1;
}
static xide_semantic_definitions* nativePointsCreate(const xls_semantic_index* index,const xide_semantic_source* sources,size_t count,xide_model_error* error)
{
 size_t nodes=xlsSemanticIndexCount(index),budget=0,used=0,textCharge=0;
 if(!index || !typedCapacity(nodes,sources,count,&budget))return NULL;
 xide_semantic_definitions* d=calloc(1,sizeof(*d));native_spans* spans=NULL;endpoint* ends=NULL;unsigned char* selections=NULL;int invalid=0;
 if(!d)return NULL;
 d->native=xrtCalloc(1,sizeof(*d->native));if(!d->native)goto done;
 native_points* p=d->native;p->index=index;p->sourcesCount=count;p->nodesCount=nodes;d->count=nodes;d->sourceCount=count;
 p->sources=xrtCalloc(count,sizeof(*p->sources));p->texts=xrtCalloc(count,sizeof(*p->texts));p->shapes=xrtCalloc(nodes+1,sizeof(*p->shapes));
 d->paths=calloc(count,sizeof(*d->paths));d->declarations=calloc(nodes+1,sizeof(*d->declarations));d->typeDeclarations=calloc(nodes+1,sizeof(*d->typeDeclarations));
 if(!p->sources || !p->texts || !p->shapes || !d->paths || !d->declarations || !d->typeDeclarations)goto done;
 for(size_t i=0;i<count;++i){
  d->paths[i]=copy(sources[i].path);if(!d->paths[i])goto done;
  p->sources[i]=(xide_semantic_source){d->paths[i],sources[i].text,sources[i].kind,sources[i].utf16Prefix};
 }
 for(size_t i=0;i<count;++i){
  /* Conservative optional admission before allocating source copies/lines.
   * The pinned XRT geometric buffers fit this allowance even for newline-only
   * text. Large views decline to the existing full navigation path. Actual
   * completed capacities are charged below,not this admission estimate. */
  size_t size=strlen(sources[i].text),current=nativePointsBytes(d);
  if(current>TYPED_DEFINITION_BYTES || size>(TYPED_DEFINITION_BYTES-current)/32 ||
     size*32>TYPED_DEFINITION_BYTES-current || 1024>TYPED_DEFINITION_BYTES-current-size*32)goto done;
  enum xls_text_error why=XLS_TEXT_OK;p->texts[i]=xlsTextCreate(sources[i].text,strlen(sources[i].text),8u*1024u*1024u,&why);
  if(!p->texts[i]){invalid=why==XLS_TEXT_ENCODING;goto done;}
  if(nativePointsBytes(d)>TYPED_DEFINITION_BYTES)goto done;
 }
 size_t temporary=(nodes+1)*sizeof(*spans)+(nodes*6+1)*sizeof(*ends)+nodes+1;
 size_t resident=nativePointsBytes(d),cursorBudget=(nodes*4+8)*sizeof(cursor_row);
 if(resident>TYPED_DEFINITION_BYTES || temporary>TYPED_DEFINITION_BYTES-resident || cursorBudget>TYPED_DEFINITION_BYTES-resident-temporary)goto done;
 if(budget>TYPED_DEFINITION_BYTES-resident-temporary-cursorBudget)budget=TYPED_DEFINITION_BYTES-resident-temporary-cursorBudget;
 spans=calloc(nodes+1,sizeof(*spans));ends=calloc(nodes*6+1,sizeof(*ends));selections=calloc(nodes+1,1);if(!spans || !ends || !selections)goto done;
 for(size_t i=0;i<nodes;++i){
  node n={0};xls_index_fact f;
  if(!nativeShape(d,i,&f,&n) || !typedRange(f.range,(int)f.source,sources,count,&spans[i].full,ends,&used) ||
     !typedRange(f.tokenRange,(int)f.source,sources,count,&spans[i].token,ends,&used) ||
     !typedRange(f.selectionRange,(int)f.selectionSource,sources,count,&spans[i].selection,ends,&used)){invalid=1;goto done;}
  p->shapes[i]=(uint16_t)(n.depth|(n.typeParams ? 512u : 0)|(n.typeConstraint ? 1024u : 0)|(n.kind==5 ? 2048u : 0));
  selections[i]=(unsigned char)(n.declared || n.declaredType);
  if(f.canonical && !nativeTextCharge(&textCharge,strlen(f.canonical),8192)){invalid=1;goto done;}
  if(spans[i].token.source && spans[i].full.source && (spans[i].token.startOffset<spans[i].full.startOffset || spans[i].token.endOffset>spans[i].full.endOffset)){invalid=1;goto done;}
  if(n.declared)d->declarations[d->declCount++]=(declaration){n.declared,i};
  if(n.declaredType)d->typeDeclarations[d->typeCount++]=(declaration){n.declaredType,i};
 }
 int fast=budget ? convertDense(ends,used,sources,count,budget) : -1;
 if(!(fast<0 ? convert(ends,used,sources) : fast)){invalid=1;goto done;}
 if(!xrtArrayInit(&d->cursors,sizeof(cursor_row)))goto done;
 d->lookupReady=1;if(!xrtArrayReserve(&d->cursors,nodes*2) || d->cursors.Capacity>nodes*4+8)goto done;
 for(size_t i=0;i<nodes;++i){
  node n={0};xstrview name,header;n.full=spans[i].full;n.token=spans[i].token;n.selection=spans[i].selection;
  if(!nodeTextSlices(&n,sources,&name,&header) || (name.Data && !nativeTextCharge(&textCharge,name.Size,512)) || (header.Data && !nativeTextCharge(&textCharge,header.Size,2048))){invalid=1;goto done;}
  int selection=selections[i];
  if(!appendCursor(d,&n.token,(uint32_t)i|(selection && sameSelection(&n.selection,&n.token) ? CURSOR_SELECTION : 0)) ||
     (selection && !sameSelection(&n.selection,&n.token) && !appendCursor(d,&n.selection,(uint32_t)i|CURSOR_SELECTION)))goto done;
 }
 qsort(d->declarations,d->declCount,sizeof(*d->declarations),compareDeclaration);
 for(size_t i=1;i<d->declCount;++i)if(d->declarations[i-1].symbol==d->declarations[i].symbol){invalid=1;goto done;}
 qsort(d->typeDeclarations,d->typeCount,sizeof(*d->typeDeclarations),compareDeclaration);
 size_t unique=0;
 for(size_t i=0;i<d->typeCount;){
  size_t end=i+1;while(end<d->typeCount && d->typeDeclarations[end].symbol==d->typeDeclarations[i].symbol)++end;
  declaration item=d->typeDeclarations[i];node a,b;
  if(!nativePointNode(d,item.node,&a)){invalid=1;goto done;}
  for(size_t j=i+1;j<end;++j){if(!nativePointNode(d,d->typeDeclarations[j].node,&b)){invalid=1;goto done;}if(!sameTypeLocation(&a,&b)){item.node=SIZE_MAX;break;}}
  d->typeDeclarations[unique++]=item;i=end;
 }
 d->typeCount=unique;if(!xrtArraySort(&d->cursors,compareCursor))goto done;
 cursor_row* rows=(cursor_row*)d->cursors.Data;
 for(size_t i=0;i<d->cursors.Count;){size_t end=i+1;while(end<d->cursors.Count && rows[end].source==rows[i].source)++end;buildCursorTree(rows,i,end);i=end;}
 if(nativePointsBytes(d)>TYPED_DEFINITION_BYTES)goto done;
 free(spans);free(ends);free(selections);return d;
done:
 free(spans);free(ends);free(selections);xideSemanticDefinitionsDestroy(d);
 if(invalid && error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Native point navigation facts failed the original projection validation.");}
 return NULL;
}
