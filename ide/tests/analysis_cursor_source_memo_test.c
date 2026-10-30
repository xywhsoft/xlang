#include "analysis_test_clock.h"
#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"analysis_cursor_source_memo_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
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
static size_t at(const char* source,const char* text,size_t prefix)
{const char* found=strstr(source,text);size_t bom=!memcmp(source,"\xef\xbb\xbf",3)?3:0;return found?(size_t)(found-source)+prefix-bom:SIZE_MAX;}
static int parity(xls_semantic_completion* result,xls_context* context,const char* entry,const xls_analysis_document* docs,size_t count,const char* path,size_t offset)
{
 xls_semantic_input inputs[3];for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(context,entry,path,offset,inputs,count,NULL,NULL,NULL,0);CHECK(result && fresh);
 char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));CHECK(sameAst(xlsSemanticCompletionAst(result),xlsSemanticCompletionAst(fresh)));
 xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCompletionDestroy(fresh);return 1;
}
static int prefix(xstrbuf* text,unsigned count)
{
 CHECK(xrtStrBufAppend(text,xrtStrView("\xef\xbb\xbf" "func identity(int value) as int\r\n return value\r\nend\r\n")));
 for(unsigned i=0;i<count;++i){
  CHECK(xrtStrBufAppendFormat(text,"func helper_%u(int value) as int\r\n int local = value+%u\r\n if local>0 then\r\n",i,i));
  for(unsigned j=0;j<3;++j)CHECK(xrtStrBufAppend(text,xrtStrView(" local = identity(local)+1\r\n")));
  CHECK(xrtStrBufAppend(text,xrtStrView(" end\r\n return local\r\nend\r\n")));
 }
 return 1;
}
static int scenario(unsigned mode)
{
 const unsigned helpers=32;char folder[180];snprintf(folder,sizeof(folder),"build/source-memo-%u-%llu-%llu",mode,(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());CHECK(xrtDirCreateAll(folder));
 char* directory=xrtPathAbs(folder);char *entry=xrtPathJoin(directory,"main.xl"),*left=xrtPathJoin(directory,"left.xl"),*right=xrtPathJoin(directory,"right.xl");CHECK(directory && entry && left && right);
 const char* main="import left\nimport right\nfunc main(...) as int\n return left.compute(3)+right.compute(4)\nend\n";CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)main,strlen(main)}));
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="fixture-linked-native",.targetKind="exe"};xls_context* context=mode==2?NULL:xlsContextCreate(&spec,1024*1024,NULL);CHECK(mode==2 || context);
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document docs[3]={{.key="left",.path=left,.identity=11},{.key="right",.path=right,.identity=12},{.key="entry",.path=entry,.identity=13}};CHECK(set(&docs[2],main));
 size_t count=mode==2?1:mode==1?3:2;const char *target=mode==2?"left":mode==1?"entry":NULL,*actualEntry=mode==2?left:entry;
 xstrbuf start={0},other={0};CHECK(prefix(&start,helpers) && prefix(&other,helpers));CHECK(xrtStrBufAppend(&other,xrtStrView("export func compute(int value) as int\r\n return identity(value)\r\nend\r\n")) && set(&docs[1],other.Data));xls_semantic_completion* initial=NULL;char* initialJson=NULL;size_t offset=0;
 for(unsigned i=0;i<24;++i){
  xstrbuf source={0};CHECK(xrtStrBufAppend(&source,(xstrview){start.Data,start.Size}));CHECK(xrtStrBufAppend(&source,xrtStrView(mode==2?"func main(...) as int\r\n":"export func compute(int value) as int\r\n")));
  for(unsigned j=0;j<i%4;++j)CHECK(xrtStrBufAppend(&source,xrtStrView(" // 中文😀 marker moves\r\n")));
  CHECK(xrtStrBufAppendFormat(&source," string 中文 = \"😀-%u\"\r\n int completionLocal = %u\r\n return completionLocal\r\nend\r\n",i,i));CHECK(set(&docs[0],source.Data));offset=at(source.Data,"return completionLocal",strlen("return completionL")+i%3);
  xls_semantic_completion* result=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(result,context,actualEntry,docs,count,left,offset) && xlsSemanticCompletionInfo(result).available);
  xls_sem_work work=xlsSemanticCompletionSemWork(result);CHECK(work.bytes>0 && work.checked>=1);
  if(i){CHECK(work.contextHits==1 && work.reused>=helpers*(mode==2?1:2));CHECK(work.checked+work.reused==(mode==2?helpers+2:2*helpers+5));}
  else {initial=xlsSemanticCompletionRetain(result);initialJson=xlsSemanticCompletionJson(initial,NULL);CHECK(initialJson);}
  char* retained=xlsSemanticCompletionJson(initial,NULL);CHECK(retained && !strcmp(retained,initialJson));xlsSemanticStringFree(retained);xlsSemanticCompletionDestroy(result);xrtStrBufFree(&source);
 }
 /* An early sibling gains/removes declarations while the projected query
  * stays in the trailing function. Both opened and closed-entry routes must
  * bind current local IDs and preserve complete fresh marker/type facts. */
 for(unsigned i=0;i<8;++i){
  const char* base=start.Data;const char* point=strstr(base,"func helper_0(");CHECK(point);point=strstr(point," if local>0 then\r\n");CHECK(point);
  xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,(xstrview){base,(size_t)(point-base)}));
  for(unsigned j=0;j<1+i%3;++j)CHECK(xrtStrBufAppendFormat(&changed," int inserted_%u = local+%u\r\n",j,j));
  /* Every source starts from the original fixed helper prefix;no cumulative
   * additions or numeric offset chains are used by this test. */
  const char* tail=strstr(start.Data,"func helper_0(");CHECK(tail);tail=strstr(tail," if local>0 then\r\n");CHECK(tail);
  CHECK(xrtStrBufAppend(&changed,xrtStrView(tail)) && xrtStrBufAppend(&changed,xrtStrView(mode==2?"func main(...) as int\r\n":"export func compute(int value) as int\r\n")) &&
   xrtStrBufAppend(&changed,xrtStrView(" int completionLocal = 7\r\n return completionLocal\r\nend\r\n")) && set(&docs[0],changed.Data));
  offset=at(changed.Data,"return completionLocal",strlen("return completionLoc"));
  xls_semantic_completion* r=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(r,context,actualEntry,docs,count,left,offset));
  if(i)CHECK(xlsSemanticCompletionSemWork(r).reused>=helpers-1);
  xlsSemanticCompletionDestroy(r);xrtStrBufFree(&changed);
 }
 /* A previously memoized sibling becomes the active query; original source
  * is valid without the projection, so later functions still check normally. */
 const char* text=xlsTextData(docs[0].text),*helper=strstr(text,"func helper_7(");CHECK(helper);const char* selected=strstr(helper,"return local");CHECK(selected);offset=(size_t)(selected-text)+strlen("return loc")-3;++docs[0].revision;
 xls_semantic_completion* result=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(result,context,actualEntry,docs,count,left,offset) && xlsSemanticCompletionInfo(result).available);CHECK(xlsSemanticCompletionSemWork(result).checked>=1);xlsSemanticCompletionDestroy(result);
 /* Moving back reuses older immutable units with their ORIGINAL marker stamp,
  * not the immediately preceding generation's marker metadata. */
 offset=at(text,"return completionLocal",strlen("return completionLoc"));++docs[0].revision;result=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(result,context,actualEntry,docs,count,left,offset) && xlsSemanticCompletionInfo(result).available);xlsSemanticCompletionDestroy(result);
 /* A wide Unicode insertion relocates all siblings. Unlocated synthetic type
  * coordinates may safely decline reuse; complete fresh parity is decisive. */
 xstrbuf moved={0};CHECK(xrtStrBufAppend(&moved,xrtStrView("\xef\xbb\xbf// before all 中文😀\r\n")) && xrtStrBufAppend(&moved,xrtStrView(text+3)) && set(&docs[0],moved.Data));offset=at(moved.Data,"return completionLocal",strlen("return completionLoc"));result=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(result,context,actualEntry,docs,count,left,offset) && xlsSemanticCompletionInfo(result).available);xlsSemanticCompletionDestroy(result);xrtStrBufFree(&moved);
 ++docs[0].identity;docs[0].revision=1;result=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(result,context,actualEntry,docs,count,left,offset) && !xlsSemanticCompletionSemWork(result).reused);xlsSemanticCompletionDestroy(result);
 /* Dot projection has an empty replacement range; bind successive point
  * markers without treating them as disjoint from their active function. */
 for(unsigned i=0;i<3;++i){
  xstrbuf dot={0};CHECK(xrtStrBufAppend(&dot,(xstrview){start.Data,start.Size}) && xrtStrBufAppend(&dot,xrtStrView("export class Box\r\n int member\r\nend\r\n")) && xrtStrBufAppend(&dot,xrtStrView(mode==2?"func main(...) as int\r\n":"export func compute(int value) as int\r\n")));
  for(unsigned j=0;j<i;++j)CHECK(xrtStrBufAppend(&dot,xrtStrView(" // point moves 中文😀\r\n")));
  CHECK(xrtStrBufAppend(&dot,xrtStrView(" auto box = Box()\r\n return box.\r\nend\r\n")) && set(&docs[0],dot.Data));offset=at(dot.Data,"return box.",strlen("return box."));
  result=xlsCompletionSessionQueryInContext(session,context,docs,count,target,"left",offset,NULL,NULL,NULL,0);CHECK(parity(result,context,actualEntry,docs,count,left,offset) && xlsSemanticCompletionInfo(result).available);
  xls_completion_info info=xlsSemanticCompletionInfo(result);CHECK(info.start==info.end && xlsSemanticCompletionSemWork(result).checked>=1);
  if(i)CHECK(xlsSemanticCompletionSemWork(result).reused>=helpers*(mode==2?1:2));
  xlsSemanticCompletionDestroy(result);xrtStrBufFree(&dot);
 }
 xlsCompletionSessionDestroy(session);char* held=xlsSemanticCompletionJson(initial,NULL);CHECK(held && !strcmp(held,initialJson));xlsSemanticStringFree(held);xlsSemanticStringFree(initialJson);xlsSemanticCompletionDestroy(initial);
 for(unsigned i=0;i<3;++i)xlsTextRelease(docs[i].text);
 xlsContextRelease(context);xrtStrBufFree(&start);xrtStrBufFree(&other);xrtFree(directory);xrtFree(entry);xrtFree(left);xrtFree(right);return 1;
}
int main(void){xrtAtomic32Init(&checks,0);if(!scenario(0)||!scenario(1)||!scenario(2))return 1;printf("analysis_cursor_source_memo_test passed: %d checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
