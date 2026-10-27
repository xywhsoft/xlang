/* Same-compilation IDs and source ranges, validated against exact inputs.
   Native projection uses bounded coordinate buckets with the complete ordered
   converter as fallback;all scalar columns and UTF16 positions stay verified. */
#include "services/semantic_definition.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#define NODE_LIMIT 65536u
#define DEPTH_LIMIT 256u
#define TYPED_DEFINITION_BYTES (64u*1024u*1024u)
typedef struct point { int line,character; } point;
typedef struct span { int source,startOffset,endOffset; point start,end; } span;
typedef struct node { unsigned parent,depth,declared,target,typeId,declaredType,referencedType; uint64_t typeAbi; int call,kind,origin,typeSyntax,typeParams,typeConstraint,nameSyntax,genericType,memberSyntax; char *name,*header,*type; span full,token,selection; } node;
typedef struct declaration { unsigned symbol; size_t node; } declaration;
typedef struct endpoint { int source,offset,line,column; point* output; } endpoint;
#define CURSOR_SELECTION UINT32_C(0x80000000)
typedef struct cursor_row { uint32_t source,sl,sc,el,ec,maximumLine,maximumCharacter,node; } cursor_row;
typedef struct reference_row { uint32_t targetSource,targetStart,targetEnd,source,start,end,node; } reference_row;
struct xide_semantic_definitions { node* nodes; size_t count,declCount,typeCount,sourceCount; declaration *declarations,*typeDeclarations; char** paths; size_t textBytes; xarray cursors,references;int lookupReady; };
#ifdef XLS_COMPILER_XRT
static int typedCapacity(size_t nodes,const xide_semantic_source* sources,size_t count,size_t* coordinateBudget)
{
 /* Bound owned data including temporary coordinate endpoints, worst-case
    lookup capacities and the existing4MiB retained text allowance. Borrowed
    source text and allocator overhead are separate,not process RSS. Cursor
    indices reserve their high bit for selections. Check before allocating. */
 size_t fixed=sizeof(xide_semantic_definitions)+count*sizeof(char*)+4u*1024u*1024u+8*(sizeof(cursor_row)+sizeof(reference_row));
 const size_t per=sizeof(node)+2*sizeof(declaration)+6*sizeof(endpoint)+4*sizeof(cursor_row)+2*sizeof(reference_row);
 if(nodes>=CURSOR_SELECTION || nodes+1>(TYPED_DEFINITION_BYTES-fixed)/per) return 0;
 size_t used=fixed+(nodes+1)*per;
 for(size_t i=0;i<count;++i) {
  if(!sources[i].path) return 0;
  size_t size=strlen(sources[i].path)+1;
  if(size>TYPED_DEFINITION_BYTES-used) return 0;
  used+=size;
 }
 *coordinateBudget=TYPED_DEFINITION_BYTES-used;
 if(*coordinateBudget>8u*1024u*1024u)*coordinateBudget=8u*1024u*1024u;
 return 1;
}
#endif
static int buildLookup(xide_semantic_definitions* d);
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* k) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,sv(k)) : NULL; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=65535 && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static int put(xvalue* o,const char* k,xvalue* v) { return v && xrtValueObjectSetNew(o,sv(k),v); }
static int number(const xvalue* v,int64* n,int64 min,int64 max) { return xrtValueGetInt(v,n) && *n>=min && *n<=max; }
static int equal(const xvalue* v,const char* s) { const char* a=string(v); return a && !strcmp(a,s); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
/* Keep only facts copied from verified source or the compiler's type object. */
static char* retain(xide_semantic_definitions* d,const char* text,size_t size,size_t limit)
{ if(size>limit || size+1>4u*1024u*1024u-d->textBytes) return NULL; char* out=malloc(size+1); if(out) { memcpy(out,text,size); out[size]=0; d->textBytes+=size+1; } return out; }
static int abi(const char* text)
{ if(!text || strlen(text)!=16) return 0; for(int i=0;i<16;++i) if(!((text[i]>='0' && text[i]<='9') || (text[i]>='a' && text[i]<='f'))) return 0; return 1; }
static int symbolKind(const char* kind)
{ if(!strcmp(kind,"func") || !strcmp(kind,"export_func")) return 12; if(!strcmp(kind,"class")) return 5; if(!strcmp(kind,"protocol")) return 11; if(!strcmp(kind,"enum")) return 10; if(!strcmp(kind,"type_alias") || !strcmp(kind,"extern_type_alias")) return 26; if(!strcmp(kind,"union")) return 23; if(!strcmp(kind,"variant")) return 22; if(!strcmp(kind,"var_decl")) return 13; if(!strcmp(kind,"const_decl")) return 14; return 0; }
static int typeKind(const node* n) { return n->kind==5 || n->kind==10 || n->kind==26 || n->kind==23 || n->kind==11; }
static int applyTypeFacts(xide_semantic_definitions* d,node* n,size_t index,int present,unsigned declared,unsigned referenced,int* metadata)
{
 if(*metadata<0) *metadata=present;
 if(*metadata!=present) return 0;
 if(!present) { n->declaredType=n->declared && typeKind(n) ? n->typeId : 0; n->referencedType=n->typeSyntax ? n->typeId : 0; return 1; }
 if((declared && referenced) || (declared && declared!=n->typeId) || (referenced && referenced!=n->typeId)) return 0;
 const node* parent=n->parent ? &d->nodes[n->parent-1] : NULL;
 n->genericType=n->nameSyntax && parent && parent->typeParams;
 if(declared && !typeKind(n) && !n->genericType) return 0;
 if(n->genericType && n->typeAbi) return 0; /* No runtime ABI for a type parameter. */
 if(referenced && !n->typeSyntax && !(n->nameSyntax && parent && (parent->typeConstraint || (parent->kind==5 && index==(size_t)n->parent)))) return 0;
 n->declaredType=(unsigned)declared; n->referencedType=(unsigned)referenced;
 if(declared && n->genericType) n->kind=26;
 return 1;
}
static int typeFacts(xide_semantic_definitions* d,node* n,const xvalue* row,size_t index,int* metadata)
{
 const xvalue *decl=get(row,"declaredTypeId"),*ref=get(row,"referencedTypeId"); int present=decl!=NULL || ref!=NULL; int64 declared=0,referenced=0;
 if(present && (!number(decl,&declared,0,UINT_MAX) || !number(ref,&referenced,0,UINT_MAX))) return 0;
 return applyTypeFacts(d,n,index,present,(unsigned)declared,(unsigned)referenced,metadata);
}
static int retainType(xide_semantic_definitions* d,node* n,const xvalue* row)
{
 int64 id=0,inside=0; const char *outer=string(get(row,"typeAbiId")); const xvalue* t=get(row,"type");
 if(!number(get(row,"typeId"),&id,0,UINT_MAX) || !abi(outer) || !t) return 0;
 n->typeId=(unsigned)id; n->typeAbi=strtoull(outer,NULL,16);
 if(xrtValueType(t)==XVALUE_NULL) return 1;
 const char *origin=string(get(t,"origin")),*hash=string(get(t,"abiId")),*name=string(get(t,"canonical"));
 if(!origin || !hash || !name || !*name || !number(get(t,"id"),&inside,0,UINT_MAX) || !abi(hash)) return 0;
 n->origin=!strcmp(origin,"declaration") ? 1 : !strcmp(origin,"inferred") ? 2 : !strcmp(origin,"syntax") ? 3 : 0;
 if(!n->origin) return 0;
 n->type=retain(d,name,strlen(name),8192); return n->type!=NULL;
}
static int compareEndpoint(const void* a,const void* b) { const endpoint *x=a,*y=b; return x->source!=y->source ? (x->source>y->source)-(x->source<y->source) : (x->offset>y->offset)-(x->offset<y->offset); }
static int compareDeclaration(const void* a,const void* b) { const declaration *x=a,*y=b; return (x->symbol>y->symbol)-(x->symbol<y->symbol); }
static int sameTypeLocation(const node* a,const node* b)
{ return a->typeAbi==b->typeAbi && a->genericType==b->genericType && a->selection.source && a->selection.source==b->selection.source && a->selection.endOffset>a->selection.startOffset && a->selection.startOffset==b->selection.startOffset && a->selection.endOffset==b->selection.endOffset && a->full.source==b->full.source && a->full.startOffset==b->full.startOffset && a->full.endOffset==b->full.endOffset; }
static int captureRange(int source,const xide_semantic_source* sources,size_t count,span* out,endpoint* ends,size_t* used,
 size_t a,size_t b,int sl,int sc,int el,int ec)
{
 if(source<1 || (size_t)source>count || !(sources[source-1].kind&1) || a>INT_MAX || b<a || b>INT_MAX || sl<1 || sc<1 || el<1 || ec<1) return 0;
 out->source=source; out->startOffset=(int)a; out->endOffset=(int)b;
 ends[(*used)++]=(endpoint){source,(int)a,sl,sc,&out->start};
 ends[(*used)++]=(endpoint){source,(int)b,el,ec,&out->end}; return 1;
}
static int range(const xvalue* value,int source,const xide_semantic_source* sources,size_t count,span* out,endpoint* ends,size_t* used)
{
 if(!value) return 0;
 if(xrtValueType(value)==XVALUE_NULL) return 1;
 if(source<1 || (size_t)source>count || !(sources[source-1].kind&1)) return 0;
 int64 a,b,sl,sc,el,ec;
 if(!number(get(value,"startOffset"),&a,0,INT_MAX) || !number(get(value,"endOffset"),&b,a,INT_MAX) || !number(get(value,"startLine"),&sl,1,INT_MAX) || !number(get(value,"startColumn"),&sc,1,INT_MAX) || !number(get(value,"endLine"),&el,1,INT_MAX) || !number(get(value,"endColumn"),&ec,1,INT_MAX)) return 0;
 return captureRange(source,sources,count,out,ends,used,(size_t)a,(size_t)b,(int)sl,(int)sc,(int)el,(int)ec);
}
static void nodeShape(xide_semantic_definitions* d,node* n,unsigned parent,unsigned declared,unsigned target,const char* kind)
{
 n->parent=parent; n->depth=parent ? d->nodes[parent-1].depth+1 : 1; n->declared=declared; n->target=target;
 n->call=!strcmp(kind,"call"); n->kind=symbolKind(kind); n->typeSyntax=!strcmp(kind,"type"); n->nameSyntax=!strcmp(kind,"name"); n->typeParams=!strcmp(kind,"type_params"); n->typeConstraint=!strcmp(kind,"generic_constraint");
 n->memberSyntax=!strcmp(kind,"member") || !strcmp(kind,"safe_member");
}
#ifdef XLS_COMPILER_XRT
static int typedRange(xls_index_range r,int source,const xide_semantic_source* sources,size_t count,span* out,endpoint* ends,size_t* used)
{ return !r.known || captureRange(source,sources,count,out,ends,used,r.startOffset,r.endOffset,r.startLine,r.startColumn,r.endLine,r.endColumn); }
static int typedNode(xide_semantic_definitions* d,node* n,const xls_semantic_index* index,size_t at,
 const xide_semantic_source* sources,size_t count,endpoint* ends,size_t* used,int* metadata)
{
 xls_index_fact f;
 if(!xlsSemanticIndexFact(index,at,&f) || f.id!=at+1 || f.parent>at || f.source>count || f.selectionSource>count ||
    (f.declaredSymbol && f.declaredSymbol!=f.symbol) || !f.kind || !*f.kind || strlen(f.kind)>64 || f.typeOrigin<0 || f.typeOrigin>3) return 0;
 nodeShape(d,n,f.parent,f.declaredSymbol,f.targetSymbol,f.kind);
 n->typeId=f.typeId; n->typeAbi=f.typeAbi; n->origin=f.typeOrigin;
 if(f.typeOrigin) {
  if(!f.canonical || !*f.canonical || !(n->type=retain(d,f.canonical,strlen(f.canonical),8192))) return 0;
 } else if(f.canonical) return 0;
 return n->depth<=DEPTH_LIMIT && applyTypeFacts(d,n,at,1,f.declaredType,f.referencedType,metadata) &&
  typedRange(f.range,(int)f.source,sources,count,&n->full,ends,used) &&
  typedRange(f.tokenRange,(int)f.source,sources,count,&n->token,ends,used) &&
  typedRange(f.selectionRange,(int)f.selectionSource,sources,count,&n->selection,ends,used);
}
#endif
#include "services/semantic_coordinate_dense.inc"
#include "services/semantic_coordinate_sorted.inc"
int xideSemanticRangesToLsp(const xide_semantic_source* sources,size_t count,xide_semantic_range_query* queries,size_t used)
{
 if(!sources || !count || count>512 || (used && !queries) || used>65536) return 0;
 span* spans=calloc(used+1,sizeof(*spans)); endpoint* ends=calloc(used*2+1,sizeof(*ends));
 int okay=spans && ends; size_t endpoints=0;
 for(size_t i=0;okay && i<count;++i) okay=sources[i].text && (sources[i].utf16Prefix==0 || sources[i].utf16Prefix==1);
 for(size_t i=0;okay && i<used;++i) okay=queries[i].range && xrtValueType(queries[i].range)==XVALUE_OBJECT && range(queries[i].range,queries[i].sourceId,sources,count,&spans[i],ends,&endpoints);
 if(okay) okay=convert(ends,endpoints,sources);
 for(size_t i=0;okay && i<used;++i) {
  queries[i].start=(xide_lsp_position){.line=spans[i].start.line,.character=spans[i].start.character};
  queries[i].end=(xide_lsp_position){.line=spans[i].end.line,.character=spans[i].end.character};
 }
 free(spans);free(ends);return okay;
}
void xideSemanticDefinitionsDestroy(xide_semantic_definitions* d)
{ if(d) { if(d->lookupReady&1)xrtArrayUnit(&d->cursors);if(d->lookupReady&2)xrtArrayUnit(&d->references);if(d->paths) for(size_t i=0;i<d->sourceCount;++i) free(d->paths[i]); free(d->paths); if(d->nodes) for(size_t i=0;i<d->count;++i) { free(d->nodes[i].name); free(d->nodes[i].header); free(d->nodes[i].type); } free(d->declarations); free(d->typeDeclarations); free(d->nodes); free(d); } }
static xide_semantic_definitions* definitionsCreate(const xvalue* index,
#ifdef XLS_COMPILER_XRT
 const xls_semantic_index* typed,
#endif
 const xide_semantic_source* sources,size_t count,xide_model_error* error)
{
 int64 schema=0; bool complete=false; const xvalue* rows=get(index,"nodes");
 if(!sources || !count || count>512) goto invalid;
 size_t nodeCount=0,coordinateBudget=0;
#ifdef XLS_COMPILER_XRT
 if(typed) nodeCount=xlsSemanticIndexCount(typed);
 else
#endif
 {
  if(!number(get(index,"schema"),&schema,1,1) || !equal(get(index,"scope"),"checked-compilation") || !xrtValueGetBool(get(index,"complete"),&complete) || !complete || !equal(get(index,"offsetEncoding"),"utf-8-after-bom") || !equal(get(index,"columnEncoding"),"unicode-scalar-1-based") || !equal(get(index,"rangeConvention"),"half-open") || !rows || xrtValueType(rows)!=XVALUE_ARRAY) goto invalid;
  nodeCount=xrtValueCount(rows);
 }
#ifdef XLS_COMPILER_XRT
 if(typed) { if(!typedCapacity(nodeCount,sources,count,&coordinateBudget)) goto invalid; }
 else
#endif
 if(nodeCount>NODE_LIMIT) goto invalid;
 xide_semantic_definitions* d=calloc(1,sizeof(*d)); endpoint* ends=NULL;
 if(!d) goto invalid;
 d->count=nodeCount; d->sourceCount=count; d->paths=calloc(count,sizeof(*d->paths)); d->nodes=calloc(d->count+1,sizeof(*d->nodes)); d->declarations=calloc(d->count+1,sizeof(*d->declarations)); d->typeDeclarations=calloc(d->count+1,sizeof(*d->typeDeclarations)); ends=calloc(d->count*6+1,sizeof(*ends));
 int okay=d->paths && d->nodes && d->declarations && d->typeDeclarations && ends; size_t used=0; int typeMetadata=-1;
 for(size_t i=0;okay && i<count;++i) okay=sources[i].path && sources[i].text && (sources[i].utf16Prefix==0 || sources[i].utf16Prefix==1) && (d->paths[i]=copy(sources[i].path))!=NULL;
 for(size_t i=0;okay && i<d->count;++i) {
  node* n=&d->nodes[i];
#ifdef XLS_COMPILER_XRT
  if(typed) okay=typedNode(d,n,typed,i,sources,count,ends,&used,&typeMetadata);
  else
#endif
  {
  const xvalue* v=xrtValueArrayGet(rows,i); int64 id,parent,source,selected,declared,symbol,target,operatorId; const char* kind=string(get(v,"kind"));
  okay=kind && *kind && strlen(kind)<=64 && number(get(v,"id"),&id,(int64)i+1,(int64)i+1) && number(get(v,"parent"),&parent,0,(int64)i) && number(get(v,"sourceId"),&source,0,(int64)count) && number(get(v,"selectionSourceId"),&selected,0,(int64)count) && number(get(v,"declaredSymbolId"),&declared,0,UINT_MAX) && number(get(v,"symbolId"),&symbol,0,UINT_MAX) && number(get(v,"targetSymbolId"),&target,0,UINT_MAX) && number(get(v,"compoundOperatorSymbolId"),&operatorId,0,UINT_MAX) && (!declared || declared==symbol);
  if(!okay) break;
  nodeShape(d,n,(unsigned)parent,(unsigned)declared,(unsigned)target,kind);
  okay=n->depth<=DEPTH_LIMIT && retainType(d,n,v) && typeFacts(d,n,v,i,&typeMetadata) && range(get(v,"range"),(int)source,sources,count,&n->full,ends,&used) && range(get(v,"tokenRange"),(int)source,sources,count,&n->token,ends,&used) && range(get(v,"selectionRange"),(int)selected,sources,count,&n->selection,ends,&used);
  }
  if(okay && n->token.source && n->full.source) okay=n->token.startOffset>=n->full.startOffset && n->token.endOffset<=n->full.endOffset;
  if(okay && n->declared) d->declarations[d->declCount++]=(declaration){n->declared,i};
  if(okay && n->declaredType) d->typeDeclarations[d->typeCount++]=(declaration){n->declaredType,i};
 }
 if(okay){int fast=coordinateBudget ? convertDense(ends,used,sources,count,coordinateBudget) : -1;okay=fast<0 ? convert(ends,used,sources) : fast;}
 for(size_t i=0;okay && i<d->count;++i) {
  node* n=&d->nodes[i];
  if(n->selection.source && n->selection.endOffset>n->selection.startOffset) {
   const char* text=sources[n->selection.source-1].text;
   n->name=retain(d,text+n->selection.startOffset,(size_t)(n->selection.endOffset-n->selection.startOffset),512); okay=n->name!=NULL;
   if(okay && n->full.source==n->selection.source) {
    if(n->selection.startOffset<n->full.startOffset || n->selection.endOffset>n->full.endOffset) { okay=0; break; }
    size_t start=(size_t)n->full.startOffset,end=start,limit=(size_t)n->full.endOffset;
    while(end<limit && text[end]!='\r' && text[end]!='\n' && end-start<2048) ++end;
    /* Do not split a UTF-8 scalar when clipping a long source line. */
    while(end>start && ((unsigned char)text[end]&0xc0)==0x80) --end;
    n->header=retain(d,text+start,end-start,2048); okay=n->header!=NULL;
   }
  }
 }
 if(okay) {
  qsort(d->declarations,d->declCount,sizeof(*d->declarations),compareDeclaration);
  for(size_t i=1;i<d->declCount;++i) if(d->declarations[i-1].symbol==d->declarations[i].symbol) { okay=0; break; }
  /* TypeId and SymbolId are separate compilation-local domains. Keep an
     ambiguous type identity non-navigable without losing ordinary symbols. */
  qsort(d->typeDeclarations,d->typeCount,sizeof(*d->typeDeclarations),compareDeclaration);
  size_t unique=0;
  for(size_t i=0;i<d->typeCount;) {
   size_t end=i+1; while(end<d->typeCount && d->typeDeclarations[end].symbol==d->typeDeclarations[i].symbol) ++end;
   declaration item=d->typeDeclarations[i];
   /* Monomorphized copies may retain the same original type declaration.
      Collapse only an exactly verified source target and matching identity. */
   for(size_t j=i+1;j<end;++j) if(!sameTypeLocation(&d->nodes[item.node],&d->nodes[d->typeDeclarations[j].node])) { item.node=SIZE_MAX; break; }
   d->typeDeclarations[unique++]=item; i=end;
  }
  d->typeCount=unique;
 }
 if(okay) okay=buildLookup(d);
 free(ends); if(okay) return d; xideSemanticDefinitionsDestroy(d);
invalid:
 if(error) { error->code=XIDE_MODEL_INVALID; snprintf(error->message,sizeof(error->message),"编译器语义索引的身份、来源或范围无法验证。"); } return NULL;
}
xide_semantic_definitions* xideSemanticDefinitionsCreate(const xvalue* index,const xide_semantic_source* sources,size_t count,xide_model_error* error)
{ return definitionsCreate(index,
#ifdef XLS_COMPILER_XRT
 NULL,
#endif
 sources,count,error); }
#ifdef XLS_COMPILER_XRT
xide_semantic_definitions* xideSemanticDefinitionsCreateTyped(const xls_semantic_index* index,const xide_semantic_source* sources,size_t count,xide_model_error* error)
{ return definitionsCreate(NULL,index,sources,count,error); }
static size_t navigationBytes(const xide_semantic_definitions* d)
{
 size_t bytes=sizeof(*d)+d->sourceCount*sizeof(char*)+(d->count+1)*(sizeof(node)+2*sizeof(declaration))+d->textBytes;
 for(size_t i=0;i<d->sourceCount;++i)bytes+=strlen(d->paths[i])+1;
 return bytes+d->cursors.Capacity*sizeof(cursor_row)+d->references.Capacity*sizeof(reference_row);
}
static void destroyNavigation(void* value){xideSemanticDefinitionsDestroy(value);}
static int navigationAppend(xstrbuf* key,const void* data,size_t bytes)
{return key->Size<=XLS_PROJECTION_KEY_BYTES && bytes<=XLS_PROJECTION_KEY_BYTES-key->Size && xrtStrBufAppend(key,(xstrview){data,bytes});}
/* -1: source/check mismatch;0: optional key admission declined;1: full key.
 * Continue source validation after key allocation failure;never turn a failed
 * cache attempt into permission to accept different source bytes/positions. */
static int navigationKey(xls_semantic_check* check,const xide_semantic_source* sources,size_t count,xstrbuf* key,int wanted)
{
 xls_inputs inputs;if(!check || !sources || !count || count>512 || !xlsSemanticCheckInputs(check,&inputs) || inputs.sources!=count)return -1;
 const uint64_t version[]={UINT64_C(0x786c734e617601),count};int okay=wanted && navigationAppend(key,version,sizeof(version));
 for(size_t i=0;i<count;++i){
  xls_input_source row;if(!xlsSemanticCheckSource(check,i,&row) || !row.path || !row.text || !sources[i].path || !sources[i].text || sources[i].kind!=row.kind || (sources[i].utf16Prefix!=0 && sources[i].utf16Prefix!=1) || !xidePathLexicalEqual(row.path,sources[i].path))return -1;
  if(row.text!=sources[i].text && (strlen(sources[i].text)!=row.size || memcmp(row.text,sources[i].text,row.size)))return -1;
  if(okay){size_t bytes=strlen(sources[i].path);uint64_t fields[]={bytes,(unsigned)sources[i].kind,(unsigned)sources[i].utf16Prefix};okay=navigationAppend(key,fields,sizeof(fields)) && navigationAppend(key,sources[i].path,bytes);}
 }
 return okay ? 1 : 0;
}
xide_semantic_definitions* xideSemanticDefinitionsAcquireTyped(xls_projection_store* cache,
 xls_semantic_check* check,const xide_semantic_source* sources,size_t count,xls_projection** lease,xide_navigation_work* work,xide_model_error* error)
{
 if(lease)*lease=NULL;
 if(work)*work=(xide_navigation_work){0};
 xstrbuf key={0};
 int ready=navigationKey(check,sources,count,&key,cache && lease);
 if(ready<0){if(error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Native navigation sources do not match their checked owner.");}xrtStrBufFree(&key);return NULL;}
 xls_projection* p=ready ? xlsProjectionAcquire(cache,check,key.Data,key.Size) : NULL;
 xide_semantic_definitions* d=p ? xlsProjectionValue(p) : xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(check),sources,count,error);
 if(!d){xrtStrBufFree(&key);return NULL;}
 if(work){work->built=p==NULL;work->hit=p!=NULL;work->bytes=navigationBytes(d);}
 if(!p && ready){p=xlsProjectionCreate(check,key.Data,key.Size,d,navigationBytes(d),destroyNavigation);if(p){p=xlsProjectionPublish(cache,p);d=xlsProjectionValue(p);}}
 if(lease)*lease=p;
 if(work)work->bytes=navigationBytes(d);
 xrtStrBufFree(&key);return d;
}
void xideSemanticDefinitionsReleaseTyped(xide_semantic_definitions* d,xls_projection* lease)
{if(lease)xlsProjectionRelease(lease);else xideSemanticDefinitionsDestroy(d);}
#endif
static int comparePoint(point a,point b) { return a.line!=b.line ? (a.line>b.line)-(a.line<b.line) : (a.character>b.character)-(a.character<b.character); }
static int contains(const span* s,int source,point p) { return s->source==source && comparePoint(s->start,p)<=0 && comparePoint(p,s->end)<0; }
static xvalue* position(point p) { xvalue* v=xrtValueObject(); if(v && put(v,"line",xrtValueInt(p.line)) && put(v,"character",xrtValueInt(p.character))) return v; xrtValueRelease(v); return NULL; }
static int sourceFor(xide_semantic_definitions* d,const char* path)
{ if(d && path) for(size_t i=0;i<d->sourceCount;++i) if(!strcmp(path,d->paths[i]) || xidePathLexicalEqual(path,d->paths[i])) return (int)i+1; return 0; }
static size_t resolveDeclaration(xide_semantic_definitions* d,size_t chosen,int source,point p)
{
 node* n=&d->nodes[chosen]; unsigned target=n->target ? n->target : n->declared;
 /* A call's first child is its callee. Only the callee's terminal token uses
    the resolved overload; arguments and a member receiver keep their IDs. */
 size_t child=chosen;
 for(unsigned parent=n->parent;parent;child=(size_t)parent-1,parent=d->nodes[parent-1].parent) {
  node* call=&d->nodes[parent-1]; node* callee=&d->nodes[child];
  if(call->call && call->target && child==(size_t)parent && callee->full.source==source && n->token.source==source && n->token.endOffset==callee->full.endOffset) { target=call->target; break; }
 }
 if(!target) for(unsigned parent=n->parent;parent;parent=d->nodes[parent-1].parent) { node* a=&d->nodes[parent-1]; if(a->declared && contains(&a->selection,source,p)) { target=a->declared; break; } }
 declaration key={target,0}; declaration* found=target ? bsearch(&key,d->declarations,d->declCount,sizeof(*d->declarations),compareDeclaration) : NULL;
 /* Explicit type facts can also name a checked base or constraint NAME.
    A variable's inferred type never redirects its SymbolId. Only a proven
    generic declaration can match ABI zero: its TypeId is compilation-local. */
 unsigned typeId=n->referencedType ? n->referencedType : n->declaredType;
 if(!target && typeId) {
  declaration typeKey={typeId,0}; declaration* type=bsearch(&typeKey,d->typeDeclarations,d->typeCount,sizeof(*d->typeDeclarations),compareDeclaration);
  if(type && type->node!=SIZE_MAX) { node* declarationNode=&d->nodes[type->node]; if(declarationNode->typeAbi==n->typeAbi && (n->typeAbi || declarationNode->genericType)) found=type; }
 }
 return found ? found->node : SIZE_MAX;
}
typedef struct cursor_search {xide_semantic_definitions* definitions;point position;int source,width;size_t chosen,visited;} cursor_search;
static void searchCursorTree(cursor_search* search,size_t lo,size_t hi)
{
 if(lo==hi)return;
 xide_semantic_definitions* d=search->definitions;size_t mid=lo+(hi-lo)/2;const cursor_row* r=(const cursor_row*)d->cursors.Data+mid;++search->visited;
 if(comparePoint((point){(int)r->maximumLine,(int)r->maximumCharacter},search->position)<=0)return;
 searchCursorTree(search,lo,mid);
 if(comparePoint((point){(int)r->sl,(int)r->sc},search->position)<=0) {
  size_t i=r->node&~CURSOR_SELECTION;node* n=d->nodes+i;const span* s=r->node&CURSOR_SELECTION ? &n->selection : &n->token;
  int suppressed=!(r->node&CURSOR_SELECTION) && (n->declared || n->declaredType) && contains(&n->selection,search->source,search->position);
  if(!suppressed && contains(s,search->source,search->position)) {int width=s->endOffset-s->startOffset;size_t chosen=search->chosen;
   if(width<search->width || (width==search->width && (chosen==SIZE_MAX || n->depth>d->nodes[chosen].depth || (n->depth==d->nodes[chosen].depth && i<chosen)))) {search->chosen=i;search->width=width;}
  }
  searchCursorTree(search,mid+1,hi);
 }
}
static size_t selectNodeIndexed(xide_semantic_definitions* d,int source,int line,int character,size_t* decl,size_t* visited)
{
 const cursor_row* rows=(const cursor_row*)d->cursors.Data;size_t lo=0,hi=d->cursors.Count;
 while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(rows[mid].source<(unsigned)source)lo=mid+1;else hi=mid;}
 size_t first=lo;hi=d->cursors.Count;
 while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(rows[mid].source<=(unsigned)source)lo=mid+1;else hi=mid;}
 point p={line,character};cursor_search search={d,p,source,INT_MAX,SIZE_MAX,0};searchCursorTree(&search,first,lo);size_t chosen=search.chosen;
 if(visited)*visited=search.visited;
 *decl=chosen==SIZE_MAX ? SIZE_MAX : resolveDeclaration(d,chosen,source,p); return chosen;
}
static size_t selectNode(xide_semantic_definitions* d,int source,int line,int character,size_t* decl)
{ return selectNodeIndexed(d,source,line,character,decl,NULL); }
static xvalue* wireRange(const span* s)
{ xvalue* r=xrtValueObject(); if(r && put(r,"start",position(s->start)) && put(r,"end",position(s->end))) return r; xrtValueRelease(r); return NULL; }
xvalue* xideSemanticDefinition(xide_semantic_definitions* d,const char* path,int line,int character)
{
 int source=sourceFor(d,path); if(!source || line<0 || character<0) return NULL;
 size_t decl=SIZE_MAX; selectNode(d,source,line,character,&decl);
 if(decl==SIZE_MAX || !d->nodes[decl].selection.source) return xrtValueNull();
 span* s=&d->nodes[decl].selection; char* uri=xideLspPathToUri(d->paths[s->source-1],NULL); xvalue* out=xrtValueObject();
 int okay=uri && out && put(out,"range",wireRange(s)) && put(out,"uri",xrtValueString(sv(uri)));
 free(uri); if(!okay) { xrtValueRelease(out); return NULL; } return out;
}
static int sameSelection(const span* a,const span* b)
{ return a->source && a->source==b->source && a->startOffset==b->startOffset && a->endOffset==b->endOffset && a->endOffset>a->startOffset; }
static const span* occurrence(const node* n,int source,point p,int atCursor)
{
 if((n->declared || n->declaredType) && n->selection.source==source && (!atCursor || contains(&n->selection,source,p))) return &n->selection;
 if((n->nameSyntax || n->typeSyntax || n->memberSyntax) && n->token.source==source && (!atCursor || contains(&n->token,source,p))) return &n->token;
 return NULL;
}
static int unsignedOrder(uint32_t a,uint32_t b) {return (a>b)-(a<b);}
static int compareCursor(const void* a,const void* b)
{
 const cursor_row *x=a,*y=b;int c=unsignedOrder(x->source,y->source);if(!c)c=unsignedOrder(x->sl,y->sl);if(!c)c=unsignedOrder(x->sc,y->sc);return c ? c : unsignedOrder(x->node,y->node);
}
static int compareReference(const void* a,const void* b)
{
 const reference_row *x=a,*y=b;int c=unsignedOrder(x->targetSource,y->targetSource);
 if(!c)c=unsignedOrder(x->targetStart,y->targetStart);
 if(!c)c=unsignedOrder(x->targetEnd,y->targetEnd);
 if(!c)c=unsignedOrder(x->source,y->source);
 if(!c)c=unsignedOrder(x->start,y->start);
 if(!c)c=unsignedOrder(x->end,y->end);
 return c ? c : unsignedOrder(x->node,y->node);
}
static int appendCursor(xide_semantic_definitions* d,const span* s,uint32_t index)
{
 if(!s->source || s->endOffset<=s->startOffset)return 1;
 cursor_row r={(uint32_t)s->source,(uint32_t)s->start.line,(uint32_t)s->start.character,(uint32_t)s->end.line,(uint32_t)s->end.character,0,0,index};
 return xrtArrayAppend(&d->cursors,&r,1);
}
/* An implicit balanced interval tree per source. A long enclosing span must
   not turn point lookup into a backwards scan through unrelated short tokens. */
static point buildCursorTree(cursor_row* rows,size_t lo,size_t hi)
{
 size_t mid=lo+(hi-lo)/2;cursor_row* r=rows+mid;point maximum={(int)r->el,(int)r->ec};
 if(lo<mid) {point left=buildCursorTree(rows,lo,mid);if(comparePoint(left,maximum)>0)maximum=left;}
 if(mid+1<hi) {point right=buildCursorTree(rows,mid+1,hi);if(comparePoint(right,maximum)>0)maximum=right;}
 r->maximumLine=(uint32_t)maximum.line;r->maximumCharacter=(uint32_t)maximum.character;return maximum;
}
static int buildLookup(xide_semantic_definitions* d)
{
 if(!xrtArrayInit(&d->cursors,sizeof(cursor_row)))return 0;
 d->lookupReady=1;if(!xrtArrayInit(&d->references,sizeof(reference_row)))return 0;
 d->lookupReady=3;
 if(!xrtArrayReserve(&d->cursors,d->count*2) || !xrtArrayReserve(&d->references,d->count))return 0;
 /* Bundled XRT uses geometric reserve with a minimum8-slot allocation.
    Account for that fixed allowance,including very small dependency roots. */
 if(d->cursors.Capacity>d->count*4+8 || d->references.Capacity>d->count*2+8)return 0;
 for(size_t i=0;i<d->count;++i) {
  node* n=d->nodes+i;
  int hasSelection=n->declared || n->declaredType;
  if(!appendCursor(d,&n->token,(uint32_t)i|(hasSelection && sameSelection(&n->selection,&n->token) ? CURSOR_SELECTION : 0)))return 0;
  if(hasSelection && !sameSelection(&n->selection,&n->token) && !appendCursor(d,&n->selection,(uint32_t)i|CURSOR_SELECTION))return 0;
 }
 if(!xrtArraySort(&d->cursors,compareCursor))return 0;
 cursor_row* cursors=(cursor_row*)d->cursors.Data;
 for(size_t i=0;i<d->cursors.Count;) {size_t end=i+1;while(end<d->cursors.Count && cursors[end].source==cursors[i].source)++end;buildCursorTree(cursors,i,end);i=end;}
 for(size_t i=0;i<d->count;++i) {
  const node* n=d->nodes+i;int atSource=(n->declared || n->declaredType) && n->selection.source ? n->selection.source : n->token.source;
  const span* at=occurrence(n,atSource,(point){0},0);if(!at || !at->source || at->endOffset<=at->startOffset)continue;
  size_t resolved=resolveDeclaration(d,i,at->source,at->start);if(resolved==SIZE_MAX)continue;
  const span* target=&d->nodes[resolved].selection;if(!target->source || target->endOffset<=target->startOffset)continue;
  reference_row r={(uint32_t)target->source,(uint32_t)target->startOffset,(uint32_t)target->endOffset,(uint32_t)at->source,(uint32_t)at->startOffset,(uint32_t)at->endOffset,(uint32_t)i};
  if(!xrtArrayAppend(&d->references,&r,1))return 0;
 }
 if(!xrtArraySort(&d->references,compareReference))return 0;
 reference_row* rows=(reference_row*)d->references.Data;size_t used=0;
 for(size_t i=0;i<d->references.Count;++i) {
  reference_row r=rows[i];
  if(used) {reference_row previous=rows[used-1];previous.node=r.node;if(!compareReference(&previous,&r))continue;}
  rows[used++]=r;
 }
 return xrtArrayResize(&d->references,used);
}
static int referenceTargetOrder(const reference_row* r,const span* target,int source)
{
 int c=unsignedOrder(r->targetSource,(uint32_t)target->source);
 if(!c)c=unsignedOrder(r->targetStart,(uint32_t)target->startOffset);
 if(!c)c=unsignedOrder(r->targetEnd,(uint32_t)target->endOffset);
 if(!c && source)c=unsignedOrder(r->source,(uint32_t)source);
 return c;
}
static int referenceOccurrenceOrder(const reference_row* r,const span* target)
{
 int c=unsignedOrder(r->source,(uint32_t)target->source);if(!c)c=unsignedOrder(r->start,(uint32_t)target->startOffset);return c ? c : unsignedOrder(r->end,(uint32_t)target->endOffset);
}
static int referenceView(xide_semantic_definitions* d,int source,int line,int character,int includeDeclaration,int onlySource,xide_semantic_reference_view* view)
{
 *view=(xide_semantic_reference_view){d,0,0,SIZE_MAX};
 size_t decl=SIZE_MAX,chosen=selectNode(d,source,line,character,&decl);
 if(chosen==SIZE_MAX || decl==SIZE_MAX || !occurrence(d->nodes+chosen,source,(point){line,character},1))return 1;
 const span* target=&d->nodes[decl].selection;if(!target->source || target->endOffset<=target->startOffset)return 1;
 const reference_row* rows=(const reference_row*)d->references.Data;size_t lo=0,hi=d->references.Count;
 while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(referenceTargetOrder(rows+mid,target,onlySource)<0)lo=mid+1;else hi=mid;}
 size_t first=lo;hi=d->references.Count;
 while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(referenceTargetOrder(rows+mid,target,onlySource)<=0)lo=mid+1;else hi=mid;}
 size_t last=lo;view->first=first;view->count=last-first;
 if(!includeDeclaration) {
  lo=first;hi=last;while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(referenceOccurrenceOrder(rows+mid,target)<0)lo=mid+1;else hi=mid;}
  if(lo<last && !referenceOccurrenceOrder(rows+lo,target)) {view->excluded=lo-first;--view->count;}
 }
 return 1;
}
int xideSemanticReferences(xide_semantic_definitions* d,const char* path,int line,int character,int includeDeclaration,xide_semantic_reference_view* view)
{
 if(view)*view=(xide_semantic_reference_view){0};
 int source=sourceFor(d,path);if(!view || !source || line<0 || character<0 || (includeDeclaration!=0 && includeDeclaration!=1))return 0;
 return referenceView(d,source,line,character,includeDeclaration,0,view);
}
int xideSemanticReferenceAt(const xide_semantic_reference_view* view,size_t index,xide_semantic_reference* out)
{
 if(out)*out=(xide_semantic_reference){0};
 if(!view || !out || !view->definitions || index>=view->count)return 0;
 const xide_semantic_definitions* d=view->definitions;
 if(view->first>d->references.Count || view->count>d->references.Count-view->first || (view->excluded!=SIZE_MAX && (view->excluded>view->count || view->count==d->references.Count-view->first)))return 0;
 size_t at=view->first+index+(view->excluded!=SIZE_MAX && index>=view->excluded);
 const reference_row* r=(const reference_row*)d->references.Data+at;const span* s=occurrence(d->nodes+r->node,(int)r->source,(point){0},0);
 if(!s || !s->source || s->startOffset!=(int)r->start || s->endOffset!=(int)r->end)return 0;
 *out=(xide_semantic_reference){d->paths[r->source-1],{.line=s->start.line,.character=s->start.character,.offset=s->startOffset},{.line=s->end.line,.character=s->end.character,.offset=s->endOffset},r->source==r->targetSource && r->start==r->targetStart && r->end==r->targetEnd};return 1;
}
xvalue* xideSemanticDocumentHighlights(xide_semantic_definitions* d,const char* path,int line,int character)
{
 int source=sourceFor(d,path);if(!source || line<0 || character<0) return NULL;
 xide_semantic_reference_view view={0};if(!referenceView(d,source,line,character,1,source,&view))return NULL;
 xvalue* out=xrtValueArray();if(!out)return NULL;int okay=1;const reference_row* rows=(const reference_row*)d->references.Data;
 for(size_t i=0;okay && i<view.count;++i) {
  const reference_row* r=rows+view.first+i;const span* s=occurrence(d->nodes+r->node,source,(point){0},0);
  xvalue* value=xrtValueObject();okay=value && put(value,"range",wireRange(s)) && put(value,"kind",xrtValueInt(1));
  if(okay) { okay=xrtValueArrayAppendNew(out,value);value=NULL; } xrtValueRelease(value);
 }
 if(!okay) { xrtValueRelease(out);return NULL; }return out;
}
xvalue* xideSemanticHover(xide_semantic_definitions* d,const char* path,int line,int character)
{
 int source=sourceFor(d,path); if(!source || line<0 || character<0) return NULL;
 size_t decl=SIZE_MAX,chosen=selectNode(d,source,line,character,&decl); if(chosen==SIZE_MAX) return xrtValueNull();
 node *n=&d->nodes[chosen],*a=decl==SIZE_MAX ? NULL : &d->nodes[decl];
 /* Function scope-name IDs and callable IDs are distinct; use the actual
    enclosing declaration header when hovering its source name. */
 if(a && !a->kind) for(unsigned parent=a->parent;parent;parent=d->nodes[parent-1].parent) { node* owner=&d->nodes[parent-1]; if(owner->kind==12 && contains(&owner->selection,source,(point){line,character})) { a=owner; break; } }
 const char *type=n->type ? n->type : a ? a->type : NULL,*header=a ? a->header : n->header;
 int origin=n->type ? n->origin : a ? a->origin : 0;
 if(!type && !header) return xrtValueNull();
 char text[12500]; int length=snprintf(text,sizeof(text),"%s%s%s%s%s",header ? header : "",header && type ? "\n" : "",type ? (origin==2 ? "类型（推导）：" : origin==1 ? "类型（声明）：" : "类型（语法）：") : "",type ? type : "",a && a->name && !header ? a->name : "");
 if(length<0 || (size_t)length>=sizeof(text)) return NULL;
 span* r=contains(&n->selection,source,(point){line,character}) ? &n->selection : &n->token;
 xvalue *out=xrtValueObject(),*content=xrtValueObject(); int okay=out && content && put(content,"kind",xrtValueString(sv("plaintext"))) && put(content,"value",xrtValueString(sv(text)));
 if(okay) { okay=put(out,"contents",content); content=NULL; } if(okay) okay=put(out,"range",wireRange(r));
 xrtValueRelease(content); if(!okay) { xrtValueRelease(out); return NULL; } return out;
}
/* Standard DocumentSymbol hierarchy, in original source order. Locals and
   parameters are excluded; generated declarations without a source name are
   omitted. Name-node scope IDs never duplicate a function declaration. */
xvalue* xideSemanticDocumentSymbols(xide_semantic_definitions* d,const char* path)
{
 int source=sourceFor(d,path); if(!source) return NULL;
 xvalue *out=xrtValueArray(),**items=calloc(d->count+1,sizeof(*items)); int okay=out && items; size_t count=0;
 for(size_t i=0;okay && i<d->count;++i) {
  node* n=&d->nodes[i]; if(!n->kind || !n->name || n->full.source!=source || n->selection.source!=source) continue;
  size_t parent=SIZE_MAX; int inFunction=0;
  for(unsigned p=n->parent;p;p=d->nodes[p-1].parent) { node* a=&d->nodes[p-1]; if(a->kind==12) inFunction=1; if(parent==SIZE_MAX && items[p-1]) parent=p-1; }
  if(inFunction || ++count>4096) { if(count>4096) okay=0; continue; }
  xvalue *v=xrtValueObject(),*children=xrtValueArray(); int kind=n->kind;
  if(kind==12 && parent!=SIZE_MAX && d->nodes[parent].kind==5) kind=6;
  okay=v && children && put(v,"name",xrtValueString(sv(n->name))) && put(v,"kind",xrtValueInt(kind)) && put(v,"range",wireRange(&n->full)) && put(v,"selectionRange",wireRange(&n->selection));
  if(okay && n->header) okay=put(v,"detail",xrtValueString(sv(n->header)));
  if(okay) { okay=put(v,"children",children); children=NULL; }
  if(okay && parent!=SIZE_MAX) { span *a=&d->nodes[parent].full; okay=n->full.startOffset>=a->startOffset && n->full.endOffset<=a->endOffset; }
  if(okay) { items[i]=v; okay=xrtValueArrayAppendNew(parent==SIZE_MAX ? out : get(items[parent],"children"),v); v=NULL; }
  xrtValueRelease(v); xrtValueRelease(children);
 }
 free(items); if(!okay) { xrtValueRelease(out); return NULL; } return out;
}
#ifdef XLS_COMPILER_XRT
#include "services/semantic_reference_shard.inc"
#endif
