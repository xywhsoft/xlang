/* The actual generated checker supplies its symbols in this fixture.
 * Optional-value allocation refusal is test-local; production has no hook. */
#include "analysis/ast_clone.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static int headerFailure=-1,headerOperation,headerRefused;
static int headerDeny(void)
{if(headerFailure<0)return 0;if(headerOperation++!=headerFailure)return 0;++headerRefused;return 1;}
static XL6_AstNode* headerClone(const XL6_AstNode* n,const xls_ast_clone_options* o,size_t* b)
{return headerDeny() ? NULL : xlsAstClone(n,o,b);}
static bool headerInit(xarray* a,size_t s){return headerDeny() ? false : xrtArrayInit(a,s);}
static bool headerResize(xarray* a,size_t n){return headerDeny() ? false : xrtArrayResize(a,n);}
static void* headerCalloc(size_t n,size_t s){return headerDeny() ? NULL : xrtCalloc(n,s);}
#define xlsAstClone headerClone
#define xrtArrayInit headerInit
#define xrtArrayResize headerResize
#define xrtCalloc headerCalloc
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#include "xl6_sem.c"
#pragma GCC diagnostic pop
#undef xlsAstClone
#undef xrtArrayInit
#undef xrtArrayResize
#undef xrtCalloc
#define main semanticHeadersValuesBaseMain
#include "analysis_semantic_memo_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"analysis_semantic_headers_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)

static int keyEqual(const XL6_AstNode* a,const XL6_AstNode* b,int module,int wanted)
{
 xstrbuf x={0},y={0};CHECK(xlsSemKeyNode(&x,a,1,module) && xlsSemKeyNode(&y,b,1,module));
 CHECK(xlsSemEqualKey(&x,&y)==wanted);CHECK(xlsSemHeaderNodeEqual(a,b,1,module)==wanted);
 xrtStrBufFree(&x);xrtStrBufFree(&y);return 1;
}
static int headerFields(void)
{
 XL6_AstNode *a=xl6AstNewText(XL6_AST_FUNC,"header😀",10,NULL);CHECK(a);
 for(unsigned i=0;i<6;++i){XL6_AstNode* c=xl6AstNewText(i==0 ? XL6_AST_NAME : XL6_AST_TYPE,i==0 ? "target" : "int",i==0 ? 6 : 3,NULL);CHECK(c);xl6AstAddChild(a,c);}
 a->inferredType=xl6AstNewText(XL6_AST_TYPE,"vector<int>",11,NULL);
 a->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"list<int>",9,NULL);
 CHECK(a->childCount==6 && a->inferredType && a->semanticDeclaredType && xl6AstSetSourceTree(a,"D:/头😀/header.xl"));
 const unsigned char binary[]={0,1,0,255};CHECK(xl6AstSetLiteralData(a->children[2],binary,sizeof(binary)));
 size_t bytes=0;XL6_AstNode* b=xlsSemHeaderClone(a,1024u*1024u,&bytes);CHECK(b && bytes && keyEqual(a,b,1,1));
 CHECK(b->source!=a->source && b->text!=a->text && b->children[0]!=a->children[0]);
 /* Every scalar included by the existing canonical key,at a nested node so
  * changing kind cannot change the root-only body exclusion predicate. */
 XL6_AstNode* at=b->children[1];XL6_AstNode saved=*at;
#define MUTATE(field) do{at->field=saved.field+1;CHECK(keyEqual(a,b,1,0));*at=saved;}while(0)
 MUTATE(kind);MUTATE(flags);MUTATE(literalSize);MUTATE(literalScalar);MUTATE(literalDecoded);
 MUTATE(semanticTypeId);MUTATE(semanticTypeAbiId);MUTATE(semanticSymbolId);MUTATE(semanticDeclaredSymbolId);
 MUTATE(semanticTargetSymbolId);MUTATE(semanticCompoundOperatorSymbolId);MUTATE(semanticCallKind);MUTATE(semanticLvalueKind);
 MUTATE(semanticOwnership);MUTATE(semanticExitKind);MUTATE(semanticScopeDepth);MUTATE(semanticTargetScopeDepth);
 MUTATE(semanticRegionId);MUTATE(semanticTargetRegionId);
#undef MUTATE
 --b->childCount;CHECK(keyEqual(a,b,1,0));++b->childCount;
 char* old=at->text;at->text="";CHECK(keyEqual(a,b,1,0));at->text=NULL;CHECK(keyEqual(a,b,1,0));at->text=old;
 at->doc="";CHECK(keyEqual(a,b,1,0));at->doc=NULL;
 b->children[2]->literalData[1]^=1;CHECK(keyEqual(a,b,1,0));b->children[2]->literalData[1]^=1;
 old=b->source->path;b->source->path="d:/头😀/header.xl";CHECK(keyEqual(a,b,1,0));b->source->path=old;
 old=b->inferredType->text;b->inferredType->text="different";CHECK(keyEqual(a,b,1,0));b->inferredType->text=old;
 old=b->semanticDeclaredType->text;b->semanticDeclaredType->text="other";CHECK(keyEqual(a,b,1,0));b->semanticDeclaredType->text=old;
 ++at->line;++at->column;at->range.known=1;at->range.startOffset=999;at->tokenRange.endOffset=1000;
 CHECK(keyEqual(a,b,1,1));a->children[3]->flags=999;CHECK(keyEqual(a,b,1,1));
 CHECK(keyEqual(a,b,0,0)); /* Nested/non-module functions include bodies. */
 b->children[3]->flags=999;CHECK(keyEqual(a,b,0,0)); /* Placeholder has different kind/text. */
 xl6AstFree(b);CHECK(!xlsSemHeaderClone(a,1,&bytes));xl6AstFree(a);return 1;
}
typedef struct header_cancel {unsigned calls,at;} header_cancel;
static int headerCancel(void* user)
{header_cancel* t=user;return ++t->calls==t->at;}
static int headerCandidates(void)
{
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
 for(unsigned i=0;i<3;++i){XL6_AstNode* f=xl6AstNew(XL6_AST_FUNC,NULL);CHECK(f);
  for(unsigned j=0;j<4;++j){XL6_AstNode* n=xl6AstNewText(j==0 ? XL6_AST_NAME : XL6_AST_TYPE,j==0 ? "overloaded" : i==1 ? "string" : "int",j==0 ? 10 : i==1 ? 6 : 3,NULL);CHECK(n);xl6AstAddChild(f,n);}
  xl6AstAddChild(root,f);}
 XL6_SemContext ctx={.root=root};xls_sem_call_scope calls;xlsSemCallsBegin(&calls,&ctx);
 xstrbuf key={0};size_t count=0;CHECK(xlsSemReadHeaderKey(&ctx,"overloaded",&key,&count) && count==3);
 xls_sem_header_value value={0};CHECK(xlsSemHeaderValueInit(&value,&ctx,"overloaded",&key,count,1024u*1024u));
 CHECK(value.ready && value.roots.Count==3 && xlsSemHeaderValueEqual(&value,&ctx,"overloaded"));
 ++root->children[1]->children[2]->flags;CHECK(!xlsSemHeaderValueEqual(&value,&ctx,"overloaded"));--root->children[1]->children[2]->flags;
 CHECK(xlsSemHeaderValueEqual(&value,&ctx,"overloaded"));xlsSemHeaderValueUnit(&value);xlsSemHeaderValueUnit(&value);
 for(int fail=0;fail<8;++fail){headerFailure=fail;headerOperation=headerRefused=0;
  int okay=xlsSemHeaderValueInit(&value,&ctx,"overloaded",&key,count,1024u*1024u);
  headerFailure=-1;CHECK(headerRefused==1 && !okay && !value.ready && !value.roots.Count);xlsSemHeaderValueUnit(&value);}
 CHECK(xlsSemHeaderValueInit(&value,&ctx,"overloaded",&key,count,1024u*1024u));
 for(unsigned at=1;at<=4;++at){
  header_cancel a={.at=at},b={.at=at};xls_frontend_cancel_scope scope;
  xlsFrontendCancelBegin(&scope,headerCancel,&a);xstrbuf proof={0};size_t actual=0;
  int serialized=xlsSemReadHeaderKey(&ctx,"overloaded",&proof,&actual);xlsFrontendCancelEnd(&scope,NULL);xrtStrBufFree(&proof);
  xlsFrontendCancelBegin(&scope,headerCancel,&b);int fast=xlsSemHeaderValueEqual(&value,&ctx,"overloaded");xlsFrontendCancelEnd(&scope,NULL);
  CHECK(serialized==fast && a.calls==b.calls && a.calls==(at<4 ? at : 3));
 }
 xlsSemHeaderValueUnit(&value);
 xrtStrBufFree(&key);CHECK(xlsSemReadHeaderKey(&ctx,"missing",&key,&count) && count==0);
 CHECK(xlsSemHeaderValueInit(&value,&ctx,"missing",&key,count,1024));
 CHECK(value.ready && !value.roots.Count && xlsSemHeaderValueEqual(&value,&ctx,"missing"));
 CHECK(!xlsSemHeaderValueEqual(&value,&ctx,"overloaded"));xlsSemHeaderValueUnit(&value);xrtStrBufFree(&key);
 xlsSemCallsEnd(&calls);xl6AstFree(root);return 1;
}
static char* headerProgram(unsigned version)
{
 xstrbuf text={0};const char* arg=version>=2 ? "input" : "value";
 int okay=xrtStrBufAppendFormat(&text,"\xef\xbb\xbf// 中文😀\r\nfunc identity(%s %s) as int\r\n return %s\r\nend\r\n",version==5 ? "string" : "int",arg,version==5 ? "0" : arg);
 if(version>=3)okay=okay && xrtStrBufAppendFormat(&text,"func identity(string text,string pad=\"%s\\0😀\") as int\r\n return 0\r\nend\r\n",version>=4 ? "b" : "a");
 for(unsigned i=0;okay && i<32;++i){okay=xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local=identity(value)\r\n",i);
  for(unsigned j=0;okay && j<16;++j)okay=xrtStrBufAppend(&text,xrtStrView(" local=identity(local)+1\r\n"));
  okay=okay && xrtStrBufAppend(&text,xrtStrView(" return local\r\nend\r\n"));}
 okay=okay && xrtStrBufAppendFormat(&text,"func unused(int %s) as int\r\n return %s\r\nend\r\nfunc main(...) as int\r\n return work_0(7)\r\nend\r\n",version ? "other" : "value",version ? "other" : "value");
 char* body=okay ? xrtStrDup(text.Data) : NULL;xrtStrBufFree(&text);return body;
}
static int headerVariants(void)
{
 char* path=xrtPathAbs("semantic-headers-😀.xl");CHECK(path);
 xls_sem_read_work reference[7]={0};
 for(unsigned variant=0;variant<4;++variant){
  xls_sem_header_control outer,inner;xls_sem_header_work work={0};
  size_t budget=variant==1 ? 0 : variant==2 ? 1 : 1024u*1024u;
  xlsFrontendSemHeadersBegin(&outer,budget);xlsFrontendSemHeadersBegin(&inner,1024u*1024u);
  xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,0,0};
  for(unsigned version=0;version<7;++version){
   char* body=headerProgram(version);CHECK(body);xls_semantic_check* result=update(session,&doc,1,body);
   CHECK(result && xlsSemanticCheckInfo(result).accepted==(version!=5) && fresh(result,&doc,1));
   xls_sem_read_work w=xlsSemanticCheckReads(result);if(!variant)reference[version]=w;
   else CHECK(w.bytes==reference[version].bytes && w.recorded==reference[version].recorded && w.negative==reference[version].negative && w.matched==reference[version].matched && w.rejected==reference[version].rejected && w.reused==reference[version].reused);
   xlsSemanticCheckDestroy(result);xrtFree(body);
  }
  xlsCheckSessionDestroy(session);xlsTextRelease(doc.text);xlsFrontendSemHeadersEnd(&inner,&work);
  CHECK(work.bytes<=budget);
  if(budget>1)CHECK(work.captures && work.comparisons && work.matches && work.nodes);
  else CHECK(!work.captures && !work.comparisons);
  xls_sem_header_work total={0};xlsFrontendSemHeadersEnd(&outer,&total);
  CHECK(!memcmp(&work,&total,sizeof(work)));
  printf("headers variant=%u captures=%llu comparisons=%llu matches=%llu declines=%llu bytes=%llu\n",variant,(unsigned long long)work.captures,(unsigned long long)work.comparisons,(unsigned long long)work.matches,(unsigned long long)work.declines,(unsigned long long)work.bytes);
 }
 xrtFree(path);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);if(!headerFields() || !headerCandidates() || !headerVariants())return 1;
 printf("analysis_semantic_headers_test passed: %u checks;complete canonical scalar/tree/header/default/overload/negative/UTF8/binary facts,owned clones,allocation refusal,bounded/nested controls,original full AST/index/report/IR parity\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
