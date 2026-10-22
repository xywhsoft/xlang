#include "analysis/completion_session.h"
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
    r=query(s,docs,2,"entry","entry",at); CHECK(r && r!=first && has(r,"member") && parity(r,docs,2,"entry","entry",at));
    xlsSemanticCompletionDestroy(first); first=r;
    r=query(s,docs,1,"entry","entry",at); CHECK(r && r!=first && has(r,"member")); xlsSemanticCompletionDestroy(first); first=r;
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
    r=query(s,docs,1,"entry","entry",at); CHECK(r && r!=first); xlsSemanticCompletionDestroy(r);
    docs[0].text=b; docs[0].revision=3; r=query(s,docs,1,"entry","entry",at);
    CHECK(r && r!=first && has(r,"firstValue") && parity(r,docs,1,"entry","entry",at)); xlsSemanticCompletionDestroy(r);
    docs[0].identity=11; docs[0].revision=1; r=query(s,docs,1,"entry","entry",at); CHECK(r && r!=first); xlsSemanticCompletionDestroy(r);
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"absent","entry",at,NULL,NULL,NULL,0));
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","absent",at,NULL,NULL,NULL,0));
    CHECK(!xlsCompletionSessionQuery(s,docs,1,"entry","entry",strlen(good)+1,NULL,NULL,NULL,0));
    docs[1]=docs[0]; CHECK(!xlsCompletionSessionQuery(s,docs,2,"entry","entry",at,NULL,NULL,NULL,0));
    xlsCompletionSessionStats(s,&stats); CHECK(stats.builds==5 && stats.hits==102);

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
    CHECK(externalInputs()); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_completion_session_test passed: %u checks; actual exact-query reuse, fresh JSON parity, atomic cancellation, revisions/identity/input-set/cursor and independent result lifetime.\n",checks); return 0; }
