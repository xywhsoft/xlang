#include "analysis/ast_intern.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
static int faultMode;
static bool indexSet(xmap* map,xbytesview key,const void* value)
{return faultMode==1 ? false : xrtMapSet(map,key,value);}
static bool indexAppend(xstrbuf* buffer,xstrview value)
{return faultMode==2 ? false : xrtStrBufAppend(buffer,value);}
static bool indexInit(xmap* map,size_t size)
{return faultMode==3 ? false : xrtMapInit(map,size);}
/* Private exact implementation. Only its optional identity index fails;
 * the real independently linked AST clone and XRT owners remain unchanged. */
#define xrtMapSet indexSet
#define xrtStrBufAppend indexAppend
#define xrtMapInit indexInit
#define xlsAstInternCapture faultCapture
#define xlsAstInternDestroy faultDestroy
#define xlsAstInternWork faultWork
#include "../src/analysis/ast_intern.c"
#undef xrtMapSet
#undef xrtStrBufAppend
#undef xrtMapInit
#undef xlsAstInternCapture
#undef xlsAstInternDestroy
#undef xlsAstInternWork

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_intern_test:%d %s\n",__LINE__,#v);return 0;}}while(0)
static int sameText(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
 if(!a || !b)return a==b;
 XL6_AstNode left=*a,right=*b;
 left.text=right.text=left.doc=right.doc=NULL;left.literalData=right.literalData=NULL;left.children=right.children=NULL;
 left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;left.source=right.source=NULL;
 left.childCapacity=right.childCapacity=0;left.xlsStorage=right.xlsStorage=NULL;
 if(memcmp(&left,&right,sizeof(left)) || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
  (a->literalData && memcmp(a->literalData,b->literalData,a->literalSize)) || !!a->source!=!!b->source)return 0;
 if(a->source && (!sameText(a->source->path,b->source->path) || a->source->completionQuery!=b->source->completionQuery ||
  a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd))return 0;
 if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
 for(size_t i=0;i<a->childCount;++i)if(!sameAst(a->children[i],b->children[i]))return 0;
 return 1;
}
static XL6_AstNode* scalar(void)
{XL6_AstNode* n=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);if(n && !xl6AstSetSourceTree(n,"/类型😀.xl")){xl6AstFree(n);return NULL;}return n;}
static int pair(const XL6_AstNode* a,const XL6_AstNode* b,int identical)
{
 const XL6_AstNode* values[]={a,b,a};xls_ast_intern_work work;
 xls_ast_intern* owner=xlsAstInternCapture(values,3,1024*1024,1024*1024,&work);CHECK(owner);
 CHECK((values[0]==values[1])==identical && values[0]==values[2] && values[0]!=a && values[1]!=b);
 CHECK(work.references==3 && work.uniqueRoots==(identical ? 1u : 2u) && work.hits==(identical ? 2u : 1u) && !work.fallbacks && work.treeBytes>0);
 CHECK(sameAst(a,values[0]) && sameAst(b,values[1]));xlsAstInternDestroy(owner);return 1;
}
static int identities(void)
{
 XL6_AstNode* a=scalar();CHECK(a);XL6_AstNode* b=xlsAstClone(a,NULL,NULL);CHECK(b && pair(a,b,1));
 b->childCapacity=9000;CHECK(pair(a,b,1));b->source->references+=13;CHECK(pair(a,b,1));b->source->references-=13;xl6AstFree(b);
#define FIELD(member) offsetof(XL6_AstNode,member)
 const size_t fields[]={FIELD(kind),FIELD(flags),FIELD(literalSize),FIELD(literalScalar),FIELD(literalDecoded),
  FIELD(semanticTypeId),FIELD(semanticTypeAbiId),FIELD(semanticSymbolId),FIELD(semanticDeclaredSymbolId),FIELD(semanticTargetSymbolId),
  FIELD(semanticCompoundOperatorSymbolId),FIELD(semanticCallKind),FIELD(semanticLvalueKind),FIELD(semanticOwnership),FIELD(semanticExitKind),
  FIELD(semanticScopeDepth),FIELD(semanticTargetScopeDepth),FIELD(semanticRegionId),FIELD(semanticTargetRegionId),FIELD(line),FIELD(column),
  FIELD(range.known),FIELD(range.startOffset),FIELD(range.endOffset),FIELD(range.startLine),FIELD(range.startColumn),FIELD(range.endLine),FIELD(range.endColumn),
  FIELD(tokenRange.known),FIELD(tokenRange.startOffset),FIELD(tokenRange.endOffset),FIELD(tokenRange.startLine),FIELD(tokenRange.startColumn),FIELD(tokenRange.endLine),FIELD(tokenRange.endColumn)};
#undef FIELD
 for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i){b=xlsAstClone(a,NULL,NULL);CHECK(b);((unsigned char*)b)[fields[i]]^=1;CHECK(pair(a,b,0));xl6AstFree(b);}
 const size_t sourceFields[]={offsetof(XL6_AstSource,completionQuery),offsetof(XL6_AstSource,completionStart),offsetof(XL6_AstSource,completionEnd)};
 for(size_t i=0;i<3;++i){b=xlsAstClone(a,NULL,NULL);CHECK(b);((unsigned char*)b->source)[sourceFields[i]]^=1;CHECK(pair(a,b,0));xl6AstFree(b);}
 b=xlsAstClone(a,NULL,NULL);CHECK(b);xrtFree(b->text);b->text=xrtStrDup("string");CHECK(b->text && pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);b->doc=xrtStrDup("");CHECK(b->doc && pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);xrtFree(b->text);b->text=NULL;CHECK(pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);xrtFree(b->source->path);b->source->path=xrtStrDup("/other.xl");CHECK(b->source->path && pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);xrtFree(b->source->path);b->source->path=NULL;CHECK(pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);xrtFree(b->source->path);b->source->path=xrtStrDup("");CHECK(b->source->path && pair(a,b,0));xl6AstFree(b);
 b=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);CHECK(b && pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);b->inferredType=xl6AstNew(XL6_AST_TYPE,NULL);CHECK(b->inferredType && pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);b->semanticDeclaredType=xl6AstNew(XL6_AST_TYPE,NULL);CHECK(b->semanticDeclaredType && pair(a,b,0));xl6AstFree(b);
 b=xlsAstClone(a,NULL,NULL);CHECK(b);xl6AstAddChild(b,xl6AstNew(XL6_AST_NAME,NULL));CHECK(b->childCount==1 && pair(a,b,0));xl6AstFree(b);
 CHECK(xl6AstSetLiteralData(a,"a\0b",3));b=xlsAstClone(a,NULL,NULL);CHECK(b && pair(a,b,1));b->literalData[2]='c';CHECK(pair(a,b,0));xl6AstFree(b);xl6AstFree(a);return 1;
}
static int budgetsAndFaults(void)
{
 XL6_AstNode* input=scalar();CHECK(input);const XL6_AstNode* values[]={input,input};xls_ast_intern_work original;
 xls_ast_intern* owner=xlsAstInternCapture(values,2,1024*1024,1024*1024,&original);CHECK(owner && original.hits==1);xlsAstInternDestroy(owner);
 for(size_t budget=0;budget<3;++budget){
  size_t bytes=budget==2 ? (size_t)original.keyBytes : budget==1 ? (size_t)original.keyBytes-1 : 0;
  values[0]=values[1]=input;xls_ast_intern_work work;owner=xlsAstInternCapture(values,2,1024*1024,bytes,&work);CHECK(owner && sameAst(input,values[0]) && sameAst(input,values[1]));
  CHECK(budget==2 ? values[0]==values[1] && !work.fallbacks : values[0]!=values[1] && work.fallbacks==1);xlsAstInternDestroy(owner);
 }
 values[0]=values[1]=input;owner=xlsAstInternCapture(values,2,(size_t)original.treeBytes,1024*1024,NULL);CHECK(owner);xlsAstInternDestroy(owner);
 values[0]=values[1]=input;CHECK(!xlsAstInternCapture(values,2,(size_t)original.treeBytes-1,1024*1024,NULL) && values[0]==input && values[1]==input);
 for(faultMode=1;faultMode<=3;++faultMode){values[0]=values[1]=input;xls_ast_intern_work work;owner=faultCapture(values,2,1024*1024,1024*1024,&work);CHECK(owner && work.fallbacks==1 && !work.hits && work.uniqueRoots==2 && values[0]!=values[1]);CHECK(sameAst(input,values[0]) && sameAst(input,values[1]));faultDestroy(owner);}
 faultMode=0;values[0]=values[1]=input;CHECK(!xlsAstInternCapture(NULL,2,1024,1024,NULL) && !xlsAstInternCapture(values,0,1024,1024,NULL) && !xlsAstInternCapture(values,2,0,1024,NULL) && !xlsAstInternCapture(values,2,1024,8u*1024u*1024u+1,NULL));CHECK(values[0]==input && values[1]==input);
 xl6AstFree(input);return 1;
}
typedef struct intern_reader {const XL6_AstNode* input;unsigned failures;} intern_reader;
static int32 cloneReads(void* user)
{intern_reader* r=user;for(unsigned i=0;i<100;++i){XL6_AstNode* copy=xlsAstClone(r->input,NULL,NULL);if(!copy || !sameAst(r->input,copy))++r->failures;xl6AstFree(copy);}return 0;}
static int ownership(void)
{
 XL6_AstNode* input=scalar();CHECK(input);xls_ast_pool_work pool;xls_ast_intern_work work;XL6_AstNode* pooled=xlsAstClonePooled(input,NULL,NULL,NULL,1024*1024,&pool,NULL);CHECK(pooled && pooled->xlsStorage);
 const XL6_AstNode* values[4096];for(size_t i=0;i<4096;++i)values[i]=i%2 ? input : pooled;
 xls_ast_intern* owner=xlsAstInternCapture(values,4096,2*1024*1024,1024*1024,&work);CHECK(owner && work.references==4096 && work.uniqueRoots==1 && work.hits==4095 && work.treeBytes<4096);
 const XL6_AstNode* retained=values[0];CHECK(retained->source!=input->source && sameAst(input,retained));size_t references=retained->source->references;xl6AstFree(input);xl6AstFree(pooled);
 intern_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(intern_reader){retained,0};threads[i]=xrtThreadCreate(cloneReads,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !readers[i].failures);xrtThreadDestroy(threads[i]);}
 CHECK(retained->source->references==references);xlsAstInternDestroy(owner);return 1;
}
int main(void)
{
 if(!identities() || !budgetsAndFaults() || !ownership())return 1;
 printf("analysis_ast_intern_test passed: %u checks;exact lexical/semantic/source/literal identity,immutable aliases,capacity-owner normalization,key/clone limits,real index failures,fallback and400concurrent independent clones.\n",checks);return 0;
}
