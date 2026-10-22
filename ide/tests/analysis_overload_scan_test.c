#include "analysis/frontend_sem.h"
#include "src/parser/xl6_parser_api.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(value) do { xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED); if(!(value)) { fprintf(stderr,"analysis_overload_scan_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
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
       a->semanticTypeId!=b->semanticTypeId || a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticSymbolId!=b->semanticSymbolId ||
       a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
       a->semanticCompoundOperatorSymbolId!=b->semanticCompoundOperatorSymbolId || a->semanticCallKind!=b->semanticCallKind ||
       a->semanticLvalueKind!=b->semanticLvalueKind || a->semanticOwnership!=b->semanticOwnership || a->semanticExitKind!=b->semanticExitKind ||
       a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticTargetScopeDepth!=b->semanticTargetScopeDepth ||
       a->semanticRegionId!=b->semanticRegionId || a->semanticTargetRegionId!=b->semanticTargetRegionId || a->childCount!=b->childCount) return 0;
    if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!sameAst(a->children[i],b->children[i])) return 0;
    return 1;
}


static int corpus(const char* text,int scale,int expectError)
{
    const char* path="overload-入口.xl";
    const size_t budgets[]={8u*1024u*1024u,0,1,8192};
    XL6_ParseResult parsed[4]; xls_sem_overload_work work[4]={{0}}; int okay[4];
    for(size_t k=0;k<4;++k) {
        xl6ParseResultInit(&parsed[k]); int parsedOkay=xl6ParseSource(text,path,&parsed[k]);
        if(!parsedOkay) for(size_t d=0;d<parsed[k].diags.count;++d) fprintf(stderr,"parse %d:%d %s\n",parsed[k].diags.items[d].line,parsed[k].diags.items[d].column,parsed[k].diags.items[d].message);
        CHECK(parsedOkay);
        okay[k]=xlsFrontendSemCheckOverloads(parsed[k].root,&parsed[k].diags,"overload",1,budgets[k],&work[k]);
    }
    if(expectError>=0) CHECK(!!parsed[0].diags.count==expectError);
    for(size_t k=0;k<4;++k) {
        CHECK(okay[k]==okay[1] && parsed[k].diags.count==parsed[1].diags.count);
        for(size_t i=0;i<parsed[k].diags.count;++i) {
            const XL6_Diag *a=&parsed[k].diags.items[i],*b=&parsed[1].diags.items[i];
            CHECK(a->line==b->line && a->column==b->column && sameText(a->file,b->file) && sameText(a->message,b->message));
        }
        CHECK(sameAst(parsed[k].root,parsed[1].root));
        CHECK(work[k].bytes<=budgets[k] && work[k].pairVisits<=work[1].pairVisits);
    }
    if(scale<0) CHECK(!work[0].indexes);
    CHECK(!work[1].indexes && !work[2].indexes && work[1].pairVisits==work[2].pairVisits);
    if(scale>0) {
        CHECK(work[0].indexes==1 && work[0].rootVisits==parsed[0].root->childCount);
        CHECK(work[1].pairVisits>work[0].pairVisits*10 && !work[3].indexes);
        if(scale==1) printf("overload candidates indexed=%llu reference=%llu, root visits=%llu, charged data=%llu bytes\n",
            (unsigned long long)work[0].pairVisits,(unsigned long long)work[1].pairVisits,
            (unsigned long long)work[0].rootVisits,(unsigned long long)work[0].bytes);
    }
    for(size_t k=0;k<4;++k) xl6ParseResultFree(&parsed[k]);
    return 1;
}
static int padded(const char* extra,int scale,int expectError)
{
    xstrbuf text; xrtStrBufInit(&text); int okay=1;
    for(unsigned i=0;okay && i<256;++i) okay=xrtStrBufAppendFormat(&text,"func helper_%u(int value) as int\n return value + 1\nend\n",i);
    okay=okay && xrtStrBufAppend(&text,xrtStrView(extra)) && xrtStrBufAppend(&text,xrtStrView("func main(...) as int\n return 0\nend\n"));
    CHECK(okay && corpus(text.Data,scale,expectError)); xrtStrBufFree(&text); return 1;
}
typedef struct reader { int passed; } reader;
static int32 parallel(void* user)
{
    reader* r=user;
    r->passed=padded("func 求值(int value) as int\n return value\nend\nfunc 求值(string value) as int\n return 0\nend\n",2,0);
    return 0;
}
static int run(void)
{
    CHECK(corpus("func main(...) as int\n return 0\nend\n",0,0));
    const char* cases[]={
        "func 求值(int value) as int\n return value\nend\nfunc 求值(string value) as int\n return 0\nend\n",
        "func choose(int value) as int\n return value\nend\nfunc choose(int value, int other = 0) as int\n return value\nend\n",
        "func a(int value) as int\n return value\nend\nfunc b(int value) as int\n return value\nend\nfunc a(int value) as int\n return value\nend\nfunc b(int value) as int\n return value\nend\nfunc a(int value) as int\n return value\nend\n",
        "typedef Alias = int\nfunc choose(Alias value) as int\n return value\nend\nfunc choose(int value) as int\n return value\nend\n",
        "func choose<T>(T value) as T\n return value\nend\nfunc choose(int value) as int\n return value\nend\n",
        "func choose(...) as int\n return 0\nend\nfunc choose(...) as int\n return 0\nend\n",
        "async func choose(int value) as int\n return value\nend\nfunc choose(int value) as int\n return value\nend\n",
        "extern \"C\" func choose(c_int value) as c_int\nfunc choose(int value) as int\n return value\nend\n",
        "export func choose(int value) as int\n return value\nend\nfunc choose(int value) as int\n return value\nend\n",
    };
    CHECK(padded(cases[0],1,0));
    for(size_t i=1;i<sizeof(cases)/sizeof(*cases);++i)
        CHECK(padded(cases[i],2,(i==4 || i==7) ? -1 : 1));
    char longName[301]; memset(longName,'z',300); longName[300]=0;
    xstrbuf longSource; xrtStrBufInit(&longSource);
    CHECK(xrtStrBufAppendFormat(&longSource,"func %s() as int\n return 0\nend\n",longName) && padded(longSource.Data,-1,0));
    xrtStrBufFree(&longSource);
    reader readers[4]={{0}}; xthread* threads[4];
    for(size_t i=0;i<4;++i) { threads[i]=xrtThreadCreate(parallel,&readers[i],0); CHECK(threads[i]); }
    for(size_t i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed); xrtThreadDestroy(threads[i]); }
    return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0); if(!run()) return 1;
    printf("analysis_overload_scan_test passed: %d assertions\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED)); return 0;
}
