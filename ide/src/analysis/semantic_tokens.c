#include "analysis/semantic_tokens.h"
#include "analysis/declaration.h"
/* The upstream static-helper header also defines the full literal scanner. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "src/lexer/xl6_literal_span.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <xrt.h>
#include <string.h>
#include <stdlib.h>

#define TOKEN_BYTES (16u*1024u*1024u)
typedef struct token_role {unsigned id,kind,modifiers;uint64_t abi;} token_role;
typedef struct token_builder {
 xarray rows,symbols,types,scopes;size_t budget,source,size;const char* text;const xls_semantic_index* index;
 int (*cancelled)(void*);void* user;enum xls_semantic_tokens_error error;
 xls_semantic_tokens_work work;
} token_builder;
struct xls_semantic_tokens {xatomic32 references;xarray rows;xls_semantic_tokens_work work;};

static int stopped(token_builder* b)
{if(b->cancelled && b->cancelled(b->user)){b->error=XLS_TOKENS_CANCELLED;return 1;}return 0;}
static size_t charged(const token_builder* b)
{return sizeof(xls_semantic_tokens)+b->rows.Capacity*sizeof(xls_semantic_token)+(b->symbols.Capacity+b->types.Capacity)*sizeof(token_role)+b->scopes.Capacity;}
static int push(token_builder* b,xarray* a,const void* value)
{
 size_t current=charged(b),wanted=a->Count+1,capacity=a->Capacity;
 if(current>b->budget){b->error=XLS_TOKENS_LIMIT;return 0;}
 if(wanted>capacity){
  if(wanted>(SIZE_MAX-8)/2){b->error=XLS_TOKENS_LIMIT;return 0;}
  size_t bound=wanted*2+8;
  if(current>b->budget || bound>capacity+(b->budget-current)/a->ItemSize){b->error=XLS_TOKENS_LIMIT;return 0;}
  if(!xrtArrayReserve(a,wanted)){b->error=XLS_TOKENS_MEMORY;return 0;}
  if(a->Capacity>bound || charged(b)>b->budget){b->error=XLS_TOKENS_LIMIT;return 0;}
 }
 if(charged(b)>b->work.peakBytes)b->work.peakBytes=charged(b);
 if(!xrtArrayPush(a,value)){b->error=XLS_TOKENS_MEMORY;return 0;}return 1;
}
static int same(const char* a,const char* z){return a && !strcmp(a,z);}
static int roleOrder(const void* a,const void* z)
{
 const token_role* x=a,*y=z;
 if(x->id!=y->id)return (x->id>y->id)-(x->id<y->id);
 return (x->abi>y->abi)-(x->abi<y->abi);
}
static int rowOrder(const void* a,const void* z)
{
 const xls_semantic_token* x=a,*y=z;
 if(x->range.startOffset!=y->range.startOffset)return (x->range.startOffset>y->range.startOffset)-(x->range.startOffset<y->range.startOffset);
 if(x->range.endOffset!=y->range.endOffset)return (x->range.endOffset>y->range.endOffset)-(x->range.endOffset<y->range.endOffset);
 return (int)(y->modifiers&XLS_TOKEN_DECLARATION)-(int)(x->modifiers&XLS_TOKEN_DECLARATION);
}
static const token_role* find(const xarray* a,unsigned id,uint64_t abi)
{
 token_role key={.id=id,.abi=abi};
 return id && a->Count ? bsearch(&key,a->Data,a->Count,sizeof(key),roleOrder) : NULL;
}
static unsigned declarationKind(const xls_declaration_fact* f,int member)
{
 switch(f->shape.kind){
 case XLS_DECL_CLASS:return XLS_TOKEN_CLASS;case XLS_DECL_INTERFACE:return XLS_TOKEN_INTERFACE;
 case XLS_DECL_ENUM:return XLS_TOKEN_ENUM;case XLS_DECL_RECORD:return XLS_TOKEN_STRUCT;
 case XLS_DECL_ALIAS:return XLS_TOKEN_TYPE;case XLS_DECL_TYPE_PARAMETER:return f->fact.declaredType ? XLS_TOKEN_TYPE_PARAMETER : 0;
 case XLS_DECL_ENUM_MEMBER:return XLS_TOKEN_ENUM_MEMBER;
 case XLS_DECL_FIELD:case XLS_DECL_PROPERTY:return XLS_TOKEN_PROPERTY;
 case XLS_DECL_CONSTANT:case XLS_DECL_VARIABLE:return member ? XLS_TOKEN_PROPERTY : XLS_TOKEN_VARIABLE;
 case XLS_DECL_FUNCTION:case XLS_DECL_SPECIAL:return member ? XLS_TOKEN_METHOD : XLS_TOKEN_FUNCTION;
 default:return same(f->fact.kind,"param") ? XLS_TOKEN_PARAMETER : 0;
 }
}
static int candidate(token_builder* b,unsigned source,xls_index_range range,unsigned kind,unsigned modifiers)
{
 if(source!=b->source || !range.known || !kind)return 1;
 if(range.endOffset<range.startOffset || range.endOffset>b->size || range.startLine<1 || range.startColumn<1 || range.endLine<range.startLine || range.endColumn<1){b->error=XLS_TOKENS_FACTS;return 0;}
 /* Symbol names are single-line atoms. Multi-line syntax ranges are not
  * semantic tokens and remain covered by the client's lexical highlighter. */
 if(range.endOffset==range.startOffset || range.startLine!=range.endLine || range.endColumn<=range.startColumn)return 1;
 xls_semantic_token row={range,kind,modifiers};++b->work.candidates;return push(b,&b->rows,&row);
}
static int declarations(token_builder* b,size_t count)
{
 for(size_t i=0;i<count;++i){xls_declaration_fact d;
  if(!(i%64) && stopped(b))return 0;
  if(!xlsSemanticIndexDeclarationFact(b->index,i,&d) || d.fact.id!=i+1 || d.fact.parent>i){b->error=XLS_TOKENS_FACTS;return 0;}++b->work.facts;
  /* Preorder parent context is computed once per fact, including unnamed
   * containers. Callable children reset class scope; member declarations use
   * the parent's context. No per-declaration ancestor walk is needed. */
  int member=d.fact.parent ? b->scopes.Data[d.fact.parent-1]!=0 : 0;
  unsigned char scope=d.shape.kind==XLS_DECL_CLASS || d.shape.kind==XLS_DECL_INTERFACE || d.shape.kind==XLS_DECL_RECORD ? 1 : d.shape.callable ? 0 : (unsigned char)member;
  if(!push(b,&b->scopes,&scope))return 0;
  unsigned kind=declarationKind(&d,member);if(!kind)continue;
  unsigned modifiers=d.shape.kind==XLS_DECL_CONSTANT ? XLS_TOKEN_READONLY : 0;
  if(same(d.fact.kind,"async_func"))modifiers|=XLS_TOKEN_ASYNC;
  token_role symbol={d.fact.declaredSymbol,kind,modifiers,0},type={d.fact.declaredType,kind,modifiers,d.fact.typeAbi};
  if(symbol.id && !push(b,&b->symbols,&symbol))return 0;
  if(type.id && !push(b,&b->types,&type))return 0;
  unsigned source=d.nameSource;xls_index_range range=d.nameRange;
  if(kind==XLS_TOKEN_PARAMETER){source=d.fact.selectionSource;range=d.fact.selectionRange;if(!range.known){source=d.fact.source;range=d.fact.tokenRange;}}
  if(!candidate(b,source,range,kind,modifiers|XLS_TOKEN_DECLARATION))return 0;
  ++b->work.declarations;
 }
 return 1;
}
static void uniqueRoles(xarray* array)
{
 token_role* rows=(token_role*)array->Data;size_t used=0;
 for(size_t i=0;i<array->Count;){size_t end=i+1;token_role row=rows[i];
  while(end<array->Count && !roleOrder(rows+i,rows+end)){
   if(row.kind!=rows[end].kind || row.modifiers!=rows[end].modifiers){row.kind=0;}
   ++end;
  }
  rows[used++]=row;i=end;
 }array->Count=used;
}
static int references(token_builder* b,size_t count)
{
 for(size_t i=0;i<count;++i){xls_index_point_fact p;
  if(!(i%64) && stopped(b))return 0;
  if(!xlsSemanticIndexPointFact(b->index,i,&p) || p.fact.id!=i+1 || p.fact.parent>i){b->error=XLS_TOKENS_FACTS;return 0;}++b->work.facts;
  const xls_index_fact* f=&p.fact;
  if(f->source!=b->source || !f->tokenRange.known || (p.kind!=XLS_INDEX_POINT_NAME && p.kind!=XLS_INDEX_POINT_TYPE && p.kind!=XLS_INDEX_POINT_MEMBER && p.kind!=XLS_INDEX_POINT_SAFE_MEMBER))continue;
  const token_role* role=find(&b->symbols,f->targetSymbol ? f->targetSymbol : f->symbol,0);
  if(!role && f->parent && f->id==f->parent+1){xls_index_point_fact parent;
   if(!xlsSemanticIndexPointFact(b->index,f->parent-1,&parent)){b->error=XLS_TOKENS_FACTS;return 0;}
   ++b->work.facts;
   if(parent.kind==XLS_INDEX_POINT_CALL)role=find(&b->symbols,parent.fact.targetSymbol,0);
  }
  if(!role && f->referencedType)role=find(&b->types,f->referencedType,f->typeAbi);
  unsigned kind=role ? role->kind : p.kind==XLS_INDEX_POINT_TYPE || f->referencedType ? XLS_TOKEN_TYPE :
   p.kind==XLS_INDEX_POINT_MEMBER || p.kind==XLS_INDEX_POINT_SAFE_MEMBER ? XLS_TOKEN_PROPERTY : 0;
  if(!candidate(b,f->source,f->tokenRange,kind,role ? role->modifiers : 0))return 0;
 }
 return 1;
}
static void uniqueRows(xarray* array)
{
 xls_semantic_token* rows=(xls_semantic_token*)array->Data;size_t used=0;
 for(size_t i=0;i<array->Count;){size_t end=i+1;xls_semantic_token row=rows[i];
  while(end<array->Count && rows[end].range.startOffset==row.range.startOffset && rows[end].range.endOffset==row.range.endOffset){
   xls_semantic_token* next=rows+end;
   if(row.kind!=next->kind && !(row.modifiers&XLS_TOKEN_DECLARATION)){
    if(row.kind==XLS_TOKEN_TYPE)row.kind=next->kind;else if(next->kind!=XLS_TOKEN_TYPE)row.kind=0;
   }
   if(row.kind==next->kind){row.modifiers|=next->modifiers;}
   ++end;
  }
  if(row.kind && (!used || row.range.startOffset>=rows[used-1].range.endOffset))rows[used++]=row;
  i=end;
 }array->Count=used;
}
/* After sorting/deduplication, only nonoverlapping ranges are scanned. Reuse
 * the compiler's lexical byte predicate; total atom work is <=source bytes,
 * including generated duplicate ranges. No separate name grammar is added. */
static int filterAtoms(token_builder* b)
{
 xls_semantic_token* rows=(xls_semantic_token*)b->rows.Data;size_t used=0;
 for(size_t i=0;i<b->rows.Count;++i){const xls_index_range* range=&rows[i].range;
  int atom=1;
  for(size_t at=range->startOffset;at<range->endOffset;++at){
   if(!(b->work.atomBytes%4096) && stopped(b))return 0;
   unsigned char ch=(unsigned char)b->text[at];++b->work.atomBytes;
   if(!xl6LiteralIdentByte(ch) || (at==range->startOffset && ch>='0' && ch<='9')){atom=0;break;}
  }
  if(atom)rows[used++]=rows[i];
 }
 b->rows.Count=used;return 1;
}
xls_semantic_tokens* xlsSemanticTokensCreate(const xls_semantic_check* check,size_t source,size_t budget,int (*cancelled)(void*),void* user,enum xls_semantic_tokens_error* error)
{
 if(error)*error=XLS_TOKENS_OK;
 token_builder b={.budget=budget ? budget : TOKEN_BYTES,.cancelled=cancelled,.user=user};xls_input_source input;
 if(!check || b.budget>TOKEN_BYTES || b.budget<sizeof(xls_semantic_tokens) || source>=UINT32_MAX){b.error=XLS_TOKENS_ARGUMENT;goto done;}
 b.index=xlsSemanticCheckTypedIndex(check);
 if(!b.index){b.error=XLS_TOKENS_NO_INDEX;goto done;}
 if(!xlsSemanticCheckSource(check,source,&input) || !(input.kind&1)){b.error=XLS_TOKENS_ARGUMENT;goto done;}
 b.source=source+1;b.size=input.size;b.text=input.text;
 if(stopped(&b))goto done;
 if(!xrtArrayInit(&b.rows,sizeof(xls_semantic_token)) || !xrtArrayInit(&b.symbols,sizeof(token_role)) || !xrtArrayInit(&b.types,sizeof(token_role)) || !xrtArrayInit(&b.scopes,1)){b.error=XLS_TOKENS_MEMORY;goto done;}
 if(charged(&b)>b.budget){b.error=XLS_TOKENS_LIMIT;goto done;}b.work.peakBytes=charged(&b);
 size_t count=xlsSemanticIndexCount(b.index);
 if(!declarations(&b,count))goto done;
 xrtArrayUnit(&b.scopes);
 if((b.symbols.Count>1 && !xrtArraySort(&b.symbols,roleOrder)) || (b.types.Count>1 && !xrtArraySort(&b.types,roleOrder))){b.error=XLS_TOKENS_MEMORY;goto done;}
 uniqueRoles(&b.symbols);uniqueRoles(&b.types);
 if(!references(&b,count) || stopped(&b))goto done;
 if(b.rows.Count>1 && !xrtArraySort(&b.rows,rowOrder)){b.error=XLS_TOKENS_MEMORY;goto done;}
 uniqueRows(&b.rows);if(!filterAtoms(&b) || stopped(&b))goto done;
 if(xlsSemanticCheckTypedIndex(check)!=b.index){b.error=XLS_TOKENS_NO_INDEX;goto done;}
 xls_semantic_tokens* result=xrtCalloc(1,sizeof(*result));if(!result){b.error=XLS_TOKENS_MEMORY;goto done;}
 xrtAtomic32Init(&result->references,1);result->rows=b.rows;memset(&b.rows,0,sizeof(b.rows));b.work.bytes=sizeof(*result)+result->rows.Capacity*sizeof(xls_semantic_token);result->work=b.work;
 xrtArrayUnit(&b.symbols);xrtArrayUnit(&b.types);return result;
done:
 xrtArrayUnit(&b.scopes);xrtArrayUnit(&b.rows);xrtArrayUnit(&b.symbols);xrtArrayUnit(&b.types);if(error)*error=b.error;return NULL;
}
xls_semantic_tokens* xlsSemanticTokensRetain(xls_semantic_tokens* tokens)
{if(tokens)xrtAtomic32FetchAdd(&tokens->references,1,XMEMORY_RELAXED);return tokens;}
void xlsSemanticTokensRelease(xls_semantic_tokens* tokens)
{if(tokens && xrtAtomic32FetchSub(&tokens->references,1,XMEMORY_ACQ_REL)==1){xrtArrayUnit(&tokens->rows);xrtFree(tokens);}}
size_t xlsSemanticTokensCount(const xls_semantic_tokens* tokens){return tokens ? tokens->rows.Count : 0;}
const xls_semantic_token* xlsSemanticTokensData(const xls_semantic_tokens* tokens){return tokens ? (const xls_semantic_token*)tokens->rows.Data : NULL;}
xls_semantic_tokens_work xlsSemanticTokensWork(const xls_semantic_tokens* tokens){return tokens ? tokens->work : (xls_semantic_tokens_work){0};}
