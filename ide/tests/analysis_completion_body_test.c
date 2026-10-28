#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_completion_body_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
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


static int set(xls_analysis_document* doc,const char* source)
{
 xls_text* text=xlsTextCreate(source,strlen(source),4u*1024u*1024u,NULL);if(!text)return 0;
 xlsTextRelease(doc->text);doc->text=text;++doc->revision;return 1;
}
typedef struct body_reader {xls_semantic_completion* result;const XL6_AstNode* expected;int okay;} body_reader;
static int32 readBody(void* user)
{
 body_reader* r=user;r->okay=1;
 for(unsigned i=0;i<12;++i){if(!sameAst(xlsSemanticCompletionAst(r->result),r->expected))r->okay=0;
  char* json=xlsSemanticCompletionJson(r->result,NULL);if(!json)r->okay=0;xlsSemanticStringFree(json);
 }
 xlsSemanticCompletionDestroy(r->result);return 0;
}
static int scenario(int imported)
{
 char directory[160];snprintf(directory,sizeof(directory),"build/body-views-%d-%llu",imported,(unsigned long long)xrtClock());CHECK(xrtDirCreateAll(directory));
 char *root=xrtPathAbs(directory),*entry=xrtPathJoin(root,"main.xl"),*query=xrtPathJoin(root,imported ? "query.xl" : "main.xl"),*stable=xrtPathJoin(root,"stable.xl");CHECK(root && entry && query && stable);
 xls_completion_session *session=xlsCompletionSessionCreate(NULL),*eager=xlsCompletionSessionCreate(NULL);CHECK(session && eager);
 xls_analysis_document docs[3]={{.key="entry",.path=entry,.identity=1},{.key="query",.path=query,.identity=2},{.key="stable",.path=stable,.identity=3}};
 xstrbuf helpers={0};CHECK(xrtStrBufAppend(&helpers,xrtStrView("func identity<T>(T value) as T\r\n return value\r\nend\r\n")));
 for(unsigned i=0;i<32;++i)CHECK(xrtStrBufAppendFormat(&helpers,"%sfunc work_%u(int value) as int\r\n string owned = \"中文😀\"\r\n if value>0 then\r\n return identity::<int>(value)+%u\r\n end\r\n return 0\r\nend\r\n",imported ? "export " : "",i,i));
 size_t count=imported ? 3 : 1,index=imported ? 1 : 0;const char* key=imported ? "query" : "entry";
 if(imported){CHECK(set(&docs[0],"import query\r\nfunc main(...) as int\r\n return query.compute(7)\r\nend\r\n") && set(&docs[2],helpers.Data));}
 xls_semantic_completion *held[8]={0},*fresh[8]={0};uint64_t deferred=0;
 for(unsigned generation=0;generation<8;++generation){
  xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,"\xef\xbb\xbf%s%s%s\r\n int completionLocal=%swork_0(7)\r\n",generation%2 ? "// moved 中文😀\r\n" : "",imported ? "import stable\r\n" : "",imported ? "export func compute(int value) as int" : "func main(...) as int",imported ? "stable." : ""));
  for(unsigned j=0;j<generation%3;++j)CHECK(xrtStrBufAppendFormat(&text," int added_%u=%u\r\n",j,j));
  const char* prefix=generation%2 ? "completionLo" : "completionL";CHECK(xrtStrBufAppendFormat(&text," return %s\r\nend\r\n",prefix));
  if(!imported)CHECK(xrtStrBufAppend(&text,(xstrview){helpers.Data,helpers.Size}));
  CHECK(set(&docs[index],text.Data));size_t offset=(size_t)(strstr(text.Data," return completion")-text.Data)+strlen(" return ")+strlen(prefix)-3;
  held[generation]=xlsCompletionSessionQuery(session,docs,count,"entry",key,offset,NULL,NULL,NULL,0);CHECK(held[generation] && xlsSemanticCompletionInfo(held[generation]).available);
  xls_sem_body_control zero;xlsFrontendSemBodyBegin(&zero,0);
  xls_semantic_completion* reference=xlsCompletionSessionQuery(eager,docs,count,"entry",key,offset,NULL,NULL,NULL,0);xlsFrontendSemBodyEnd(&zero);
  CHECK(reference && xlsSemanticCompletionInfo(reference).available && !xlsSemanticCompletionTailWork(reference).bodyFunctions);
  xls_semantic_input inputs[3];for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
  fresh[generation]=xlsSemanticCompletionCreate(entry,query,offset,inputs,count,NULL,NULL,NULL,0);CHECK(fresh[generation]);
  char *a=xlsSemanticCompletionJson(held[generation],NULL),*b=xlsSemanticCompletionJson(reference,NULL),*c=xlsSemanticCompletionJson(fresh[generation],NULL);CHECK(a && b && c && !strcmp(a,b) && !strcmp(a,c));xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticStringFree(c);
  xls_sem_tail_work work=xlsSemanticCompletionTailWork(held[generation]);CHECK(!work.bodyMaterialized && !work.bodyFailures && !work.bodyRetries && work.bodyBytes<=64u*1024u*1024u);
  if(generation){CHECK(work.bodyFunctions>=24 && work.bodyBytes>0 && xlsSemanticCompletionStorageWork(held[generation]).nodes<xlsSemanticCompletionStorageWork(reference).nodes);deferred+=work.bodyFunctions;}
  else CHECK(!work.bodyFunctions);
  xlsSemanticCompletionDestroy(reference);xrtStrBufFree(&text);
 }
 xlsCompletionSessionDestroy(session);xlsCompletionSessionDestroy(eager);
 for(size_t i=0;i<count;++i)xlsTextRelease(docs[i].text);
 xrtStrBufFree(&helpers);xrtFree(root);xrtFree(entry);xrtFree(query);xrtFree(stable);
 /* None of these held generations has exposed its body AST. Their units,
  * mapped coordinates and cursor sources outlive the session and all text. */
 const XL6_AstNode* expected=xlsSemanticCompletionAst(fresh[7]);CHECK(expected);
 body_reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(body_reader){xlsSemanticCompletionRetain(held[7]),expected,0};threads[i]=xrtThreadCreate(readBody,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 for(unsigned i=0;i<8;++i){
  CHECK(sameAst(xlsSemanticCompletionAst(held[i]),xlsSemanticCompletionAst(fresh[i])));
  xls_sem_tail_work work=xlsSemanticCompletionTailWork(held[i]);CHECK(work.materialized && work.bodyMaterialized==work.bodyFunctions && !work.bodyBytes);
  xlsSemanticCompletionDestroy(held[i]);xlsSemanticCompletionDestroy(fresh[i]);
 }
 printf("body views imported=%d independently retained deferred functions=%llu\n",imported,(unsigned long long)deferred);return 1;
}
int main(void)
{
 if(!scenario(0) || !scenario(1))return 1;
 printf("analysis_completion_body_test passed: %u checks;complete query/source/AST parity,eager control,relocated symbols/positions,unmaterialized generations,independent lifetime and parallel AST readers\n",checks);return 0;
}

/* Exercise the original semantic-fact body-view fallback deliberately. The
 * separate raw-native fixture tests the default compiler-copy producer.
 * No language/parity/failure assertion below is relaxed. */
#include "analysis/frontend_raw_body.h"
int __wrap_xlsRawBodyMemoAdmit(const void* memo,xls_raw_unit* unit,const char* path,const XL6_AstRange* range,size_t* bytes)
{(void)memo;(void)unit;(void)path;(void)range;(void)bytes;return 0;}
