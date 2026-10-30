#include "analysis_test_clock.h"
#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_cursor_memo_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
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
{xls_text* text=xlsTextCreate(source,strlen(source),8u*1024u*1024u,NULL);if(!text)return 0;xlsTextRelease(doc->text);doc->text=text;++doc->revision;return 1;}
static size_t cursor(const char* source,const char* end)
{const char* at=strstr(source,end);size_t bom=!memcmp(source,"\xef\xbb\xbf",3)?3:0;return at ? (size_t)(at-source)+strlen(end)-bom:SIZE_MAX;}
static int parity(xls_semantic_completion* result,xls_context* ctx,const char* entry,const xls_analysis_document* docs,size_t count,const char* path,size_t at)
{
 xls_semantic_input inputs[4];for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(ctx,entry,path,at,inputs,count,NULL,NULL,NULL,0);CHECK(result && fresh);
 char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
 CHECK(sameAst(xlsSemanticCompletionAst(result),xlsSemanticCompletionAst(fresh)));xlsSemanticCompletionDestroy(fresh);return 1;
}
static int cancel(void* user){unsigned* calls=user;return ++*calls>=12;}
static int scenario(unsigned opened)
{
 char folder[180];snprintf(folder,sizeof(folder),"build/cursor-memo-%u-%llu-%llu",opened,(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());CHECK(xrtDirCreateAll(folder));
 char* directory=xrtPathAbs(folder);char *entry=xrtPathJoin(directory,"main.xl"),*left=xrtPathJoin(directory,"left.xl"),*right=xrtPathJoin(directory,"right.xl");CHECK(entry && left && right);
 const char* main="import left\nimport right\nfunc main(...) as int\n return left.compute(3)+right.compute(4)\nend\n";
 CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)main,strlen(main)}));
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="fixture-linked-native",.targetKind="exe"};
 xls_context* ctx=xlsContextCreate(&spec,1024*1024,NULL);xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(ctx && session);
 xls_analysis_document docs[3]={{.key="left",.path=left,.identity=11},{.key="right",.path=right,.identity=12},{.key="entry",.path=entry,.identity=13}};CHECK(set(&docs[2],main));
 xstrbuf stable={0};CHECK(xrtStrBufAppend(&stable,xrtStrView("func identity<T>(T v) as T\r\n return v\r\nend\r\n")));
 for(unsigned i=0;i<24;++i)CHECK(xrtStrBufAppendFormat(&stable,"func work_%u(int value) as int\r\n if value>0 then\r\n return identity::<int>(value)+%u\r\n end\r\n return 0\r\nend\r\n",i,i));
 CHECK(xrtStrBufAppend(&stable,xrtStrView("export func compute(int value) as int\r\n return identity::<int>(value)\r\nend\r\n")));CHECK(set(&docs[1],stable.Data));
 size_t count=opened?3:2;const char* target=opened?"entry":NULL;xls_semantic_completion* initial=NULL;size_t at=0;
 for(unsigned i=0;i<18;++i){
  const char* prefix=i%3==0?"mem":i%3==1?"memb":"membe";xstrbuf source={0};
  CHECK(xrtStrBufAppendFormat(&source,"\xef\xbb\xbf%sfunc sibling(int value) as int\r\n if value>0 then\r\n return value\r\n end\r\n return 0\r\nend\r\nexport func compute(int value) as int\r\n string 中文 = \"😀\"\r\n int member = %u\r\n return %s\r\nend\r\n",i%2?"// moved 中文😀\r\n":"",i,prefix));
  CHECK(set(&docs[0],source.Data));at=cursor(source.Data,i%3==0?"return mem":i%3==1?"return memb":"return membe");
  xls_semantic_completion* result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,NULL,NULL,NULL,0);CHECK(parity(result,ctx,entry,docs,count,left,at) && xlsSemanticCompletionInfo(result).available);
  xls_sem_work work=xlsSemanticCompletionSemWork(result);CHECK(work.bytes>0 && work.checked>=2);
  if(i) {
   CHECK(work.reused>0 && work.contextHits==1 && work.checked+work.reused==29);
  } else {CHECK(!work.reused);initial=xlsSemanticCompletionRetain(result);}
  CHECK(initial && xlsSemanticCompletionValidate(initial) && xlsSemanticCompletionInfo(initial).available);
  xlsSemanticCompletionDestroy(result);xrtStrBufFree(&source);
 }
 /* Same-body revisions are exact hits; reopening cannot borrow a semantic baseline. */
 ++docs[0].revision;xls_semantic_completion* result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,NULL,NULL,NULL,0);CHECK(result && xlsSemanticCompletionInfo(result).available);xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);CHECK(stats.hits==1);xlsSemanticCompletionDestroy(result);
 ++docs[1].identity;++docs[1].revision;result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,NULL,NULL,NULL,0);CHECK(parity(result,ctx,entry,docs,count,left,at) && !xlsSemanticCompletionSemWork(result).reused);xlsSemanticCompletionDestroy(result);
 /* Change the independent source, then prove new facts can seed later edits. */
 CHECK(set(&docs[1],"export func compute(int value) as int\n if value>0 then\n return value+900\n end\n return 0\nend\n"));++docs[0].revision;
 result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,NULL,NULL,NULL,0);CHECK(parity(result,ctx,entry,docs,count,left,at) && !xlsSemanticCompletionSemWork(result).reused);xlsSemanticCompletionDestroy(result);
 /* An unavailable projection and cancellation never replace the last good memo. */
 CHECK(set(&docs[0],"export func compute(int value) as int\n int member = 7\n return missing.mem\nend\n"));at=cursor(xlsTextData(docs[0].text),"missing.mem");unsigned calls=0;
 result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,cancel,&calls,NULL,0);CHECK(!result);
 result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,NULL,NULL,NULL,0);CHECK(result && parity(result,ctx,entry,docs,count,left,at) && !xlsSemanticCompletionInfo(result).available);xlsSemanticCompletionDestroy(result);
 CHECK(set(&docs[0],"export func compute(int value) as int\n int member = 7\n return mem\nend\n"));at=cursor(xlsTextData(docs[0].text),"return mem");
 result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"left",at,NULL,NULL,NULL,0);CHECK(parity(result,ctx,entry,docs,count,left,at) && xlsSemanticCompletionInfo(result).available);xlsSemanticCompletionDestroy(result);
 /* Change query source: previous query metadata must never move into right. */
 CHECK(set(&docs[0],"export func compute(int value) as int\n return value\nend\n"));CHECK(set(&docs[1],"export func compute(int value) as int\n int member = value\n return mem\nend\n"));at=cursor(xlsTextData(docs[1].text),"return mem");
 result=xlsCompletionSessionQueryInContext(session,ctx,docs,count,target,"right",at,NULL,NULL,NULL,0);CHECK(parity(result,ctx,entry,docs,count,right,at) && !xlsSemanticCompletionSemWork(result).reused);xlsSemanticCompletionDestroy(result);
 xlsCompletionSessionDestroy(session);CHECK(initial && xlsSemanticCompletionValidate(initial) && xlsSemanticCompletionInfo(initial).available);char* retained=xlsSemanticCompletionJson(initial,NULL);CHECK(retained);xlsSemanticStringFree(retained);xlsSemanticCompletionDestroy(initial);
 for(unsigned i=0;i<3;++i) xlsTextRelease(docs[i].text);
 xlsContextRelease(ctx);xrtStrBufFree(&stable);xrtFree(directory);xrtFree(entry);xrtFree(left);xrtFree(right);return 1;
}
int main(void){if(!scenario(0)||!scenario(1))return 1;printf("analysis_cursor_memo_test passed: %u checks\n",checks);return 0;}
