#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(call) do { ++checks; if(!(call)) { fprintf(stderr,"analysis_input_view_test:%d: %s\n",__LINE__,#call); return 0; } } while(0)
static const xvalue* field(const xvalue* value,const char* name) { return xrtValueObjectGet(value,xrtStrView(name)); }
static const char* string(const xvalue* value) { xstrview text; return xrtValueGetString(value,&text) ? text.Data : NULL; }
static int integer(const xvalue* value,size_t expected) { int64 actual=-1; return xrtValueGetInt(value,&actual) && actual>=0 && (uint64)actual==expected; }
static int digest(const xvalue* value,const unsigned char expected[32])
{
    char hex[65]; for(size_t i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",expected[i]);
    const char* actual=string(value); return actual && !strcmp(actual,hex);
}
static int parityInputs(xls_semantic_check* result,xls_semantic_completion* cursor)
{
    xls_inputs inputs; CHECK(cursor ? xlsSemanticCompletionInputs(cursor,&inputs) : xlsSemanticCheckInputs(result,&inputs));
    size_t size=0; char* text=cursor ? xlsSemanticCompletionJson(cursor,&size) : xlsSemanticCheckReportJson(result,&size); CHECK(text && size);
    xvalue* json=xrtJsonParse(xrtStrViewN(text,size)); xlsSemanticStringFree(text); CHECK(json);
    const xvalue *files=field(field(json,"sourceReport"),"files"),*resolution=field(json,"resolutionReport"),*probes=field(resolution,"probes");
    CHECK(inputs.sources==xrtValueCount(files) && inputs.probes==xrtValueCount(probes));
    for(size_t i=0;i<inputs.sources;++i) {
        xls_input_source source; CHECK(cursor ? xlsSemanticCompletionSource(cursor,i,&source) : xlsSemanticCheckSource(result,i,&source));
        const xvalue* item=xrtValueArrayGet(files,i); unsigned char hash[32];
        CHECK(!strcmp(source.path,string(field(item,"path"))) && !strcmp(source.origin,string(field(item,"origin"))));
        CHECK(integer(field(item,"kind"),(size_t)source.kind) && integer(field(item,"textSize"),source.size) && integer(field(item,"rawSize"),source.rawSize));
        CHECK(source.size==strlen(source.text) && xrtUtf8Valid(xrtStrViewN(source.text,source.size),NULL));
        CHECK(xrtSha256(source.text,source.size,hash) && !memcmp(hash,source.textSha256,32));
        CHECK(digest(field(item,"textSha256"),source.textSha256) && digest(field(item,"rawSha256"),source.rawSha256));
    }
    for(size_t i=0;i<inputs.probes;++i) {
        xls_input_probe probe; bool exists=false; const xvalue* item=xrtValueArrayGet(probes,i);
        CHECK((cursor ? xlsSemanticCompletionProbe(cursor,i,&probe) : xlsSemanticCheckProbe(result,i,&probe)) && !strcmp(probe.path,string(field(item,"path"))));
        CHECK(xrtValueGetBool(field(item,"exists"),&exists) && exists==probe.exists);
    }
    bool observed=false; CHECK(xrtValueGetBool(field(resolution,"packagePathObserved"),&observed) && observed==inputs.packagePathObserved);
    const char* package=string(field(resolution,"XLANG_PACKAGE_PATH"));
    CHECK(package && inputs.packagePath ? !strcmp(package,inputs.packagePath) : package==inputs.packagePath);
    xls_input_source source; xls_input_probe probe; memset(&source,0x55,sizeof(source)); memset(&probe,0x55,sizeof(probe));
    CHECK(!(cursor ? xlsSemanticCompletionSource(cursor,inputs.sources,&source) : xlsSemanticCheckSource(result,inputs.sources,&source)) && !source.path && !source.text && !source.size && !source.kind && !source.textSha256[0]);
    CHECK(!(cursor ? xlsSemanticCompletionProbe(cursor,inputs.probes,&probe) : xlsSemanticCheckProbe(result,inputs.probes,&probe)) && !probe.path && !probe.exists);
    CHECK(cursor ? !xlsSemanticCompletionInputs(cursor,NULL) && !xlsSemanticCompletionSource(cursor,0,NULL) && !xlsSemanticCompletionProbe(cursor,0,NULL) : !xlsSemanticCheckInputs(result,NULL) && !xlsSemanticCheckSource(result,0,NULL) && !xlsSemanticCheckProbe(result,0,NULL));
    xrtValueRelease(json); return 1;
}
static int parity(xls_semantic_check* result) {return parityInputs(result,NULL);}
static int run(void)
{
    xls_check_inputs inputs; xls_check_source source; xls_check_probe probe;
    memset(&inputs,0x55,sizeof(inputs)); memset(&source,0x55,sizeof(source)); memset(&probe,0x55,sizeof(probe));
    CHECK(!xlsSemanticCheckInputs(NULL,&inputs) && !inputs.sources && !inputs.probes && !inputs.packagePath && !inputs.packagePathObserved);
    CHECK(!xlsSemanticCheckSource(NULL,0,&source) && !source.path && !source.rawSize && !source.rawSha256[0]);
    CHECK(!xlsSemanticCheckProbe(NULL,SIZE_MAX,&probe) && !probe.path && !probe.exists);
    char root[160]; snprintf(root,sizeof(root),"build/input-view-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(root),*entry=directory ? xrtPathJoin(directory,"entry.xl") : NULL;
    CHECK(directory && entry && xrtDirCreateAll(directory));
    const char* body="\xef\xbb\xbf// 中文😀\r\nfunc main(...) as int\r\n return 0\r\nend\r\n";
    xls_text* original=xlsTextCreate(body,strlen(body),4096,NULL); xls_check_session* session=xlsCheckSessionCreate(NULL); CHECK(original && session);
    xls_analysis_document doc={"entry",entry,original,1,1}; char error[256];
    xls_semantic_check* result=xlsCheckSessionCheck(session,&doc,1,"entry",1,NULL,NULL,error,sizeof(error)); CHECK(result && xlsSemanticCheckInfo(result).accepted);
    xlsCheckSessionDestroy(session); xlsTextRelease(original);
    CHECK(parity(result) && xlsSemanticCheckInputs(result,&inputs) && inputs.sources==1 && !inputs.packagePathObserved);
    CHECK(xlsSemanticCheckSource(result,0,&source) && !strcmp(source.origin,"overlay") && !strcmp(source.text,body+3) && source.rawSize==strlen(body)-3 && !memcmp(source.textSha256,source.rawSha256,32));
    xls_semantic_check* retained=xlsSemanticCheckRetain(result); xlsSemanticCheckDestroy(result); result=retained;
    CHECK(!strcmp(source.text,body+3) && xlsSemanticCheckValidate(result)); xlsSemanticCheckDestroy(result);
    /* Real source package: manifests and BOM/raw disk bytes belong to the view. */
    char *package=xrtPathJoin(directory,"pkg"),*src=package ? xrtPathJoin(package,"src") : NULL;
    char *manifest=package ? xrtPathJoin(package,"xlang.package.json") : NULL,*api=src ? xrtPathJoin(src,"api.xl") : NULL;
    CHECK(package && src && manifest && api && xrtDirCreateAll(src));
    const char* packageJson="{\"format\":\"xlang-package-1\",\"name\":\"pkg\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"src/api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";
    const char* apiBody="\xef\xbb\xbf" "export func value() as int\r\n return 42\r\nend\r\n";
    CHECK(xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)packageJson,strlen(packageJson)}) && xrtFileWriteAtomic(api,(xbytesview){(const unsigned char*)apiBody,strlen(apiBody)}));
    const char* imported="import pkg\nfunc main(...) as int\n return pkg.value()\nend\n";
    xls_semantic_input overlay={entry,imported,strlen(imported)};
    result=xlsSemanticCheckCreate(entry,&overlay,1,1,NULL,NULL,error,sizeof(error));
    if(!result) fprintf(stderr,"input-view package: %s\n",error);
    CHECK(result && xlsSemanticCheckInfo(result).accepted && parity(result) && xlsSemanticCheckInputs(result,&inputs));
    int foundManifest=0,foundApi=0;
    for(size_t i=0;i<inputs.sources;++i) {
        CHECK(xlsSemanticCheckSource(result,i,&source));
        if(!strcmp(source.path,manifest)) { ++foundManifest; CHECK((source.kind&2) && !strcmp(source.origin,"disk") && !strcmp(source.text,packageJson)); }
        if(!strcmp(source.path,api)) { ++foundApi; CHECK((source.kind&1) && source.rawSize==source.size+3 && !strcmp(source.text,apiBody+3) && memcmp(source.rawSha256,source.textSha256,32)); }
    }
    CHECK(foundManifest==1 && foundApi==1);
    /* A snapshot read does not perform new I/O or silently validate. Replacement
     * is discovered by the explicit validator; every later getter rejects. */
    CHECK(xrtFileWriteAtomic(api,(xbytesview){(const unsigned char*)apiBody,strlen(apiBody)}));
    CHECK(xlsSemanticCheckInputs(result,&inputs));
    CHECK(!xlsSemanticCheckValidate(result));
    memset(&inputs,0x55,sizeof(inputs)); memset(&source,0x55,sizeof(source)); memset(&probe,0x55,sizeof(probe));
    CHECK(!xlsSemanticCheckInputs(result,&inputs) && !inputs.sources && !inputs.packagePath);
    CHECK(!xlsSemanticCheckSource(result,0,&source) && !source.path && !source.text && !source.textSha256[0]);
    CHECK(!xlsSemanticCheckProbe(result,0,&probe) && !probe.path && !probe.exists); xlsSemanticCheckDestroy(result);
    overlay.text="import view_missing_module\n"; overlay.size=strlen(overlay.text);
    result=xlsSemanticCheckCreate(entry,&overlay,1,1,NULL,NULL,error,sizeof(error));
    CHECK(result && !xlsSemanticCheckInfo(result).accepted && parity(result) && xlsSemanticCheckInputs(result,&inputs) && inputs.probes && inputs.packagePathObserved);
    char* candidate=xrtPathJoin(directory,"view_missing_module.xl"); CHECK(candidate);
    int found=0; for(size_t i=0;i<inputs.probes;++i) { CHECK(xlsSemanticCheckProbe(result,i,&probe)); if(!strcmp(probe.path,candidate)) { ++found; CHECK(!probe.exists); } }
    CHECK(found==1 && xrtFileWriteAtomic(candidate,(xbytesview){(const unsigned char*)apiBody,strlen(apiBody)}));
    CHECK(!xlsSemanticCheckValidate(result) && !xlsSemanticCheckInputs(result,&inputs)); xlsSemanticCheckDestroy(result);
    overlay.text="import another_view_missing_package\n"; overlay.size=strlen(overlay.text);
    result=xlsSemanticCheckCreate(entry,&overlay,1,0,NULL,NULL,error,sizeof(error)); CHECK(result && xlsSemanticCheckInputs(result,&inputs) && inputs.packagePathObserved);
    char* environment=xrtEnvGet("XLANG_PACKAGE_PATH"); const char* captured=inputs.packagePath;
    CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",directory));
    CHECK(xlsSemanticCheckInputs(result,&inputs) && inputs.packagePath==captured);
    int invalidated=!xlsSemanticCheckValidate(result); int restored=environment ? xrtEnvSet("XLANG_PACKAGE_PATH",environment) : xrtEnvRemove("XLANG_PACKAGE_PATH");
    CHECK(restored && invalidated && !xlsSemanticCheckInputs(result,&inputs)); xrtFree(environment); xlsSemanticCheckDestroy(result);
    overlay.text="func main(\n"; overlay.size=strlen(overlay.text);
    result=xlsSemanticCheckCreate(entry,&overlay,1,0,NULL,NULL,error,sizeof(error)); CHECK(result && !xlsSemanticCheckInfo(result).accepted && parity(result)); xlsSemanticCheckDestroy(result);
    xrtFree(candidate); xrtFree(package); xrtFree(src); xrtFree(manifest); xrtFree(api); xrtFree(entry); xrtFree(directory); return 1;
}
typedef struct cursor_reader {
    xls_semantic_completion* owner;
    xls_input_source source;
    size_t index;
    xatomic32 *phase,*arrived,*done;
    int okay;
} cursor_reader;
static int32 readCursor(void* user)
{
    cursor_reader* r=user;r->okay=1;xrtAtomic32FetchAdd(r->arrived,1,XMEMORY_ACQ_REL);
    while(!xrtAtomic32Load(r->phase,XMEMORY_ACQUIRE)) xrtThreadYield();
    for(unsigned i=0;i<4000;++i) {
        xls_inputs inputs;xls_input_source source;
        if(!xlsSemanticCompletionInputs(r->owner,&inputs) || !xlsSemanticCompletionSource(r->owner,r->index,&source) ||
           source.text!=r->source.text || source.size!=r->source.size || strcmp(source.origin,"disk") || !inputs.sources) r->okay=0;
    }
    xrtAtomic32FetchAdd(r->done,1,XMEMORY_ACQ_REL);
    while(xrtAtomic32Load(r->phase,XMEMORY_ACQUIRE)<2) xrtThreadYield();
    xls_inputs inputs;memset(&inputs,0x55,sizeof(inputs));
    r->okay=r->okay && !xlsSemanticCompletionInputs(r->owner,&inputs) && !inputs.sources && !inputs.packagePath &&
        !strcmp(r->source.text,"export func value() as int\r\n return 42\r\nend\r\n");
    xlsSemanticCompletionDestroy(r->owner);return 0;
}
static int cursorViews(void)
{
    xls_inputs inputs;xls_input_source source;xls_input_probe probe;
    memset(&inputs,0x55,sizeof(inputs));memset(&source,0x55,sizeof(source));memset(&probe,0x55,sizeof(probe));
    CHECK(!xlsSemanticCompletionInputs(NULL,&inputs) && !inputs.sources && !inputs.probes && !inputs.packagePathObserved && !inputs.packagePath);
    CHECK(!xlsSemanticCompletionSource(NULL,SIZE_MAX,&source) && !source.path && !source.size && !source.rawSha256[0]);
    CHECK(!xlsSemanticCompletionProbe(NULL,0,&probe) && !probe.path && !probe.exists);
    char root[160],error[256];snprintf(root,sizeof(root),"build/cursor-input-view-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(root),*entry=directory ? xrtPathJoin(directory,"entry.xl") : NULL,*dep=directory ? xrtPathJoin(directory,"view_cursor_dep.xl") : NULL;
    CHECK(directory && entry && dep && xrtDirCreateAll(directory));
    const char* dependency="\xef\xbb\xbf" "export func value() as int\r\n return 42\r\nend\r\n";
    const char* body="\xef\xbb\xbf// 中文😀\r\nimport view_cursor_dep\r\nfunc main(...) as int\r\n return view_cursor_dep.value()\r\nend\r\n";
    CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}));
    size_t at=(size_t)(strstr(body,".value")-body)+3-3;
    xls_text* text=xlsTextCreate(body,strlen(body),4096,NULL);xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(text && session);
    xls_analysis_document doc={"entry",entry,text,11,1};
    xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",at,NULL,NULL,error,sizeof(error));
    CHECK(result && xlsSemanticCompletionInfo(result).available);xlsCompletionSessionDestroy(session);xlsTextRelease(text);
    CHECK(parityInputs(NULL,result) && xlsSemanticCompletionInputs(result,&inputs) && inputs.sources==2 && inputs.probes);
    size_t index=SIZE_MAX;
    for(size_t i=0;i<inputs.sources;++i) {
        CHECK(xlsSemanticCompletionSource(result,i,&source));
        if(!strcmp(source.path,entry)) CHECK(!strcmp(source.origin,"overlay") && !strcmp(source.text,body+3) && !memcmp(source.textSha256,source.rawSha256,32));
        if(!strcmp(source.path,dep)) {index=i;CHECK(!strcmp(source.origin,"disk") && !strcmp(source.text,dependency+3) && source.rawSize==source.size+3 && memcmp(source.textSha256,source.rawSha256,32));}
    }
    CHECK(index!=SIZE_MAX && xlsSemanticCompletionSource(result,index,&source));
    xatomic32 phase,arrived,done;xrtAtomic32Init(&phase,0);xrtAtomic32Init(&arrived,0);xrtAtomic32Init(&done,0);
    cursor_reader readers[2];xthread* threads[2];
    for(unsigned i=0;i<2;++i) {readers[i]=(cursor_reader){xlsSemanticCompletionRetain(result),source,index,&phase,&arrived,&done,0};threads[i]=xrtThreadCreate(readCursor,&readers[i],0);CHECK(threads[i]);}
    uint64_t until=xrtClock()+5000000;while(xrtAtomic32Load(&arrived,XMEMORY_ACQUIRE)<2 && xrtClock()<until) xrtSleep(1);CHECK(xrtAtomic32Load(&arrived,XMEMORY_ACQUIRE)==2);
    xrtAtomic32Store(&phase,1,XMEMORY_RELEASE);for(unsigned i=0;i<10;++i) CHECK(xlsSemanticCompletionValidate(result));
    until=xrtClock()+5000000;while(xrtAtomic32Load(&done,XMEMORY_ACQUIRE)<2 && xrtClock()<until) xrtSleep(1);CHECK(xrtAtomic32Load(&done,XMEMORY_ACQUIRE)==2);
    CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)dependency,strlen(dependency)}) && !xlsSemanticCompletionValidate(result));
    memset(&source,0x55,sizeof(source));memset(&probe,0x55,sizeof(probe));
    CHECK(!xlsSemanticCompletionSource(result,index,&source) && !source.path && !source.text && !source.rawSha256[0]);
    CHECK(!xlsSemanticCompletionProbe(result,0,&probe) && !probe.path && !probe.exists);
    xlsSemanticCompletionDestroy(result);result=NULL;xrtAtomic32Store(&phase,2,XMEMORY_RELEASE);
    for(unsigned i=0;i<2;++i) {CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    /* A larger borrowed source stays exact. It is never reconstructed from
       JSON strings; getters expose the same report-owned bytes repeatedly. */
    xstrbuf large;xrtStrBufInit(&large);CHECK(xrtStrBufAppend(&large,xrtStrView(body)));
    int built=1;while(large.Size<1024u*1024u) if(!xrtStrBufAppend(&large,xrtStrView("// unchanged input bytes 中文😀\r\n"))) {built=0;break;}
    CHECK(built && large.Size>=1024u*1024u);
    xls_semantic_input overlay={entry,large.Data,large.Size};result=xlsSemanticCompletionCreate(entry,entry,at,&overlay,1,NULL,NULL,error,sizeof(error));
    CHECK(result && xlsSemanticCompletionInfo(result).available && xlsSemanticCompletionInputs(result,&inputs));
    CHECK(xlsSemanticCompletionSource(result,0,&source) && source.size==large.Size-3 && !memcmp(source.text,large.Data+3,source.size));
    const char* borrowed=source.text;xrtStrBufFree(&large);CHECK(!strncmp(borrowed,"// 中文😀\r\n",strlen("// 中文😀\r\n")) && parityInputs(NULL,result));xlsSemanticCompletionDestroy(result);
    overlay=(xls_semantic_input){entry,"",0};result=xlsSemanticCompletionCreate(entry,entry,0,&overlay,1,NULL,NULL,error,sizeof(error));
    CHECK(result && !xlsSemanticCompletionInfo(result).available && parityInputs(NULL,result) && xlsSemanticCompletionSource(result,0,&source) && !source.size && source.text && !*source.text);xlsSemanticCompletionDestroy(result);
    const char* missing="import cursor_view_missing_package\nfunc main(...) as int\n return missingValue\nend\n";
    overlay=(xls_semantic_input){entry,missing,strlen(missing)};at=(size_t)(strstr(missing,"missingValue")-missing)+4;
    result=xlsSemanticCompletionCreate(entry,entry,at,&overlay,1,NULL,NULL,error,sizeof(error));
    CHECK(result && !xlsSemanticCompletionInfo(result).available && parityInputs(NULL,result) && xlsSemanticCompletionInputs(result,&inputs) && inputs.packagePathObserved && inputs.probes);
    const char* package=inputs.packagePath;char* old=xrtEnvGet("XLANG_PACKAGE_PATH");CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",directory));
    CHECK(xlsSemanticCompletionInputs(result,&inputs) && inputs.packagePath==package);
    int invalid=!xlsSemanticCompletionValidate(result),restored=old ? xrtEnvSet("XLANG_PACKAGE_PATH",old) : xrtEnvRemove("XLANG_PACKAGE_PATH");
    CHECK(restored && invalid && !xlsSemanticCompletionInputs(result,&inputs) && !inputs.sources);xrtFree(old);xlsSemanticCompletionDestroy(result);
    for(unsigned mode=0;mode<2;++mode) {
        xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=entry,.directory=directory,.configuration="inputs",.analysisProfile="fixture-linked-native",.targetKind="exe",.packagePath=mode ? "" : NULL};
        xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);CHECK(context);
        result=xlsSemanticCompletionCreateInContext(context,entry,entry,at,&overlay,1,NULL,NULL,error,sizeof(error));xlsContextRelease(context);
        CHECK(result && parityInputs(NULL,result) && xlsSemanticCompletionInputs(result,&inputs) && inputs.packagePathObserved && (mode ? inputs.packagePath && !*inputs.packagePath : !inputs.packagePath));xlsSemanticCompletionDestroy(result);
    }
    xrtFree(directory);xrtFree(entry);xrtFree(dep);return 1;
}
int main(void) { if(!run() || !cursorViews()) return 1; printf("analysis_input_view_test passed: %u checks plus8000 concurrent cursor reads; shared check/cursor report rows, source/manifests/probes/environment, JSON parity, invalidation and retained ownership\n",checks); return 0; }
