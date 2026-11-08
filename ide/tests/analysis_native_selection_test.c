#include "analysis/frontend_internal.h"
#include "analysis/frontend_context.h"
#include "analysis/check_ir.h"
#include "analysis/dependency_capture.h"
#include "analysis_ast_same.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"analysis_native_selection_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const char* code="import mode_package\nfunc main(...) as int\n return mode_package.value()\nend\n";
static int writeText(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int manifest(const char* path,const char* name,const char* linkage,int variants)
{
 char text[2048];int n=snprintf(text,sizeof(text),"{\"format\":\"xlang-package-1\",\"name\":\"%s\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"native\":{\"linkage\":\"%s\",\"variants\":{%s}},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}",name,linkage,variants ? "\"dynamic\":\"dynamic.json\"":"");
 return n>0 && (size_t)n<sizeof(text) && writeText(path,text);
}
static xls_context* context(const char* entry,const char* directory,enum xls_native_selection mode)
{
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.nativeSelection=mode,.entry=entry,.directory=directory,.targetKind="exe",.analysisProfile="native-selection-fixture",.packagePath=""};
 return xlsContextCreate(&spec,16u*1024u*1024u,NULL);
}
static int dynamicCheckSource(const xls_semantic_check* r)
{
 xls_inputs inputs;if(!xlsSemanticCheckInputs(r,&inputs))return -1;
 for(size_t i=0;i<inputs.sources;++i){xls_input_source source;if(!xlsSemanticCheckSource(r,i,&source))return -1;if(strstr(source.path,"dynamic.json"))return 1;}
 return 0;
}
static int dynamicCompletionSource(const xls_semantic_completion* r)
{
 xls_inputs inputs;if(!xlsSemanticCompletionInputs(r,&inputs))return -1;
 for(size_t i=0;i<inputs.sources;++i){xls_input_source source;if(!xlsSemanticCompletionSource(r,i,&source))return -1;if(strstr(source.path,"dynamic.json"))return 1;}
 return 0;
}
static char* irJson(const xls_semantic_check* check)
{
 XL6_Output out;const XL6_IrModule* ir=xlsSemanticCheckIr(check);if(!ir || !xl6OutputOpenMemory(&out))return NULL;
 xl6IrDumpModuleJson(&out,ir);char* text=xl6OutputHasFailed(&out) ? NULL:xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return text;
}
static int pair(xls_context* ctx,const char* entry,xls_semantic_check** previous,int accepted,int dynamic)
{
 xls_semantic_input input={entry,code,strlen(code)};char error[256]={0};
 xls_semantic_check *r=xlsSemanticCheckCreateIncrementalInContext(ctx,entry,&input,1,1,NULL,*previous,NULL,NULL,error,sizeof(error)),
  *fresh=xlsSemanticCheckCreateInContext(ctx,entry,&input,1,1,NULL,NULL,error,sizeof(error));
 if(!r || !fresh)fprintf(stderr,"native selection pair: %s\n",error);
 CHECK(r && fresh && xlsSemanticCheckInfo(r).accepted==accepted && xlsSemanticCheckInfo(fresh).accepted==accepted);
 char *a=xlsSemanticCheckJson(r,NULL),*b=xlsSemanticCheckJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);
 CHECK(sameAst(xlsSemanticCheckAst(r),xlsSemanticCheckAst(fresh)));
 if(accepted){CHECK(dynamicCheckSource(r)==dynamic);a=irJson(r);b=irJson(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);}
 xlsSemanticCheckDestroy(fresh);xlsSemanticCheckDestroy(*previous);*previous=r;CHECK(!xlsFrontendContextCurrent());return 1;
}
typedef struct concurrent_case {xls_context* context;const char* entry;int dynamic,okay,entered;xatomic32* arrived;} concurrent_case;
static int rendezvous(void* user)
{
 concurrent_case* data=user;
 if(!data->entered){data->entered=1;data->okay=1;xrtAtomic32FetchAdd(data->arrived,1,XMEMORY_RELEASE);size_t n=0;while(xrtAtomic32Load(data->arrived,XMEMORY_ACQUIRE)!=2 && n++<10000)xrtSleep(1);if(n>=10000)data->okay=0;}
 return 0;
}
static int concurrent(void* user)
{
 concurrent_case* data=user;xls_semantic_input input={data->entry,code,strlen(code)};
 xls_semantic_check* result=xlsSemanticCheckCreateInContext(data->context,data->entry,&input,1,1,rendezvous,data,NULL,0);
 data->okay=data->okay && result && xlsSemanticCheckInfo(result).accepted && dynamicCheckSource(result)==data->dynamic && !xlsFrontendContextCurrent();
 xlsSemanticCheckDestroy(result);return 0;
}
static int run(void)
{
 CHECK(xlsFrontendNativeSelectionAvailable());
 char* folder=xrtPathAbs("native-selection-fixture");char* pkg=folder ? xrtPathJoin(folder,"mode_package"):NULL;
 char *entry=folder ? xrtPathJoin(folder,"main.xl"):NULL,*base=pkg ? xrtPathJoin(pkg,"xlang.package.json"):NULL,
  *variant=pkg ? xrtPathJoin(pkg,"dynamic.json"):NULL,*api=pkg ? xrtPathJoin(pkg,"api.xl"):NULL;
 CHECK(folder && pkg && entry && base && variant && api && xrtDirCreateAll(pkg));
 CHECK(writeText(entry,"// use open overlay\n") && writeText(api,"export func value() as int\n return 7\nend\n"));
 CHECK(manifest(base,"mode_package","static",1) && manifest(variant,"mode_package","dynamic",0));
 xls_context* modes[4];for(unsigned i=0;i<4;++i){modes[i]=context(entry,folder,(enum xls_native_selection)i);CHECK(modes[i]);}
 xls_frontend_context_scope outer,inner;xlsFrontendContextBegin(&outer,modes[1]);CHECK(xlsFrontendContextNativeSelection()==XLS_NATIVE_STATIC);
 xlsFrontendContextBegin(&inner,modes[2]);CHECK(xlsFrontendContextNativeSelection()==XLS_NATIVE_DYNAMIC && xlsFrontendContextEnd(&inner));
 CHECK(xlsFrontendContextNativeSelection()==XLS_NATIVE_STATIC && xlsFrontendContextEnd(&outer) && xlsFrontendContextNativeSelection()==XLS_NATIVE_AUTO_JIT);
 xls_semantic_check* previous=NULL;
 for(unsigned i=0;i<8;++i){unsigned mode=i%4;CHECK(pair(modes[mode],entry,&previous,1,mode==0 || mode==2));}
 xlsSemanticCheckDestroy(previous);previous=NULL;
 xls_semantic_input input={entry,code,strlen(code)};size_t at=(size_t)(strstr(code,"mode_package.value")-code)+strlen("mode_package.val");char error[256]={0};
 xls_semantic_completion* cursors[4];
 for(unsigned i=0;i<4;++i){
  cursors[i]=xlsSemanticCompletionCreateInContext(modes[i],entry,entry,at,&input,1,NULL,NULL,error,sizeof(error));
  if(!cursors[i])fprintf(stderr,"native selection completion: %s\n",error);
  CHECK(cursors[i] && xlsSemanticCompletionInfo(cursors[i]).available && dynamicCompletionSource(cursors[i])==(i==0 || i==2));
  int found=0;for(size_t j=0;j<xlsSemanticCompletionInfo(cursors[i]).count;++j){xls_completion_item item;CHECK(xlsSemanticCompletionItem(cursors[i],j,&item));if(!strcmp(item.label,"value"))found=1;}CHECK(found);
 }
 /* A changed unselected variant must not invalidate static/AOT identities. */
 CHECK(manifest(variant,"wrong_package","dynamic",0));
 CHECK(xlsSemanticCompletionValidate(cursors[1]) && xlsSemanticCompletionValidate(cursors[3]));
 CHECK(!xlsSemanticCompletionValidate(cursors[0]) && !xlsSemanticCompletionValidate(cursors[2]));
 for(unsigned i=0;i<4;++i){CHECK(pair(modes[i],entry,&previous,i==1 || i==3,i==0 || i==2));xlsSemanticCompletionDestroy(cursors[i]);}
 xls_dependency_capture* capture=xlsDependencyCaptureCreate(modes[1],NULL,entry,&input,1,NULL,NULL,error,sizeof(error));CHECK(capture && xlsDependencyCaptureInfo(capture).expanded);xlsDependencyCaptureRelease(capture);
 capture=xlsDependencyCaptureCreate(modes[2],NULL,entry,&input,1,NULL,NULL,error,sizeof(error));CHECK(capture && !xlsDependencyCaptureInfo(capture).expanded);xlsDependencyCaptureRelease(capture);
 CHECK(manifest(variant,"mode_package","dynamic",0));
 for(unsigned i=0;i<4;++i)CHECK(pair(modes[i],entry,&previous,1,i==0 || i==2));
 xlsSemanticCheckDestroy(previous);previous=NULL;
 for(unsigned round=0;round<4;++round){
  xatomic32 arrived;xrtAtomic32Init(&arrived,0);concurrent_case left={modes[1],entry,0,0,0,&arrived},right={modes[2],entry,1,0,0,&arrived};
  xthread *a=xrtThreadCreate(concurrent,&left,0),*b=xrtThreadCreate(concurrent,&right,0);CHECK(a && b);
  CHECK(xrtThreadWait(a)==XWAIT_OK && xrtThreadWait(b)==XWAIT_OK && left.okay && right.okay);xrtThreadDestroy(a);xrtThreadDestroy(b);
 }
 /* A missing chosen manifest is a proven import diagnostic, not an internal
  * error or an unguarded cached success. Its creation invalidates both views. */
 char* missing=xrtPathJoin(pkg,"missing-static.json");CHECK(missing);
 CHECK(manifest(base,"mode_package","dynamic",0));
 char baseText[2048];int length=snprintf(baseText,sizeof(baseText),"{\"format\":\"xlang-package-1\",\"name\":\"mode_package\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"native\":{\"linkage\":\"dynamic\",\"variants\":{\"static\":\"missing-static.json\"}},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}");
 CHECK(length>0 && (size_t)length<sizeof(baseText) && writeText(base,baseText));
 CHECK(pair(modes[1],entry,&previous,0,0) && xlsSemanticCheckValidate(previous));
 xls_semantic_completion* absent=xlsSemanticCompletionCreateInContext(modes[1],entry,entry,at,&input,1,NULL,NULL,error,sizeof(error));
 CHECK(absent && !xlsSemanticCompletionInfo(absent).available && xlsSemanticCompletionValidate(absent));
 CHECK(manifest(missing,"mode_package","static",0));
 CHECK(!xlsSemanticCheckValidate(previous) && !xlsSemanticCompletionValidate(absent));xlsSemanticCompletionDestroy(absent);
 CHECK(pair(modes[1],entry,&previous,1,0) && pair(modes[3],entry,&previous,1,0));xlsSemanticCheckDestroy(previous);
 CHECK(remove(missing)==0);xrtFree(missing);
 for(unsigned i=0;i<4;++i)xlsContextRelease(modes[i]);
 xrtFree(folder);xrtFree(pkg);xrtFree(entry);xrtFree(base);xrtFree(variant);xrtFree(api);xrtClearError();return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_native_selection_test passed: %u checks; context changes, native source receipts, full AST/IR parity, cursor freshness, dependency-only loading and concurrent TLS scopes\n",checks);return 0;}
