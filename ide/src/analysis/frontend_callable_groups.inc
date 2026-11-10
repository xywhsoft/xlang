/* On-demand relation capture in the existing immutable native collector. */
#include "analysis/callable.h"
#include "analysis/frontend_callable_native.h"
#ifndef XLS_CALLABLE_ALLOC
#define XLS_CALLABLE_ALLOC xrtMalloc
#endif
#define XLS_CALLABLE_BYTES (32u*1024u*1024u)
typedef struct callable_row {uint32_t parent,symbol;unsigned kind,global;const char* name;const XL6_AstNode* node;} callable_row;
struct xls_callable_relations {size_t bytes,count;xls_callable_relation rows[];};
static int callableStopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static int callableRow(const XL6_IndexBuild* b,size_t at,callable_row* out)
{
 const XL6_IndexFact* f=xlsIndexFactAt(b,at);if(!f || !f->node || !f->node->semanticDeclaredSymbolId || !f->parent || f->parent>b->count)return 0;
 const XL6_IndexFact* p=xlsIndexFactAt(b,f->parent-1);if(!p || !p->node)return 0;
 int global=p->node->kind==XL6_AST_MODULE;
 if(!global){
  if(p->node->kind!=XL6_AST_LIST || !p->parent || p->parent>b->count)return 0;
  const XL6_IndexFact* owner=xlsIndexFactAt(b,p->parent-1);if(!owner || !owner->node || (owner->node->kind!=XL6_AST_CLASS && owner->node->kind!=XL6_AST_PROTOCOL))return 0;
 }
 unsigned kind=0;const char* name=xlsNativeCallableName(f->node,global,&kind);if(!kind || !name || !*name)return 0;
 *out=(callable_row){f->parent,f->node->semanticDeclaredSymbolId,kind,(unsigned)global,name,f->node};return 1;
}
static int callableOrder(const void* a,const void* b)
{
 const callable_row *x=a,*y=b;if(x->parent!=y->parent)return (x->parent>y->parent)-(x->parent<y->parent);
 if(x->kind!=y->kind)return (x->kind>y->kind)-(x->kind<y->kind);
 int n=strcmp(x->name,y->name);return n ? n : (x->symbol>y->symbol)-(x->symbol<y->symbol);
}
static int callableSameBucket(const callable_row* a,const callable_row* b)
{return a->parent==b->parent && a->kind==b->kind && !strcmp(a->name,b->name);}
void xlsCallableRelationsDestroy(xls_callable_relations* p){xrtFree(p);}
size_t xlsCallableRelationsBytes(const xls_callable_relations* p){return p ? p->bytes : 0;}
size_t xlsCallableRelationsCount(const xls_callable_relations* p){return p ? p->count : 0;}
int xlsCallableRelationsAt(const xls_callable_relations* p,size_t at,xls_callable_relation* out)
{if(out)*out=(xls_callable_relation){0};if(!p || !out || at>=p->count)return 0;*out=p->rows[at];return 1;}
xls_callable_relations* xlsFrontendCallableRelations(const xls_semantic_index* index,size_t limit,
 int (*cancelled)(void*),void* user,enum xls_callable_error* output)
{
 enum xls_callable_error why=XLS_CALLABLE_INVALID;xls_callable_relations* result=NULL;callable_row* rows=NULL;size_t count=0,scratch=0,relations=0;
 if(!limit)limit=XLS_CALLABLE_BYTES;
 if(!index || limit>XLS_CALLABLE_BYTES)goto done;
 const XL6_IndexBuild* b=&index->build;
 for(size_t i=0;i<b->count;++i){if(!(i%64) && callableStopped(cancelled,user)){why=XLS_CALLABLE_CANCELLED;goto done;}callable_row r;if(callableRow(b,i,&r))++count;}
 why=XLS_CALLABLE_LIMIT;if(count>(limit<sizeof(*result) ? 0 : (limit-sizeof(*result))/sizeof(*rows)))goto done;scratch=count*sizeof(*rows);
 if(count){rows=XLS_CALLABLE_ALLOC(scratch);why=XLS_CALLABLE_MEMORY;if(!rows)goto done;}
 size_t filled=0;
 for(size_t i=0;i<b->count;++i){if(!(i%64) && callableStopped(cancelled,user)){why=XLS_CALLABLE_CANCELLED;goto done;}callable_row r;if(callableRow(b,i,&r)){if(filled>=count){why=XLS_CALLABLE_INVALID;goto done;}rows[filled++]=r;}}
 if(filled!=count){why=XLS_CALLABLE_INVALID;goto done;}if(count)qsort(rows,count,sizeof(*rows),callableOrder);
 for(size_t i=1;i<count;++i){if(!(i%64) && callableStopped(cancelled,user)){why=XLS_CALLABLE_CANCELLED;goto done;}if(callableSameBucket(rows+i-1,rows+i)){
   if(!xlsNativeCallableRelated(rows[i-1].node,rows[i].node,rows[i].global)){why=XLS_CALLABLE_INVALID;goto done;}++relations;
 }}
 why=XLS_CALLABLE_LIMIT;if(scratch>limit || sizeof(*result)>limit-scratch || relations>(limit-scratch-sizeof(*result))/sizeof(xls_callable_relation))goto done;
 size_t bytes=sizeof(*result)+relations*sizeof(xls_callable_relation);result=XLS_CALLABLE_ALLOC(bytes);why=XLS_CALLABLE_MEMORY;if(!result)goto done;result->bytes=bytes;result->count=relations;size_t used=0;
 for(size_t i=1;i<count;++i){if(!(i%64) && callableStopped(cancelled,user)){why=XLS_CALLABLE_CANCELLED;goto done;}if(callableSameBucket(rows+i-1,rows+i))result->rows[used++]=(xls_callable_relation){rows[i-1].symbol,rows[i].symbol,rows[i].kind};}
 why=callableStopped(cancelled,user) ? XLS_CALLABLE_CANCELLED : XLS_CALLABLE_OK;
done:
 xrtFree(rows);if(why!=XLS_CALLABLE_OK){xlsCallableRelationsDestroy(result);result=NULL;}if(output)*output=why;return result;
}
