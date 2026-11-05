#include "analysis/frontend_internal.h"
#include "analysis/check_ir.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(v) do{if(!(v)){fprintf(stderr,"final_interop:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static int fixture(const char* file,int expected,const char* packages)
{
 size_t size=0;unsigned char* data=xrtFileReadAllLimit(file,1024u*1024u,&size);
 CHECK(data && size && !memchr(data,0,size));char* path=xrtPathAbs(file);CHECK(path);
 xls_semantic_check* previous=NULL;xls_context* context=NULL;
 if(packages){
  char* directory=xrtPathAbs(".");CHECK(directory);
  xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=path,.directory=directory,.analysisProfile="final-c-interop-20261105",.targetKind="exe",.packagePath=packages};
  context=xlsContextCreate(&spec,1024u*1024u,NULL);xrtFree(directory);CHECK(context);
 }
 for(unsigned round=0;round<3;++round){
  xstrbuf body={0};CHECK(xrtStrBufAppend(&body,(xstrview){(const char*)data,size}));
  char suffix[64];snprintf(suffix,sizeof(suffix),"\n// final-cohort round %u\n",round);CHECK(xrtStrBufAppend(&body,xrtStrView(suffix)));
  xls_semantic_input input={path,body.Data,body.Size};char error[256]={0};clock_t start=clock();
  xls_semantic_check* result=xlsSemanticCheckCreateIncrementalInContext(context,path,&input,1,1,NULL,previous,NULL,NULL,error,sizeof(error));
  double ms=1000.0*(double)(clock()-start)/CLOCKS_PER_SEC;
  if(!result)fprintf(stderr,"infrastructure:%s\n",error);
  CHECK(result);
  xls_check_info info=xlsSemanticCheckInfo(result);
  if(info.accepted!=expected){
   fprintf(stderr,"%s accepted=%d phase=%s diag=%zu error=%s\n",file,info.accepted,info.phase,info.diagnostics,info.error?info.error:"");
   for(size_t i=0;i<info.diagnostics;++i){xls_check_diagnostic d;if(xlsSemanticCheckDiagnostic(result,i,&d))fprintf(stderr,"%d:%d:%s\n",d.line,d.column,d.message);}
  }
  CHECK(info.accepted==expected);
  if(expected){
   CHECK(info.irNodes && info.irTypes && !info.diagnostics);
   xls_check_ir_work ir=xlsSemanticCheckIrWork(result);xls_check_ir_persistent_work persistent=xlsSemanticCheckIrPersistentWork(result);
   CHECK(!ir.reused && !persistent.reused);
   xls_check_inputs captured={0};CHECK(xlsSemanticCheckInputs(result,&captured));
   if(packages)CHECK(captured.sources>1 && captured.sources<=128 && captured.probes<=1024);
   printf("analysis round=%u ms=%.3f sources=%zu nodes=%zu rawDeclined=%llu persistentDeclined=%llu\n",round,ms,captured.sources,info.irNodes,(unsigned long long)ir.declined,(unsigned long long)persistent.declined);
  }else CHECK(info.diagnostics);
  xlsSemanticCheckDestroy(previous);previous=result;xrtStrBufFree(&body);
 }
 xlsSemanticCheckDestroy(previous);xlsContextRelease(context);xrtFree(path);xrtFree(data);return 0;
}
static int completion(void)
{
 const char* text="func main(...) as int\n int localValue=3\n return loca\nend\n";
 const char* mark=strstr(text,"loca\n");CHECK(mark);char* path=xrtPathAbs("final-completion.xl");CHECK(path);
 xls_semantic_input input={path,text,strlen(text)};char error[256]={0};
 xls_semantic_completion* result=xlsSemanticCompletionCreate(path,path,(size_t)(mark-text)+4,&input,1,NULL,NULL,error,sizeof(error));
 CHECK(result);xls_completion_info info=xlsSemanticCompletionInfo(result);CHECK(info.available);
 int found=0;for(size_t i=0;i<info.count;++i){xls_completion_item item;CHECK(xlsSemanticCompletionItem(result,i,&item));if(!strcmp(item.label,"localValue"))found=1;}
 CHECK(found);xlsSemanticCompletionDestroy(result);xrtFree(path);return 0;
}
int main(int argc,char** argv)
{
 CHECK(argc==3 || argc==4);CHECK(!fixture(argv[1],strcmp(argv[2],"0")!=0,argc==4 ? argv[3]:NULL));CHECK(!completion());puts("passed: final frontend features/diagnostics/completion");return 0;
}
