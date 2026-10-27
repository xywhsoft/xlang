/* An ephemeral, independently owned value of the exact original header key.
 * All overloads, default expressions and live semantic facts are retained.
 * No AST/context pointer from the mutable original module escapes the call. */
#include "analysis/ast_clone.h"
typedef struct xls_sem_header_value {xarray roots;size_t bytes;int ready;} xls_sem_header_value;
static _Thread_local xls_sem_header_control* xlsActiveHeaderControl;
static size_t xlsSemHeaderBudget(void)
{return xlsActiveHeaderControl ? xlsActiveHeaderControl->budget : 1024u*1024u;}
static void xlsSemHeaderWorkAdd(xls_sem_header_work* out,const xls_sem_header_work* in)
{
 out->captures+=in->captures;out->comparisons+=in->comparisons;out->matches+=in->matches;
 out->declines+=in->declines;out->nodes+=in->nodes;if(in->bytes>out->bytes)out->bytes=in->bytes;
}
void xlsFrontendSemHeadersBegin(xls_sem_header_control* c,size_t bytes)
{
 if(c){size_t limit=xlsSemHeaderBudget();*c=(xls_sem_header_control){.previous=xlsActiveHeaderControl,.budget=bytes<limit ? bytes : limit};xlsActiveHeaderControl=c;}
}
void xlsFrontendSemHeadersEnd(xls_sem_header_control* c,xls_sem_header_work* work)
{
 if(!c || xlsActiveHeaderControl!=c)return;
 xlsActiveHeaderControl=c->previous;if(work)*work=c->work;
 if(c->previous)xlsSemHeaderWorkAdd(&c->previous->work,&c->work);
}
static void xlsSemHeaderValueUnit(xls_sem_header_value* value)
{
 if(!value)return;
 for(size_t i=0;i<value->roots.Count;++i)xl6AstFree(*(XL6_AstNode**)xrtArrayGet(&value->roots,i));
 xrtArrayUnit(&value->roots);*value=(xls_sem_header_value){0};
}
static int xlsSemHeaderNodeEqual(const XL6_AstNode* a,const XL6_AstNode* b,size_t depth,int module)
{
 if(depth>512)return 0;
 if(!a || !b)return a==b;
 if(xlsActiveHeaderControl)++xlsActiveHeaderControl->work.nodes;
 uint64_t x[XLS_SEM_KEY_FIELDS],y[XLS_SEM_KEY_FIELDS];xlsSemKeyScalars(a,x);xlsSemKeyScalars(b,y);
 if(memcmp(x,y,sizeof(x)) || !xlsSemReadValueText(a->text,b->text) || !xlsSemReadValueText(a->doc,b->doc) ||
    !xlsSemReadValueText(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
    (a->literalData!=NULL)!=(b->literalData!=NULL) ||
    (a->literalData && memcmp(a->literalData,b->literalData,a->literalSize)) ||
    !xlsSemHeaderNodeEqual(a->inferredType,b->inferredType,depth+1,0) ||
    !xlsSemHeaderNodeEqual(a->semanticDeclaredType,b->semanticDeclaredType,depth+1,0))return 0;
 for(size_t i=0;i<a->childCount;++i){
  if(module && depth==1 && xlsSemFunction(a) && i==3)continue;
  if(!xlsSemHeaderNodeEqual(a->children[i],b->children[i],depth+1,module))return 0;
 }
 return 1;
}
static XL6_AstNode* xlsSemHeaderClone(const XL6_AstNode* node,size_t limit,size_t* bytes)
{
 if(!node || !limit)return NULL;
 XL6_AstNode filtered=*node,dummy={0};XL6_AstNode** children=NULL;
 size_t temporary=0;
 if(xlsSemFunction(node) && node->childCount>3){
  if(node->childCount>limit/sizeof(*children))return NULL;
  temporary=node->childCount*sizeof(*children);if(temporary>=limit)return NULL;
  children=xrtCalloc(node->childCount,sizeof(*children));if(!children)return NULL;
  memcpy(children,node->children,temporary);children[3]=&dummy;filtered.children=children;
 }
 /* The clone API requires non-null child nodes. A private empty placeholder
  * preserves child count while the unchanged key omits exactly root body 3.
  * Nulls elsewhere decline this optional path and retain serialized proof. */
 xls_ast_clone_options options={.bytes=limit-temporary};
 XL6_AstNode* owned=xlsAstClone(&filtered,&options,bytes);xrtFree(children);return owned;
}
static int xlsSemHeaderValueInit(xls_sem_header_value*,XL6_SemContext*,const char*,const xstrbuf*,size_t,size_t);
static int xlsSemHeaderValueEqual(const xls_sem_header_value*,XL6_SemContext*,const char*);
