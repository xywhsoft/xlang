#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include "analysis/completion_unit.h"
#include "src/sem/xl6_completion.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_completion_session_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static xls_text* body(const char* text) { return xlsTextCreate(text,strlen(text),8u*1024u*1024u,NULL); }
static size_t offset(const char* text,const char* marker)
{ const char* at=strstr(text,marker); size_t bom=strlen(text)>=3 && !memcmp(text,"\xef\xbb\xbf",3) ? 3 : 0; return at ? (size_t)(at-text)+strlen(marker)-bom : SIZE_MAX; }
static xls_semantic_completion* query(xls_completion_session* s,const xls_analysis_document* d,size_t n,const char* entry,const char* target,size_t at)
{
    char error[256]; xls_semantic_completion* r=xlsCompletionSessionQuery(s,d,n,entry,target,at,NULL,NULL,error,sizeof(error));
    if(!r) fprintf(stderr,"completion session: %s\n",error);
    return r;
}
static int has(xls_semantic_completion* r,const char* name)
{
    xls_completion_info info=xlsSemanticCompletionInfo(r);
    for(size_t i=0;i<info.count;++i) { xls_completion_item item; if(xlsSemanticCompletionItem(r,i,&item) && !strcmp(item.label,name)) return 1; }
    return 0;
}
static int parity(xls_semantic_completion* r,const xls_analysis_document* docs,size_t count,const char* entry,const char* target,size_t at)
{
    xls_semantic_input inputs[64]; const char *entryPath=NULL,*targetPath=NULL;
    for(size_t i=0;i<count;++i) {
        inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
        if(!strcmp(docs[i].key,entry)) entryPath=docs[i].path;
        if(!strcmp(docs[i].key,target)) targetPath=docs[i].path;
    }
    xls_semantic_completion* fresh=xlsSemanticCompletionCreate(entryPath,targetPath,at,inputs,count,NULL,NULL,NULL,0);
    char *a=xlsSemanticCompletionJson(r,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);
    CHECK(fresh && a && b && !strcmp(a,b));
    xlsSemanticStringFree(a); xlsSemanticStringFree(b); xlsSemanticCompletionDestroy(fresh); return 1;
}
static int stop(void* user) { (void)user; return 1; }
static int parityContext(xls_semantic_completion* result,xls_context* context,const char* entry,
    const xls_analysis_document* docs,size_t count,const char* path,size_t at)
{
    xls_semantic_input inputs[3];CHECK(count<=3);
    for(size_t i=0;i<count;++i) inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(context,entry,path,at,inputs,count,NULL,NULL,NULL,0);
    char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);
    CHECK(result && fresh && a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCompletionDestroy(fresh);return 1;
}
static int importedCursor(void)
{
    char relative[160];snprintf(relative,sizeof(relative),"build/imported-cursor-%llu",(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(relative),*entry=xrtPathJoin(directory,"main.xl"),*dep=xrtPathJoin(directory,"dep.xl"),*support=xrtPathJoin(directory,"support.xl");
    const char* main="import dep\nfunc main(...) as int\n return dep.value()\nend\n";
    const char* supportSource="export func number() as int\n return 7\nend\n";
    CHECK(directory && entry && dep && support && xrtDirCreateAll(directory) && xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)main,strlen(main)}));
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=entry,.directory=directory,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);CHECK(context);
    xstrbuf prefix={0};CHECK(xrtStrBufAppend(&prefix,xrtStrView("\xef\xbb\xbfimport support\r\n")));
    for(unsigned i=0;i<40;++i) CHECK(xrtStrBufAppendFormat(&prefix,"func helper_%u(int value) as int\r\n return value+1\r\nend\r\n",i));
    for(unsigned opened=0;opened<2;++opened) {
        xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_semantic_completion* initial=NULL;
        xls_analysis_document docs[3]={{.key="dep",.path=dep,.identity=5},{.key="support",.path=support,.text=body(supportSource),.identity=6,.revision=1},{.key="entry",.path=entry,.text=body(main),.identity=1,.revision=1}};CHECK(docs[1].text && docs[2].text);
        size_t at=0,whole=0;
        for(unsigned i=0;i<13;++i) {
            xstrbuf source={0};CHECK(xrtStrBufAppend(&source,(xstrview){prefix.Data,prefix.Size}) && xrtStrBufAppendFormat(&source,"export func value() as int\r\n string 中文 = \"😀-%u\"\r\n int edited = %u\r\n auto member = support.number()\r\n return mem\r\nend\r\n",i,i));
            xlsTextRelease(docs[0].text);docs[0].text=body(source.Data);docs[0].revision=i+1;CHECK(docs[0].text);at=offset(source.Data,"return mem");
            char error[256];xls_semantic_completion* result=xlsCompletionSessionQueryInContext(session,context,docs,2+opened,opened ? "entry" : NULL,"dep",at,NULL,NULL,error,sizeof(error));
            CHECK(result && has(result,"member") && xlsSemanticCompletionCursorSyntaxApplied(result) && parityContext(result,context,entry,docs,2+opened,dep,at));
            xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);
            CHECK(stats.cursorFullParses==1 && stats.cursorFragmentParses==i && stats.cursorReusedDeclarations==41u*i && !stats.cursorFreshBuilds);
            if(!i){whole=source.Size;initial=xlsSemanticCompletionRetain(result);}CHECK(stats.cursorParsedBytes<=whole+i*512u);
            xlsSemanticCompletionDestroy(result);xrtStrBufFree(&source);
        }
        xlsTextRelease(docs[1].text);docs[1].text=body("export func number() as string\n return \"changed\"\nend\n");++docs[1].revision;CHECK(docs[1].text);
        xls_semantic_completion* result=xlsCompletionSessionQueryInContext(session,context,docs,2+opened,opened ? "entry" : NULL,"dep",at,NULL,NULL,NULL,0);
        CHECK(result && has(result,"member") && xlsSemanticCompletionCursorSyntaxApplied(result) && parityContext(result,context,entry,docs,2+opened,dep,at));
        xls_completion_item item;int stringType=0;for(size_t i=0;i<xlsSemanticCompletionInfo(result).count;++i) if(xlsSemanticCompletionItem(result,i,&item) && !strcmp(item.label,"member")) stringType=item.detail && strstr(item.detail,"string");CHECK(stringType);
        xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);CHECK(stats.cursorSyntaxHits==1 && stats.cursorFullParses==1 && stats.cursorFragmentParses==12);xlsSemanticCompletionDestroy(result);
        size_t helperAt=offset(xlsTextData(docs[0].text),"return val");result=xlsCompletionSessionQueryInContext(session,context,docs,2+opened,opened ? "entry" : NULL,"dep",helperAt,NULL,NULL,NULL,0);
        CHECK(result && has(result,"value") && parityContext(result,context,entry,docs,2+opened,dep,helperAt));xlsSemanticCompletionDestroy(result);xlsCompletionSessionStats(session,&stats);CHECK(stats.cursorFullParses==2);
        ++docs[0].identity;docs[0].revision=1;result=xlsCompletionSessionQueryInContext(session,context,docs,2+opened,opened ? "entry" : NULL,"dep",at,NULL,NULL,NULL,0);
        CHECK(result && has(result,"member") && parityContext(result,context,entry,docs,2+opened,dep,at));xlsSemanticCompletionDestroy(result);xlsCompletionSessionStats(session,&stats);CHECK(stats.cursorFullParses==3 && !stats.cursorFreshBuilds);
        xlsCompletionSessionDestroy(session);CHECK(initial && has(initial,"member") && xlsSemanticCompletionValidate(initial));xlsSemanticCompletionDestroy(initial);
        for(unsigned i=0;i<3;++i)xlsTextRelease(docs[i].text);
    }
    /* Rejected imported projections own their error after syntax/input release. */
    const char* rejected="export func value() as int\n int member = 7\n return mem\nend\n";xls_text* text=body(rejected);CHECK(text);size_t at=offset(rejected,"member = ");XL6_CompletionQuery projection={.path=dep,.offset=at};xls_parse_work work={0};xls_syntax* syntax=xlsSyntaxBuildCursor(dep,text,NULL,&projection,&work);CHECK(syntax && projection.error);xl6CompletionFree(&projection);
    xls_semantic_input inputs[]={{entry,main,strlen(main)},{dep,rejected,strlen(rejected)}};xls_semantic_completion* result=xlsSemanticCompletionCreateParsedInContext(context,entry,dep,at,inputs,2,syntax,NULL,NULL,NULL,0);
    CHECK(result && !xlsSemanticCompletionInfo(result).available && xlsSemanticCompletionCursorSyntaxApplied(result));
    CHECK(!xlsSemanticCompletionCreateParsedInContext(context,entry,dep,at-1,inputs,2,syntax,NULL,NULL,NULL,0));xlsSyntaxRelease(syntax);xlsTextRelease(text);
    CHECK(xlsSemanticCompletionInfo(result).error && strstr(xlsSemanticCompletionInfo(result).error,"identifier"));char* json=xlsSemanticCompletionJson(result,NULL);CHECK(json);xlsSemanticStringFree(json);xlsSemanticCompletionDestroy(result);
    xrtStrBufFree(&prefix);xlsContextRelease(context);xrtFree(directory);xrtFree(entry);xrtFree(dep);xrtFree(support);return 1;
}
static int lateStop(void* user) { unsigned* calls=user; return ++*calls>=4; }
static int externalInputs(void)
{
    char root[160]; snprintf(root,sizeof(root),"build/cursor-session-%llu-%llu",
        (unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock()); CHECK(xrtDirCreateAll(root));
    char *relative=xrtPathJoin(root,"entry.xl"),*disk=xrtPathJoin(root,"cursor_dep.xl"),
        *extra=xrtPathJoin(root,"extra.xl"),*missing=xrtPathJoin(root,"absent_cursor_dep.xl");
    char *path=relative ? xrtPathAbs(relative) : NULL,*extraPath=extra ? xrtPathAbs(extra) : NULL;
    CHECK(path && extraPath && disk && missing);
    const char* dependency="export class Box\n int member\nend\n";
    CHECK(xrtFileWriteAtomic(disk,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    const char* source="import cursor_dep\nfunc main(...) as int\n auto box = cursor_dep.Box()\n return box.\nend\n";
    const char* absent="import absent_cursor_dep\nfunc main(...) as int\n auto box = absent_cursor_dep.Box()\n return box.\nend\n";
    const char* unknown="import nonexistent_cursor_package\nfunc main(...) as int\n int member = 7\n return mem\nend\n";
    xls_text *text=body(source),*unrelated=body("func extra() as int\n return 0\nend\n"),*notFound=body(absent),*package=body(unknown);
    xls_completion_session* s=xlsCompletionSessionCreate(NULL); CHECK(s && text && unrelated && notFound && package);
    xls_analysis_document docs[2]={{"entry",path,text,10,1},{"extra",extraPath,unrelated,20,1}};
    size_t at=offset(source,"return box."); xls_semantic_completion* first=query(s,docs,1,"entry","entry",at);
    CHECK(first && has(first,"member") && parity(first,docs,1,"entry","entry",at));
    xls_semantic_completion* r=query(s,docs,1,"entry","entry",at); CHECK(r==first); xlsSemanticCompletionDestroy(r);
    CHECK(xrtFileWriteAtomic(disk,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    r=query(s,docs,1,"entry","entry",at);
    CHECK(r && r!=first && has(r,"member") && !xlsSemanticCompletionValidate(first) && parity(r,docs,1,"entry","entry",at));
    xls_completion_stats stats; xlsCompletionSessionStats(s,&stats); CHECK(stats.builds==2 && stats.hits==1 && stats.invalidations==1);
    xlsSemanticCompletionDestroy(first); first=r;
    r=query(s,docs,2,"entry","entry",at); CHECK(r && r==first && has(r,"member") && parity(r,docs,2,"entry","entry",at));
    xlsSemanticCompletionDestroy(first); first=r;
    r=query(s,docs,1,"entry","entry",at); CHECK(r && r==first && has(r,"member") && parity(r,docs,1,"entry","entry",at)); xlsSemanticCompletionDestroy(first); first=r;
    docs[0].key="renamed";
    r=query(s,docs,1,"renamed","renamed",at); CHECK(r && r!=first && parity(r,docs,1,"renamed","renamed",at));
    xlsSemanticCompletionDestroy(first); first=r;
    char *cwd=xrtPathCwd(),*directory=xrtPathAbs(root); CHECK(cwd && directory && xrtPathSetCwd(directory));
    r=query(s,docs,1,"renamed","renamed",at); int restored=xrtPathSetCwd(cwd);
    CHECK(restored && r && r!=first && has(r,"member")); xlsSemanticCompletionDestroy(first); xlsSemanticCompletionDestroy(r);
    docs[0].text=notFound; docs[0].revision=2; at=offset(absent,"return box.");
    first=query(s,docs,1,"renamed","renamed",at); CHECK(first && !xlsSemanticCompletionInfo(first).available);
    r=query(s,docs,1,"renamed","renamed",at); CHECK(r==first); xlsSemanticCompletionDestroy(r);
    CHECK(xrtFileWriteAtomic(missing,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    r=query(s,docs,1,"renamed","renamed",at);
    CHECK(r && r!=first && has(r,"member") && !xlsSemanticCompletionValidate(first) && parity(r,docs,1,"renamed","renamed",at));
    xlsSemanticCompletionDestroy(first); xlsSemanticCompletionDestroy(r);
    docs[0].text=package; docs[0].revision=3; at=offset(unknown,"return mem");
    first=query(s,docs,1,"renamed","renamed",at); CHECK(first && !xlsSemanticCompletionInfo(first).available);
    char* environment=xrtEnvGet("XLANG_PACKAGE_PATH"); CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",directory));
    r=query(s,docs,1,"renamed","renamed",at);
    restored=environment ? xrtEnvSet("XLANG_PACKAGE_PATH",environment) : xrtEnvRemove("XLANG_PACKAGE_PATH");
    CHECK(restored && r && r!=first && !xlsSemanticCompletionValidate(first));
    xlsCompletionSessionStats(s,&stats); CHECK(stats.invalidations==3 && stats.documents==1 && stats.textBytes==strlen(unknown));
    xlsSemanticCompletionDestroy(first); xlsSemanticCompletionDestroy(r); xlsCompletionSessionDestroy(s);
    xlsTextRelease(text); xlsTextRelease(unrelated); xlsTextRelease(notFound); xlsTextRelease(package);
    xrtFree(relative); xrtFree(disk); xrtFree(extra); xrtFree(missing); xrtFree(path); xrtFree(extraPath); xrtFree(cwd); xrtFree(directory); xrtFree(environment);
    return 1;
}

static int parsedEntry(void)
{
    char *path=xrtPathAbs("parsed-cursor-entry.xl"),*other=xrtPathAbs("parsed-cursor-other.xl");
    char* source=xrtStrDup("func main(...) as int\n int result = 7\n return res\nend\n");CHECK(path&&other&&source);
    xls_text* text=body(source);CHECK(text);size_t at=offset(source,"return res");
    XL6_CompletionQuery q={.path=path,.offset=at};xls_parse_work work={0};
    xls_syntax* syntax=xlsSyntaxBuildCursor(path,text,NULL,&q,&work);CHECK(syntax);xl6CompletionFree(&q);
    xls_semantic_input input={path,source,strlen(source)};char error[256];
    xls_semantic_completion* result=xlsSemanticCompletionCreateParsed(path,path,at,&input,1,syntax,NULL,NULL,error,sizeof(error));
    CHECK(result&&has(result,"result"));xls_analysis_document doc={"entry",path,text,1,1};CHECK(parity(result,&doc,1,"entry","entry",at));
    /* URI paths can use forward separators while XRT's absolute paths use
     * native separators. Selection uses the compiler overlay's own identity. */
    char* slash=xrtStrDup(path);CHECK(slash);for(char* c=slash;*c;++c)if(*c=='\\')*c='/';
    xls_semantic_completion* normalized=xlsSemanticCompletionCreateParsed(slash,slash,at,&input,1,syntax,NULL,NULL,error,sizeof(error));
    CHECK(normalized&&has(normalized,"result")&&parity(normalized,&doc,1,"entry","entry",at));xlsSemanticCompletionDestroy(normalized);xrtFree(slash);
    CHECK(!xlsSemanticCompletionCreateParsed(path,path,at-1,&input,1,syntax,NULL,NULL,error,sizeof(error))&&*error);
    char* changed=xrtStrDup(source);CHECK(changed);char* value=strstr(changed,"= 7");CHECK(value);value[2]='8';input.text=changed;
    CHECK(!xlsSemanticCompletionCreateParsed(path,path,at,&input,1,syntax,NULL,NULL,error,sizeof(error))&&*error);input.text=source;xrtFree(changed);
    input.path=other;CHECK(!xlsSemanticCompletionCreateParsed(other,other,at,&input,1,syntax,NULL,NULL,error,sizeof(error))&&*error);input.path=path;
    work=(xls_parse_work){0};xls_syntax* ordinary=xlsSyntaxBuild(path,text,NULL,&work);CHECK(ordinary);
    CHECK(!xlsSemanticCompletionCreateParsed(path,path,at,&input,1,ordinary,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCheckCreateParsed(path,&input,1,0,syntax,NULL,NULL,error,sizeof(error))&&strstr(error,"cursor syntax"));
    CHECK(!xlsSemanticCompletionCreateParsed(path,path,at,&input,1,syntax,stop,NULL,error,sizeof(error))&&strstr(error,"cancelled"));
    char* expected=xlsSemanticCompletionJson(result,NULL);CHECK(expected);
    xlsSyntaxRelease(ordinary);xlsSyntaxRelease(syntax);xlsTextRelease(text);memset(source,'?',strlen(source));xrtFree(source);xrtFree(path);
    char* actual=xlsSemanticCompletionJson(result,NULL);CHECK(actual&&!strcmp(actual,expected)&&has(result,"result"));
    xlsSemanticStringFree(actual);xlsSemanticStringFree(expected);xlsSemanticCompletionDestroy(result);
    /* Query errors belong to the result after the source snapshot is gone. */
    path=xrtPathAbs("parsed-cursor-rejected.xl");source=xrtStrDup("func main(...) as int\n int result = 7\n return res\nend\n");CHECK(path&&source);text=body(source);CHECK(text);at=offset(source,"result = ");
    q=(XL6_CompletionQuery){.path=path,.offset=at};work=(xls_parse_work){0};syntax=xlsSyntaxBuildCursor(path,text,NULL,&q,&work);CHECK(syntax&&!xlsSyntaxAccepted(syntax)&&q.error);xl6CompletionFree(&q);
    input=(xls_semantic_input){path,source,strlen(source)};result=xlsSemanticCompletionCreateParsed(path,path,at,&input,1,syntax,NULL,NULL,error,sizeof(error));
    CHECK(result&&!xlsSemanticCompletionInfo(result).available);doc=(xls_analysis_document){"entry",path,text,1,1};CHECK(parity(result,&doc,1,"entry","entry",at));
    xlsSyntaxRelease(syntax);xlsTextRelease(text);xrtFree(source);xrtFree(path);
    CHECK(xlsSemanticCompletionInfo(result).error&&strstr(xlsSemanticCompletionInfo(result).error,"identifier"));
    actual=xlsSemanticCompletionJson(result,NULL);CHECK(actual);xlsSemanticStringFree(actual);xlsSemanticCompletionDestroy(result);xrtFree(other);return 1;
}
static int incrementalEntry(void)
{
    xstrbuf source;xrtStrBufInit(&source);
    for(unsigned i=0;i<80;++i)CHECK(xrtStrBufAppendFormat(&source,"func helper_%u(int value) as int\n return value + 1\nend\n",i));
    size_t tail=source.Size;CHECK(xrtStrBufAppend(&source,xrtStrView("func main(...) as int\n int result = 7\n return res\nend\n")));
    char* path=xrtPathAbs("cursor-incremental-session.xl");CHECK(path);xls_completion_session* s=xlsCompletionSessionCreate(NULL);CHECK(s);
    xls_analysis_document doc={"entry",path,body(source.Data),40,1};CHECK(doc.text);size_t at=offset(source.Data,"return res");
    xls_semantic_completion* first=query(s,&doc,1,"entry","entry",at);CHECK(first&&has(first,"result")&&parity(first,&doc,1,"entry","entry",at));
    xls_completion_stats stats;xlsCompletionSessionStats(s,&stats);CHECK(stats.cursorFullParses==1&&!stats.cursorFreshBuilds);
    size_t fullBytes=source.Size;
    for(unsigned i=0;i<30;++i) {
        xstrbuf changed;xrtStrBufInit(&changed);CHECK(xrtStrBufAppend(&changed,(xstrview){source.Data,tail})&&xrtStrBufAppendFormat(&changed,"func main(...) as int\n string 中文 = \"😀-%u\"\n%s int result = %u\n return res\nend\n",i,i&1 ? "\n" : "",i+8));
        xlsTextRelease(doc.text);doc.text=body(changed.Data);++doc.revision;CHECK(doc.text);at=offset(changed.Data,"return res");
        xls_semantic_completion* result=query(s,&doc,1,"entry","entry",at);CHECK(result&&has(result,"result")&&parity(result,&doc,1,"entry","entry",at));
        xlsCompletionSessionStats(s,&stats);CHECK(stats.cursorFullParses==1&&stats.cursorFragmentParses==i+1&&stats.cursorReusedDeclarations==(i+1)*80u&&!stats.cursorFreshBuilds);
        CHECK(stats.cursorParsedBytes<fullBytes+(i+1)*256u);xlsSemanticCompletionDestroy(result);xrtStrBufFree(&changed);
    }
    CHECK(has(first,"result"));
    xls_semantic_completion* same=query(s,&doc,1,"entry","entry",at);CHECK(same);xlsSemanticCompletionDestroy(same);
    xlsCompletionSessionStats(s,&stats);CHECK(stats.hits==1&&stats.cursorFragmentParses==30);
    same=query(s,&doc,1,"entry","entry",at-1);CHECK(same&&parity(same,&doc,1,"entry","entry",at-1));xlsSemanticCompletionDestroy(same);
    xlsCompletionSessionStats(s,&stats);CHECK(stats.cursorFullParses==2&&!stats.cursorFreshBuilds);
    doc.identity=41;doc.revision=1;same=query(s,&doc,1,"entry","entry",at);CHECK(same);xlsSemanticCompletionDestroy(same);
    xlsCompletionSessionStats(s,&stats);CHECK(stats.cursorFullParses==3);
    xlsCompletionSessionDestroy(s);CHECK(has(first,"result"));xlsSemanticCompletionDestroy(first);xlsTextRelease(doc.text);xrtStrBufFree(&source);xrtFree(path);return 1;
}
typedef struct stop_at {unsigned calls,at;} stop_at;
static int stopAt(void* user) {stop_at* s=user;return ++s->calls>=s->at;}
static int cancellationSyntax(void)
{
    const char *a="func main(...) as int\n int result = 7\n return res\nend\n",*b="func main(...) as int\n int result = 8\n return res\nend\n";
    char* path=xrtPathAbs("cursor-cancel-syntax.xl");CHECK(path);xls_text *old=body(a),*next=body(b);CHECK(old&&next);size_t at=offset(a,"return res");
    xls_completion_session* probe=xlsCompletionSessionCreate(NULL);CHECK(probe);xls_analysis_document observed={"entry",path,old,10,1};
    xls_semantic_completion* base=query(probe,&observed,1,"entry","entry",at);CHECK(base);observed.text=next;observed.revision=2;
    stop_at checkpoints={0,~0u};xls_semantic_completion* completed=xlsCompletionSessionQuery(probe,&observed,1,"entry","entry",at,stopAt,&checkpoints,NULL,0);
    CHECK(completed&&checkpoints.calls);unsigned points=checkpoints.calls;
    xlsSemanticCompletionDestroy(base);xlsSemanticCompletionDestroy(completed);xlsCompletionSessionDestroy(probe);
    for(unsigned stopCount=1;stopCount<=points;++stopCount) {
        xls_completion_session* s=xlsCompletionSessionCreate(NULL);CHECK(s);xls_analysis_document doc={"entry",path,old,10,1};
        xls_semantic_completion* first=query(s,&doc,1,"entry","entry",at);CHECK(first&&has(first,"result"));
        doc.text=next;doc.revision=2;stop_at cancel={0,stopCount};
        xls_semantic_completion* stopped=xlsCompletionSessionQuery(s,&doc,1,"entry","entry",at,stopAt,&cancel,NULL,0);
        CHECK(!stopped&&cancel.calls>=stopCount);doc.text=old;doc.revision=1;
        xls_semantic_completion* restored=query(s,&doc,1,"entry","entry",at);CHECK(restored==first&&has(restored,"result"));
        xlsSemanticCompletionDestroy(restored);xlsSemanticCompletionDestroy(first);xlsCompletionSessionDestroy(s);
    }
    printf("cursor syntax cancellation checkpoints=%u, each preserves previous batch/result\n",points);
    xlsTextRelease(old);xlsTextRelease(next);xrtFree(path);return 1;
}
static int cloneFallback(void)
{
    xstrbuf source;xrtStrBufInit(&source);CHECK(xrtStrBufAppend(&source,xrtStrView("func main(...) as int\n int result = 7\n int deepValue = 1")));
    for(unsigned i=0;i<550;++i)CHECK(xrtStrBufAppend(&source,xrtStrView(" + 1")));
    CHECK(xrtStrBufAppend(&source,xrtStrView("\n return res\nend\n")));char* path=xrtPathAbs("cursor-clone-fallback.xl");CHECK(path);
    xls_completion_session* s=xlsCompletionSessionCreate(NULL);xls_analysis_document doc={"entry",path,body(source.Data),1,1};CHECK(s&&doc.text);
    size_t at=offset(source.Data,"return res");xls_semantic_completion* result=query(s,&doc,1,"entry","entry",at);
    CHECK(result&&has(result,"result")&&parity(result,&doc,1,"entry","entry",at));
    xls_completion_stats stats;xlsCompletionSessionStats(s,&stats);CHECK(stats.cursorFullParses==1&&stats.cursorFreshBuilds==1);
    xls_semantic_completion* again=query(s,&doc,1,"entry","entry",at);CHECK(again==result);xlsSemanticCompletionDestroy(again);
    xlsCompletionSessionDestroy(s);CHECK(has(result,"result"));xlsSemanticCompletionDestroy(result);xlsTextRelease(doc.text);xrtFree(path);xrtStrBufFree(&source);return 1;
}

static int run(void)
{
    const char *good="func main(...) as int\n int firstValue = 7\n return firstV\nend\n",
        *changed="func main(...) as int\n int firstValue = 8\n return firstV\nend\n";
    size_t at=offset(good,"return firstV"); char *path=xrtPathAbs("cursor-session.xl"),*dep=xrtPathAbs("cursor_dep.xl");
    xls_text *a=body(good),*same=body(good),*b=body(changed); CHECK(path && dep && a && same && b);
    xls_completion_session* s=xlsCompletionSessionCreate(NULL); CHECK(s);
    xls_analysis_document docs[2]={{"entry",path,a,10,1},{0}};
    xls_semantic_completion* first=query(s,docs,1,"entry","entry",at); CHECK(first && has(first,"firstValue"));
    for(unsigned i=0;i<100;++i) {
        docs[0].text=i&1 ? a : same; xls_semantic_completion* r=query(s,docs,1,"entry","entry",at);
        CHECK(r==first); xlsSemanticCompletionDestroy(r);
    }
    docs[0].revision=2; xls_semantic_completion* r=query(s,docs,1,"entry","entry",at); CHECK(r==first); xlsSemanticCompletionDestroy(r);
    xls_completion_stats stats; xlsCompletionSessionStats(s,&stats);
    CHECK(stats.builds==1 && stats.hits==101 && stats.documents==1 && stats.textBytes==strlen(good));
    unsigned calls=0; docs[0].revision=99;
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",at,lateStop,&calls,NULL,0) && calls==4);
    docs[0].revision=2; r=query(s,docs,1,"entry","entry",at); CHECK(r==first); xlsSemanticCompletionDestroy(r);
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",at,stop,NULL,NULL,0));
    docs[0].revision=1; CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",at,NULL,NULL,NULL,0));
    docs[0].revision=2; docs[0].text=b; CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",at,NULL,NULL,NULL,0));
    docs[0].text=a; r=query(s,docs,1,"entry","entry",at-1); CHECK(r && r!=first && parity(r,docs,1,"entry","entry",at-1)); xlsSemanticCompletionDestroy(r);
    r=query(s,docs,1,"entry","entry",at); CHECK(r && r!=first && xlsSemanticCompletionUnitWork(r).projected &&
        !xlsSemanticCompletionSemWork(r).checked && parity(r,docs,1,"entry","entry",at)); xlsSemanticCompletionDestroy(r);
    docs[0].text=b; docs[0].revision=3; r=query(s,docs,1,"entry","entry",at);
    CHECK(r && r!=first && has(r,"firstValue") && parity(r,docs,1,"entry","entry",at)); xlsSemanticCompletionDestroy(r);
    docs[0].identity=11; docs[0].revision=1; r=query(s,docs,1,"entry","entry",at); CHECK(r && r!=first); xlsSemanticCompletionDestroy(r);
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"absent","entry",at,NULL,NULL,NULL,0));
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","absent",at,NULL,NULL,NULL,0));
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",strlen(good)+1,NULL,NULL,NULL,0));
    docs[1]=docs[0]; CHECK(!xlsCompletionSessionQuery(s,docs,2,"entry","entry",at,NULL,NULL,NULL,0));
    xlsCompletionSessionStats(s,&stats); CHECK(stats.builds==3 && stats.wordViews==2 && stats.builds+stats.wordViews==5 && stats.wordProofNodes && stats.hits==102);

    const char* entry="import cursor_dep\nfunc main(...) as int\n return cursor_dep.value()\nend\n";
    const char* dependency="export func value() as int\n int member = 7\n return mem\nend\n";
    xls_text *imported=body(entry),*dependencyBody=body(dependency); CHECK(imported && dependencyBody);
    docs[0]=(xls_analysis_document){"entry",path,imported,11,2}; docs[1]=(xls_analysis_document){"dep",dep,dependencyBody,20,31};
    size_t member=offset(dependency,"return mem"); r=query(s,docs,2,"entry","dep",member);
    CHECK(r && has(r,"member") && parity(r,docs,2,"entry","dep",member));
    xls_analysis_document reversed[2]={docs[1],docs[0]}; xls_semantic_completion* again=query(s,reversed,2,"entry","dep",member);
    CHECK(again==r); xlsSemanticCompletionDestroy(again); xlsSemanticCompletionDestroy(r);
    docs[0].revision=3; docs[1].revision=32; r=query(s,docs,2,"entry","dep",member); CHECK(r && has(r,"member")); xlsSemanticCompletionDestroy(r);
    const char* unicode="\xef\xbb\xbf" "func main(...) as int\r\n string 结果 = \"😀\"\r\n return 结\r\nend\r\n";
    xls_text* u=body(unicode); CHECK(u); docs[0]=(xls_analysis_document){"entry",path,u,30,1};
    size_t scalar=offset(unicode,"return 结"); r=query(s,docs,1,"entry","entry",scalar);
    CHECK(r && has(r,"结果") && parity(r,docs,1,"entry","entry",scalar));
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",scalar-1,NULL,NULL,NULL,0));
    xlsCompletionSessionDestroy(s); CHECK(xlsSemanticCompletionValidate(r) && has(r,"结果") && has(first,"firstValue"));
    xlsSemanticCompletionDestroy(first); xlsSemanticCompletionDestroy(r);
    s=xlsCompletionSessionCreate(&(xls_session_limits){1,32,32}); CHECK(s);
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",scalar,NULL,NULL,NULL,0));
    xlsCompletionSessionDestroy(s); CHECK(!xlsCompletionSessionCreate(&(xls_session_limits){65,32,64}));
    xlsTextRelease(a); xlsTextRelease(same); xlsTextRelease(b); xlsTextRelease(imported); xlsTextRelease(dependencyBody); xlsTextRelease(u); xrtFree(path); xrtFree(dep);
    CHECK(externalInputs()&&parsedEntry()&&incrementalEntry()&&cancellationSyntax()&&cloneFallback()&&importedCursor()); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_completion_session_test passed: %u checks; actual exact-query reuse, fresh JSON parity, atomic cancellation, revisions/identity/input-set/cursor and independent result lifetime.\n",checks); return 0; }
