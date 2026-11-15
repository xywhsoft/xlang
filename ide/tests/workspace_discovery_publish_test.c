#include "services/workspace_discovery.h"
#include "services/rename_candidate.h"
#include "services/analysis_context.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static volatile LONG checks,frameCalls,liveGuardFiles;static PVOID volatile heldFile;static DWORD owner;static int mutation;static const char *latePath,*manifestPath,*manifestBody;
static HANDLE framedEvent,resumeEvent;
#define CHECK(v) do{InterlockedIncrement(&checks);if(!(v)){fprintf(stderr,"discovery-publish:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static void writeFile(const char* p,const char* body){CHECK(xideIoWriteAtomic(p,(xbytesview){(const unsigned char*)body,strlen(body)}));}
xfile __real_xideIoOpen(const char*,uint32);
bool __real_xrtClose(xfile);
xfile __wrap_xideIoOpen(const char* path,uint32 flags)
{
 xfile file=__real_xideIoOpen(path,flags);
 if(file&&mutation==8&&InterlockedCompareExchange(&frameCalls,0,0)&&!strcmp(path,manifestPath)){
  CHECK(GetCurrentThreadId()!=owner&&!InterlockedExchangePointer(&heldFile,file));InterlockedIncrement(&liveGuardFiles);CHECK(SetEvent(framedEvent));CHECK(WaitForSingleObject(resumeEvent,30000)==WAIT_OBJECT_0);
 }
 return file;
}
bool __wrap_xrtClose(xfile file)
{
 bool okay=__real_xrtClose(file);if(file&&InterlockedCompareExchangePointer(&heldFile,NULL,file)==file)InterlockedDecrement(&liveGuardFiles);return okay;
}
static char* framed(const xvalue* reply,size_t* size,xide_model_error* error)
{
 char* result=xideLspFrame(reply,size,error);if(!result)return NULL;InterlockedIncrement(&frameCalls);CHECK(GetCurrentThreadId()!=owner);
 if(mutation==1||mutation==4)writeFile(latePath,"func main(...) as int\n return 0\nend\n");
 else if(mutation==2)writeFile(manifestPath,manifestBody);
 else if(mutation==3){char* changed=_strdup(manifestBody);CHECK(changed);char* p=strstr(changed,"runtime-only");CHECK(p);memcpy(p,"runtime-new!",12);writeFile(manifestPath,changed);free(changed);}
 else if(mutation==5){CHECK(SetEvent(framedEvent));CHECK(WaitForSingleObject(resumeEvent,30000)==WAIT_OBJECT_0);}
 return result;
}
#define xideLspFrame framed
#include "../src/services/rename_candidate.c"
#undef xideLspFrame
static char* join(const char* base,const char* leaf){char* p=xidePathResolveLexical(base,leaf);CHECK(p);return p;}
static xvalue* json(const char* body){xvalue* value=xrtJsonParse(xrtStrView(body));CHECK(value);return value;}
static int receive(const xvalue* value,void* user){xvalue** out=user;CHECK(!*out);*out=xrtValueDeepClone(value);return *out!=NULL;}
static const char* body="import shared\nfunc main(...) as int\n return shared.amount(1)\nend\n";
static const char* project="{\"format\":\"xlang-project-1\",\"entry\":\"main.xl\",\"ide\":{\"name\":\"p\",\"files\":[{\"id\":\"s\",\"path\":\"shared.xl\"},{\"id\":\"c\",\"path\":\"closed.xl\"}],\"configurations\":[{\"id\":\"debug\",\"arguments\":[\"runtime-only\"]}],\"activeConfiguration\":\"debug\"}}";
static xide_analysis_catalog* discover(const char* base)
{
 xide_model_error error={0};xvalue* init=xrtValueObject(),*folders=xrtValueArray(),*row=xrtValueObject();char* uri=xideLspPathToUri(base,NULL);CHECK(init&&folders&&row&&uri);
 CHECK(xrtValueObjectSetNew(row,xrtStrView("uri"),xrtValueString(xrtStrView(uri)))&&xrtValueObjectSetNew(row,xrtStrView("name"),xrtValueString(xrtStrView("proof")))&&xrtValueArrayAppendNew(folders,row)&&xrtValueObjectSetNew(init,xrtStrView("workspaceFolders"),folders));free(uri);
 xide_workspace_folders* roots=xideWorkspaceFoldersRead(init,&error);xrtValueRelease(init);CHECK(roots);xide_workspace_discovery* store=xideWorkspaceDiscoveryCreate(NULL,&error);CHECK(store);xide_discovery_query* q=xideWorkspaceDiscoverySubmit(store,roots,NULL);CHECK(q);xideWorkspaceFoldersRelease(roots);
 ULONGLONG until=GetTickCount64()+30000;int state;while((state=xideWorkspaceDiscoveryResult(q,NULL,NULL,NULL,0))==XIDE_DISCOVERY_PENDING){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(state==XIDE_DISCOVERY_READY);xide_discovery_snapshot* snapshot=xideWorkspaceDiscoveryTake(q);CHECK(snapshot);
 xide_analysis_catalog* catalog=xideAnalysisCatalogRetain(xideDiscoverySnapshotCatalog(snapshot));xide_analysis_catalog* plain=xideAnalysisCatalogCreate(xideDiscoverySnapshotValue(snapshot),NULL,0);CHECK(catalog&&plain&&xideAnalysisCatalogBytes(catalog)>xideAnalysisCatalogBytes(plain));xideAnalysisCatalogDestroy(plain);
 xideWorkspaceDiscoveryQueryDestroy(q);xideDiscoverySnapshotRelease(snapshot);while(!xideWorkspaceDiscoveryIdle(store)){CHECK(GetTickCount64()<until);Sleep(1);}xideWorkspaceDiscoveryDestroy(store);return catalog;
}
static void runCase(const char* data,int mode)
{
 char leaf[32];snprintf(leaf,sizeof(leaf),"case-%d",mode);char* base=join(data,leaf);CHECK(xideIoCreateDirectories(base));char *main=join(base,"main.xl"),*closed=join(base,"closed.xl"),*shared=join(base,"shared.xl"),*loose=join(base,"loose.xl"),*manifest=join(base,"p.xlp"),*late=join(base,"late.xl"),*compiler=join(base,"missing-xl.exe");
 writeFile(main,body);writeFile(closed,body);writeFile(loose,body);writeFile(shared,"export func amount(int value) as int\n return value\nend\n");writeFile(manifest,project);
 xide_analysis_catalog* catalog=discover(base);xide_toolchains* cache=xideToolchainsCreate();xls_module_store* modules=xlsModuleStoreCreate(NULL);xide_project_references* references=xideProjectReferencesCreate(cache,compiler,modules);xide_rename_candidates* renames=xideRenameCandidatesCreate(references);CHECK(cache&&modules&&references&&renames);
 char* uri=xideLspPathToUri(main,NULL);xvalue *params=json("{\"position\":{\"line\":2,\"character\":15},\"newName\":\"updated\"}"),*doc=xrtValueObject(),*id=xrtValueString(xrtStrView("proof-publish"));CHECK(uri&&doc&&id&&xrtValueObjectSetNew(doc,xrtStrView("uri"),xrtValueString(xrtStrView(uri)))&&xrtValueObjectSetNew(params,xrtStrView("textDocument"),doc));xls_text* text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);CHECK(text);xls_analysis_document input={uri,main,text,1,(uint64_t)((int64_t)1-INT_MIN)};
 mutation=mode;latePath=late;manifestPath=manifest;manifestBody=project;InterlockedExchange(&frameCalls,0);ResetEvent(framedEvent);ResetEvent(resumeEvent);
 if(mode==6)writeFile(late,"func main(...) as int\n return 0\nend\n");
 if(mode==7)writeFile(manifest,project);
 xide_rename_candidate_query* q=mode==4?xideRenamePrepareSubmit(renames,id,params,catalog,&input,1):xideRenameStandardSubmit(renames,id,1,params,catalog,&input,1);CHECK(q);xideAnalysisCatalogDestroy(catalog);xrtValueRelease(params);xrtValueRelease(id);xlsTextRelease(text);free(uri);
 if(mode==5||mode==8){CHECK(WaitForSingleObject(framedEvent,30000)==WAIT_OBJECT_0);xideRenameCandidateCancel(q);if(mode==8){int pendingCode=0;CHECK(!xideRenameCandidatesIdle(renames)&&InterlockedCompareExchange(&liveGuardFiles,0,0)==1);CHECK(xideRenameCandidateResult(q,&pendingCode,NULL,0)==0);}CHECK(SetEvent(resumeEvent));}
 int state,code=0;char error[512];ULONGLONG until=GetTickCount64()+30000;while((state=xideRenameCandidateResult(q,&code,error,sizeof(error)))==0){CHECK(GetTickCount64()<until);Sleep(1);}size_t size=0;char* frame=xideRenameCandidateTakeFrame(q,&size);
 if(!mode){CHECK(state==1&&code==0&&frame&&size);xvalue* reply=NULL;xide_lsp_stream stream={0};CHECK(xideLspStreamFeed(&stream,frame,size,receive,&reply,NULL)&&reply);const xvalue* changes=xrtValueObjectGet(xrtValueObjectGet(reply,xrtStrView("result")),xrtStrView("documentChanges"));CHECK(xrtValueType(changes)==XVALUE_ARRAY&&xrtValueCount(changes)==4);xrtValueRelease(reply);xideLspStreamClear(&stream);}
 else{CHECK(!frame&&!size);CHECK((mode==5||mode==8)?(state==-1&&code==-32800):(state==-1&&code==-32801));}
 CHECK(InterlockedCompareExchange(&frameCalls,0,0)==((mode==6||mode==7)?0:1));
 while(!xideRenameCandidatesIdle(renames)){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(!InterlockedCompareExchange(&liveGuardFiles,0,0)&&!InterlockedCompareExchangePointer(&heldFile,NULL,NULL));
 free(frame);xideRenameCandidateDestroy(q);xideRenameCandidatesDestroy(renames);xideProjectReferencesDestroy(references);xlsModuleStoreRelease(modules);xideToolchainsDestroy(cache);free(main);free(closed);free(shared);free(loose);free(manifest);free(late);free(compiler);free(base);
}
int main(int argc,char** argv)
{
 CHECK(argc==2);owner=GetCurrentThreadId();framedEvent=CreateEventW(NULL,TRUE,FALSE,NULL);resumeEvent=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(framedEvent&&resumeEvent);char leaf[64];snprintf(leaf,sizeof(leaf),"publish-%lu-%llu",(unsigned long)GetCurrentProcessId(),GetTickCount64());char* data=join(argv[1],leaf);CHECK(xideIoCreateDirectories(data));for(int mode=0;mode<9;++mode)runCase(data,mode);free(data);CloseHandle(framedEvent);CloseHandle(resumeEvent);printf("workspace_discovery_publish_test passed:%u checks;snapshot/store-independent catalog evidence;pre-analysis/post-frame guards;cancel with a physical manifest reader.\n",(unsigned)checks);return 0;
}
