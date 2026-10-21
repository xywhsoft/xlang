#include "analysis/check_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_incremental_check_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static int append(xstrbuf* b,const char* s) { return xrtStrBufAppend(b,xrtStrView(s)); }
static xls_text* text(const char* value) { return xlsTextCreate(value,strlen(value),8u*1024u*1024u,NULL); }
static xls_semantic_check* query(xls_check_session* session,const xls_analysis_document* docs,size_t count,int capture)
{
    char error[256]={0};
    xls_semantic_check* result=xlsCheckSessionCheck(session,docs,count,"entry",capture,NULL,NULL,error,sizeof(error));
    if(!result) fprintf(stderr,"incremental check: %s\n",error);
    return result;
}
static int equalFresh(xls_semantic_check* result,const xls_analysis_document* docs,size_t count,int capture)
{
    xls_semantic_input inputs[4]; CHECK(count<=4);
    for(size_t i=0;i<count;++i) inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_check* fresh=xlsSemanticCheckCreate(docs[0].path,inputs,count,capture,NULL,NULL,NULL,0); CHECK(result && fresh);
    xls_check_info a=xlsSemanticCheckInfo(result),b=xlsSemanticCheckInfo(fresh);
    CHECK(a.accepted==b.accepted && a.hasIndex==b.hasIndex && a.diagnostics==b.diagnostics && a.irTypes==b.irTypes && a.irNodes==b.irNodes && !strcmp(a.phase,b.phase));
    char *first=xlsSemanticCheckJson(result,NULL),*second=xlsSemanticCheckJson(fresh,NULL);
    if(first && second && strcmp(first,second)) {
        xrtFileWriteAtomic("build/incremental-check-result.json",(xbytesview){(const unsigned char*)first,strlen(first)});
        xrtFileWriteAtomic("build/incremental-check-fresh.json",(xbytesview){(const unsigned char*)second,strlen(second)});
    }
    CHECK(first && second && !strcmp(first,second));
    xlsSemanticStringFree(first); xlsSemanticStringFree(second); xlsSemanticCheckDestroy(fresh); return 1;
}
static int run(void)
{
    char *path=xrtPathAbs("incremental-entry.xl"),*dep=xrtPathAbs("incremental_dep.xl"); CHECK(path && dep);
    const char* tail="func tail() as int\n return 3\nend\n";
    xls_check_session* session=xlsCheckSessionCreate(NULL); CHECK(session);
    xls_analysis_document docs[2]={{"entry",path,NULL,100,0},{"dep",dep,NULL,200,1}};
    xls_semantic_check* original=NULL; size_t initialBytes=0;
    for(unsigned i=0;i<30;++i) {
        xstrbuf source; xrtStrBufInit(&source);
        CHECK(xrtStrBufAppendFormat(&source,"\xef\xbb\xbf/// entry doc\r\nfunc main(...) as int\r\n string value = \"中文😀%u\"\r\n int n = %u\r\n return n\r\nend\r\n",i,i));
        for(unsigned j=0;j<i%3;++j) CHECK(append(&source,"// shifted 中文😀\r\n"));
        CHECK(append(&source,tail));
        xlsTextRelease(docs[0].text); docs[0].text=text(source.Data); docs[0].revision=i+1; CHECK(docs[0].text);
        xrtStrBufFree(&source);
        xls_semantic_check* r=query(session,docs,1,1); CHECK(r && xlsSemanticCheckInfo(r).accepted && equalFresh(r,docs,1,1));
        /* The trailing inter-declaration comments change outside the function
         * in this corpus, so compare correctness without claiming local work. */
        if(!i) { original=xlsSemanticCheckRetain(r); initialBytes=xlsTextSize(docs[0].text); }
        xlsSemanticCheckDestroy(r);
    }
    CHECK(xlsSemanticCheckInfo(original).accepted && xlsSemanticCheckValidate(original));
    xlsSemanticCheckDestroy(original); xlsCheckSessionDestroy(session);

    /* Many unchanged declarations, with edits strictly inside one function. */
    session=xlsCheckSessionCreate(NULL); CHECK(session); docs[0].identity=300; docs[0].revision=0;
    uint64_t expectedBytes=0;
    for(unsigned i=0;i<25;++i) {
        xstrbuf source; xrtStrBufInit(&source);
        CHECK(xrtStrBufAppendFormat(&source,"func main(...) as int\n int changed = %u\n",i));
        if(i&1) CHECK(append(&source," // 中文😀 extra line\n"));
        CHECK(append(&source," return changed\nend\n"));
        size_t functionBytes=source.Size;
        for(unsigned j=0;j<100;++j) CHECK(xrtStrBufAppendFormat(&source,"func helper%u() as int\n return %u\nend\n",j,j));
        expectedBytes+=i ? functionBytes-1 : source.Size;
        xlsTextRelease(docs[0].text); docs[0].text=text(source.Data); ++docs[0].revision; CHECK(docs[0].text); xrtStrBufFree(&source);
        xls_semantic_check* r=query(session,docs,1,1); CHECK(r && xlsSemanticCheckInfo(r).accepted && equalFresh(r,docs,1,1));
        xlsSemanticCheckDestroy(r);
    }
    xls_check_stats stats; xlsCheckSessionStats(session,&stats);
    CHECK(stats.builds==25 && stats.entryFullParses==1 && stats.entryFragmentParses==24 && stats.entryReusedDeclarations==2400 && !stats.entrySyntaxHits);
    CHECK(stats.entryParsedBytes==expectedBytes && initialBytes);
    xls_semantic_check* before=query(session,docs,1,1); CHECK(before);
    ++docs[0].revision; xls_semantic_check* r=query(session,docs,1,1); CHECK(r==before); xlsSemanticCheckDestroy(r);
    r=query(session,docs,1,0); CHECK(r && equalFresh(r,docs,1,0)); xlsSemanticCheckDestroy(r);
    xlsCheckSessionStats(session,&stats); CHECK(stats.entryFullParses==1 && stats.entryFragmentParses==24 && stats.entrySyntaxHits==1 && stats.hits==2);
    xlsSemanticCheckDestroy(before);

    const char* sources[]={
        "import incremental_dep\nfunc main(...) as int\n return incremental_dep.value()\nend\n",
        "import incremental_dep\nfunc main(...) as int\n int n = 5\n return incremental_dep.value() + n\nend\n",
        "import incremental_dep\nfunc main(...) as int\n return \"wrong return\"\nend\n",
        "func main(...) as int\n return \"unclosed\nend\n",
        "func main(...) as int\n return 0\nend\n",
        "func main(...) as int\n return 0\nend\nfunc appended() as int\n return 1\nend\n"
    };
    docs[1].text=text("export func value() as int\n return 4\nend\n"); CHECK(docs[1].text);
    for(unsigned i=0;i<sizeof(sources)/sizeof(sources[0]);++i) {
        xlsTextRelease(docs[0].text); docs[0].text=text(sources[i]); ++docs[0].revision; CHECK(docs[0].text);
        r=query(session,docs,2,1); CHECK(r && equalFresh(r,docs,2,1));
        CHECK(xlsSemanticCheckInfo(r).accepted==(i!=2 && i!=3)); xlsSemanticCheckDestroy(r);
        if(!i) {
            xlsCheckSessionStats(session,&stats); uint64_t full=stats.entryFullParses,fragment=stats.entryFragmentParses,hits=stats.entrySyntaxHits;
            xlsTextRelease(docs[1].text); docs[1].text=text("export func value() as int\n return 8\nend\n"); ++docs[1].revision; CHECK(docs[1].text);
            r=query(session,docs,2,1); CHECK(r && equalFresh(r,docs,2,1)); xlsSemanticCheckDestroy(r);
            xlsCheckSessionStats(session,&stats); CHECK(stats.entryFullParses==full && stats.entryFragmentParses==fragment && stats.entrySyntaxHits==hits+1);
        }
    }
    xlsCheckSessionStats(session,&stats); uint64_t full=stats.entryFullParses;
    ++docs[0].identity; docs[0].revision=1; r=query(session,docs,2,1); CHECK(r && equalFresh(r,docs,2,1)); xlsSemanticCheckDestroy(r);
    xlsCheckSessionStats(session,&stats); CHECK(stats.entryFullParses==full+1);
    xlsCheckSessionDestroy(session);

    /* The internal parsed-entry boundary rejects wrong source/path and never
     * passes an immutable published AST into mutating semantic phases. */
    xls_parse_work work={0}; xls_syntax* syntax=xlsSyntaxBuild(path,docs[0].text,NULL,&work); CHECK(syntax);
    const XL6_AstNode* immutable=xlsSyntaxAst(syntax); CHECK(immutable && !immutable->semanticTypeId);
    xls_semantic_input in={path,xlsTextData(docs[0].text),xlsTextSize(docs[0].text)};
    r=xlsSemanticCheckCreateParsed(path,&in,1,1,syntax,NULL,NULL,NULL,0); CHECK(r && equalFresh(r,docs,1,1));
    CHECK(xlsSyntaxAst(syntax)==immutable && !immutable->semanticTypeId); xlsSemanticCheckDestroy(r);
    in.text="func other()\nend\n"; in.size=strlen(in.text);
    CHECK(!xlsSemanticCheckCreateParsed(path,&in,1,1,syntax,NULL,NULL,NULL,0));
    in.path=dep; in.text=xlsTextData(docs[0].text); in.size=xlsTextSize(docs[0].text);
    CHECK(!xlsSemanticCheckCreateParsed(dep,&in,1,1,syntax,NULL,NULL,NULL,0));
    xlsSyntaxRelease(syntax);
    xlsTextRelease(docs[0].text); xlsTextRelease(docs[1].text); xrtFree(path); xrtFree(dep); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_incremental_check_test passed: %u checks; parsed-entry full-check parity, local parsing, dependency reuse, errors and immutable ownership.\n",checks); return 0; }
