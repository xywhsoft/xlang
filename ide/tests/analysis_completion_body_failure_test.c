/* Compile the actual generated frontend privately with one-shot XRT
 * operation refusals. Product code has no fault hook. The ordinary core's
 * semantic TU is not pulled from the archive: this fixture supplies it. */
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"
#include "analysis/signature_internal.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static _Thread_local size_t failRawSize;
static _Thread_local unsigned failRawNth,failTailOnce;
static int denyTail(size_t count,size_t size);
static bool bodyArrayInit(xarray* array,size_t size)
{if(size==failRawSize && failRawNth && !--failRawNth)return false;return xrtArrayInit(array,size);}
static void* bodyCalloc(size_t count,size_t size)
{if(denyTail(count,size)){failRawNth=1;return NULL;}return xrtCalloc(count,size);}
#define xrtArrayInit bodyArrayInit
#define xrtCalloc bodyCalloc
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
#undef xrtArrayInit
#undef xrtCalloc
static int denyTail(size_t count,size_t size)
{
 if(failTailOnce && count==1 && size==sizeof(xls_sem_tail) && xlsActiveSemBuild && xlsActiveSemBuild->bodyJobs.Count){--failTailOnce;return 1;}
 return 0;
}
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_completion_body_failure_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
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


static int setSource(xls_analysis_document* doc,unsigned generation,int signature,size_t* offset)
{
 xstrbuf text={0};
 for(unsigned i=0;i<16;++i)CHECK(xrtStrBufAppendFormat(&text,"func stable_%u(int value) as int\n if value>0 then\n return value+%u\n end\n return 0\nend\n",i,i));
 CHECK(xrtStrBufAppend(&text,xrtStrView("func choose(int left,string right) as int\n return left\nend\nfunc choose(int left,int right) as int\n return right\nend\nfunc main(...) as int\n")));
 if(signature){CHECK(xrtStrBufAppendFormat(&text," return choose(stable_0(7),\"%u\")\nend\n",generation));*offset=(size_t)(strrchr(text.Data,'"')-text.Data);}
 else{CHECK(xrtStrBufAppendFormat(&text," int completionLocal=stable_0(%u)\n return completionL\nend\n",generation));*offset=(size_t)(strstr(text.Data,"return completionL")-text.Data)+strlen("return completionL");}
 xls_text* next=xlsTextCreate(text.Data,text.Size,1024*1024,NULL);CHECK(next);xlsTextRelease(doc->text);doc->text=next;++doc->revision;xrtStrBufFree(&text);return 1;
}
static int completionParity(xls_semantic_completion* result,const xls_analysis_document* doc,size_t offset)
{
 xls_semantic_input in={doc->path,xlsTextData(doc->text),xlsTextSize(doc->text)};
 xls_semantic_completion* fresh=xlsSemanticCompletionCreate(doc->path,doc->path,offset,&in,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCompletionInfo(fresh).available);
 char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
 CHECK(sameAst(xlsSemanticCompletionAst(result),xlsSemanticCompletionAst(fresh)));xlsSemanticCompletionDestroy(fresh);return 1;
}
static int completionFailures(void)
{
 char* path=xrtPathAbs("completion-body-failures.xl");CHECK(path);xls_analysis_document doc={.key="entry",.path=path,.identity=1};
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);size_t offset=0;
 CHECK(setSource(&doc,0,0,&offset));xls_semantic_completion* initial=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(initial && xlsSemanticCompletionInfo(initial).available);xlsSemanticCompletionDestroy(initial);
 CHECK(setSource(&doc,1,0,&offset));xls_semantic_completion* first=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(first && xlsSemanticCompletionTailWork(first).bodyFunctions>=16);
 char* before=xlsSemanticCompletionJson(first,NULL);CHECK(before);failRawNth=1;
 CHECK(!xlsSemanticCompletionAst(first) && !failRawNth && xlsSemanticCompletionInfo(first).available);
 xls_sem_tail_work w=xlsSemanticCompletionTailWork(first);CHECK(w.bodyFailures==1 && !w.bodyMaterialized && !w.materialized);
 char* after=xlsSemanticCompletionJson(first,NULL);CHECK(after && !strcmp(before,after));xlsSemanticStringFree(after);xlsSemanticStringFree(before);
 CHECK(completionParity(first,&doc,offset));w=xlsSemanticCompletionTailWork(first);CHECK(w.bodyFailures==1 && w.bodyMaterialized==w.bodyFunctions && w.materialized);
 CHECK(setSource(&doc,2,0,&offset));xls_semantic_completion* partial=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(partial && xlsSemanticCompletionTailWork(partial).bodyFunctions>=16);
 failRawNth=2;CHECK(!xlsSemanticCompletionAst(partial) && !failRawNth);w=xlsSemanticCompletionTailWork(partial);CHECK(w.bodyMaterialized==1 && w.bodyFailures==1 && !w.materialized);
 /* A later query may read this immutable memo while its prior AST remains
  * partially materialized. Destruction needs neither a retry nor old owner. */
 CHECK(setSource(&doc,3,0,&offset));xls_semantic_completion* next=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(next && xlsSemanticCompletionInfo(next).available && completionParity(next,&doc,offset));xlsSemanticCompletionDestroy(partial);xlsSemanticCompletionDestroy(next);
 CHECK(setSource(&doc,4,0,&offset));failTailOnce=1;
 xls_semantic_completion* fallback=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(fallback && !failTailOnce && !failRawNth && xlsSemanticCompletionInfo(fallback).available);
 w=xlsSemanticCompletionTailWork(fallback);CHECK(w.bodyRetries==1 && !w.deferred && !w.bodyFunctions && completionParity(fallback,&doc,offset));xlsSemanticCompletionDestroy(fallback);
 xlsCompletionSessionDestroy(session);CHECK(xlsSemanticCompletionAst(first));xlsSemanticCompletionDestroy(first);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
static int sameSignatures(xls_signature_help* result,xls_signature_help* fresh)
{
 xls_signature_info a=xlsSignatureInfo(result),b=xlsSignatureInfo(fresh);CHECK(a.available && b.available && a.count==b.count && a.activeSignature==b.activeSignature && a.argument==b.argument && a.offset==b.offset);
 for(size_t i=0;i<a.count;++i){xls_signature_item x,y;CHECK(xlsSignatureItem(result,i,&x) && xlsSignatureItem(fresh,i,&y) && sameText(x.label,y.label) && sameText(x.documentation,y.documentation) && x.parameters==y.parameters && x.activeParameter==y.activeParameter && x.symbol==y.symbol);
  for(size_t j=0;j<x.parameters;++j){xls_signature_parameter p,q;CHECK(xlsSignatureParameter(result,i,j,&p) && xlsSignatureParameter(fresh,i,j,&q) && sameText(p.name,q.name) && p.labelStart==q.labelStart && p.labelEnd==q.labelEnd && p.optional==q.optional && p.variadic==q.variadic);}
 }
 char *left=xlsSignatureInputJson(result,NULL),*right=xlsSignatureInputJson(fresh,NULL);CHECK(left && right && !strcmp(left,right));xlsSemanticStringFree(left);xlsSemanticStringFree(right);
 CHECK(sameAst(xlsSemanticCompletionAst(xlsSignatureSemantic(result)),xlsSemanticCompletionAst(xlsSignatureSemantic(fresh))));return 1;
}
static int signatureFailure(void)
{
 char* path=xrtPathAbs("signature-body-failures.xl");CHECK(path);xls_analysis_document doc={.key="entry",.path=path,.identity=7};xls_signature_session* session=xlsSignatureSessionCreate(NULL);CHECK(session);size_t offset=0;
 CHECK(setSource(&doc,0,1,&offset));xls_signature_help* first=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(first && xlsSignatureInfo(first).available);xlsSignatureRelease(first);
 CHECK(setSource(&doc,1,1,&offset));failTailOnce=1;
 xls_signature_help* result=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(result && !failTailOnce && !failRawNth && xlsSignatureInfo(result).available);
 CHECK(xlsSemanticCompletionTailWork(xlsSignatureSemantic(result)).bodyRetries==1);
 xls_semantic_input in={path,xlsTextData(doc.text),xlsTextSize(doc.text)};xls_signature_help* fresh=xlsSignatureCreateInContext(NULL,path,path,offset,&in,1,NULL,NULL,NULL,0);CHECK(fresh && sameSignatures(result,fresh));
 xlsSignatureSessionDestroy(session);xlsTextRelease(doc.text);CHECK(xlsSignatureInfo(result).available);xlsSignatureRelease(result);xlsSignatureRelease(fresh);xrtFree(path);return 1;
}
int main(void)
{
 failRawSize=sizeof(xls_sem_raw);
 if(!completionFailures() || !signatureFailure())return 1;
 printf("analysis_completion_body_failure_test passed: %u checks;real generated frontend,one-shot container refusals,atomic/delayed/partial AST retry,independent memo lifetime,and original-route completion/signature fallback\n",checks);return 0;
}
