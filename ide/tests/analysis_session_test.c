#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_session_test:%d: %s\n",__LINE__,#value); return 1; } } while(0)
static xls_text* text(const char* source)
{ return xlsTextCreate(source,strlen(source),1024u*1024u,NULL); }
static const XL6_AstNode* find(const XL6_AstNode* node,XL6_AstKind kind,const char* name)
{
    if(!node) return NULL;
    if(node->kind==kind && (!name || (node->text && !strcmp(name,node->text)))) return node;
    for(size_t i=0;i<node->childCount;++i) { const XL6_AstNode* found=find(node->children[i],kind,name); if(found) return found; }
    return NULL;
}
static int equalText(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static const XL6_AstNode* function(const XL6_AstNode* node,const char* name)
{
    if(!node) return NULL;
    if(node->kind==XL6_AST_FUNC && node->childCount && node->children[0] &&
        node->children[0]->kind==XL6_AST_NAME && equalText(node->children[0]->text,name)) return node;
    for(size_t i=0;i<node->childCount;++i) { const XL6_AstNode* found=function(node->children[i],name); if(found) return found; }
    return NULL;
}
/* Compare reused results with a fresh compiler parse, including source ranges
 * and literal payload. No alternate grammar or lexer lives in the test. */
static int equalAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    if(a->kind!=b->kind || a->flags!=b->flags || !equalText(a->text,b->text) ||
        a->childCount!=b->childCount || a->range.known!=b->range.known ||
        a->range.startOffset!=b->range.startOffset || a->range.endOffset!=b->range.endOffset ||
        a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar ||
        (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize))) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!equalAst(a->children[i],b->children[i])) return 0;
    return 1;
}
int main(void)
{
    const char *alpha="func alpha() as int\n return 1\nend\n",*beta="func beta(int value) as int\n return value\nend\n";
    xls_session_limits limits={3,4096,8192}; xls_session* session=xlsSessionCreate(&limits);
    xls_text *a=text(alpha),*b=text(beta),*a2=text("func alpha() as int\n return 2\nend\n");
    xls_analysis_snapshot first={0},other={0},query={0}; xls_session_stats stats={0};
    CHECK(session && a && b && a2);
    CHECK(xlsSessionUpdate(session,"a","/memory/alpha.xl",10,1,a) && xlsSessionUpdate(session,"b","/memory/beta.xl",20,1,b));
    xlsSessionStats(session,&stats); CHECK(stats.documents==2 && stats.parses==0 && stats.textBytes==strlen(alpha)+strlen(beta));
    CHECK(xlsSessionSyntax(session,"a",&first) && first.identity==10 && first.revision==1 && xlsSyntaxAccepted(first.syntax));
    CHECK(function(xlsSyntaxAst(first.syntax),"alpha") && find(xlsSyntaxAst(first.syntax),XL6_AST_INT,"1") && !xlsSyntaxDiagnosticCount(first.syntax));
    CHECK(xlsSessionSyntax(session,"a",&query) && query.syntax==first.syntax && query.generation==first.generation); xlsSyntaxRelease(query.syntax);
    CHECK(xlsSessionSyntax(session,"b",&other) && xlsSyntaxAccepted(other.syntax) && function(xlsSyntaxAst(other.syntax),"beta"));
    xlsSessionStats(session,&stats); CHECK(stats.parses==2 && stats.cacheHits==1);
    CHECK(!xlsSessionUpdate(session,"a","/memory/alpha.xl",10,1,a2));
    CHECK(xlsSessionUpdate(session,"a","/memory/alpha.xl",10,2,a2));
    CHECK(xlsSessionSyntax(session,"a",&query) && query.syntax!=first.syntax && query.revision==2 && query.generation>first.generation && find(xlsSyntaxAst(query.syntax),XL6_AST_INT,"2"));
    CHECK(find(xlsSyntaxAst(first.syntax),XL6_AST_INT,"1") && !strcmp(xlsTextData(xlsSyntaxText(first.syntax)),alpha)); xlsSyntaxRelease(query.syntax);
    CHECK(xlsSessionSyntax(session,"b",&query) && query.syntax==other.syntax); xlsSyntaxRelease(query.syntax);
    xls_text* copied=text(xlsTextData(a2)); CHECK(copied && xlsSessionUpdate(session,"a","/memory/alpha.xl",10,3,copied));
    CHECK(xlsSessionSyntax(session,"a",&query) && query.revision==3); xls_syntax* unchanged=query.syntax;
    CHECK(xlsSessionUpdate(session,"a","/memory/alpha.xl",10,3,copied));
    CHECK(!xlsSessionUpdate(session,"a","/memory/renamed.xl",10,2,copied));
    CHECK(xlsSessionSyntax(session,"a",&query) && query.syntax==unchanged && query.revision==3); xlsSyntaxRelease(query.syntax); xlsSyntaxRelease(unchanged);
    xlsSessionStats(session,&stats); CHECK(stats.parses==3);
    CHECK(xlsSessionUpdate(session,"a","/memory/renamed.xl",10,3,copied));
    CHECK(xlsSessionSyntax(session,"a",&query) && !strcmp(xlsSyntaxPath(query.syntax),"/memory/renamed.xl")); xlsSyntaxRelease(query.syntax);
    CHECK(xlsSessionUpdate(session,"a","/memory/renamed.xl",11,1,copied));
    CHECK(xlsSessionSyntax(session,"a",&query) && query.identity==11 && query.revision==1); xlsSyntaxRelease(query.syntax);
    xlsSessionStats(session,&stats); CHECK(stats.parses==5);
    CHECK(!xlsSessionUpdate(session,"","/memory/a.xl",1,1,a) && !xlsSessionUpdate(session,"bad","\xff",1,1,a));
    CHECK(xlsSessionUpdate(session,"third","/memory/c.xl",30,1,a) && !xlsSessionUpdate(session,"fourth","/memory/d.xl",40,1,a));
    char large[4098]; memset(large,' ',sizeof(large)-1); large[sizeof(large)-1]=0; xls_text* oversized=text(large);
    CHECK(oversized && !xlsSessionUpdate(session,"a","/memory/renamed.xl",11,2,oversized));
    CHECK(xlsSessionSyntax(session,"a",&query) && query.revision==1 && query.identity==11); xlsSyntaxRelease(query.syntax);
    CHECK(xlsSessionClose(session,"third") && !xlsSessionClose(session,"third") && !xlsSessionSyntax(session,"third",&query) && !query.syntax && !query.identity);
    /* Bodies and error results survive independent document edits and closes. */
    const char* samples[]={"", "int answer = 42\n", "func broken(\n", "class Box\n int value\nend\n", "//😀\nfunc 中文(int 值) as int\n return 值\nend\n", "\xef\xbb\xbf" "func bom() as int\r\n return 7\r\nend\r\n", "import missing\n", "string value = \"unterminated\n", "typedef int Number\n", "func generic<T>(T value) as T\n return value\nend\n"};
    for(size_t i=0;i<sizeof(samples)/sizeof(*samples);++i) {
        xls_text* source=text(samples[i]); CHECK(source && xlsSessionUpdate(session,"corpus","/memory/语法😀.xl",50,i+1,source)); xlsTextRelease(source);
        CHECK(xlsSessionSyntax(session,"corpus",&query));
        XL6_ParseResult fresh; xl6ParseResultInit(&fresh); int accepted=xl6ParseSource(samples[i],"/memory/语法😀.xl",&fresh);
        CHECK(accepted==xlsSyntaxAccepted(query.syntax) && fresh.diags.count==xlsSyntaxDiagnosticCount(query.syntax) && equalAst(fresh.root,xlsSyntaxAst(query.syntax)));
        for(size_t j=0;j<fresh.diags.count;++j) {
            xls_syntax_diagnostic diagnostic={0}; CHECK(xlsSyntaxDiagnostic(query.syntax,j,&diagnostic));
            CHECK(diagnostic.line==fresh.diags.items[j].line && diagnostic.column==fresh.diags.items[j].column && equalText(diagnostic.message,fresh.diags.items[j].message) && !strcmp(diagnostic.path,"/memory/语法😀.xl"));
        }
        xls_syntax_diagnostic missing={0}; CHECK(!xlsSyntaxDiagnostic(query.syntax,fresh.diags.count,&missing) && !missing.message);
        xlsSyntaxRelease(query.syntax); xl6ParseResultFree(&fresh);
    }
    CHECK(xlsSessionClose(session,"corpus"));
    for(unsigned i=0;i<500;++i) {
        char source[160],value[20]; snprintf(value,sizeof(value),"%u",i); snprintf(source,sizeof(source),"func alpha() as int\n return %s\nend\n",value);
        xls_text* input=text(source); CHECK(input && xlsSessionUpdate(session,"a","/memory/renamed.xl",11,i+2,input)); xlsTextRelease(input);
        CHECK(xlsSessionSyntax(session,"a",&query) && find(xlsSyntaxAst(query.syntax),XL6_AST_INT,value) && xlsSyntaxAccepted(query.syntax)); xlsSyntaxRelease(query.syntax);
        CHECK(xlsSessionSyntax(session,"b",&query) && query.syntax==other.syntax); xlsSyntaxRelease(query.syntax);
    }
    CHECK(xlsSessionClose(session,"a") && xlsSessionClose(session,"b"));
    xlsSessionStats(session,&stats); CHECK(!stats.documents && !stats.textBytes && stats.parses==515);
    xlsSessionDestroy(session);
    CHECK(xlsSyntaxAccepted(first.syntax) && find(xlsSyntaxAst(first.syntax),XL6_AST_INT,"1") && xlsSyntaxAccepted(other.syntax));
    xlsSyntaxRelease(first.syntax); xlsSyntaxRelease(other.syntax);
    xlsTextRelease(a); xlsTextRelease(b); xlsTextRelease(a2); xlsTextRelease(copied); xlsTextRelease(oversized);
    printf("analysis_session_test passed: %u checks; real shared xlang parser, lazy retained results, independent documents, revisions/identity, immutable old snapshots, capacity/validation, fresh-parse equivalence and 500 body edits.\n",checks);
    return 0;
}
