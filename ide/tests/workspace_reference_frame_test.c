#include "services/project_references.h"
#include "services/workspace_discovery.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static volatile LONG tChecks,tFrames,tLive,tOwnerIO,tTracking;static PVOID volatile tHeld;
static DWORD tOwner;static int tMode;static uint64_t tFrameThread;static HANDLE tEntered,tResume;
static const char *tManifest,*tSource,*tLibrary,*tLate,*tRoute;
#define CHECK(v) do{InterlockedIncrement(&tChecks);if(!(v)){fprintf(stderr,"workspace-reference-frame:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static const char* tBody="\nimport extlibs.shared as api\nfunc main(...) as int\n return api.amount(1)\nend\n";
static const char* tDeclaration="export func amount(int value) as int\n return value\nend\n";
static const char* tProject="{\"format\":\"xlang-project-1\",\"entry\":\"a.xl\",\"ide\":{\"files\":[{\"id\":\"closed\",\"path\":\"closed.xl\"}],\"configurations\":[{\"id\":\"debug\",\"arguments\":[\"runtime-only\"]}],\"activeConfiguration\":\"debug\"}}";
static void tWrite(const char* path,const char* body){CHECK(xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}));}
static void tObserveIO(void){if(InterlockedCompareExchange(&tTracking,0,0)&&GetCurrentThreadId()==tOwner)InterlockedIncrement(&tOwnerIO);}
xfile __real_xideIoOpen(const char*,uint32);bool __real_xrtClose(xfile);
int __real_xideIoStat(const char*,int,xfileinfo*);int __real_xideIoReceiptStat(const char*,xfileinfo*);
xfile __wrap_xideIoOpen(const char* path,uint32 flags)
{tObserveIO();xfile file=__real_xideIoOpen(path,flags);if(file&&tMode==6&&InterlockedCompareExchange(&tFrames,0,0)&&!strcmp(path,tManifest)){CHECK(!InterlockedExchangePointer(&tHeld,file));InterlockedIncrement(&tLive);CHECK(SetEvent(tEntered)&&WaitForSingleObject(tResume,30000)==WAIT_OBJECT_0);}return file;}
bool __wrap_xrtClose(xfile file){bool okay=__real_xrtClose(file);if(file&&InterlockedCompareExchangePointer(&tHeld,NULL,file)==file)InterlockedDecrement(&tLive);return okay;}
int __wrap_xideIoStat(const char* path,int follow,xfileinfo* info){tObserveIO();return __real_xideIoStat(path,follow,info);}
int __wrap_xideIoReceiptStat(const char* path,xfileinfo* info){tObserveIO();return __real_xideIoReceiptStat(path,info);}
static char* tFrame(const xvalue* value,size_t* bytes,xide_model_error* error)
{
 InterlockedIncrement(&tFrames);tFrameThread=xrtThreadCurrentId();CHECK(GetCurrentThreadId()!=tOwner);if(tMode==5)return NULL;
 char* frame=xideLspFrame(value,bytes,error);CHECK(frame);
 if(tMode==1){char* body=_strdup(tProject);CHECK(body);char* p=strstr(body,"runtime-only");CHECK(p);memcpy(p,"runtime-new!",12);tWrite(tManifest,body);free(body);}
 else if(tMode==2)tWrite(tSource,"func main(...) as int\n return 2\nend\n");
 else if(tMode==3)tWrite(tLate,tBody);
 else if(tMode==4){wchar_t* path=xidePathNativeWide(tLibrary);CHECK(path&&SetFileAttributesW(path,FILE_ATTRIBUTE_NORMAL));free(path);tWrite(tLibrary,"export func amount(int value) as int\n return value+1\nend\n");}
 return frame;
}
static int tCapture(xide_language_check* check,xide_reference_shard** shard,xide_reference_key** key)
{int okay=xideLanguageCheckCaptureReferences(check,shard,key);if(okay&&key&&*key&&tMode==16&&tRoute){tWrite(tRoute,"import closed as extra\nfunc main(...) as int\n return 0\nend\n");tRoute=NULL;}return okay;}
#define xideLspFrame tFrame
#define xideLanguageCheckCaptureReferences tCapture
#include "../src/services/project_references.c"
#undef xideLspFrame
#undef xideLanguageCheckCaptureReferences
static char* tJoin(const char* base,const char* leaf){char* path=xidePathResolveLexical(base,leaf);CHECK(path);return path;}
static const xvalue* tGet(const xvalue* value,const char* name){return xrtValueObjectGet(value,xrtStrView(name));}
static xvalue* tJson(const char* text){xvalue* value=xrtJsonParse(xrtStrView(text));CHECK(value);return value;}
static int tReceive(const xvalue* value,void* user){xvalue** out=user;CHECK(!*out);*out=xrtValueDeepClone(value);return *out!=NULL;}
static xvalue* tParse(const char* frame,size_t bytes){xvalue* reply=NULL;xide_lsp_stream stream={0};CHECK(xideLspStreamFeed(&stream,frame,bytes,tReceive,&reply,NULL)&&reply);xideLspStreamClear(&stream);return reply;}
static xide_analysis_catalog* tCatalog(const char* base)
{
 char* uri=xideLspPathToUri(base,NULL);xvalue* init=tJson("{\"workspaceFolders\":[{\"name\":\"references\"}]}");xvalue* row=(xvalue*)xrtValueArrayGet(tGet(init,"workspaceFolders"),0);CHECK(uri&&xrtValueObjectSetNew(row,xrtStrView("uri"),xrtValueString(xrtStrView(uri))));free(uri);
 xide_workspace_folders* roots=xideWorkspaceFoldersRead(init,NULL);xrtValueRelease(init);CHECK(roots);xide_workspace_discovery* service=xideWorkspaceDiscoveryCreate(NULL,NULL);CHECK(service);xide_discovery_query* q=xideWorkspaceDiscoverySubmit(service,roots,NULL);xideWorkspaceFoldersRelease(roots);CHECK(q);
 int state;char error[512];ULONGLONG until=GetTickCount64()+30000;while((state=xideWorkspaceDiscoveryResult(q,NULL,NULL,error,sizeof(error)))==XIDE_DISCOVERY_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}if(state!=XIDE_DISCOVERY_READY)fprintf(stderr,"discovery=%d:%s\n",state,error);CHECK(state==XIDE_DISCOVERY_READY);
 xide_discovery_snapshot* snapshot=xideWorkspaceDiscoveryTake(q);CHECK(snapshot);xide_analysis_catalog* catalog=xideAnalysisCatalogRetain(xideDiscoverySnapshotCatalog(snapshot));CHECK(catalog&&xideAnalysisCatalogCount(catalog)==3&&xideAnalysisCatalogFileCount(catalog)==1);xideWorkspaceDiscoveryQueryDestroy(q);xideDiscoverySnapshotRelease(snapshot);while(!xideWorkspaceDiscoveryIdle(service)){CHECK(GetTickCount64()<until);Sleep(1);}xideWorkspaceDiscoveryDestroy(service);return catalog;
}
static int tWait(xide_reference_query* query,xide_project_reference_stats* work)
{int state;char error[512]={0};ULONGLONG until=GetTickCount64()+30000;while((state=xideProjectReferencesResult(query,NULL,work,error,sizeof(error)))==XIDE_REFERENCES_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}if(state==XIDE_REFERENCES_FAILED)fprintf(stderr,"case=%d:%s\n",tMode,error);return state;}
static void tAdmission(xide_project_references* store,xide_analysis_catalog* catalog,xls_context* selected,const char* path,xls_analysis_document* docs)
{
 xvalue* id=xrtValueObject();CHECK(id&&!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"cursor",path,3,14,1,0,id));xrtValueRelease(id);
 id=xrtValueString((xstrview){"a\0b",3});CHECK(id&&!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"cursor",path,3,14,1,0,id));xrtValueRelease(id);
 char* large=malloc(65536);CHECK(large);memset(large,'x',65536);id=xrtValueString((xstrview){large,65536});CHECK(id&&!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"cursor",path,3,14,1,0,id));xrtValueRelease(id);free(large);
 id=xrtValueInt(7);CHECK(id);
 CHECK(!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,65,"cursor",path,3,14,1,0,id));
 CHECK(!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"wrong",path,3,14,1,0,id));
 CHECK(!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"cursor",path,-1,14,1,0,id));
 CHECK(!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"cursor",path,3,14,2,0,id));
 CHECK(!xideProjectReferencesSubmitFramed(store,catalog,selected,docs,1,"cursor",path,3,14,1,2,id));
 xls_context_spec foreign=*xlsContextSpec(selected);foreign.configuration="foreign";xls_context* context=xlsContextCreate(&foreign,16u*1024u*1024u,NULL);CHECK(context&&!xideProjectReferencesSubmitFramed(store,catalog,context,docs,1,"cursor",path,3,14,1,0,id));xlsContextRelease(context);xrtValueRelease(id);
}
static void tCase(const char* data,int mode)
{
 char leaf[32];snprintf(leaf,sizeof(leaf),"case-%d",mode);char* base=tJoin(data,leaf);CHECK(xideIoCreateDirectories(base));
 const char* names[]={"a.xl","closed.xl","b.xl","c.xl","loose.xl","A.xlp","B.xlp","C.xlp","w.xlw","extlibs/shared.xl","late.xl","missing-xl.exe","virtual.xl"};char* p[13];for(size_t i=0;i<13;++i)p[i]=tJoin(base,names[i]);
 char* libs=tJoin(base,"extlibs");CHECK(xideIoCreateDirectories(libs));free(libs);tWrite(p[0],tBody);tWrite(p[1],tBody);tWrite(p[2],tBody);tWrite(p[3],"func amount(int value) as int\n return value\nend\nfunc main(...) as int\n return amount(1)\nend\n");tWrite(p[4],tBody);tWrite(p[9],tDeclaration);tWrite(p[5],tProject);tWrite(p[6],"{\"format\":\"xlang-project-1\",\"entry\":\"b.xl\"}");tWrite(p[7],"{\"format\":\"xlang-project-1\",\"entry\":\"c.xl\"}");tWrite(p[8],"{\"format\":\"xlang-workspace-1\",\"name\":\"refs\",\"projects\":[{\"id\":\"a\",\"path\":\"A.xlp\"},{\"id\":\"b\",\"path\":\"B.xlp\"},{\"id\":\"c\",\"path\":\"C.xlp\"}]}");
 wchar_t* readonly=xidePathNativeWide(p[9]);CHECK(readonly&&SetFileAttributesW(readonly,FILE_ATTRIBUTE_READONLY));free(readonly);
 xide_analysis_catalog* catalog=tCatalog(base);xide_toolchains* cache=xideToolchainsCreate();xls_module_store* modules=xlsModuleStoreCreate(NULL);xide_project_references* store=xideProjectReferencesCreate(cache,p[11],modules);CHECK(cache&&modules&&store);
 const char* cursor=mode==16?p[1]:p[0];const xls_context* selected=NULL;CHECK(xideAnalysisCatalogFind(catalog,cursor,&selected)==XIDE_ANALYSIS_PROJECT&&selected);
 xls_text* text=xlsTextCreate(tBody,strlen(tBody),strlen(tBody),NULL);CHECK(text);xls_analysis_document docs[]={{"cursor",cursor,text,41,UINT32_MAX},{"other",p[2],text,42,0},{"virtual",p[12],text,43,0}};
 size_t count=mode==7?0:mode==8?1:mode==12?3:1;const char* trigger=count&&mode!=8?"cursor":NULL;if(mode==8)docs[0]=docs[1];
 xvalue* id=mode==9?xrtValueInt(INT64_MAX):xrtValueString(xrtStrView("引用😀\\\"id"));if(mode==10){char* longId=malloc(65536);CHECK(longId);memset(longId,'x',65535);xrtValueRelease(id);id=xrtValueString((xstrview){longId,65535});free(longId);}if(mode==11){xrtValueRelease(id);id=xrtValueString(xrtStrView(""));}CHECK(id);xvalue* expected=xrtValueDeepClone(id);CHECK(expected);
 tMode=mode;tManifest=p[5];tSource=p[3];tLibrary=p[9];tLate=p[10];tRoute=p[0];InterlockedExchange(&tFrames,0);InterlockedExchange(&tOwnerIO,0);ResetEvent(tEntered);ResetEvent(tResume);
 if(mode==15)tWrite(p[5],tProject);
 InterlockedExchange(&tTracking,1);
 if(mode==18){tAdmission(store,catalog,(xls_context*)selected,cursor,docs);CHECK(!tOwnerIO&&!tFrames&&xideProjectReferencesIdle(store));InterlockedExchange(&tTracking,0);xrtValueRelease(id);xrtValueRelease(expected);xlsTextRelease(text);xideAnalysisCatalogDestroy(catalog);goto cleanup;}
 xide_reference_query* q=xideProjectReferencesSubmitFramed(store,catalog,(xls_context*)selected,docs,count,trigger,cursor,mode==13?0:mode==14?999:3,mode==13?0:14,mode!=17,mode==12,id);CHECK(q);xideAnalysisCatalogDestroy(catalog);catalog=NULL;xrtValueRelease(id);xlsTextRelease(text);
 if(mode==6){CHECK(WaitForSingleObject(tEntered,30000)==WAIT_OBJECT_0);xideProjectReferencesCancel(q);CHECK(tLive==1&&!xideProjectReferencesIdle(store)&&xideProjectReferencesResult(q,NULL,NULL,NULL,0)==XIDE_REFERENCES_PENDING);CHECK(SetEvent(tResume));}
 xide_project_reference_stats work={0};int state=tWait(q,&work),code=xideProjectReferencesErrorCode(q);size_t bytes=0;CHECK(!xideProjectReferencesTake(q));char* frame=xideProjectReferencesTakeFrame(q,&bytes);
 if(mode==0||(mode>=7&&mode<=13)||mode==17){
  CHECK(state==XIDE_REFERENCES_READY&&!code&&frame&&bytes&&work.encodedFrameBytes==bytes&&work.scopeValidations==2&&work.thread==tFrameThread);
  xvalue* reply=tParse(frame,bytes);CHECK(xrtValueEqual(tGet(reply,"id"),expected));const xvalue* rows=tGet(reply,"result");CHECK(xrtValueType(rows)==XVALUE_ARRAY&&xrtValueCount(rows)==(mode==13?0:mode==12?6:mode==17?4:5));
  if(mode!=13)CHECK(work.workspaceScopes==work.workspaceCompleted&&work.workspaceScopes==(mode==12?5:4));else CHECK(work.noSymbol&&!work.workspaceScopes);
  for(size_t i=0;i<xrtValueCount(rows);++i){const xvalue* row=xrtValueArrayGet(rows,i);xstrview uri={0};CHECK(xrtValueGetString(tGet(row,"uri"),&uri)&&!strstr(uri.Data,"/c.xl"));if(mode==12)CHECK(tGet(tGet(row,"xlang"),"document"));else CHECK(!tGet(row,"xlang"));}
  xrtValueRelease(reply);CHECK(!xideProjectReferencesTakeFrame(q,NULL));
 }else{if(frame||bytes)fprintf(stderr,"unexpected frame mode=%d state=%d code=%d noSymbol=%zu\n",mode,state,code,work.noSymbol);CHECK(!frame&&!bytes);if(mode==6)CHECK(state==XIDE_REFERENCES_CANCELLED&&code==-32800);else{CHECK(state==XIDE_REFERENCES_FAILED&&code==(mode==5?-32603:mode==14?-32602:-32801));if(mode==15)CHECK(!tFrames&&!work.captured);if(mode==16)CHECK(!tRoute);}}
 free(frame);xrtValueRelease(expected);xideProjectReferencesQueryDestroy(q);ULONGLONG until=GetTickCount64()+30000;while(!xideProjectReferencesIdle(store)){CHECK(GetTickCount64()<until);Sleep(1);}InterlockedExchange(&tTracking,0);CHECK(!tLive&&!tHeld&&!tOwnerIO);
cleanup:xideProjectReferencesDestroy(store);xlsModuleStoreRelease(modules);xideToolchainsDestroy(cache);for(size_t i=0;i<13;++i)free(p[i]);free(base);
}
int main(int argc,char** argv)
{CHECK(argc==2);tOwner=GetCurrentThreadId();tEntered=CreateEventW(NULL,TRUE,FALSE,NULL);tResume=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(tEntered&&tResume);char leaf[80];snprintf(leaf,sizeof(leaf),"workspace-references-%lu-%llu",(unsigned long)GetCurrentProcessId(),GetTickCount64());char* data=tJoin(argv[1],leaf);CHECK(xideIoCreateDirectories(data));for(int mode=0;mode<19;++mode)tCase(data,mode);free(data);CloseHandle(tEntered);CloseHandle(tResume);printf("workspace_reference_frame_test passed:%u checks;19 cases;exact native references,closed and overlay cursors,worker framing,scope/source/routing guards,admission and physical cancellation.\n",(unsigned)tChecks);return 0;}
