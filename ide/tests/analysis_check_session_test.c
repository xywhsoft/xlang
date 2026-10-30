#include "analysis_test_clock.h"
#include "analysis/check_session.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_check_session_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static xls_text* body(const char* text) { return xlsTextCreate(text,strlen(text),8u*1024u*1024u,NULL); }
static int stop(void* user) { (void)user; return 1; }
static xls_semantic_check* query(xls_check_session* s,const xls_analysis_document* docs,size_t count,const char* target)
{
    char error[256]; xls_semantic_check* r=xlsCheckSessionCheck(s,docs,count,target,1,NULL,NULL,error,sizeof(error));
    if(!r) fprintf(stderr,"session query: %s\n",error);
    return r;
}
typedef struct reader { xls_semantic_check* result; unsigned failures; } reader;
static int32 readResult(void* user)
{
    reader* r=user;
    for(unsigned i=0;i<30;++i) {
        const char* index=xlsSemanticCheckIndex(r->result); char* json=xlsSemanticCheckJson(r->result,NULL);
        if(!index || !json || !xlsSemanticCheckValidate(r->result) || !xlsSemanticCheckInfo(r->result).accepted) ++r->failures;
        xlsSemanticStringFree(json);
    }
    xlsSemanticCheckDestroy(r->result); return 0;
}
static int run(void)
{
    const char* good="func main(...) as int\n int first = 7\n return first\nend\n";
    const char* changed="func main(...) as int\n int second = 9\n return second\nend\n";
    char *path=xrtPathAbs("session-memory.xl"),*dep=xrtPathAbs("session_dep.xl");
    xls_text *a=body(good),*same=body(good),*b=body(changed); CHECK(path && dep && a && same && b);
    xls_check_session* s=xlsCheckSessionCreate(NULL); CHECK(s);
    xls_analysis_document docs[2]={{"entry",path,a,10,1},{0}};
    xls_semantic_check* first=query(s,docs,1,"entry"); CHECK(first && xlsSemanticCheckInfo(first).hasIndex);
    xls_check_stats stats;
    for(unsigned i=0;i<100;++i) {
        docs[0].text=i&1 ? a : same;
        xls_semantic_check* r=query(s,docs,1,"entry"); CHECK(r==first); xlsSemanticCheckDestroy(r);
    }
    docs[0].revision=2; xls_semantic_check* r=query(s,docs,1,"entry"); CHECK(r==first); xlsSemanticCheckDestroy(r);
    xlsCheckSessionStats(s,&stats); CHECK(stats.builds==1 && stats.hits==101 && stats.documents==1 && stats.textBytes==strlen(good));
    docs[0].revision=1; CHECK(!xlsCheckSessionCheck(s,docs,1,"entry",1,NULL,NULL,NULL,0));
    docs[0].revision=2; docs[0].text=b; CHECK(!xlsCheckSessionCheck(s,docs,1,"entry",1,NULL,NULL,NULL,0));
    docs[0].revision=3; r=query(s,docs,1,"entry"); CHECK(r && r!=first && xlsSemanticCheckInfo(r).accepted);
    xls_semantic_input input={path,changed,strlen(changed)};
    xls_semantic_check* fresh=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);
    char *cachedJson=xlsSemanticCheckJson(r,NULL),*freshJson=xlsSemanticCheckJson(fresh,NULL);
    CHECK(cachedJson && freshJson && !strcmp(cachedJson,freshJson));
    xlsSemanticStringFree(cachedJson); xlsSemanticStringFree(freshJson); xlsSemanticCheckDestroy(fresh); xlsSemanticCheckDestroy(r);
    CHECK(strstr(xlsSemanticCheckIndex(first),"first") && !strstr(xlsSemanticCheckIndex(first),"second"));
    docs[0].identity=20; docs[0].revision=1; r=query(s,docs,1,"entry"); CHECK(r && r!=first); xlsSemanticCheckDestroy(r);
    CHECK(!xlsCheckSessionCheck(s,docs,1,"entry",1,stop,NULL,NULL,0));
    xlsCheckSessionStats(s,&stats); CHECK(stats.builds==3 && stats.hits==101);
    CHECK(!xlsCheckSessionCheck(s,docs,1,"absent",1,NULL,NULL,NULL,0));
    docs[1]=docs[0]; CHECK(!xlsCheckSessionCheck(s,docs,2,"entry",1,NULL,NULL,NULL,0));
    const char* entry="import session_dep\nfunc main(...) as int\n return session_dep.value()\nend\n";
    const char* dependency="export func value() as int\n return 7\nend\n";
    xls_text *imported=body(entry),*dependencyBody=body(dependency),*bad=body("export func value() as int\n return \"bad\"\nend\n"); CHECK(imported && dependencyBody && bad);
    docs[0]=(xls_analysis_document){"entry",path,imported,20,2}; docs[1]=(xls_analysis_document){"dep",dep,dependencyBody,30,1};
    r=query(s,docs,2,"entry"); CHECK(r && xlsSemanticCheckInfo(r).accepted);
    xls_analysis_document reversed[2]={docs[1],docs[0]}; xls_semantic_check* again=query(s,reversed,2,"entry"); CHECK(again==r); xlsSemanticCheckDestroy(again); xlsSemanticCheckDestroy(r);
    docs[1].text=bad; docs[1].revision=2; r=query(s,docs,2,"entry");
    CHECK(r && !xlsSemanticCheckInfo(r).accepted && xlsSemanticCheckInfo(r).diagnostics==1 && !xlsSemanticCheckTypedIndex(r));
    again=query(s,docs,2,"entry"); CHECK(again==r); xlsSemanticCheckDestroy(again); xlsSemanticCheckDestroy(r);
    xlsCheckSessionDestroy(s);

    /* Independently retained results support simultaneous exports/validation,
     * even after the session that built them has been destroyed. */
    reader readers[4]; xthread* threads[4];
    for(unsigned i=0;i<4;++i) { readers[i]=(reader){xlsSemanticCheckRetain(first),0}; threads[i]=xrtThreadCreate(readResult,&readers[i],0); CHECK(threads[i]); }
    for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !readers[i].failures); xrtThreadDestroy(threads[i]); }
    xlsSemanticCheckDestroy(first);

    /* Real dependency replacement and negative resolution candidates. The
     * old lease must be permanently invalidated, not silently reused. */
    char root[160]; snprintf(root,sizeof(root),"build/check-session-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros()); CHECK(xrtDirCreateAll(root));
    char *entryPath=xrtPathJoin(root,"entry.xl"),*disk=xrtPathJoin(root,"session_dep.xl");
    char *absolute=entryPath ? xrtPathAbs(entryPath) : NULL; CHECK(absolute && disk);
    CHECK(xrtFileWriteAtomic(disk,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    s=xlsCheckSessionCreate(NULL); CHECK(s); docs[0]=(xls_analysis_document){"disk-entry",absolute,imported,40,1};
    first=query(s,docs,1,"disk-entry"); CHECK(first && xlsSemanticCheckInfo(first).accepted);
    CHECK(xrtFileWriteAtomic(disk,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    r=query(s,docs,1,"disk-entry"); CHECK(r && r!=first && xlsSemanticCheckInfo(r).accepted && !xlsSemanticCheckTypedIndex(first)); xlsSemanticCheckDestroy(r);
    xlsCheckSessionStats(s,&stats); CHECK(stats.builds==2 && stats.invalidations==1); xlsSemanticCheckDestroy(first);
    xls_text* missing=body("import missing_dep\nfunc main(...) as int\n return missing_dep.value()\nend\n"); CHECK(missing);
    docs[0].text=missing; ++docs[0].revision; first=query(s,docs,1,"disk-entry"); CHECK(first && !xlsSemanticCheckInfo(first).accepted);
    char* candidate=xrtPathJoin(root,"missing_dep.xl"); CHECK(candidate && xrtFileWriteAtomic(candidate,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    r=query(s,docs,1,"disk-entry"); CHECK(r && r!=first && xlsSemanticCheckInfo(r).accepted && !xlsSemanticCheckValidate(first));
    xlsCheckSessionStats(s,&stats); CHECK(stats.builds==4 && stats.invalidations==2);
    xlsSemanticCheckDestroy(first); first=r;
    char* cwd=xrtPathCwd(); char* directory=xrtPathAbs(root); CHECK(cwd && directory && xrtPathSetCwd(directory));
    r=query(s,docs,1,"disk-entry"); int restored=xrtPathSetCwd(cwd);
    CHECK(restored && r && r!=first && xlsSemanticCheckInfo(r).accepted);
    xlsSemanticCheckDestroy(first); xlsSemanticCheckDestroy(r);
    xls_text* unknown=body("import absent_package_for_environment\n"); CHECK(unknown);
    docs[0].text=unknown; ++docs[0].revision; first=query(s,docs,1,"disk-entry"); CHECK(first && !xlsSemanticCheckInfo(first).accepted);
    char* env=xrtEnvGet("XLANG_PACKAGE_PATH"); CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",directory));
    r=query(s,docs,1,"disk-entry"); int envRestored=env ? xrtEnvSet("XLANG_PACKAGE_PATH",env) : xrtEnvRemove("XLANG_PACKAGE_PATH");
    CHECK(envRestored && r && r!=first && !xlsSemanticCheckInfo(r).accepted && !xlsSemanticCheckValidate(first));
    xlsCheckSessionStats(s,&stats); CHECK(stats.invalidations==3);
    xrtFree(env); xrtFree(cwd); xrtFree(directory); xlsTextRelease(unknown);
    xlsSemanticCheckDestroy(r); xlsSemanticCheckDestroy(first); xlsCheckSessionDestroy(s);
    s=xlsCheckSessionCreate(&(xls_session_limits){1,1024,1024}); CHECK(s);
    docs[0]=(xls_analysis_document){"entry",path,a,50,1};
    first=xlsCheckSessionCheck(s,docs,1,"entry",0,NULL,NULL,NULL,0); CHECK(first && !xlsSemanticCheckInfo(first).hasIndex);
    r=query(s,docs,1,"entry"); CHECK(r && r!=first && xlsSemanticCheckInfo(r).hasIndex);
    xlsCheckSessionStats(s,&stats); CHECK(stats.builds==2 && !stats.hits);
    CHECK(!xlsCheckSessionCheck(s,docs,2,"entry",1,NULL,NULL,NULL,0));
    xlsSemanticCheckDestroy(first); xlsSemanticCheckDestroy(r); xlsCheckSessionDestroy(s);
    xlsTextRelease(missing); xlsTextRelease(imported); xlsTextRelease(dependencyBody); xlsTextRelease(bad); xlsTextRelease(a); xlsTextRelease(same); xlsTextRelease(b);
    xrtFree(path); xrtFree(dep); xrtFree(entryPath); xrtFree(absolute); xrtFree(disk); xrtFree(candidate);
    CHECK(!xlsCheckSessionCreate(&(xls_session_limits){65,100,1000})); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_check_session_test passed: %u checks; retained semantic/IR reuse, versions, dependency/negative invalidation, fresh-result parity and concurrent lifetime.\n",checks); return 0; }
