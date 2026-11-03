#include "analysis/check_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#ifndef XLS_UNIFIED_CASES_DIRECTORY
#error Unified frontend fixture requires the captured compiler corpus
#endif
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_unified_semantic_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static const struct fixture {const char* name;int accepted;} cases[]={
#include "unified_semantic_cases.inc"
};
static int sameText(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int sameRange(XL6_AstRange a,XL6_AstRange b)
{return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn;}
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
 if(!a || !b)return a==b;
 if(a->kind!=b->kind || a->flags!=b->flags || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) ||
    a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded || !!a->literalData!=!!b->literalData ||
    (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || a->line!=b->line || a->column!=b->column ||
    !sameRange(a->range,b->range) || !sameRange(a->tokenRange,b->tokenRange) || !sameText(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
    a->semanticTypeId!=b->semanticTypeId || a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticSymbolId!=b->semanticSymbolId ||
    a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
    a->semanticReceiverSymbolId!=b->semanticReceiverSymbolId || !sameText(a->compiledPackageLink,b->compiledPackageLink) ||
    a->compiledPackageError!=b->compiledPackageError || a->compiledPackageRoot!=b->compiledPackageRoot ||
    a->semanticCompoundOperatorSymbolId!=b->semanticCompoundOperatorSymbolId || a->semanticCallKind!=b->semanticCallKind ||
    a->semanticLvalueKind!=b->semanticLvalueKind || a->semanticOwnership!=b->semanticOwnership || a->semanticExitKind!=b->semanticExitKind ||
    a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticTargetScopeDepth!=b->semanticTargetScopeDepth ||
    a->semanticRegionId!=b->semanticRegionId || a->semanticTargetRegionId!=b->semanticTargetRegionId || a->childCount!=b->childCount)return 0;
 if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
 for(size_t i=0;i<a->childCount;++i)if(!sameAst(a->children[i],b->children[i]))return 0;
 return 1;
}
static char* irJson(const xls_semantic_check* check)
{
 const XL6_IrModule* ir=xlsSemanticCheckIr(check);if(!ir)return NULL;
 XL6_Output output;if(!xl6OutputOpenMemory(&output))return NULL;xl6IrDumpModuleJson(&output,ir);
 char* text=xl6OutputHasFailed(&output) ? NULL : xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);return text;
}
static int parity(xls_semantic_check* actual,const xls_analysis_document* document,int accepted)
{
 xls_semantic_input input={document->path,xlsTextData(document->text),xlsTextSize(document->text)};
 xls_semantic_check* fresh=xlsSemanticCheckCreate(document->path,&input,1,1,NULL,NULL,NULL,0);CHECK(actual && fresh);
 xls_check_info info=xlsSemanticCheckInfo(actual);CHECK(info.accepted==accepted && xlsSemanticCheckInfo(fresh).accepted==accepted);
 char *a=xlsSemanticCheckJson(actual,NULL),*b=xlsSemanticCheckJson(fresh,NULL);
 if(a && b && strcmp(a,b)){xrtFileWriteAtomic("check-actual.json",(xbytesview){(const unsigned char*)a,strlen(a)});xrtFileWriteAtomic("check-fresh.json",(xbytesview){(const unsigned char*)b,strlen(b)});}
 CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
 CHECK(sameAst(xlsSemanticCheckAst(actual),xlsSemanticCheckAst(fresh)));
 if(accepted){a=irJson(actual);b=irJson(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);}
 else CHECK(info.diagnostics && !xlsSemanticCheckTypedIndex(actual));
 xlsSemanticCheckDestroy(fresh);return 1;
}
static int runCase(const struct fixture* fixture)
{
 char file[256];snprintf(file,sizeof(file),"%s/%s.xl",XLS_UNIFIED_CASES_DIRECTORY,fixture->name);
 size_t size=0;unsigned char* bytes=xrtFileReadAllLimit(file,1024u*1024u,&size);CHECK(bytes && size && !memchr(bytes,0,size));
 char relative[256];snprintf(relative,sizeof(relative),"build/unified-semantic/%s.xl",fixture->name);char* path=xrtPathAbs(relative);CHECK(path);
 xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_semantic_check* previous=NULL;
 xls_analysis_document document={"entry",path,NULL,1,0};
 for(unsigned generation=0;generation<4;++generation){
  xstrbuf body={0};size_t skip=size>=3 && !memcmp(bytes,"\xef\xbb\xbf",3) ? 3 : 0;
  if(generation)CHECK(xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf// 中文 😀\r\n")));
  CHECK(xrtStrBufAppend(&body,(xstrview){(const char*)bytes+(generation ? skip : 0),size-(generation ? skip : 0)}));
  if(generation>=2)CHECK(xrtStrBufAppendFormat(&body,"\nfunc xls_unified_anchor() as int\n return 3\nend\nfunc xls_unified_probe() as int\n return %u\nend\n",generation==2 ? 17u : 18u));
  xlsTextRelease(document.text);document.text=xlsTextCreate(body.Data,body.Size,8u*1024u*1024u,NULL);xrtStrBufFree(&body);CHECK(document.text);++document.revision;
  char error[256];xls_semantic_check* actual=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
  if(!actual)fprintf(stderr,"%s generation%u:%s\n",fixture->name,generation,error);
  CHECK(actual && parity(actual,&document,fixture->accepted));
  xls_semantic_check* hit=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(hit==actual);xlsSemanticCheckDestroy(hit);
  if(generation==3 && fixture->accepted){xls_check_semantic_work work=xlsSemanticCheckWork(actual);CHECK(work.contextHits && work.functionsReused);}
  xlsSemanticCheckDestroy(previous);previous=actual;
 }
 xlsCheckSessionDestroy(session);CHECK(parity(previous,&document,fixture->accepted));xlsSemanticCheckDestroy(previous);
 xlsTextRelease(document.text);xrtFree(bytes);xrtFree(path);printf("unified corpus %s passed\n",fixture->name);return 1;
}
int main(void)
{
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i)if(!runCase(&cases[i]))return 1;
 printf("analysis_unified_semantic_test passed: %u checks;fresh full AST/index/IR parity across Unicode/coordinate/declaration/literal edits and exact hits\n",checks);return 0;
}
