/* Same-compilation IDs and source ranges, validated against exact inputs.
   Positions are converted in one sorted pass per source, not one scan/node. */
#include "services/semantic_definition.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#define NODE_LIMIT 65536u
#define DEPTH_LIMIT 256u
typedef struct point { int line,character; } point;
typedef struct span { int source,startOffset,endOffset; point start,end; } span;
typedef struct node { unsigned parent,depth,declared,target,typeId,declaredType,referencedType; uint64_t typeAbi; int call,kind,origin,typeSyntax,typeParams,typeConstraint,nameSyntax,genericType; char *name,*header,*type; span full,token,selection; } node;
typedef struct declaration { unsigned symbol; size_t node; } declaration;
typedef struct endpoint { int source,offset,line,column; point* output; } endpoint;
struct xide_semantic_definitions { node* nodes; size_t count,declCount,typeCount,sourceCount; declaration *declarations,*typeDeclarations; char** paths; size_t textBytes; };
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
static int typeFacts(xide_semantic_definitions* d,node* n,const xvalue* row,size_t index,int* metadata)
{
 const xvalue *decl=get(row,"declaredTypeId"),*ref=get(row,"referencedTypeId"); int present=decl!=NULL || ref!=NULL; int64 declared=0,referenced=0;
 if(*metadata<0) *metadata=present;
 if(*metadata!=present) return 0;
 if(!present) { n->declaredType=n->declared && typeKind(n) ? n->typeId : 0; n->referencedType=n->typeSyntax ? n->typeId : 0; return 1; }
 if(!number(decl,&declared,0,UINT_MAX) || !number(ref,&referenced,0,UINT_MAX) || (declared && referenced) || (declared && declared!=n->typeId) || (referenced && referenced!=n->typeId)) return 0;
 const node* parent=n->parent ? &d->nodes[n->parent-1] : NULL;
 n->genericType=n->nameSyntax && parent && parent->typeParams;
 if(declared && !typeKind(n) && !n->genericType) return 0;
 if(n->genericType && n->typeAbi) return 0; /* No runtime ABI for a type parameter. */
 if(referenced && !n->typeSyntax && !(n->nameSyntax && parent && (parent->typeConstraint || (parent->kind==5 && index==(size_t)n->parent)))) return 0;
 n->declaredType=(unsigned)declared; n->referencedType=(unsigned)referenced;
 if(declared && n->genericType) n->kind=26;
 return 1;
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
static int range(const xvalue* value,int source,const xide_semantic_source* sources,size_t count,span* out,endpoint* ends,size_t* used)
{
 if(!value) return 0;
 if(xrtValueType(value)==XVALUE_NULL) return 1;
 if(source<1 || (size_t)source>count || !(sources[source-1].kind&1)) return 0;
 int64 a,b,sl,sc,el,ec;
 if(!number(get(value,"startOffset"),&a,0,INT_MAX) || !number(get(value,"endOffset"),&b,a,INT_MAX) || !number(get(value,"startLine"),&sl,1,INT_MAX) || !number(get(value,"startColumn"),&sc,1,INT_MAX) || !number(get(value,"endLine"),&el,1,INT_MAX) || !number(get(value,"endColumn"),&ec,1,INT_MAX)) return 0;
 out->source=source; out->startOffset=(int)a; out->endOffset=(int)b;
 ends[(*used)++]=(endpoint){source,(int)a,(int)sl,(int)sc,&out->start};
 ends[(*used)++]=(endpoint){source,(int)b,(int)el,(int)ec,&out->end}; return 1;
}
static int convert(endpoint* ends,size_t used,const xide_semantic_source* sources)
{
 qsort(ends,used,sizeof(*ends),compareEndpoint); int source=0,line=1,column=1,character=0; size_t at=0,size=0; const char* text=NULL;
 for(size_t i=0;i<used;++i) {
  endpoint* e=&ends[i];
  if(e->source!=source) { source=e->source; text=sources[source-1].text; if(!text) return 0; size=strlen(text); at=0; line=column=1; character=sources[source-1].utf16Prefix; }
  if((size_t)e->offset>size) return 0;
  while(at<(size_t)e->offset) {
   unsigned char c=(unsigned char)text[at]; size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
   if(c=='\r' && at+1<size && text[at+1]=='\n') n=2;
   if(n>size-at || at+n>(size_t)e->offset) return 0;
   if(c=='\r' || c=='\n') { ++line; column=1; character=0; } else { ++column; character+=n==4 ? 2 : 1; }
   at+=n;
  }
  if(e->line!=line || e->column!=column) return 0;
  *e->output=(point){line-1,character};
 }
 return 1;
}
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
{ if(d) { if(d->paths) for(size_t i=0;i<d->sourceCount;++i) free(d->paths[i]); free(d->paths); if(d->nodes) for(size_t i=0;i<d->count;++i) { free(d->nodes[i].name); free(d->nodes[i].header); free(d->nodes[i].type); } free(d->declarations); free(d->typeDeclarations); free(d->nodes); free(d); } }
xide_semantic_definitions* xideSemanticDefinitionsCreate(const xvalue* index,const xide_semantic_source* sources,size_t count,xide_model_error* error)
{
 int64 schema=0; bool complete=false; const xvalue* rows=get(index,"nodes");
 if(!sources || !count || count>512 || !number(get(index,"schema"),&schema,1,1) || !equal(get(index,"scope"),"checked-compilation") || !xrtValueGetBool(get(index,"complete"),&complete) || !complete || !equal(get(index,"offsetEncoding"),"utf-8-after-bom") || !equal(get(index,"columnEncoding"),"unicode-scalar-1-based") || !equal(get(index,"rangeConvention"),"half-open") || !rows || xrtValueType(rows)!=XVALUE_ARRAY || xrtValueCount(rows)>NODE_LIMIT) goto invalid;
 xide_semantic_definitions* d=calloc(1,sizeof(*d)); endpoint* ends=NULL;
 if(!d) goto invalid;
 d->count=xrtValueCount(rows); d->sourceCount=count; d->paths=calloc(count,sizeof(*d->paths)); d->nodes=calloc(d->count+1,sizeof(*d->nodes)); d->declarations=calloc(d->count+1,sizeof(*d->declarations)); d->typeDeclarations=calloc(d->count+1,sizeof(*d->typeDeclarations)); ends=calloc(d->count*6+1,sizeof(*ends));
 int okay=d->paths && d->nodes && d->declarations && d->typeDeclarations && ends; size_t used=0; int typeMetadata=-1;
 for(size_t i=0;okay && i<count;++i) okay=sources[i].path && sources[i].text && (sources[i].utf16Prefix==0 || sources[i].utf16Prefix==1) && (d->paths[i]=copy(sources[i].path))!=NULL;
 for(size_t i=0;okay && i<d->count;++i) {
  const xvalue* v=xrtValueArrayGet(rows,i); node* n=&d->nodes[i]; int64 id,parent,source,selected,declared,symbol,target,operatorId; const char* kind=string(get(v,"kind"));
  okay=kind && *kind && strlen(kind)<=64 && number(get(v,"id"),&id,(int64)i+1,(int64)i+1) && number(get(v,"parent"),&parent,0,(int64)i) && number(get(v,"sourceId"),&source,0,(int64)count) && number(get(v,"selectionSourceId"),&selected,0,(int64)count) && number(get(v,"declaredSymbolId"),&declared,0,UINT_MAX) && number(get(v,"symbolId"),&symbol,0,UINT_MAX) && number(get(v,"targetSymbolId"),&target,0,UINT_MAX) && number(get(v,"compoundOperatorSymbolId"),&operatorId,0,UINT_MAX) && (!declared || declared==symbol);
  if(!okay) break;
  n->parent=(unsigned)parent; n->depth=parent ? d->nodes[parent-1].depth+1 : 1; n->declared=(unsigned)declared; n->target=(unsigned)target; n->call=!strcmp(kind,"call"); n->kind=symbolKind(kind); n->typeSyntax=!strcmp(kind,"type"); n->nameSyntax=!strcmp(kind,"name"); n->typeParams=!strcmp(kind,"type_params"); n->typeConstraint=!strcmp(kind,"generic_constraint");
  okay=n->depth<=DEPTH_LIMIT && retainType(d,n,v) && typeFacts(d,n,v,i,&typeMetadata) && range(get(v,"range"),(int)source,sources,count,&n->full,ends,&used) && range(get(v,"tokenRange"),(int)source,sources,count,&n->token,ends,&used) && range(get(v,"selectionRange"),(int)selected,sources,count,&n->selection,ends,&used);
  if(okay && n->token.source && n->full.source) okay=n->token.startOffset>=n->full.startOffset && n->token.endOffset<=n->full.endOffset;
  if(okay && declared) d->declarations[d->declCount++]=(declaration){(unsigned)declared,i};
  if(okay && n->declaredType) d->typeDeclarations[d->typeCount++]=(declaration){n->declaredType,i};
 }
 if(okay) okay=convert(ends,used,sources);
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
 free(ends); if(okay) return d; xideSemanticDefinitionsDestroy(d);
invalid:
 if(error) { error->code=XIDE_MODEL_INVALID; snprintf(error->message,sizeof(error->message),"编译器语义索引的身份、来源或范围无法验证。"); } return NULL;
}
static int comparePoint(point a,point b) { return a.line!=b.line ? (a.line>b.line)-(a.line<b.line) : (a.character>b.character)-(a.character<b.character); }
static int contains(const span* s,int source,point p) { return s->source==source && comparePoint(s->start,p)<=0 && comparePoint(p,s->end)<0; }
static xvalue* position(point p) { xvalue* v=xrtValueObject(); if(v && put(v,"line",xrtValueInt(p.line)) && put(v,"character",xrtValueInt(p.character))) return v; xrtValueRelease(v); return NULL; }
static int sourceFor(xide_semantic_definitions* d,const char* path)
{ if(d && path) for(size_t i=0;i<d->sourceCount;++i) if(xidePathLexicalEqual(path,d->paths[i])) return (int)i+1; return 0; }
static size_t selectNode(xide_semantic_definitions* d,int source,int line,int character,size_t* decl)
{
 point p={line,character}; size_t chosen=SIZE_MAX; int width=INT_MAX;
 for(size_t i=0;i<d->count;++i) {
  node* n=&d->nodes[i]; const span* s=contains(&n->selection,source,p) && (n->declared || n->declaredType) ? &n->selection : &n->token;
  if(contains(s,source,p)) { int w=s->endOffset-s->startOffset; if(w<width || (w==width && (chosen==SIZE_MAX || n->depth>d->nodes[chosen].depth))) { chosen=i; width=w; } }
 }
 if(chosen==SIZE_MAX) return SIZE_MAX;
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
 *decl=found ? found->node : SIZE_MAX; return chosen;

}
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
