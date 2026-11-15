#include "services/workspace_symbols.h"
#include "services/workspace_discovery.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static volatile LONG tChecks,tFrames,tLive,tOwnerIO,tTracking;static PVOID volatile tHeld;static DWORD tOwner;static int tMode;
static HANDLE tEntered,tResume;static const char *tManifest,*tSource,*tLate;static uint64_t tFrameThread;
#define CHECK(v) do{InterlockedIncrement(&tChecks);if(!(v)){fprintf(stderr,"workspace-symbol-frame:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static const char* tProject="{\"format\":\"xlang-project-1\",\"entry\":\"main.xl\",\"ide\":{\"files\":[{\"id\":\"closed\",\"path\":\"closed.xl\"}],\"configurations\":[{\"id\":\"debug\",\"arguments\":[\"runtime-only\"]}],\"activeConfiguration\":\"debug\"}}";
static void tWrite(const char* path,const char* body){CHECK(xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}));}
static void tObserveIO(void){if(InterlockedCompareExchange(&tTracking,0,0)&&GetCurrentThreadId()==tOwner)InterlockedIncrement(&tOwnerIO);}
xfile __real_xideIoOpen(const char*,uint32);
bool __real_xrtClose(xfile);
int __real_xideIoStat(const char*,int,xfileinfo*);
int __real_xideIoReceiptStat(const char*,xfileinfo*);
xfile __wrap_xideIoOpen(const char* path,uint32 flags)
{
 tObserveIO();xfile file=__real_xideIoOpen(path,flags);
 if(file&&tMode==6&&InterlockedCompareExchange(&tFrames,0,0)&&!strcmp(path,tManifest)){
  CHECK(!InterlockedExchangePointer(&tHeld,file));InterlockedIncrement(&tLive);CHECK(SetEvent(tEntered)&&WaitForSingleObject(tResume,30000)==WAIT_OBJECT_0);
 }return file;
}
bool __wrap_xrtClose(xfile file){bool okay=__real_xrtClose(file);if(file&&InterlockedCompareExchangePointer(&tHeld,NULL,file)==file)InterlockedDecrement(&tLive);return okay;}
int __wrap_xideIoStat(const char* path,int follow,xfileinfo* info){tObserveIO();return __real_xideIoStat(path,follow,info);}
int __wrap_xideIoReceiptStat(const char* path,xfileinfo* info){tObserveIO();return __real_xideIoReceiptStat(path,info);}
static char* tFrame(const xvalue* value,size_t* bytes,xide_model_error* error)
{
 InterlockedIncrement(&tFrames);tFrameThread=xrtThreadCurrentId();CHECK(GetCurrentThreadId()!=tOwner);if(tMode==5)return NULL;
 char* frame=xideLspFrame(value,bytes,error);CHECK(frame);
 if(tMode==1){char* body=_strdup(tProject);CHECK(body);char* p=strstr(body,"runtime-only");CHECK(p);memcpy(p,"runtime-new!",12);tWrite(tManifest,body);free(body);}
 else if(tMode==2)tWrite(tSource,"export func changedOnly() as int\n return 2\nend\n");
 else if(tMode==3)tWrite(tLate,"func main(...) as int\n return 0\nend\n");
 else if(tMode==4)tWrite(tManifest,tProject);
 return frame;
}
static int tCommit(xide_workspace_symbol_update* update,xide_model_error* error)
{int okay=xideWorkspaceSymbolIndexCommit(update,error);if(okay&&tMode==13)tWrite(tManifest,tProject);return okay;}
#define xideLspFrame tFrame
#define xideWorkspaceSymbolIndexCommit tCommit
#include "../src/services/workspace_symbols.c"
#undef xideLspFrame
#undef xideWorkspaceSymbolIndexCommit
static char* tJoin(const char* base,const char* leaf){char* path=xidePathResolveLexical(base,leaf);CHECK(path);return path;}
static const xvalue* tGet(const xvalue* value,const char* name){return xrtValueObjectGet(value,xrtStrView(name));}
static xvalue* tJson(const char* text){xvalue* value=xrtJsonParse(xrtStrView(text));CHECK(value);return value;}
static int tReceive(const xvalue* value,void* user){xvalue** out=user;CHECK(!*out);*out=xrtValueDeepClone(value);return *out!=NULL;}
static xvalue* tParse(const char* frame,size_t bytes)
{xvalue* reply=NULL;xide_lsp_stream stream={0};CHECK(xideLspStreamFeed(&stream,frame,bytes,tReceive,&reply,NULL)&&reply);xideLspStreamClear(&stream);return reply;}
static xide_analysis_catalog* tCatalog(const char* base)
{
 char* uri=xideLspPathToUri(base,NULL);xvalue* init=tJson("{\"workspaceFolders\":[{\"name\":\"symbols\"}]}");xvalue* row=(xvalue*)xrtValueArrayGet(tGet(init,"workspaceFolders"),0);CHECK(uri&&xrtValueObjectSetNew(row,xrtStrView("uri"),xrtValueString(xrtStrView(uri))));free(uri);xide_workspace_folders* roots=xideWorkspaceFoldersRead(init,NULL);xrtValueRelease(init);CHECK(roots);xide_workspace_discovery* service=xideWorkspaceDiscoveryCreate(NULL,NULL);CHECK(service);xide_discovery_query* q=xideWorkspaceDiscoverySubmit(service,roots,NULL);xideWorkspaceFoldersRelease(roots);CHECK(q);
 int state;char error[512];ULONGLONG until=GetTickCount64()+30000;while((state=xideWorkspaceDiscoveryResult(q,NULL,NULL,error,sizeof(error)))==XIDE_DISCOVERY_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}if(state!=XIDE_DISCOVERY_READY)fprintf(stderr,"discovery state=%d:%s\n",state,error);CHECK(state==XIDE_DISCOVERY_READY);xide_discovery_snapshot* snapshot=xideWorkspaceDiscoveryTake(q);CHECK(snapshot);xide_analysis_catalog* catalog=xideAnalysisCatalogRetain(xideDiscoverySnapshotCatalog(snapshot));CHECK(catalog&&xideAnalysisCatalogCount(catalog)==2&&xideAnalysisCatalogFileCount(catalog)==1);xideWorkspaceDiscoveryQueryDestroy(q);xideDiscoverySnapshotRelease(snapshot);while(!xideWorkspaceDiscoveryIdle(service)){CHECK(GetTickCount64()<until);Sleep(1);}xideWorkspaceDiscoveryDestroy(service);return catalog;
}
static int tWait(xide_workspace_symbol_query* query,xide_workspace_symbol_work* work)
{int state;char error[512];ULONGLONG until=GetTickCount64()+30000;while((state=xideWorkspaceSymbolsResult(query,NULL,work,error,sizeof(error)))==XIDE_WORKSPACE_SYMBOL_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}return state;}
static xls_context* tFile(const char* path)
{char* directory=xidePathDirectory(path);xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=path,.directory=directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){&path,1};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);free(directory);CHECK(context);return context;}
static void tAdmission(xide_workspace_symbols* store,xide_analysis_catalog* catalog,const char* base)
{
 xvalue* id=xrtValueObject();CHECK(id&&!xideWorkspaceSymbolsSubmitFramed(store,catalog,NULL,0,NULL,0,"Only",200,0,id,NULL));xrtValueRelease(id);id=xrtValueString((xstrview){"a\0b",3});CHECK(id&&!xideWorkspaceSymbolsSubmitFramed(store,catalog,NULL,0,NULL,0,"Only",200,0,id,NULL));xrtValueRelease(id);
 char* large=malloc(65537);CHECK(large);memset(large,'x',65536);large[65536]=0;id=xrtValueString((xstrview){large,65536});CHECK(id&&!xideWorkspaceSymbolsSubmitFramed(store,catalog,NULL,0,NULL,0,"Only",200,0,id,NULL));xrtValueRelease(id);free(large);id=xrtValueInt(7);CHECK(id);
 const xls_context* project=xideAnalysisCatalogAt(catalog,0);xls_context_spec changed=*xlsContextSpec(project);char* foreign=tJoin(base,"foreign.xlp");changed.project=foreign;xls_context* context=xlsContextCreate(&changed,16u*1024u*1024u,NULL);CHECK(context&&!xideWorkspaceSymbolsSubmitFramed(store,catalog,&context,1,NULL,0,"Only",200,0,id,NULL));xlsContextRelease(context);free(foreign);
 context=tFile(xlsContextSpec(project)->entry);CHECK(!xideWorkspaceSymbolsSubmitFramed(store,catalog,&context,1,NULL,0,"Only",200,0,id,NULL));xlsContextRelease(context);context=(xls_context*)project;CHECK(!xideWorkspaceSymbolsSubmitFramed(store,NULL,&context,1,NULL,0,"Only",200,0,id,NULL));xrtValueRelease(id);
}
static void tCase(const char* data,int mode)
{
 char leaf[32];snprintf(leaf,sizeof(leaf),"case-%d",mode);char* base=tJoin(data,leaf);CHECK(xideIoCreateDirectories(base));char *main=tJoin(base,"main.xl"),*closed=tJoin(base,"closed.xl"),*loose=tJoin(base,"loose.xl"),*manifest=tJoin(base,"p.xlp"),*workspace=tJoin(base,"w.xlw"),*external=tJoin(base,"extlibs/other"),*late=tJoin(base,"late.xl"),*compiler=tJoin(base,"missing-xl.exe");CHECK(xideIoCreateDirectories(external));char *other=tJoin(external,"main.xl"),*otherProject=tJoin(external,"q.xlp");
 tWrite(main,"func main(...) as int\n return 0\nend\n");tWrite(closed,"export func closedOnly() as int\n return 1\nend\n");tWrite(loose,"export func looseOnly() as int\n return 1\nend\nfunc main(...) as int\n return 0\nend\n");tWrite(other,"export func otherOnly() as int\n return 1\nend\nfunc main(...) as int\n return 0\nend\n");tWrite(manifest,tProject);tWrite(otherProject,"{\"format\":\"xlang-project-1\",\"entry\":\"main.xl\"}");tWrite(workspace,"{\"format\":\"xlang-workspace-1\",\"name\":\"w\",\"projects\":[{\"id\":\"p\",\"path\":\"p.xlp\"},{\"id\":\"q\",\"path\":\"extlibs/other/q.xlp\"}]}");
 xide_analysis_catalog* catalog=tCatalog(base);xide_toolchains* cache=xideToolchainsCreate();xls_module_store* modules=xlsModuleStoreCreate(NULL);xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(cache,compiler,modules);CHECK(cache&&modules&&store);
 tMode=mode;tManifest=manifest;tSource=closed;tLate=late;InterlockedExchange(&tFrames,0);InterlockedExchange(&tOwnerIO,0);ResetEvent(tEntered);ResetEvent(tResume);
 if(mode==11){InterlockedExchange(&tTracking,1);tAdmission(store,catalog,base);InterlockedExchange(&tTracking,0);CHECK(!tOwnerIO);goto cleanup;}
 xvalue* id=mode==9?xrtValueInt(INT64_MAX):xrtValueString(xrtStrView("符号😀\\\"id"));if(mode==10){char* longId=malloc(65536);CHECK(longId);memset(longId,'x',65535);longId[65535]=0;xrtValueRelease(id);id=xrtValueString((xstrview){longId,65535});free(longId);}CHECK(id);xvalue* expectedId=xrtValueDeepClone(id);CHECK(expectedId);
 xls_context* extra=NULL;char* extraPath=NULL;if(mode==12){extraPath=tJoin(data,"extra.xl");tWrite(extraPath,"export func extraOnly() as int\n return 1\nend\nfunc main(...) as int\n return 0\nend\n");extra=tFile(extraPath);}
 xvalue* selection=NULL;
 if(mode==8){tMode=0;xide_workspace_symbol_query* warm=xideWorkspaceSymbolsSubmitFramed(store,catalog,NULL,0,NULL,0,"Only",200,0,id,NULL);xide_workspace_symbol_work work={0};CHECK(warm&&tWait(warm,&work)==XIDE_WORKSPACE_SYMBOL_READY);size_t bytes=0;char* frame=xideWorkspaceSymbolsTakeFrame(warm,&bytes);CHECK(frame);xvalue* reply=tParse(frame,bytes);selection=xrtValueRetain((xvalue*)xrtValueArrayGet(tGet(reply,"result"),0));CHECK(selection);free(frame);xrtValueRelease(reply);xideWorkspaceSymbolsQueryDestroy(warm);tMode=8;InterlockedExchange(&tFrames,0);}
 if(mode==7)tWrite(manifest,tProject);
 InterlockedExchange(&tTracking,1);xide_workspace_symbol_query* query=mode>=13?xideWorkspaceSymbolsRefreshCatalog(store,catalog,NULL,0,NULL,0):xideWorkspaceSymbolsSubmitFramed(store,catalog,extra?&extra:NULL,extra?1:0,NULL,0,"Only",200,0,id,selection);CHECK(query);xideAnalysisCatalogDestroy(catalog);catalog=NULL;xrtValueRelease(id);xrtValueRelease(selection);xlsContextRelease(extra);free(extraPath);
 if(mode==6){CHECK(WaitForSingleObject(tEntered,30000)==WAIT_OBJECT_0);xideWorkspaceSymbolsCancel(query);CHECK(tLive==1&&!xideWorkspaceSymbolsIdle(store)&&xideWorkspaceSymbolsResult(query,NULL,NULL,NULL,0)==XIDE_WORKSPACE_SYMBOL_PENDING);CHECK(SetEvent(tResume));}
 xide_workspace_symbol_work work={0};int state=tWait(query,&work),code=xideWorkspaceSymbolsErrorCode(query);size_t bytes=0;CHECK(!xideWorkspaceSymbolsTakeLsp(query));char* frame=xideWorkspaceSymbolsTakeFrame(query,&bytes);
 if(mode==0||mode==8||mode==9||mode==10||mode==12){CHECK(state==XIDE_WORKSPACE_SYMBOL_READY&&!code&&frame&&bytes&&work.encodedFrameBytes==bytes&&work.scopeValidations==2&&work.coordinatorThread==tFrameThread);xvalue* reply=tParse(frame,bytes);CHECK(xrtValueEqual(tGet(reply,"id"),expectedId));const xvalue* result=tGet(reply,"result");if(mode==8)CHECK(xrtValueType(result)==XVALUE_OBJECT&&tGet(result,"symbol")&&tGet(result,"document")&&work.reused>0);else CHECK(xrtValueType(result)==XVALUE_ARRAY&&xrtValueCount(result)==(mode==12?4:3));xrtValueRelease(reply);CHECK(!xideWorkspaceSymbolsTakeFrame(query,NULL));}
 else if(mode==14){CHECK(state==XIDE_WORKSPACE_SYMBOL_READY&&!code&&!frame&&!bytes&&work.scopeValidations==2&&!work.encodedFrameBytes&&!tFrames);}
 else{CHECK(!frame&&!bytes);if(mode==6)CHECK(state==XIDE_WORKSPACE_SYMBOL_CANCELLED&&code==-32800);else CHECK(state==XIDE_WORKSPACE_SYMBOL_FAILED&&code==(mode==5?-32603:mode==2?-32803:-32801));if(mode==7)CHECK(!tFrames&&!work.analysisBuilds&&!work.encodedFrameBytes);if(mode==13)CHECK(!tFrames&&!work.encodedFrameBytes&&work.scopeValidations==2);}
 free(frame);xrtValueRelease(expectedId);xideWorkspaceSymbolsQueryDestroy(query);ULONGLONG until=GetTickCount64()+30000;while(!xideWorkspaceSymbolsIdle(store)){CHECK(GetTickCount64()<until);Sleep(1);}InterlockedExchange(&tTracking,0);CHECK(!tLive&&!tHeld&&!tOwnerIO);
cleanup:
 xideAnalysisCatalogDestroy(catalog);xideWorkspaceSymbolsDestroy(store);xlsModuleStoreRelease(modules);xideToolchainsDestroy(cache);free(main);free(closed);free(loose);free(manifest);free(workspace);free(external);free(late);free(compiler);free(other);free(otherProject);free(base);
}
int main(int argc,char** argv)
{
 CHECK(argc==2);tOwner=GetCurrentThreadId();tEntered=CreateEventW(NULL,TRUE,FALSE,NULL);tResume=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(tEntered&&tResume);char leaf[64];snprintf(leaf,sizeof(leaf),"symbol-frames-%lu-%llu",(unsigned long)GetCurrentProcessId(),GetTickCount64());char* data=tJoin(argv[1],leaf);CHECK(xideIoCreateDirectories(data));for(int mode=0;mode<15;++mode)tCase(data,mode);free(data);CloseHandle(tEntered);CloseHandle(tResume);printf("workspace_symbol_frame_test passed:%u checks;complete catalog scopes,worker frames,source/metadata guards,IDs,navigation,refresh and physical cancellation.\n",(unsigned)tChecks);return 0;
}
