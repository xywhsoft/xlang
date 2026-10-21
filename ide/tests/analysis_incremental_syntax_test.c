#include "analysis/frontend_internal.h"
#include "analysis/folding.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_incremental_syntax_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static xls_text* text(const char* s) { return xlsTextCreate(s,strlen(s),2u*1024u*1024u,NULL); }
static int same(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int equalRange(XL6_AstRange a,XL6_AstRange b)
{ return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn; }
static int equalAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    if(a->kind!=b->kind || a->flags!=b->flags || !same(a->text,b->text) || !same(a->doc,b->doc) || !same(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
       a->line!=b->line || a->column!=b->column || !equalRange(a->range,b->range) || !equalRange(a->tokenRange,b->tokenRange) || a->childCount!=b->childCount ||
       a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || a->semanticTypeId!=b->semanticTypeId || a->semanticSymbolId!=b->semanticSymbolId ||
       a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
       a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticRegionId!=b->semanticRegionId || !equalAst(a->inferredType,b->inferredType) || !equalAst(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!equalAst(a->children[i],b->children[i])) return 0;
    return 1;
}
static char* replace(const char* original,const char* needle,const char* replacement)
{
    const char* at=strstr(original,needle); if(!at) return NULL;
    xstrbuf out; xrtStrBufInit(&out);
    int okay=xrtStrBufAppend(&out,(xstrview){original,(size_t)(at-original)}) && xrtStrBufAppend(&out,xrtStrView(replacement)) && xrtStrBufAppend(&out,xrtStrView(at+strlen(needle)));
    char* result=okay ? xrtStrDup(out.Data) : NULL; xrtStrBufFree(&out); return result;
}
static int verify(xls_syntax* syntax,const char* body)
{
    XL6_ParseResult fresh; xl6ParseResultInit(&fresh); int accepted=xl6ParseSource(body,xlsSyntaxPath(syntax),&fresh);
    CHECK(accepted==xlsSyntaxAccepted(syntax) && fresh.diags.count==xlsSyntaxDiagnosticCount(syntax));
    CHECK(equalAst(fresh.root,xlsSyntaxAst(syntax)));
    for(size_t i=0;i<fresh.diags.count;++i) {
        xls_syntax_diagnostic d; CHECK(xlsSyntaxDiagnostic(syntax,i,&d) && d.line==fresh.diags.items[i].line && d.column==fresh.diags.items[i].column && same(d.message,fresh.diags.items[i].message));
    }
    xl6ParseResultFree(&fresh); return 1;
}
typedef struct nodes { const char* name; const XL6_AstNode* node; XL6_AstRange current; } nodes;
static int find(const XL6_AstNode* node,const XL6_AstRange* current,void* user)
{
    nodes* n=user;
    if((node->kind==XL6_AST_FUNC || node->kind==XL6_AST_EXPORT_FUNC || node->kind==XL6_AST_ASYNC_FUNC) && node->childCount && same(node->children[0]->text,n->name)) { n->node=node; n->current=*current; return 0; }
    return 1;
}
static int update(xls_session* session,const char* body,uint64_t version,xls_analysis_snapshot* snapshot)
{
    xls_text* source=text(body); CHECK(source);
    int okay=xlsSessionUpdate(session,"entry","/memory/函数😀.xl",1,version,source); xlsTextRelease(source);
    CHECK(okay && xlsSessionSyntax(session,"entry",snapshot)); return 1;
}
static int corpus(const char* original,const char* needle,const char* replacement,int expectFragment)
{
    xls_session* session=xlsSessionCreate(NULL); CHECK(session);
    xls_analysis_snapshot first={0},next={0}; CHECK(update(session,original,1,&first) && xlsSyntaxAccepted(first.syntax) && verify(first.syntax,original));
    char* changed=replace(original,needle,replacement); CHECK(changed && update(session,changed,2,&next));
    CHECK(verify(next.syntax,changed) && verify(first.syntax,original));
    xls_session_stats stats; xlsSessionStats(session,&stats);
    if(expectFragment) CHECK(stats.fullParses==1 && stats.fragmentParses==1 && stats.reusedDeclarations>=1 && stats.parsedBytes<strlen(original)+strlen(changed));
    else CHECK(stats.fullParses==2);
    xlsSyntaxRelease(first.syntax); xlsSyntaxRelease(next.syntax); xlsSessionDestroy(session); xrtFree(changed); return 1;
}
typedef struct reader { xls_syntax* syntax; int failed; } reader;
static int32 readView(void* user)
{
    reader* r=user; const XL6_AstNode* root=xlsSyntaxAst(r->syntax);
    if(!root || root->childCount<100) r->failed=1;
    for(unsigned i=0;i<30;++i) if(xlsSyntaxAst(r->syntax)!=root) r->failed=1;
    xlsSyntaxRelease(r->syntax); return 0;
}
static int run(void)
{
    const char* plain="func edited() as int\n return 7\nend\nfunc tail() as int\n return 9\nend\n";
    CHECK(corpus(plain,"return 7","int extra = 8\n return extra",1));
    CHECK(corpus(plain,"return 7","return 777777",1));
    CHECK(corpus(plain,"as int","as float",0));
    CHECK(corpus(plain,"return 7","return \"unterminated",0));
    CHECK(corpus(plain,"return 7","/* unterminated",0));
    CHECK(corpus(plain,"return 7","return 7\nend\nfunc inserted() as int\n return 1",0));
    const char* unicode="\xef\xbb\xbf" "// header😀\r\nfunc edited(int 值) as int\r\n string 文本 = f\"😀={值}\"\r\n bytes raw = b\"a\\0b\"; string empty = \"\"\r\n return 7\r\nend\r\nfunc tail() as int\r\n return 9\r\nend\r\n";
    CHECK(corpus(unicode,"return 7","if true\r\n  string 新值 = \"😀\\0\"\r\n  return 7\r\n end\r\n return 0",1));
    CHECK(corpus("#doc\nfunction documentation\n#end\nfunc edited() as int\n return 7\nend\nfunc tail()\nend\n","return 7","return 8",1));
    CHECK(corpus("func edited<T>(T value) as T\n return value\nend\nfunc tail()\nend\n","return value","auto other = value\n return other",1));
    CHECK(corpus("export \"exported\" func edited(int value) as int\n return value\nend\nfunc tail()\nend\n","return value","auto other = value\n return other",1));
    CHECK(corpus("async func edited() as int\n return 7\nend\nfunc tail()\nend\n","return 7","return 8",1));
    CHECK(corpus("class Box\n func edited() as int\n return 7\n end\nend\nfunc tail()\nend\n","return 7","return 8",0));

    /* Many declarations, repeated length/line/Unicode changes in one body.
     * Compare every AST field/range/literal with a fresh full shared parse;
     * pointer identity proves the tail declaration was actually retained. */
    xstrbuf source; xrtStrBufInit(&source);
    for(unsigned i=0;i<150;++i) { char line[128]; int n=snprintf(line,sizeof(line),"func helper_%u(int value) as int\n return value + 1\nend\n",i); CHECK(n>0 && xrtStrBufAppend(&source,(xstrview){line,(size_t)n})); }
    CHECK(xrtStrBufAppend(&source,xrtStrView(plain)));
    xls_session* session=xlsSessionCreate(NULL); xls_analysis_snapshot first={0},next={0}; CHECK(session && update(session,source.Data,1,&first));
    nodes before={"tail",NULL,{0}}; CHECK(xlsSyntaxVisit(first.syntax,find,&before,NULL,NULL) && before.node);
    size_t originalBytes=strlen(source.Data); xls_session_stats stats;
    for(unsigned i=0;i<100;++i) {
        char replacement[160]; snprintf(replacement,sizeof(replacement),"string value = \"😀-%u\"\n%s return %u",i,i&1 ? "\n" : "",i);
        char* changed=replace(source.Data,"return 7",replacement); CHECK(changed && update(session,changed,i+2,&next) && verify(next.syntax,changed));
        nodes after={"tail",NULL,{0}}; CHECK(xlsSyntaxVisit(next.syntax,find,&after,NULL,NULL) && after.node==before.node && after.current.startLine>before.current.startLine);
        xlsSessionStats(session,&stats); CHECK(stats.fullParses==1 && stats.fragmentParses==i+1 && stats.reusedDeclarations==(i+1)*151u);
        xlsSyntaxRelease(next.syntax); xrtFree(changed);
    }
    CHECK(stats.parsedBytes<originalBytes+100u*256u && verify(first.syntax,source.Data));
    xlsSyntaxRelease(first.syntax); xlsSessionDestroy(session);

    /* Four snapshots share chunks but materialize independent legacy trees on
     * different XRT threads. No shared non-atomic frontend Source refcounts. */
    session=xlsSessionCreate(NULL); CHECK(session); reader readers[4]; xthread* threads[4];
    for(unsigned i=0;i<4;++i) {
        char replacement[64]; snprintf(replacement,sizeof(replacement),"return %u",100+i);
        char* changed=replace(source.Data,"return 7",replacement); CHECK(changed && update(session,changed,i+1,&next));
        readers[i]=(reader){next.syntax,0}; xrtFree(changed);
    }
    xlsSessionDestroy(session);
    for(unsigned i=0;i<4;++i) { threads[i]=xrtThreadCreate(readView,&readers[i],0); CHECK(threads[i]); }
    for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !readers[i].failed); xrtThreadDestroy(threads[i]); }
    xrtStrBufFree(&source); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_incremental_syntax_test passed: %u checks; real function fragment parsing, retained declarations, full fresh AST/diagnostic equivalence, rebased Unicode ranges, fallback and concurrent lifetimes.\n",checks); return 0; }
