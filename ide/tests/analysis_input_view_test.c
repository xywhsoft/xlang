#include "analysis/check_session.h"
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
static int parity(xls_semantic_check* result)
{
    xls_check_inputs inputs; CHECK(xlsSemanticCheckInputs(result,&inputs));
    size_t size=0; char* text=xlsSemanticCheckReportJson(result,&size); CHECK(text && size);
    xvalue* json=xrtJsonParse(xrtStrViewN(text,size)); xlsSemanticStringFree(text); CHECK(json);
    const xvalue *files=field(field(json,"sourceReport"),"files"),*resolution=field(json,"resolutionReport"),*probes=field(resolution,"probes");
    CHECK(inputs.sources==xrtValueCount(files) && inputs.probes==xrtValueCount(probes));
    for(size_t i=0;i<inputs.sources;++i) {
        xls_check_source source; CHECK(xlsSemanticCheckSource(result,i,&source));
        const xvalue* item=xrtValueArrayGet(files,i); unsigned char hash[32];
        CHECK(!strcmp(source.path,string(field(item,"path"))) && !strcmp(source.origin,string(field(item,"origin"))));
        CHECK(integer(field(item,"kind"),(size_t)source.kind) && integer(field(item,"textSize"),source.size) && integer(field(item,"rawSize"),source.rawSize));
        CHECK(source.size==strlen(source.text) && xrtUtf8Valid(xrtStrViewN(source.text,source.size),NULL));
        CHECK(xrtSha256(source.text,source.size,hash) && !memcmp(hash,source.textSha256,32));
        CHECK(digest(field(item,"textSha256"),source.textSha256) && digest(field(item,"rawSha256"),source.rawSha256));
    }
    for(size_t i=0;i<inputs.probes;++i) {
        xls_check_probe probe; bool exists=false; const xvalue* item=xrtValueArrayGet(probes,i);
        CHECK(xlsSemanticCheckProbe(result,i,&probe) && !strcmp(probe.path,string(field(item,"path"))));
        CHECK(xrtValueGetBool(field(item,"exists"),&exists) && exists==probe.exists);
    }
    bool observed=false; CHECK(xrtValueGetBool(field(resolution,"packagePathObserved"),&observed) && observed==inputs.packagePathObserved);
    const char* package=string(field(resolution,"XLANG_PACKAGE_PATH"));
    CHECK(package && inputs.packagePath ? !strcmp(package,inputs.packagePath) : package==inputs.packagePath);
    xls_check_source source; xls_check_probe probe; memset(&source,0x55,sizeof(source)); memset(&probe,0x55,sizeof(probe));
    CHECK(!xlsSemanticCheckSource(result,inputs.sources,&source) && !source.path && !source.text && !source.size && !source.kind && !source.textSha256[0]);
    CHECK(!xlsSemanticCheckProbe(result,inputs.probes,&probe) && !probe.path && !probe.exists);
    CHECK(!xlsSemanticCheckInputs(result,NULL) && !xlsSemanticCheckSource(result,0,NULL) && !xlsSemanticCheckProbe(result,0,NULL));
    xrtValueRelease(json); return 1;
}
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
int main(void) { if(!run()) return 1; printf("analysis_input_view_test passed: %u checks; actual sources/manifests/probes/environment, JSON parity and retained ownership\n",checks); return 0; }
