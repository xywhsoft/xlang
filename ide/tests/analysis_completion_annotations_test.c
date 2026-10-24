#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"analysis_completion_annotations_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int sameText(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int sameRange(XL6_AstRange a,XL6_AstRange b)
{ return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn; }
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    if(a->kind!=b->kind || a->flags!=b->flags || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) ||
       a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || a->line!=b->line || a->column!=b->column ||
       !sameRange(a->range,b->range) || !sameRange(a->tokenRange,b->tokenRange) || !sameText(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
       !!a->source!=!!b->source || (a->source && (a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)) || a->semanticTypeId!=b->semanticTypeId || a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticSymbolId!=b->semanticSymbolId ||
       a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
       a->semanticCompoundOperatorSymbolId!=b->semanticCompoundOperatorSymbolId || a->semanticCallKind!=b->semanticCallKind ||
       a->semanticLvalueKind!=b->semanticLvalueKind || a->semanticOwnership!=b->semanticOwnership || a->semanticExitKind!=b->semanticExitKind ||
       a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticTargetScopeDepth!=b->semanticTargetScopeDepth ||
       a->semanticRegionId!=b->semanticRegionId || a->semanticTargetRegionId!=b->semanticTargetRegionId || a->childCount!=b->childCount) return 0;
    if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!sameAst(a->children[i],b->children[i])) return 0;
    return 1;
}



typedef struct reader {xls_semantic_completion* result;const XL6_AstNode* expected;const char* json;int okay;} reader;
static int32 parallel(void* user)
{
 reader* r=user;r->okay=1;
 for(unsigned i=0;i<24;++i){
  if((i%3)==0 && !sameAst(xlsSemanticCompletionAst(r->result),r->expected))r->okay=0;
  char* json=xlsSemanticCompletionJson(r->result,NULL);if(!json || strcmp(json,r->json))r->okay=0;xlsSemanticStringFree(json);
  xls_completion_item item;if(!xlsSemanticCompletionItem(r->result,0,&item) || !item.hasSelection)r->okay=0;
 }
 xlsSemanticCompletionDestroy(r->result);return 0;
}
static int scenario(unsigned kind,size_t budget,int readers)
{
 char* path=xrtPathAbs("completion-annotations.xl");CHECK(path);
 const char* headers[]={"", "typedef Count = int\nclass Box\n int member\nend\nfunc count(Count value) as Count\n return value\nend\n",
  "func identity<T>(T value) as T\n return value\nend\n",
  "func guard() as int\n try\n string item = \"owned\"\n return 1\n catch e\n return 2\n finally\n string after = \"after\"\n end\nend\n"};
 xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView(headers[kind])) && xrtStrBufAppend(&text,xrtStrView("func stable(int value) as int\n if value>0 then\n return value\n end\n return 0\nend\nfunc main(...) as int\n int completionLocal = 7\n return completionL\nend\n")));
 xls_semantic_input input={path,text.Data,text.Size};size_t offset=(size_t)(strstr(text.Data,"return completionL")-text.Data)+strlen("return completionL");
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);xls_analysis_document doc={"entry",path,xlsTextCreate(text.Data,text.Size,8u*1024u*1024u,NULL),31,1};CHECK(session && doc.text);
 xls_sem_tail_control outer,inner;xlsFrontendSemTailBegin(&outer,0);xlsFrontendSemTailBegin(&inner,budget);
 xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);xlsFrontendSemTailEnd(&inner);
 CHECK(result && xlsSemanticCompletionInfo(result).available);
 xls_sem_tail_work before=xlsSemanticCompletionTailWork(result);CHECK(before.deferred==(budget>1) && !before.materialized && before.retainedBytes<=8u*1024u*1024u);
 xls_completion_session* other=xlsCompletionSessionCreate(NULL);CHECK(other);
 xls_semantic_completion* eager=xlsCompletionSessionQuery(other,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(eager && xlsSemanticCompletionInfo(eager).available && !xlsSemanticCompletionTailWork(eager).deferred);
 xlsSemanticCompletionDestroy(eager);xlsCompletionSessionDestroy(other);
 xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCompletionInfo(fresh).available);
 xls_sem_tail_work ordinary=xlsSemanticCompletionTailWork(fresh);CHECK(!ordinary.deferred && !ordinary.materialized && !ordinary.retainedBytes);
 char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));
 CHECK(xlsSemanticCompletionTailWork(result).materialized==0);
 /* End of inner scope restores zero budget without affecting the retained
  * result. Its AST remains lazy even after its session/context are gone. */
 xlsCompletionSessionDestroy(session);xlsTextRelease(doc.text);xrtStrBufFree(&text);xrtFree(path);
 if(readers){
  const XL6_AstNode* expected=xlsSemanticCompletionAst(fresh);CHECK(expected);reader values[4];xthread* threads[4];
  for(unsigned i=0;i<4;++i){values[i]=(reader){xlsSemanticCompletionRetain(result),expected,a,0};threads[i]=xrtThreadCreate(parallel,&values[i],0);CHECK(threads[i]);}
  for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && values[i].okay);xrtThreadDestroy(threads[i]);}
 }
 const XL6_AstNode* first=xlsSemanticCompletionAst(result);CHECK(first && sameAst(first,xlsSemanticCompletionAst(fresh)) && first==xlsSemanticCompletionAst(result));
 xls_sem_tail_work after=xlsSemanticCompletionTailWork(result);CHECK(after.deferred==before.deferred && after.materialized==before.deferred && after.retainedBytes<=before.retainedBytes);
 char* held=xlsSemanticCompletionJson(result,NULL);CHECK(held && !strcmp(held,a));xlsSemanticStringFree(held);
 xlsFrontendSemTailEnd(&outer);xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCompletionDestroy(fresh);xlsSemanticCompletionDestroy(result);return 1;
}
static int fallback(void)
{
 char* path=xrtPathAbs("completion-annotations-invalid.xl");CHECK(path);
 const char* text="func main(...) as int\n int value = 7\n return value.\nend\n";xls_analysis_document doc={"entry",path,xlsTextCreate(text,strlen(text),1024,NULL),11,1};xls_completion_session* s=xlsCompletionSessionCreate(NULL);CHECK(s && doc.text);
 size_t offset=(size_t)(strstr(text,"value.")-text)+strlen("value.");xls_semantic_completion* r=xlsCompletionSessionQuery(s,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(r && !xlsSemanticCompletionInfo(r).available && !xlsSemanticCompletionTailWork(r).deferred && xlsSemanticCompletionAst(r));
 xlsSemanticCompletionDestroy(r);xlsCompletionSessionDestroy(s);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);CHECK(!xlsSemanticCompletionAst(NULL) && !xlsSemanticCompletionTailWork(NULL).deferred);
 xlsFrontendSemTailMaterialize(NULL);xlsFrontendSemTailDestroy(NULL);CHECK(!xlsFrontendSemTailWork(NULL).deferred);
 size_t budgets[]={8u*1024u*1024u,0,1,SIZE_MAX};
 for(unsigned k=0;k<4;++k)for(unsigned b=0;b<4;++b)if(!scenario(k,budgets[b],b==0))return 1;
 if(!fallback())return 1;
 printf("analysis_completion_annotations_test passed: %d checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
