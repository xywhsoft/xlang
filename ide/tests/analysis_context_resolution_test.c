#include "analysis_test_clock.h"
#include "analysis/worker.h"
#include "analysis/frontend_context.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(call) do { ++checks; if(!(call)) { fprintf(stderr,"analysis_context_resolution_test:%d: %s\n",__LINE__,#call); return 0; } } while(0)
static xls_context* context(const char* project,const char* entry,const char* directory,const char* configuration,const char* package)
{
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration=configuration,.entry=entry,
        .directory=directory,.analysisProfile="fixture-linked-native",.targetKind="exe",.packagePath=package};
    return xlsContextCreate(&spec,1024*1024,NULL);
}
static int writeFile(const char* path,const char* text)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int package(const char* directory,const char* body)
{
    char* folder=xrtPathJoin(directory,"packages/ctx_twin_package");
    char *manifest=folder ? xrtPathJoin(folder,"xlang.package.json") : NULL,*api=folder ? xrtPathJoin(folder,"api.xl") : NULL;
    const char* json="{\"format\":\"xlang-package-1\",\"name\":\"ctx_twin_package\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";
    int okay=folder && manifest && api && xrtDirCreateAll(folder) && writeFile(manifest,json) && writeFile(api,body);
    xrtFree(folder);xrtFree(manifest);xrtFree(api);return okay;
}
static int observed(xls_semantic_check* result,const char* directory,int accepted)
{
    CHECK(result && xlsSemanticCheckInfo(result).accepted==accepted && xlsSemanticCheckValidate(result));
    xls_check_inputs inputs;CHECK(xlsSemanticCheckInputs(result,&inputs) && inputs.packagePathObserved && inputs.packagePath && !strcmp(inputs.packagePath,"packages"));
    char* expected=xrtPathJoin(directory,"packages/ctx_twin_package/api.xl");CHECK(expected);int found=0;
    for(size_t i=0;i<inputs.sources;++i) { xls_check_source source;CHECK(xlsSemanticCheckSource(result,i,&source));if(!strcmp(source.path,expected)) ++found; }
    CHECK(found==1);xrtFree(expected);return 1;
}
static int label(xls_semantic_completion* result,const char* expected,const char* excluded)
{
    CHECK(result && xlsSemanticCompletionInfo(result).available && xlsSemanticCompletionValidate(result));
    int found=0;for(size_t i=0;i<xlsSemanticCompletionInfo(result).count;++i) {xls_completion_item item;CHECK(xlsSemanticCompletionItem(result,i,&item));if(!strcmp(item.label,expected)) ++found;CHECK(strcmp(item.label,excluded));}
    CHECK(found==1);return 1;
}
typedef struct nested_input {xls_context *outer,*inner;const char* entry;xls_semantic_input input;int called,okay;} nested_input;
static int nested(void* user)
{
    nested_input* data=user;
    if(data->called++) return 0;
    data->okay=xlsFrontendContextCurrent()==data->outer;
    xls_semantic_check* result=xlsSemanticCheckCreateInContext(data->inner,data->entry,&data->input,1,0,NULL,NULL,NULL,0);
    data->okay=data->okay && result && !xlsSemanticCheckInfo(result).accepted && xlsFrontendContextCurrent()==data->outer;
    xlsSemanticCheckDestroy(result);
    /* The default API explicitly restores process-based legacy semantics,
     * even when invoked from a context-bearing analysis callback. */
    result=xlsSemanticCheckCreate(data->entry,&data->input,1,0,NULL,NULL,NULL,0);
    data->okay=data->okay && result && !xlsSemanticCheckInfo(result).accepted && !xlsSemanticCheckContext(result) && xlsFrontendContextCurrent()==data->outer;
    xlsSemanticCheckDestroy(result);return 0;
}
static int stop(void* user) { (void)user;return 1; }
typedef struct concurrent_input {xls_context* context;const char* entry;xls_semantic_input input;xatomic32* arrived;int expected,called,okay;} concurrent_input;
static int barrier(void* user)
{
    concurrent_input* data=user;
    if(!data->called++) {
        data->okay=xlsFrontendContextCurrent()==data->context;
        xrtAtomic32FetchAdd(data->arrived,1,XMEMORY_ACQ_REL);
        while(xrtAtomic32Load(data->arrived,XMEMORY_ACQUIRE)<2) xrtThreadYield();
    }
    return 0;
}
static int concurrent(void* user)
{
    concurrent_input* data=user;
    xls_semantic_check* result=xlsSemanticCheckCreateInContext(data->context,data->entry,&data->input,1,1,barrier,data,NULL,0);
    data->okay=data->okay && result && xlsSemanticCheckInfo(result).accepted==data->expected && xlsSemanticCheckValidate(result) && !xlsFrontendContextCurrent();
    xlsSemanticCheckDestroy(result);return 0;
}
static xls_semantic_check* waitCheck(xls_query* query)
{
    xls_semantic_check* result=NULL;char error[256];
    for(size_t i=0;i<30000;++i) {
        enum xls_query_state state=xlsQueryCheckResult(query,&result,NULL,NULL,error,sizeof(error));
        if(state==XLS_QUERY_READY) return result;
        if(state!=XLS_QUERY_PENDING) {fprintf(stderr,"context worker: %s\n",error);return NULL;}
        xrtSleep(1);
    }
    return NULL;
}
static xls_semantic_completion* waitCompletion(xls_query* query)
{
    xls_semantic_completion* result=NULL;char error[256];
    for(size_t i=0;i<30000;++i) {
        enum xls_query_state state=xlsQueryCompletionResult(query,&result,NULL,NULL,error,sizeof(error));
        if(state==XLS_QUERY_READY) return result;
        if(state!=XLS_QUERY_PENDING) {fprintf(stderr,"context cursor worker: %s\n",error);return NULL;}
        xrtSleep(1);
    }
    return NULL;
}
static int run(void)
{
    char root[160];snprintf(root,sizeof(root),"build/context-resolution-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());
    char *directory=xrtPathAbs(root),*a=directory ? xrtPathJoin(directory,"工程 A") : NULL,*b=directory ? xrtPathJoin(directory,"工程 B") : NULL;
    char *shared=directory ? xrtPathJoin(directory,"shared") : NULL,*entry=shared ? xrtPathJoin(shared,"entry.xl") : NULL;
    char *pa=a ? xrtPathJoin(a,"app.xlp") : NULL,*pb=b ? xrtPathJoin(b,"app.xlp") : NULL;
    CHECK(directory && a && b && shared && entry && pa && pb && xrtDirCreateAll(shared));
    CHECK(package(a,"export func value() as int\n return 7\nend\nexport func alpha() as int\n return 1\nend\n"));
    CHECK(package(b,"export func value() as string\n return \"B\"\nend\nexport func beta() as int\n return 2\nend\n"));
    xls_context *ca=context(pa,entry,a,"dev","packages"),*cb=context(pb,entry,b,"dev","packages");CHECK(ca && cb && !xlsContextEqual(ca,cb));
    const char* body="import ctx_twin_package\nfunc main(...) as int\n return ctx_twin_package.value()\nend\n";
    const char* cursor="import ctx_twin_package\nfunc main(...) as int\n ctx_twin_package.\n return 0\nend\n";
    size_t offset=(size_t)(strstr(cursor," ctx_twin_package.")-cursor)+strlen(" ctx_twin_package.");
    xls_semantic_input input={entry,body,strlen(body)};char error[256];
    char* oldEnvironment=xrtEnvGet("XLANG_PACKAGE_PATH"),*cwd=xrtPathCwd(),*wrong=xrtPathJoin(b,"packages");CHECK(cwd && wrong && xrtEnvSet("XLANG_PACKAGE_PATH",wrong));
    xls_semantic_check* first=xlsSemanticCheckCreateInContext(ca,entry,&input,1,1,NULL,NULL,error,sizeof(error));CHECK(observed(first,a,1));
    xls_semantic_check* second=xlsSemanticCheckCreateInContext(cb,entry,&input,1,1,NULL,NULL,error,sizeof(error));CHECK(observed(second,b,0));
    CHECK(xlsSemanticCheckContext(first)==ca && xlsSemanticCheckContext(second)==cb && !xlsFrontendContextCurrent());
    nested_input nesting={ca,cb,entry,input,0,0};xls_semantic_check* result=xlsSemanticCheckCreateInContext(ca,entry,&input,1,1,nested,&nesting,error,sizeof(error));
    CHECK(result && xlsSemanticCheckInfo(result).accepted && nesting.okay && !xlsFrontendContextCurrent());xlsSemanticCheckDestroy(result);
    CHECK(!xlsSemanticCheckCreateInContext(ca,entry,&input,1,1,stop,NULL,error,sizeof(error)) && strstr(error,"cancelled") && !xlsFrontendContextCurrent());
    /* Same entry/key/body/revision, different project context must rebuild. */
    xls_text* text=xlsTextCreate(body,strlen(body),4096,NULL);xls_check_session* checksSession=xlsCheckSessionCreate(NULL);CHECK(text && checksSession);
    xls_analysis_document doc={"entry",entry,text,1,1};
    result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    xls_context* clone=xlsContextCreate(xlsContextSpec(ca),1024*1024,NULL);CHECK(clone && xlsContextEqual(ca,clone));
    result=xlsCheckSessionCheckInContext(checksSession,clone,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    xls_check_stats stats;xlsCheckSessionStats(checksSession,&stats);CHECK(stats.builds==1 && stats.hits==1);
    result=xlsCheckSessionCheckInContext(checksSession,cb,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(result && !xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    CHECK(xrtPathSetCwd(b));result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));int restored=xrtPathSetCwd(cwd);
    CHECK(restored && result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);xlsCheckSessionStats(checksSession,&stats);CHECK(stats.builds==3 && stats.hits==2);
    /* Declared unsupported build options reject atomically, not silently pass. */
    xls_context_spec unsupported=*xlsContextSpec(ca);const char* includes[]={a};unsupported.lists[XLS_CONTEXT_INCLUDES]=(xls_context_strings){includes,1};
    xls_context* bad=xlsContextCreate(&unsupported,1024*1024,NULL);CHECK(bad);
    CHECK(!xlsSemanticCheckCreateInContext(bad,entry,&input,1,1,NULL,NULL,error,sizeof(error)) && strstr(error,"build options"));
    CHECK(!xlsCheckSessionCheckInContext(checksSession,bad,&doc,1,"entry",1,NULL,NULL,error,sizeof(error)));
    result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    xlsCheckSessionStats(checksSession,&stats);CHECK(stats.builds==3 && stats.hits==3);xlsCheckSessionDestroy(checksSession);
    xls_semantic_input completionInput={entry,cursor,strlen(cursor)};
    xls_semantic_completion* ac=xlsSemanticCompletionCreateInContext(ca,entry,entry,offset,&completionInput,1,NULL,NULL,error,sizeof(error));CHECK(label(ac,"alpha","beta"));
    xls_semantic_completion* bc=xlsSemanticCompletionCreateInContext(cb,entry,entry,offset,&completionInput,1,NULL,NULL,error,sizeof(error));CHECK(label(bc,"beta","alpha"));
    CHECK(xlsSemanticCompletionContext(ac)==ca && xlsSemanticCompletionContext(bc)==cb);
    xls_text* cursorText=xlsTextCreate(cursor,strlen(cursor),4096,NULL);xls_completion_session* completionSession=xlsCompletionSessionCreate(NULL);CHECK(cursorText && completionSession);
    xls_analysis_document cursorDoc={"entry",entry,cursorText,1,2};
    xls_semantic_completion* completion=xlsCompletionSessionQueryInContext(completionSession,ca,&cursorDoc,1,"entry","entry",offset,NULL,NULL,error,sizeof(error));CHECK(label(completion,"alpha","beta"));xlsSemanticCompletionDestroy(completion);
    completion=xlsCompletionSessionQueryInContext(completionSession,clone,&cursorDoc,1,"entry","entry",offset,NULL,NULL,error,sizeof(error));CHECK(label(completion,"alpha","beta"));xlsSemanticCompletionDestroy(completion);
    completion=xlsCompletionSessionQueryInContext(completionSession,cb,&cursorDoc,1,"entry","entry",offset,NULL,NULL,error,sizeof(error));CHECK(label(completion,"beta","alpha"));xlsSemanticCompletionDestroy(completion);
    xls_completion_stats completionStats;xlsCompletionSessionStats(completionSession,&completionStats);CHECK(completionStats.builds==2 && completionStats.hits==1);xlsCompletionSessionDestroy(completionSession);
    /* Two active scoped frontend calls meet inside their first checkpoint. */
    for(unsigned round=0;round<8;++round) {
        xatomic32 arrived;xrtAtomic32Init(&arrived,0);
        concurrent_input left={ca,entry,input,&arrived,1,0,0},right={cb,entry,input,&arrived,0,0,0};
        xthread *ta=xrtThreadCreate(concurrent,&left,0),*tb=xrtThreadCreate(concurrent,&right,0);CHECK(ta && tb);
        CHECK(xrtThreadWait(ta)==XWAIT_OK && xrtThreadWait(tb)==XWAIT_OK && left.okay && right.okay);xrtThreadDestroy(ta);xrtThreadDestroy(tb);
    }
    xls_worker *wa=xlsWorkerCreate(NULL,8),*wb=xlsWorkerCreate(NULL,8);CHECK(wa && wb);
    CHECK(!xlsWorkerCheckInContext(wa,bad,&doc,1,"entry"));xlsContextRelease(bad);
    xls_query *qa=xlsWorkerCheckInContext(wa,clone,&doc,1,"entry"),*qb=xlsWorkerCheckInContext(wb,cb,&doc,1,"entry");
    xlsContextRelease(clone);clone=NULL;CHECK(qa && qb);
    xls_query *qac=xlsWorkerCompletionInContext(wa,ca,&cursorDoc,1,"entry","entry",offset),*qbc=xlsWorkerCompletionInContext(wb,cb,&cursorDoc,1,"entry","entry",offset);CHECK(qac && qbc);
    result=waitCheck(qa);xls_semantic_check* other=waitCheck(qb);completion=waitCompletion(qac);xls_semantic_completion* otherCompletion=waitCompletion(qbc);
    CHECK(result && other && completion && otherCompletion);xlsQueryDestroy(qa);xlsQueryDestroy(qb);xlsQueryDestroy(qac);xlsQueryDestroy(qbc);xlsWorkerDestroy(wa);xlsWorkerDestroy(wb);
    CHECK(observed(result,a,1) && observed(other,b,0) && label(completion,"alpha","beta") && label(otherCompletion,"beta","alpha"));
    xlsSemanticCheckDestroy(result);xlsSemanticCheckDestroy(other);xlsSemanticCompletionDestroy(completion);xlsSemanticCompletionDestroy(otherCompletion);
    /* Explicit absent/empty environments never fall back to process B. */
    xls_context* absent=context(pa,entry,a,"absent",NULL);xls_context* empty=context(pa,entry,a,"empty","");CHECK(absent && empty);
    result=xlsSemanticCheckCreateInContext(absent,entry,&input,1,0,NULL,NULL,error,sizeof(error));CHECK(result && !xlsSemanticCheckInfo(result).accepted);
    xls_check_inputs inputs;CHECK(xlsSemanticCheckInputs(result,&inputs) && inputs.packagePathObserved && !inputs.packagePath);xlsSemanticCheckDestroy(result);
    result=xlsSemanticCheckCreateInContext(empty,entry,&input,1,0,NULL,NULL,error,sizeof(error));CHECK(result && !xlsSemanticCheckInfo(result).accepted);
    CHECK(xlsSemanticCheckInputs(result,&inputs) && inputs.packagePathObserved && inputs.packagePath && !*inputs.packagePath);xlsSemanticCheckDestroy(result);
#ifdef _WIN32
    xls_context* ambiguous=context(pa,entry,a,"drive-relative","Z:packages");CHECK(ambiguous);
    CHECK(!xlsSemanticCheckCreateInContext(ambiguous,entry,&input,1,0,NULL,NULL,error,sizeof(error)) && strstr(error,"context path"));xlsContextRelease(ambiguous);
#endif
    CHECK(xrtEnvSet("XLANG_PACKAGE_PATH","unrelated-process-setting"));
    CHECK(xlsSemanticCheckValidate(first) && xlsSemanticCheckValidate(second) && xlsSemanticCompletionValidate(ac) && xlsSemanticCompletionValidate(bc));
    xlsContextRelease(ca);xlsContextRelease(cb);CHECK(xlsSemanticCheckContext(first) && xlsSemanticCompletionContext(ac));
    CHECK(observed(first,a,1) && observed(second,b,0) && label(ac,"alpha","beta") && label(bc,"beta","alpha"));
    xlsContextRelease(absent);xlsContextRelease(empty);xlsSemanticCheckDestroy(first);xlsSemanticCheckDestroy(second);xlsSemanticCompletionDestroy(ac);xlsSemanticCompletionDestroy(bc);xlsTextRelease(text);xlsTextRelease(cursorText);
    char* now=xrtPathCwd();CHECK(now && !strcmp(now,cwd));xrtFree(now);
    CHECK(oldEnvironment ? xrtEnvSet("XLANG_PACKAGE_PATH",oldEnvironment) : xrtEnvRemove("XLANG_PACKAGE_PATH"));
    xrtFree(oldEnvironment);xrtFree(cwd);xrtFree(wrong);xrtFree(directory);xrtFree(a);xrtFree(b);xrtFree(shared);xrtFree(entry);xrtFree(pa);xrtFree(pb);return 1;
}
static int diskEntry(void)
{
    char root[160],error[256];snprintf(root,sizeof(root),"build/context-disk-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());
    char* directory=xrtPathAbs(root);char* entry=directory ? xrtPathJoin(directory,"main.xl") : NULL;
    char* helper=directory ? xrtPathJoin(directory,"helper.xl") : NULL;char* project=directory ? xrtPathJoin(directory,"project.xlp") : NULL;
    const char* mainText="\xef\xbb\xbfimport helper\r\nfunc main(...) as int\r\n return helper.value()\r\nend\r\n";
    const char* good="import ctx_twin_package as tp\nexport func value() as int\n return tp.value()\nend\n";
    const char* cursor="import ctx_twin_package as tp\nexport func value() as int\n tp.\n return 0\nend\n";
    const char* api="export func value() as int\n return 7\nend\nexport func alpha() as int\n return 1\nend\n";
    CHECK(directory && entry && helper && project && xrtDirCreateAll(directory) && writeFile(entry,mainText) && package(directory,api));
    xls_context *ca=context(project,entry,directory,"debug","packages"),*copy=context(project,entry,directory,"debug","packages");CHECK(ca && copy);
    xls_text *text=xlsTextCreate(good,strlen(good),4096,NULL),*cursorText=xlsTextCreate(cursor,strlen(cursor),4096,NULL);CHECK(text && cursorText);
    xls_analysis_document doc={"member",helper,text,10,1},cursorDoc={"member",helper,cursorText,20,1};
    xls_check_session* checksSession=xlsCheckSessionCreate(NULL);xls_completion_session* cursors=xlsCompletionSessionCreate(NULL);CHECK(checksSession && cursors);
    CHECK(!xlsCheckSessionCheck(checksSession,&doc,1,NULL,1,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsCompletionSessionQuery(cursors,&cursorDoc,1,NULL,"member",0,NULL,NULL,error,sizeof(error)));
    xls_semantic_check* first=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(observed(first,directory,1));
    xls_check_inputs input;CHECK(xlsSemanticCheckInputs(first,&input));int found=0;
    for(size_t i=0;i<input.sources;++i) {
        xls_check_source source;CHECK(xlsSemanticCheckSource(first,i,&source));
        if(!strcmp(source.path,entry)) {CHECK(!strcmp(source.origin,"disk") && source.rawSize==strlen(mainText) && source.size==strlen(mainText)-3 && !memcmp(source.text,mainText+3,source.size));++found;}
        if(!strcmp(source.path,helper)) CHECK(!strcmp(source.origin,"overlay"));
    }
    CHECK(found==1);
    xls_semantic_check* result=xlsCheckSessionCheckInContext(checksSession,copy,&doc,1,NULL,1,NULL,NULL,error,sizeof(error));CHECK(result==first);xlsSemanticCheckDestroy(result);
    doc.revision=2;result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,NULL,NULL,error,sizeof(error));CHECK(result==first);xlsSemanticCheckDestroy(result);
    doc.revision=1;CHECK(!xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,NULL,NULL,error,sizeof(error)));doc.revision=2;
    CHECK(!xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,stop,NULL,error,sizeof(error)) && xlsSemanticCheckValidate(first));
    xls_check_stats checkStats;xlsCheckSessionStats(checksSession,&checkStats);CHECK(checkStats.builds==1 && checkStats.hits==2 && !checkStats.entryFullParses && checkStats.documents==1 && checkStats.textBytes==strlen(good));
    size_t offset=(size_t)(strstr(cursor," tp.")-cursor)+4;
    xls_semantic_completion* initial=xlsCompletionSessionQueryInContext(cursors,ca,&cursorDoc,1,NULL,"member",offset,NULL,NULL,error,sizeof(error));CHECK(label(initial,"alpha","beta"));
    xls_semantic_completion* completion=xlsCompletionSessionQueryInContext(cursors,copy,&cursorDoc,1,NULL,"member",offset,NULL,NULL,error,sizeof(error));CHECK(completion==initial);xlsSemanticCompletionDestroy(completion);
    cursorDoc.revision=2;completion=xlsCompletionSessionQueryInContext(cursors,ca,&cursorDoc,1,NULL,"member",offset,NULL,NULL,error,sizeof(error));CHECK(completion==initial);xlsSemanticCompletionDestroy(completion);
    CHECK(!xlsCompletionSessionQueryInContext(cursors,ca,&cursorDoc,1,NULL,"member",offset,stop,NULL,error,sizeof(error)) && xlsSemanticCompletionValidate(initial));
    xls_completion_stats cursorStats;xlsCompletionSessionStats(cursors,&cursorStats);CHECK(cursorStats.builds==1 && cursorStats.hits==2 && !cursorStats.cursorFreshBuilds && cursorStats.cursorFullParses==1);
    /* Atomic replacement with identical bytes changes file identity. Both
       cached results must permanently invalidate, not just compare text. */
    CHECK(writeFile(entry,mainText) && !xlsSemanticCheckValidate(first) && !xlsSemanticCompletionValidate(initial));
    result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,NULL,NULL,error,sizeof(error));CHECK(result && result!=first && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    completion=xlsCompletionSessionQueryInContext(cursors,ca,&cursorDoc,1,NULL,"member",offset,NULL,NULL,error,sizeof(error));CHECK(completion && completion!=initial && label(completion,"alpha","beta"));xlsSemanticCompletionDestroy(completion);
    xlsCheckSessionStats(checksSession,&checkStats);xlsCompletionSessionStats(cursors,&cursorStats);CHECK(checkStats.builds==2 && checkStats.invalidations==1 && cursorStats.builds==2 && cursorStats.invalidations==1);
    CHECK(package(directory,"export func value() as int\n return 7\nend\nexport func gamma() as int\n return 1\nend\n"));
    completion=xlsCompletionSessionQueryInContext(cursors,ca,&cursorDoc,1,NULL,"member",offset,NULL,NULL,error,sizeof(error));CHECK(label(completion,"gamma","alpha"));xlsSemanticCompletionDestroy(completion);
    CHECK(writeFile(entry,"import helper\nfunc main(...) as int\n return \"wrong\"\nend\n"));
    result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,NULL,NULL,error,sizeof(error));
    xls_semantic_input overlay={helper,good,strlen(good)};xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(ca,entry,&overlay,1,1,NULL,NULL,error,sizeof(error));
    CHECK(result && fresh && !xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckInfo(result).diagnostics==xlsSemanticCheckInfo(fresh).diagnostics);
    xlsSemanticCheckDestroy(result);xlsSemanticCheckDestroy(fresh);CHECK(writeFile(entry,mainText));
    /* Switching between an actual open entry and context.entry changes the
       batch, even when all source bytes agree. No fictitious revision/identity. */
    xls_text* entryText=xlsTextCreate(mainText,strlen(mainText),4096,NULL);CHECK(entryText);
    xls_analysis_document opened[]={doc,{"entry",entry,entryText,30,1}};
    result=xlsCheckSessionCheckInContext(checksSession,ca,opened,2,"entry",1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    result=xlsCheckSessionCheckInContext(checksSession,ca,&doc,1,NULL,1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);xlsSemanticCheckDestroy(result);
    xls_worker* actor=xlsWorkerCreate(NULL,4);CHECK(actor && !xlsWorkerCheck(actor,&doc,1,NULL) && !xlsWorkerSyntax(actor,&doc,1,NULL));
    xls_query *q=xlsWorkerCheckInContext(actor,copy,&doc,1,NULL),*qc=xlsWorkerCompletionInContext(actor,copy,&cursorDoc,1,NULL,"member",offset);CHECK(q && qc);
    xlsContextRelease(copy);xlsTextRelease(text);xlsTextRelease(cursorText);xlsTextRelease(entryText);
    result=waitCheck(q);completion=waitCompletion(qc);CHECK(result && completion);xlsQueryDestroy(q);xlsQueryDestroy(qc);xlsWorkerDestroy(actor);
    CHECK(xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckValidate(result) && label(completion,"gamma","alpha"));
    xlsContextRelease(ca);xlsCheckSessionDestroy(checksSession);xlsCompletionSessionDestroy(cursors);
    CHECK(xlsSemanticCheckContext(result) && xlsSemanticCompletionContext(completion) && xlsSemanticCheckValidate(result) && xlsSemanticCompletionValidate(completion));
    xlsSemanticCheckDestroy(first);xlsSemanticCheckDestroy(result);xlsSemanticCompletionDestroy(completion);xlsSemanticCompletionDestroy(initial);
    xrtFree(directory);xrtFree(entry);xrtFree(helper);xrtFree(project);return 1;
}
int main(void) {if(!run() || !diskEntry()) return 1;printf("analysis_context_resolution_test passed: %u checks; same-entry project isolation, real packages, retained disk entry check/cursor proofs, nested/TLS actors and independent lifetimes\n",checks);return 0;}
