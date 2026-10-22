#include "analysis/frontend_stdlib.h"
#include "src/parser/xl6_parser_api.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(value) do { xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED); if(!(value)) { fprintf(stderr,"analysis_stdlib_scan_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
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

static int corpus(const char* text,int scale)
{
    const char* path="D:/stdlib-scan/入口.xl";
    XL6_ParseResult a,b,c; xl6ParseResultInit(&a); xl6ParseResultInit(&b); xl6ParseResultInit(&c);
    CHECK(xl6ParseSource(text,path,&a) && xl6ParseSource(text,path,&b) && xl6ParseSource(text,path,&c));
    xls_stdlib_work indexed={0},reference={0},bounded={0};
    int aa=xlsFrontendStdlibExpand(a.root,&a.diags,8u*1024u*1024u,&indexed);
    int bb=xlsFrontendStdlibExpandReference(b.root,&b.diags,&reference);
    int cc=xlsFrontendStdlibExpand(c.root,&c.diags,1,&bounded);
    CHECK(aa==bb && cc==bb && a.diags.count==b.diags.count && c.diags.count==b.diags.count);
    for(size_t i=0;i<a.diags.count;++i) CHECK(sameText(a.diags.items[i].message,b.diags.items[i].message) && sameText(c.diags.items[i].message,b.diags.items[i].message));
    CHECK(sameAst(a.root,b.root) && sameAst(c.root,b.root));
    CHECK(!reference.pruned && !bounded.pruned && reference.predicateVisits==bounded.predicateVisits);
    CHECK(indexed.nodes && indexed.names && indexed.bytes<=8u*1024u*1024u && indexed.pruned);
    CHECK(indexed.queries==reference.queries && indexed.predicateVisits<=reference.predicateVisits);
    if(scale) {
        CHECK(reference.predicateVisits>indexed.predicateVisits*10 && indexed.pruned>100);
        printf("stdlib predicate visits indexed=%llu reference=%llu, one index walk=%llu, index data=%llu bytes\n",
            (unsigned long long)indexed.predicateVisits,(unsigned long long)reference.predicateVisits,
            (unsigned long long)indexed.nodes,(unsigned long long)indexed.bytes);
    }
    xl6ParseResultFree(&a); xl6ParseResultFree(&b); xl6ParseResultFree(&c); return 1;
}
typedef struct reader { int passed; } reader;
static int32 parallel(void* user)
{
    reader* r=user;
    r->passed=corpus("func main(...) as int\r\n string 中文 = \"Buffer\\0😀\"\r\n return 0\r\nend\r\n",0);
    return 0;
}
static int run(void)
{
    const char* cases[]={
        "func main(...) as int\n return 0\nend\n",
        "func Buffer() as int\n return 1\nend\nfunc main(...) as int\n string marker = \"Pattern\"\n return 0\nend\n",
        "func main(...) as int\n Buffer value = Buffer()\n return 0\nend\n",
        "func main(...) as int\n auto value = Buffer::<int>()\n return 0\nend\n",
        "class Mine\n int value\nend\nfunc main(...) as int\n Mine value = Mine()\n return 0\nend\n",
        "func main(...) as int\n auto value = File.open(\"example.txt\")\n return 0\nend\n",
        "func main(...) as int\n string value = \"x\"\n string changed = value.trim().upper()\n return 0\nend\n",
        "func helper(Regex value)\n value.replaceTo()\nend\nfunc main(...) as int\n return 0\nend\n",
        "func main(...) as int\n auto value = NetAddress(\"localhost\")\n return 0\nend\n",
        "func main(...) as int\r\n string 中文 = \"Buffer\\0😀\"\r\n return 0\r\nend\r\n"
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i) { printf("stdlib corpus %zu\n",i); CHECK(corpus(cases[i],0)); }
    xstrbuf large; xrtStrBufInit(&large);
    CHECK(xrtStrBufAppend(&large,xrtStrView("func choose<T>(T value) as T\n return value\nend\n")));
    for(unsigned i=0;i<40;++i) {
        CHECK(xrtStrBufAppendFormat(&large,"func helper_%u(int value) as int\n int result = value\n",i));
        for(unsigned j=0;j<25;++j) CHECK(xrtStrBufAppend(&large,xrtStrView(" result = choose::<int>(result + 1)\n")));
        CHECK(xrtStrBufAppend(&large,xrtStrView(" return result\nend\n")));
    }
    CHECK(xrtStrBufAppend(&large,xrtStrView("func main(...) as int\n return 0\nend\n")) && corpus(large.Data,1)); xrtStrBufFree(&large);
    reader readers[4]; xthread* threads[4];
    for(size_t i=0;i<4;++i) { readers[i]=(reader){0}; threads[i]=xrtThreadCreate(parallel,&readers[i],0); CHECK(threads[i]); }
    for(size_t i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed); xrtThreadDestroy(threads[i]); }
    return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0); if(!run()) return 1;
    printf("analysis_stdlib_scan_test passed: %u checks; full original expansion AST/diagnostic parity, real text filtering, tiny-budget fallback and concurrent TLS ownership.\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED)); return 0;
}
