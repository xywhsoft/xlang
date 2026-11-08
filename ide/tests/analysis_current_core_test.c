#include "analysis/frontend_internal.h"
#include "analysis/check_ir.h"
#include "analysis_ast_same.h"
#include "src/package/xl6_package.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"analysis_current_core_test:%d: %s\n",__LINE__,#v); return 0; } } while(0)
static int writeText(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static char* irJson(const xls_semantic_check* check)
{
 const XL6_IrModule* ir=xlsSemanticCheckIr(check);if(!ir)return NULL;
 XL6_Output output;CHECK(xl6OutputOpenMemory(&output));xl6IrDumpModuleJson(&output,ir);
 char* text=xl6OutputHasFailed(&output) ? NULL : xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);return text;
}
static int semantic(const char* code,int accepted)
{
 char* path=xrtPathAbs("current-core-analysis.xl");CHECK(path);xls_semantic_check* previous=NULL;
 for(unsigned round=0;round<3;++round) {
  xstrbuf body={0};CHECK(xrtStrBufAppend(&body,xrtStrView(round==1 ? "// 中文😀\n\n" : "")) && xrtStrBufAppend(&body,xrtStrView(code)));
  xls_semantic_input input={path,body.Data,body.Size};char error[256]={0};
  xls_semantic_check* result=xlsSemanticCheckCreateIncrementalInContext(NULL,path,&input,1,1,NULL,previous,NULL,NULL,error,sizeof(error));
  xls_semantic_check* fresh=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
  CHECK(result && fresh);
  if(xlsSemanticCheckInfo(result).accepted!=accepted) {
   char* detail=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"unexpected check: %s\n",detail ? detail:error);xlsSemanticStringFree(detail);
  }
  CHECK(xlsSemanticCheckInfo(result).accepted==accepted && xlsSemanticCheckInfo(fresh).accepted==accepted);
  char *a=xlsSemanticCheckJson(result,NULL),*b=xlsSemanticCheckJson(fresh,NULL);
  CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  CHECK(sameAst(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)));
  if(accepted) {a=irJson(result);b=irJson(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);}
  else CHECK(xlsSemanticCheckInfo(result).diagnostics);
  xlsSemanticCheckDestroy(previous);previous=result;xlsSemanticCheckDestroy(fresh);xrtStrBufFree(&body);
 }
 xlsSemanticCheckDestroy(previous);xrtFree(path);return 1;
}
static int manifest(const char* path,const char* name,const char* source,const char* flags,const char* native)
{
 char body[4096];int n=snprintf(body,sizeof(body),"{\"format\":\"xlang-package-1\",\"name\":\"%s\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"%s\"],\"nativeLink\":%s},\"native\":%s,\"dependencies\":[],\"links\":[],\"loadlibs\":[]}",name,source,flags,native);
 return n>0 && (size_t)n<sizeof(body) && writeText(path,body);
}
static int packageAnalysis(const char* dynamicPath)
{
 char* path=xrtPathAbs("current-core-package-main.xl");CHECK(path);
 const char* code="import current_core_package\nfunc main(...) as int\n return current_core_package.value()\nend\n";
 xls_semantic_input input={path,code,strlen(code)};xls_semantic_check* previous=NULL;
 for(unsigned round=0;round<3;++round) {
  CHECK(manifest(dynamicPath,round==1 ? "wrong" : "current_core_package","api.xl","{\"gcSections\":true,\"printGcSections\":true}","{\"linkage\":\"dynamic\"}"));
  if(previous)CHECK(!xlsSemanticCheckValidate(previous));
  char error[256]={0};int accepted=round!=1;
  xls_semantic_check* result=xlsSemanticCheckCreateIncrementalInContext(NULL,path,&input,1,1,NULL,previous,NULL,NULL,error,sizeof(error));
  xls_semantic_check* fresh=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
  CHECK(result && fresh);
  if(xlsSemanticCheckInfo(result).accepted!=accepted) {char* detail=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"package check: %s\n",detail ? detail:error);xlsSemanticStringFree(detail);}
  CHECK(xlsSemanticCheckInfo(result).accepted==accepted && xlsSemanticCheckInfo(fresh).accepted==accepted);
  char *a=xlsSemanticCheckJson(result,NULL),*b=xlsSemanticCheckJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  CHECK(sameAst(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)));
  if(accepted) {a=irJson(result);b=irJson(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);}
  xls_check_inputs captured={0};CHECK(xlsSemanticCheckInputs(result,&captured));int found=0;
  for(size_t i=0;i<captured.sources;++i) {xls_check_source item;CHECK(xlsSemanticCheckSource(result,i,&item));if(!strcmp(item.path,dynamicPath))found=1;}
  CHECK(found && xlsSemanticCheckValidate(result));
  xlsSemanticCheckDestroy(previous);previous=result;xlsSemanticCheckDestroy(fresh);
 }
 xlsSemanticCheckDestroy(previous);xrtFree(path);return 1;
}
static int packages(void)
{
 CHECK(xrtDirCreateAll("current_core_package"));char *basePath=xrtPathAbs("current_core_package/xlang.package.json"),*dynamicPath=xrtPathAbs("current_core_package/dynamic.json");CHECK(basePath && dynamicPath);
 const char* statik="{\"linkage\":\"static\",\"variants\":{\"dynamic\":\"dynamic.json\"}}";
 CHECK(writeText("current_core_package/api.xl","export func value() as int\n return 7\nend\n"));
 CHECK(manifest(basePath,"current_core_package","api.xl","{\"functionSections\":true,\"dataSections\":true}",statik));
 CHECK(manifest(dynamicPath,"current_core_package","api.xl","{\"gcSections\":true,\"printGcSections\":true}","{\"linkage\":\"dynamic\"}"));
 XL6_PackageManifest base,selected;xl6PackageManifestInit(&base);xl6PackageManifestInit(&selected);XL6_SourceReport report;xl6SourceReportInit(&report);char error[512]={0};
 CHECK(xl6PackageLoadManifestObserved(basePath,&base,&report));
 CHECK(base.nativeLinkFlags==(XL6_NATIVE_LINK_FUNCTION_SECTIONS|XL6_NATIVE_LINK_DATA_SECTIONS) && base.nativeInfo.linkage==1 && !strcmp(base.nativeInfo.dynamicManifest,"dynamic.json"));
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_STATIC,&selected,&report,error,sizeof(error))==0);
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_AUTO_AOT,&selected,&report,error,sizeof(error))==0);
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_AUTO_JIT,&selected,&report,error,sizeof(error))==1);
 CHECK(selected.nativeInfo.linkage==2 && selected.nativeLinkFlags==(XL6_NATIVE_LINK_GC_SECTIONS|XL6_NATIVE_LINK_PRINT_GC_SECTIONS));
 CHECK(!strcmp(base.packageId,selected.packageId) && report.count==2 && xl6SourceReportValidate(&report));
 CHECK(packageAnalysis(dynamicPath));
 xl6PackageManifestFree(&selected);xl6PackageManifestInit(&selected);
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_DYNAMIC,&selected,NULL,error,sizeof(error))==1);xl6PackageManifestFree(&selected);xl6PackageManifestInit(&selected);
 CHECK(manifest(dynamicPath,"wrong","api.xl","{}","{\"linkage\":\"dynamic\"}"));
 CHECK(!xl6SourceReportValidate(&report));xl6SourceReportFree(&report);
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_DYNAMIC,&selected,NULL,error,sizeof(error))==-1 && strstr(error,"changes package identity"));xl6PackageManifestFree(&selected);xl6PackageManifestInit(&selected);
 CHECK(manifest(dynamicPath,"current_core_package","different.xl","{}","{\"linkage\":\"dynamic\"}"));
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_DYNAMIC,&selected,NULL,error,sizeof(error))==-1);xl6PackageManifestFree(&selected);xl6PackageManifestFree(&base);
 for(unsigned mode=0;mode<4;++mode) {
  xl6PackageManifestInit(&base);xl6PackageManifestInit(&selected);
  CHECK(manifest(basePath,"current_core_package","api.xl",mode==0 ? "{\"printGcSections\":true}" : mode==1 ? "{\"unknown\":true}" : mode==2 ? "{\"gcSections\":1}" : "{}",mode==3 ? "{\"linkage\":\"unsupported\"}" : "{}"));
  CHECK(!xl6PackageLoadManifestObserved(basePath,&base,NULL));xl6PackageManifestFree(&base);xrtClearError();
 }
 xl6PackageManifestInit(&base);xl6PackageManifestInit(&selected);
 CHECK(manifest(basePath,"current_core_package","api.xl","{}","{\"linkage\":\"static\",\"variants\":{\"dynamic\":\"C:/forbidden.json\"}}"));
 CHECK(xl6PackageLoadManifestObserved(basePath,&base,NULL));
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_DYNAMIC,&selected,NULL,error,sizeof(error))==-1 && strstr(error,"relative manifest path"));xl6PackageManifestFree(&selected);xl6PackageManifestFree(&base);
 xl6PackageManifestInit(&base);xl6PackageManifestInit(&selected);
 CHECK(manifest(basePath,"current_core_package","api.xl","{}","{}") && xl6PackageLoadManifestObserved(basePath,&base,NULL));
 CHECK(xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_STATIC,&selected,NULL,error,sizeof(error))==0 && xl6PackageSelectNativeVariant(&base,XL6_NATIVE_SELECT_DYNAMIC,&selected,NULL,error,sizeof(error))==0);
 xl6PackageManifestFree(&selected);xl6PackageManifestFree(&base);xrtFree(basePath);xrtFree(dynamicPath);xrtClearError();return 1;
}
static int run(void)
{
 CHECK(packages());
 CHECK(semantic("channel<int> globalChannel\nclass Owner\n channel<int> pipe\nend\nfunc main(...) as int\n channel<int> local\n Owner object = Owner()\n return 0\nend\n",1));
 CHECK(semantic("channel<int> pipe\npipe.send(\"wrong\")\n",0));
 CHECK(semantic("channel<int> pipe\npipe.send(7)\n",1));
 return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_current_core_test passed: %u checks\n",checks);return 0;}
