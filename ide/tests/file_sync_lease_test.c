/* Compile the exact producer, exposing its private monitor only to place a
 * deterministic blocked-read boundary. No alternate synchronization logic. */
#include "../src/services/file_sync.c"
#include "theme/theme.h"
static volatile LONG checks;
#define CHECK(v) do{InterlockedIncrement(&checks);if(!(v)){fprintf(stderr,"sync-lease:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static HANDLE arrived,releaseRead;static volatile LONG blocked,failMalloc;
static DWORD volume,indexLow,indexHigh;
extern BOOL (WINAPI* __real___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
void* __real_malloc(size_t);
void* __wrap_malloc(size_t n){return InterlockedCompareExchange(&failMalloc,0,1)==1?NULL:__real_malloc(n);}
static BOOL WINAPI readProbe(HANDLE h,LPVOID b,DWORD n,LPDWORD got,LPOVERLAPPED at)
{
 BY_HANDLE_FILE_INFORMATION i={0};if(n&&InterlockedCompareExchange(&blocked,0,0)&&GetFileInformationByHandle(h,&i)&&i.dwVolumeSerialNumber==volume&&i.nFileIndexLow==indexLow&&i.nFileIndexHigh==indexHigh&&InterlockedCompareExchange(&blocked,0,1)==1){CHECK(SetEvent(arrived)&&WaitForSingleObject(releaseRead,30000)==WAIT_OBJECT_0);}return __real___imp_ReadFile(h,b,n,got,at);
}
BOOL (WINAPI* __wrap___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=readProbe;
static void writeText(const char* path,const char* text){CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}));}
static void waitIdle(xide_app* app)
{uint64_t end=GetTickCount64()+12000;do{xideFileSyncPoll(app);if(xideFileSyncSettled(app))return;Sleep(5);}while(GetTickCount64()<end);CHECK(0);}
static void waitHold(xide_app* app,uint64_t token)
{uint64_t end=GetTickCount64()+12000;do{xideFileSyncPoll(app);if(xideFileSyncSuspended(app,token))return;Sleep(5);}while(GetTickCount64()<end);CHECK(0);}
int main(int argc,char** argv)
{
 CHECK(argc==2&&xrtDirCreateAll(argv[1]));xide_app app={.hidden=1,.noSession=1,.disableAutomaticDiagnostics=1,.viewportWidth=1280,.viewportHeight=800};xide_model_error e={0};
 char* a=xidePathResolveLexical(argv[1],"main.c");char* b=xidePathResolveLexical(argv[1],"helper.xl");CHECK(a&&b);writeText(a,"int before;\n");writeText(b,"func before()\nend\n");
 CHECK(xuiCreate(&app.context)==XUI_OK&&ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK&&xuiInputViewport(app.context,1280,800)==XUI_OK&&xideWorkbenchCreate(&app)==XUI_OK);CHECK(xideEditorOpen(&app,a,&e));xide_document* c=app.activeDocument;CHECK(xideEditorOpen(&app,b,&e));xide_document* xl=app.activeDocument;
 CHECK(xideFileSyncStart(&app,&e));waitIdle(&app);CHECK(!xideFileSyncCount(&app));
 CHECK(xideFileChangesViewOpen(&app));writeText(a,"int stale_cached;\n");uint64_t end=GetTickCount64()+12000;do{xideFileSyncPoll(&app);if(xideFileSyncCount(&app))break;Sleep(5);}while(GetTickCount64()<end);CHECK(xideFileSyncCount(&app)==1);const xide_file_change* old=xideFileSyncAt(&app,0);CHECK(old->snapshot.bytes&&!strcmp((const char*)old->snapshot.bytes,"int stale_cached;\n"));uint64_t oldToken=old->snapshot.token,sequence=old->snapshot.sequence;
 CHECK(xideDialogSetOpen(xideFileChangesViewWindow(&app),0)==XUI_OK);uint64_t token=xideFileSyncSuspend(&app,&e);CHECK(token&&!xideFileSyncSettled(&app));waitHold(&app,token);CHECK(!xideFileSyncSuspend(&app,&e)&&!xideFileSyncRebind(&app,&e)&&!xideFileSyncResume(&app,token+1,&e));CHECK(!xideFileSyncKeep(&app,oldToken,sequence)&&!xideFileSyncReload(&app,oldToken,sequence,xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),1,&e));
 /* Ordinary editing/undo remains responsive; the before-save path blocks disk
  * writes even for nonmanifest C files with explicit overwrite permission. */
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"// user\n")==XUI_OK&&xl->dirty);CHECK(!xideDocumentSave(xl,NULL,XIDE_DOCUMENT_OVERWRITE_EXTERNAL,&e)&&e.code==XIDE_MODEL_CONFLICT);CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK&&!xl->dirty);
 writeText(a,"int committed;\n");xideFileSyncRefresh(&app);xideFileSyncPoll(&app);CHECK(!strcmp(xuiCodeEditGetText(c->editor),"int before;\n"));
 InterlockedExchange(&failMalloc,1);CHECK(!xideFileSyncResume(&app,token,&e)&&!InterlockedCompareExchange(&failMalloc,0,0)&&xideFileSyncSuspended(&app,token));
 wchar_t* path=xidePathNativeWide(a);HANDLE h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);free(path);BY_HANDLE_FILE_INFORMATION i;CHECK(h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(h,&i)&&CloseHandle(h));volume=i.dwVolumeSerialNumber;indexLow=i.nFileIndexLow;indexHigh=i.nFileIndexHigh;
 arrived=CreateEventW(NULL,TRUE,FALSE,NULL);releaseRead=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(arrived&&releaseRead);InterlockedExchange(&blocked,1);CHECK(xideFileSyncResume(&app,token,&e));CHECK(WaitForSingleObject(arrived,12000)==WAIT_OBJECT_0);
 /* Fresh worker data is definitely unavailable here. A stale cached change
  * must not gain the new epoch through resubscription or replay on this poll. */
 old=xideFileSyncAt(&app,0);CHECK(old&&old->snapshot.epoch==0);CHECK(!xideFileSyncReload(&app,oldToken,sequence,xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),1,&e));CHECK(!xideFileSyncKeep(&app,oldToken,sequence));xideFileSyncPoll(&app);CHECK(!strcmp(xuiCodeEditGetText(c->editor),"int before;\n")&&!c->dirty);
 /* A normal rebind before the fresh read completes cannot revive epoch0. */
 char* third=xidePathResolveLexical(argv[1],"new-open.c");CHECK(third);writeText(third,"int another;\n");CHECK(xideEditorOpen(&app,third,&e)&&xideFileSyncRebind(&app,&e));free(third);old=xideFileSyncAt(&app,0);CHECK(old&&old->snapshot.epoch==0);xideFileSyncPoll(&app);CHECK(!strcmp(xuiCodeEditGetText(c->editor),"int before;\n"));
 CHECK(SetEvent(releaseRead));waitIdle(&app);CHECK(!strcmp(xuiCodeEditGetText(c->editor),"int committed;\n")&&!c->dirty&&!xideFileSyncCount(&app));CHECK(!xideFileSyncResume(&app,token,&e));
 /* Compose the real document admission participant with this monitor. An old
  * subscription/cache cannot acquire the new document epoch during resume. */
 CHECK(xideFileChangesViewOpen(&app));writeText(a,"int previous_epoch;\n");end=GetTickCount64()+12000;do{xideFileSyncPoll(&app);if(xideFileSyncCount(&app))break;Sleep(5);}while(GetTickCount64()<end);CHECK(xideFileSyncCount(&app)==1);old=xideFileSyncAt(&app,0);oldToken=old->snapshot.token;sequence=old->snapshot.sequence;uint64_t oldDocumentEpoch=xideDocumentsSourceEpoch(&app.documents);
 CHECK(xideDialogSetOpen(xideFileChangesViewWindow(&app),0)==XUI_OK);uint64_t documentToken=xideDocumentsSuspendSources(&app.documents,&e);token=xideFileSyncSuspend(&app,&e);CHECK(documentToken && token && xideDocumentsSourcesSuspended(&app.documents,documentToken));waitHold(&app,token);
 CHECK(!xideFileSyncSettled(&app) && !xideDocumentOpen(&app.documents,a,&e) && !xideDocumentReload(c,1,&e));writeText(a,"int current_epoch;\n");path=xidePathNativeWide(a);h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);free(path);CHECK(h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&i) && CloseHandle(h));volume=i.dwVolumeSerialNumber;indexLow=i.nFileIndexLow;indexHigh=i.nFileIndexHigh;CHECK(ResetEvent(arrived) && ResetEvent(releaseRead));InterlockedExchange(&blocked,1);CHECK(xideFileSyncResume(&app,token,&e));CHECK(WaitForSingleObject(arrived,12000)==WAIT_OBJECT_0);
 CHECK(app.fileSync->documentEpoch==documentToken && !xideFileSyncCount(&app));xideFileSyncPoll(&app);CHECK(!strcmp(xuiCodeEditGetText(c->editor),"int committed;\n"));CHECK(xideDocumentsResumeSources(&app.documents,documentToken,&e) && !xideFileSyncReload(&app,oldToken,sequence,xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),1,&e));
 xide_text_file oldSnapshot={0};CHECK(xideTextFileDecode((const unsigned char*)"int previous_epoch;\n",20,&oldSnapshot,&e));unsigned char* oldRaw=oldSnapshot.raw;CHECK(!xideDocumentAcceptExternal(c,&oldSnapshot,1,oldDocumentEpoch,&e) && oldSnapshot.raw==oldRaw);xideTextFileClear(&oldSnapshot);CHECK(SetEvent(releaseRead));waitIdle(&app);CHECK(!strcmp(xuiCodeEditGetText(c->editor),"int current_epoch;\n") && !c->dirty);
 token=xideFileSyncSuspend(&app,&e);CHECK(token);waitHold(&app,token);xideFileSyncStop(&app);CHECK(!xideFileSyncSuspended(&app,token));CHECK(CloseHandle(arrived)&&CloseHandle(releaseRead));xideAppDestroy(&app);free(a);free(b);
 printf("file_sync_lease_test passed: %ld checks; actual IOCP monitor and C/xlang models, cached-event epoch fence, failed resume retention, ordinary input/undo and before-save gate, exact tokens and held shutdown\n",checks);return 0;
}
