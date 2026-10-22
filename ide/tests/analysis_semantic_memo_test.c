#include "analysis/check_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static xatomic32 checks;
#define CHECK(value) do { xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED); if(!(value)) { fprintf(stderr,"analysis_semantic_memo_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
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
static char* irJson(const xls_semantic_check* result)
{
    const XL6_IrModule* ir=xlsSemanticCheckIr(result); if(!ir) return NULL;
    XL6_Output output; if(!xl6OutputOpenMemory(&output)) return NULL;
    xl6IrDumpModuleJson(&output,ir);
    char* text=xl6OutputHasFailed(&output) ? NULL : xl6OutputTakeMemoryText(&output,NULL);
    xl6OutputDiscard(&output); return text;
}
static int fresh(xls_semantic_check* result,const xls_analysis_document* docs,size_t count)
{
    xls_semantic_input inputs[2]; CHECK(count<=2);
    for(size_t i=0;i<count;++i) inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_check* baseline=xlsSemanticCheckCreate(docs[0].path,inputs,count,1,NULL,NULL,NULL,0); CHECK(result && baseline);
    char *a=xlsSemanticCheckJson(result,NULL),*b=xlsSemanticCheckJson(baseline,NULL);
    if(a && b && strcmp(a,b)) {
        xrtFileWriteAtomic("build/memo-actual.json",(xbytesview){(const unsigned char*)a,strlen(a)});
        xrtFileWriteAtomic("build/memo-fresh.json",(xbytesview){(const unsigned char*)b,strlen(b)});
    }
    CHECK(a && b && !strcmp(a,b)); xlsSemanticStringFree(a); xlsSemanticStringFree(b);
    CHECK(sameAst(xlsSemanticCheckAst(result),xlsSemanticCheckAst(baseline)));
    CHECK(xlsSemanticCheckInfo(result).accepted==xlsSemanticCheckInfo(baseline).accepted);
    if(xlsSemanticCheckInfo(result).accepted) {
        a=irJson(result); b=irJson(baseline); CHECK(a && b && !strcmp(a,b)); xrtFree(a); xrtFree(b);
    }
    xlsSemanticCheckDestroy(baseline); return 1;
}
static xls_semantic_check* update(xls_check_session* session,xls_analysis_document* docs,size_t count,const char* source)
{
    xls_text* body=xlsTextCreate(source,strlen(source),8u*1024u*1024u,NULL); if(!body) return NULL;
    xlsTextRelease(docs[0].text); docs[0].text=body; ++docs[0].revision;
    char error[256]; xls_semantic_check* result=xlsCheckSessionCheck(session,docs,count,"entry",1,NULL,NULL,error,sizeof(error));
    if(!result) fprintf(stderr,"memo update: %s\n",error);
    return result;
}
static char* source(unsigned value,int extra,int before)
{
    xstrbuf body; xrtStrBufInit(&body); int okay=xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf"));
    const char* main="func main(...) as int\r\n string 中文 = \"a\\0😀\"\r\n int changed = %u\r\n%s return changed\r\nend\r\n";
    if(before) okay=okay && xrtStrBufAppendFormat(&body,main,value,extra ? " int extra = 1\r\n // changed line\r\n" : "");
    for(unsigned i=0;i<60;++i) {
        okay=okay && xrtStrBufAppendFormat(&body,"func helper_%u(int value) as int\r\n int local = value + %u\r\n",i,i);
        for(unsigned j=0;j<12;++j) okay=okay && xrtStrBufAppend(&body,xrtStrView(" local = local + 1\r\n"));
        okay=okay && xrtStrBufAppend(&body,xrtStrView(" return local\r\nend\r\n"));
    }
    if(!before) okay=okay && xrtStrBufAppendFormat(&body,main,value,extra ? " int extra = 1\r\n // changed line\r\n" : "");
    char* result=okay ? xrtStrDup(body.Data) : NULL; xrtStrBufFree(&body); return result;
}
static int corpus(const char* prefix,const char* edited,const char* replacement,size_t expectedChecks)
{
    xls_check_session* session=xlsCheckSessionCreate(NULL); char* path=xrtPathAbs("memo-corpus.xl"); CHECK(session && path);
    xls_analysis_document doc={"entry",path,NULL,1,0};
    xstrbuf body; xrtStrBufInit(&body); CHECK(xrtStrBufAppend(&body,xrtStrView(prefix)) && xrtStrBufAppend(&body,xrtStrView(edited)));
    xls_semantic_check* first=update(session,&doc,1,body.Data); CHECK(first && xlsSemanticCheckInfo(first).accepted && fresh(first,&doc,1));
    xrtStrBufClear(&body); CHECK(xrtStrBufAppend(&body,xrtStrView(prefix)) && xrtStrBufAppend(&body,xrtStrView(replacement)));
    xls_semantic_check* next=update(session,&doc,1,body.Data); CHECK(next && xlsSemanticCheckInfo(next).accepted && fresh(next,&doc,1));
    xls_check_semantic_work work=xlsSemanticCheckWork(next);
    if(work.contextHits!=1 || !work.functionsReused || work.functionsChecked!=expectedChecks)
        fprintf(stderr,"corpus work: checked=%zu reused=%zu context=%zu bytes=%zu\n",work.functionsChecked,work.functionsReused,work.contextHits,work.memoBytes);
    CHECK(work.contextHits==1 && work.functionsReused>0 && work.functionsChecked==expectedChecks);
    CHECK(xlsSemanticCheckValidate(first) && xlsSemanticCheckInfo(first).accepted);
    xlsSemanticCheckDestroy(first); xlsSemanticCheckDestroy(next); xlsCheckSessionDestroy(session); xlsTextRelease(doc.text); xrtFree(path); xrtStrBufFree(&body); return 1;
}
typedef struct reader { unsigned index; int passed; } reader;
static int32 parallel(void* user)
{
    reader* r=user; r->passed=corpus("func choose(int value) as int\n if value > 0 then\n return value\n end\n return 0\nend\n",
        "func main(...) as int\n return 7\nend\n","func main(...) as int\n return 8\nend\n",1); return 0;
}
static int run(void)
{
    char *path=xrtPathAbs("memo-entry.xl"),*dep=xrtPathAbs("memo_dep.xl"); CHECK(path && dep);
    xls_check_session* session=xlsCheckSessionCreate(NULL); CHECK(session);
    xls_analysis_document docs[2]={{"entry",path,NULL,100,0},{"dep",dep,NULL,200,1}};
    xls_semantic_check* original=NULL; xls_check_stats stats; size_t sourceReferences=0;
    for(unsigned i=0;i<20;++i) {
        char* body=source(i,0,0); CHECK(body);
        xls_semantic_check* result=update(session,docs,1,body); CHECK(result && xlsSemanticCheckInfo(result).accepted && fresh(result,docs,1));
        xls_check_semantic_work work=xlsSemanticCheckWork(result);
        if(!i) { original=xlsSemanticCheckRetain(result); sourceReferences=xlsSemanticCheckAst(original)->source->references; CHECK(work.functionsChecked==61 && !work.functionsReused); }
        else CHECK(work.functionsChecked==1 && work.functionsReused==60 && work.contextHits==1);
        CHECK(work.memoBytes && work.memoBytes<=64u*1024u*1024u);
        CHECK(xlsSemanticCheckAst(original)->source->references==sourceReferences);
        xlsSemanticCheckDestroy(result); xrtFree(body);
    }
    CHECK(xlsSemanticCheckInfo(original).accepted); xlsSemanticCheckDestroy(original); xlsCheckSessionDestroy(session);
    session=xlsCheckSessionCreate(NULL); CHECK(session); docs[0].identity=250; docs[0].revision=0;
    for(unsigned i=0;i<12;++i) {
        char* body=source(i,0,1); CHECK(body); xstrbuf shifted; xrtStrBufInit(&shifted);
        const char* end=strstr(body," return changed"); CHECK(end);
        CHECK(xrtStrBufAppend(&shifted,(xstrview){body,(size_t)(end-body)}) &&
            xrtStrBufAppend(&shifted,xrtStrView(i&1 ? " // 中文😀 shifted line\r\n\r\n" : "")) && xrtStrBufAppend(&shifted,xrtStrView(end)));
        xls_semantic_check* r=update(session,docs,1,shifted.Data); CHECK(r && fresh(r,docs,1));
        if(i) CHECK(xlsSemanticCheckWork(r).functionsChecked==1 && xlsSemanticCheckWork(r).functionsReused==60);
        xlsSemanticCheckDestroy(r); xrtStrBufFree(&shifted); xrtFree(body);
    }
    xlsCheckSessionDestroy(session);
    session=xlsCheckSessionCreate(NULL); CHECK(session); docs[0].identity=300; docs[0].revision=0;
    for(unsigned i=0;i<5;++i) {
        char* body=source(i,i>=2,1); CHECK(body);
        xls_semantic_check* result=update(session,docs,1,body); CHECK(result && fresh(result,docs,1));
        xls_check_semantic_work work=xlsSemanticCheckWork(result);
        if(i==1 || i>=3) CHECK(work.functionsChecked==1 && work.functionsReused==60);
        if(i==2) CHECK(work.functionsChecked==61 && !work.functionsReused); /* numbering changed; recheck suffix */
        xlsSemanticCheckDestroy(result); xrtFree(body);
    }
    xls_semantic_check* result=update(session,docs,1,"func helper() as int\n return 1\nend\nfunc main(...) as int\n return \"bad\"\nend\n");
    CHECK(result && !xlsSemanticCheckInfo(result).accepted && fresh(result,docs,1)); xlsSemanticCheckDestroy(result);
    result=update(session,docs,1,"func helper() as int\n return 1\nend\nfunc main(...) as int\n return 2\nend\n");
    CHECK(result && xlsSemanticCheckInfo(result).accepted && fresh(result,docs,1) && xlsSemanticCheckWork(result).functionsReused==0); xlsSemanticCheckDestroy(result);
    const char* depBody="export func value() as int\n return 7\nend\n";
    docs[1].text=xlsTextCreate(depBody,strlen(depBody),1024,NULL);
    CHECK(docs[1].text);
    const char* imported="import memo_dep\nfunc main(...) as int\n return memo_dep.value()\nend\n";
    result=update(session,docs,2,imported); CHECK(result && fresh(result,docs,2)); xlsSemanticCheckDestroy(result);
    xlsTextRelease(docs[1].text); const char* changed="export func value() as int\n return 8\nend\n";
    docs[1].text=xlsTextCreate(changed,strlen(changed),1024,NULL); ++docs[1].revision;
    result=xlsCheckSessionCheck(session,docs,2,"entry",1,NULL,NULL,NULL,0); CHECK(result && fresh(result,docs,2) && !xlsSemanticCheckWork(result).contextHits); xlsSemanticCheckDestroy(result);
    xlsCheckSessionStats(session,&stats); CHECK(stats.semanticFunctionsReused>=180 && stats.semanticContextHits>=4);
    xlsCheckSessionDestroy(session); xlsTextRelease(docs[0].text); xlsTextRelease(docs[1].text); xrtFree(path); xrtFree(dep);
    const char* initial="func main(...) as int\n return 7\nend\n",*next="func main(...) as int\n return 8\nend\n";
    CHECK(corpus("func identity<T>(T value) as T\n return value\nend\nfunc choose(int value) as int\n return identity::<int>(value)\nend\n",initial,next,1));
    CHECK(corpus("class Box\n string value\n func get() as string\n return self.value\n end\nend\nfunc text(Box value) as string\n return value.get()\nend\n",initial,next,2)); /* source-less stdlib type: conservative recheck */
    CHECK(corpus("func values(int count) as generator<string>\n string owned = \"owned\"\n for i in range(count)\n yield owned + string(i)\n end\nend\n",initial,next,1));
    CHECK(corpus("func closure() as func() as int\n array values = [41]\n return func() as int\n return int(values[0])\n end\nend\n",initial,next,1));
    CHECK(corpus("func guard() as int\n try\n string item = \"owned\"\n return 1\n catch e\n return 2\n finally\n string after = \"after\"\n end\nend\n",initial,next,1));
    /* Shared semantic normalization frees the original MEMBER nodes.
     * Such bodies must be checked again, never read through stale pointers. */
    CHECK(corpus("func rewritten(string value) as string\n return value.trim().upper()\nend\nfunc stable(int value) as int\n if value > 0 then\n return value\n end\n return 0\nend\n",initial,next,2));
    /* A reused generic must advance the registry before a later generic miss. */
    CHECK(corpus("func identity<T>(T value) as T\n return value\nend\n",
        "func changed<U>(U value) as U\n return value\nend\nfunc main(...) as int\n return 7\nend\n",
        "func changed<U>(U value) as U\n U copy = value\n return copy\nend\nfunc main(...) as int\n return 7\nend\n",2));
    reader readers[4]; xthread* threads[4];
    for(unsigned i=0;i<4;++i) { readers[i]=(reader){i,0}; threads[i]=xrtThreadCreate(parallel,&readers[i],0); CHECK(threads[i]); }
    for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed); xrtThreadDestroy(threads[i]); }
    return 1;
}
int main(void)
{ xrtAtomic32Init(&checks,0); if(!run()) return 1; printf("analysis_semantic_memo_test passed: %u checks; actual function reuse, whole fresh index/IR parity, identity/coordinate changes, complex bodies, dependency invalidation and concurrent ownership.\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED)); return 0; }
