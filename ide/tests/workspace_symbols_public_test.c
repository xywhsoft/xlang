#include "services/workspace_symbols.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbols_public_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static int writeText(const char* path,const char* body){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
static void signalWake(void* p){xrtEventSet(p);}
static size_t named(const xide_workspace_symbol_result* result,const char* name)
{size_t n=0;for(size_t i=0;i<xideWorkspaceSymbolResultCount(result);++i){xide_workspace_symbol_view row;if(xideWorkspaceSymbolResultAt(result,i,&row) && !strcmp(row.declaration.name,name))++n;}return n;}
int main(void)
{
 char* directory=xrtPathAbs("build/workspace-symbols-public");CHECK(directory && xrtDirCreateAll(directory));char *entry=xrtPathJoin(directory,"main.xl"),*dependency=xrtPathJoin(directory,"helper.xl"),*compiler=xrtPathJoin(directory,"absent-xl.exe");CHECK(entry && dependency && compiler);
 const char* mainBody="import helper\nfunc main(...) as int\n return helper.value(7)\nend\n";const char* body="export func value(int n) as int\n return n+1\nend\n";const char* dirtyBody="export func value(int n) as int\n return n+2\nend\nexport func added() as int\n return 1\nend\n";CHECK(writeText(entry,mainBody) && writeText(dependency,body));
 const char* members[]={dependency};xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,1};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_context* contexts[]={context};xide_toolchains* cache=xideToolchainsCreate();xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(cache,compiler,NULL);xevent* event=xrtEventCreate(false,false);CHECK(context && cache && store && event);
 xls_text* text=xlsTextCreate(dirtyBody,strlen(dirtyBody),1024*1024,NULL);xls_analysis_document doc={"helper",dependency,text,1,1};CHECK(text);xide_workspace_symbol_result* old=NULL;
 for(unsigned round=0;round<3;++round){xide_workspace_symbol_query* query=xideWorkspaceSymbolsSubmit(store,contexts,1,round==2?&doc:NULL,round==2?1:0,"",64);CHECK(query && xideWorkspaceSymbolsSetWake(query,signalWake,event));xide_workspace_symbol_work work={0};char error[512]={0};enum xide_workspace_symbol_state state;
  for(unsigned spin=0;;++spin){state=xideWorkspaceSymbolsResult(query,NULL,&work,error,sizeof(error));if(state!=XIDE_WORKSPACE_SYMBOL_PENDING)break;if(spin>=30000)CHECK(0);xrtEventWaitFor(event,XIDE_XRT_TIMEOUT_MS(10));}
  if(state!=XIDE_WORKSPACE_SYMBOL_READY)fprintf(stderr,"%s\n",error);
  CHECK(state==XIDE_WORKSPACE_SYMBOL_READY);xide_workspace_symbol_result* result=xideWorkspaceSymbolsTake(query);CHECK(result && !xideWorkspaceSymbolsTake(query) && named(result,"value")==1 && named(result,"main")==1 && named(result,"added")== (round==2));
  CHECK(work.roots==2 && work.covered==1 && work.index.rows==(round==2?3u:2u));if(round==1)CHECK(!work.captured && work.reused==1);else CHECK(work.captured==1);xideWorkspaceSymbolsQueryDestroy(query);if(!round)old=result;else xideWorkspaceSymbolResultDestroy(result);
 }
 xide_workspace_symbol_query* wire=xideWorkspaceSymbolsSubmitLsp(store,contexts,1,&doc,1,"added",64,UINT32_C(0x3ffffff));CHECK(wire);enum xide_workspace_symbol_state state;
 for(unsigned spin=0;;++spin){state=xideWorkspaceSymbolsResult(wire,NULL,NULL,NULL,0);if(state!=XIDE_WORKSPACE_SYMBOL_PENDING)break;if(spin>=30000)CHECK(0);xrtSleep(1);}CHECK(state==XIDE_WORKSPACE_SYMBOL_READY);xvalue* encoded=xideWorkspaceSymbolsTakeLsp(wire);CHECK(encoded && xrtValueCount(encoded)==1 && !xideWorkspaceSymbolsTakeLsp(wire));xrtValueRelease(encoded);xideWorkspaceSymbolsQueryDestroy(wire);
 CHECK(xideWorkspaceSymbolQueryValid("") && xideWorkspaceSymbolQueryValid("计值 Box") && !xideWorkspaceSymbolQueryValid(NULL));
 /* Identity/revision zero are valid native inputs,including LSP INT_MIN. */
 for(unsigned round=0;round<3;++round){doc.identity=0;doc.revision=round==2?UINT32_MAX:round;
  wire=xideWorkspaceSymbolsSubmitLsp(store,contexts,1,&doc,1,"added",64,UINT32_C(0x3ffffff));CHECK(wire);
  for(unsigned spin=0;;++spin){state=xideWorkspaceSymbolsResult(wire,NULL,NULL,NULL,0);if(state!=XIDE_WORKSPACE_SYMBOL_PENDING)break;if(spin>=30000)CHECK(0);xrtSleep(1);}CHECK(state==XIDE_WORKSPACE_SYMBOL_READY);
  encoded=xideWorkspaceSymbolsTakeLsp(wire);CHECK(encoded && xrtValueCount(encoded)==1);xrtValueRelease(encoded);xideWorkspaceSymbolsQueryDestroy(wire);
 }
 xideWorkspaceSymbolsDestroy(store);CHECK(old && named(old,"value")==1 && !named(old,"added"));xideWorkspaceSymbolResultDestroy(old);xrtEventDestroy(event);xlsTextRelease(text);xlsContextRelease(context);xideToolchainsDestroy(cache);xrtFree(directory);xrtFree(entry);xrtFree(dependency);xrtFree(compiler);printf("workspace_symbols_public_test passed: %u checks;ordinary production module,zero-open captures,retained reuse,real dependency overlay,standard wire and detached result.\n",checks);return 0;
}
