#include "workspace_edit_spool_fixture.h"
static int atomicMode,atomicCalls;static size_t flushCalls,flushFail,writeCalls,writeFail,verifiedBodies;static const char* activeScope;
#ifdef XLS_COMPILER_XRT
bool __real_xrtFileWriteAtomic(cstr,xbytesview);
#define REAL_ATOMIC __real_xrtFileWriteAtomic
#else
extern bool (*__real___imp_xrtFileWriteAtomic)(cstr,xbytesview);
#define REAL_ATOMIC __real___imp_xrtFileWriteAtomic
#endif
extern BOOL (WINAPI* __real___imp_FlushFileBuffers)(HANDLE);
extern BOOL (WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static void bodiesReady(xbytesview bytes)
{
 xvalue* root=xrtJsonParse((xstrview){(const char*)bytes.Data,bytes.Size});CHECK(root);const xvalue* bodies=field(root,"bodies");CHECK(xrtValueCount(bodies)==10&&flushCalls>=xrtValueCount(bodies));
 for(size_t i=0;i<xrtValueCount(bodies);++i){const xvalue* b=xrtValueArrayGet(bodies,i);char* path=xidePathResolveLexical(activeScope,asText(field(b,"ref")));CHECK(path);xide_model_error e={0};xide_file_snapshot s={0};char digest[65];size_t size=0;CHECK(xideFileSnapshotInspect(path,&s,&e)&&xrtHexEncode(s.sha256,32,digest,sizeof(digest),&size,0)&&size==64&&!strcmp(digest,asText(field(b,"sha256"))));CHECK(s.length==strtoull(asText(field(b,"length")),NULL,10));free(path);++verifiedBodies;}
 xrtValueRelease(root);
}
bool __wrap_xrtFileWriteAtomic(cstr path,xbytesview bytes)
{
 if(atomicMode&&strstr(path,"edit-journal.json")){++atomicCalls;bodiesReady(bytes);if(atomicMode==1)return false;bool okay=REAL_ATOMIC(path,bytes);return atomicMode==2&&okay?false:okay;}
 return REAL_ATOMIC(path,bytes);
}
#ifndef XLS_COMPILER_XRT
bool (*__wrap___imp_xrtFileWriteAtomic)(cstr,xbytesview)=__wrap_xrtFileWriteAtomic;
#endif
static BOOL WINAPI probeFlush(HANDLE h){++flushCalls;if(flushCalls==flushFail){SetLastError(ERROR_DISK_FULL);return FALSE;}return __real___imp_FlushFileBuffers(h);}
static BOOL WINAPI probeWrite(HANDLE h,LPCVOID bytes,DWORD size,LPDWORD wrote,LPOVERLAPPED ov){++writeCalls;if(writeCalls==writeFail){*wrote=0;SetLastError(ERROR_DISK_FULL);return FALSE;}return __real___imp_WriteFile(h,bytes,size,wrote,ov);}
BOOL (WINAPI* __wrap___imp_FlushFileBuffers)(HANDLE)=probeFlush;
BOOL (WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=probeWrite;
static char* casePath(const char* root,const char* name){char* p=xidePathResolveLexical(root,name);CHECK(p);return p;}
static size_t bodyFiles(const char* scope)
{
 char* path=casePath(scope,"recovery/*");wchar_t* native=xidePathNativeWide(path);free(path);CHECK(native);WIN32_FIND_DATAW item={0};HANDLE find=FindFirstFileW(native,&item);free(native);if(find==INVALID_HANDLE_VALUE){CHECK(GetLastError()==ERROR_FILE_NOT_FOUND);return 0;}size_t count=0;do{if(wcscmp(item.cFileName,L".")&&wcscmp(item.cFileName,L".."))++count;}while(FindNextFileW(find,&item));CHECK(GetLastError()==ERROR_NO_MORE_FILES&&FindClose(find));return count;
}
static int hasMarker(const char* scope){char* p=casePath(scope,"edit-journal.json");int result=xideIoExists(p);free(p);return result;}
static void sourceUnchanged(data* d){for(unsigned i=0;i<3;++i)diskEquals(d->paths[i],savedRaw[i]);}
static void prepareModes(const char* root)
{
 for(int mode=1;mode<=3;++mode){char name[32];snprintf(name,sizeof(name),"atomic-%d",mode);char* path=casePath(root,name);data d;init(&d,path);char* scope=casePath(path,"scope");activeScope=scope;atomicCalls=0;verifiedBodies=flushCalls=writeCalls=0;atomicMode=mode;xide_model_error e={0};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,NULL,NULL,&e);atomicMode=0;CHECK(atomicCalls==1&&verifiedBodies==10);if(mode<3){CHECK(!p&&e.code&&bodyFiles(scope)==10);if(mode==1)CHECK(!hasMarker(scope));else{CHECK(hasMarker(scope));p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(p);inspect(p,0);}}else{CHECK(p&&hasMarker(scope));inspect(p,1);}xideEditSpoolDestroy(p);sourceUnchanged(&d);freeData(&d);free(scope);free(path);activeScope=NULL;xrtClearError();}
 for(int mode=0;mode<2;++mode){char name[32];snprintf(name,sizeof(name),"io-%d",mode);char* path=casePath(root,name);data d;init(&d,path);char* scope=casePath(path,"scope");flushCalls=writeCalls=0;if(mode)flushFail=3;else writeFail=3;xide_model_error e={0};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,NULL,NULL,&e);writeFail=flushFail=0;CHECK(!p&&e.code==XIDE_MODEL_IO&&!hasMarker(scope)&&bodyFiles(scope)==0);sourceUnchanged(&d);freeData(&d);free(scope);free(path);xrtClearError();}
}
typedef struct cancellation {size_t calls,at;} cancellation;
static int cancelAt(void* user){cancellation* c=user;return ++c->calls==c->at;}
static void cancellationCases(const char* root)
{
 char* path=casePath(root,"cancel-source");data d;init(&d,path);char* scope=casePath(path,"observe");xide_model_error e={0};cancellation observed={0,SIZE_MAX};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,cancelAt,&observed,&e);CHECK(p&&observed.calls>0);xideEditSpoolDestroy(p);free(scope);
 for(size_t i=1;i<=observed.calls;++i){char name[64];snprintf(name,sizeof(name),"cancel-%zu",i);scope=casePath(path,name);cancellation c={0,i};p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,cancelAt,&c,&e);CHECK(!p&&e.code==XIDE_MODEL_CONFLICT&&c.calls==i&&!hasMarker(scope)&&bodyFiles(scope)==0);free(scope);xrtClearError();}
 sourceUnchanged(&d);freeData(&d);free(path);printf("spool cancellation: %zu prepare checkpoints\n",observed.calls);
}
static void writeRecord(const char* path,const xvalue* root)
{size_t size=0;char* json=xrtJsonStringify(root,false,&size);CHECK(json&&xideIoWriteAtomic(path,(xbytesview){(unsigned char*)json,size}));xrtFree(json);}
static void damagedRecords(const char* root)
{
 char* path=casePath(root,"damaged");data d;init(&d,path);char* scope=casePath(path,"scope");xide_model_error e={0};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,NULL,NULL,&e);CHECK(p);xvalue* original=xrtValueDeepClone(xideEditSpoolManifest(p));CHECK(original);xideEditSpoolDestroy(p);char* marker=casePath(scope,"edit-journal.json");
 for(unsigned mode=0;mode<15;++mode){xvalue* changed=xrtValueDeepClone(original);CHECK(changed);xvalue *docs=field(changed,"documents"),*first=xrtValueArrayGet(docs,0),*second=xrtValueArrayGet(docs,1),*bodies=field(changed,"bodies"),*body=xrtValueArrayGet(bodies,0);
  if(mode==0)CHECK(putEdit(changed,"format",xrtValueString(xrtStrView("future-format"))));
  if(mode==1)CHECK(putEdit(changed,"state",xrtValueString(xrtStrView("committed"))));
  if(mode==2)CHECK(putEdit(changed,"approved",xrtValueBool(true)));
  if(mode==3)CHECK(putEdit(first,"encoded",xrtValueInt(asIndex(field(first,"before")))));
  if(mode==4)CHECK(putEdit(second,"baseline",xrtValueInt(asIndex(field(second,"after")))));
  if(mode==5)CHECK(putEdit(second,"version",xrtValueNull()));
  if(mode==6)CHECK(putEdit(field(first,"disk"),"sha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));
  if(mode==7)CHECK(putEdit(body,"ref",xrtValueString(xrtStrView("recovery/../outside.body"))));
  if(mode==8)CHECK(putEdit(first,"path",xrtValueString(xrtStrView(d.paths[1]))));
  if(mode==9)CHECK(putEdit(first,"changed",xrtValueBool(false)));
  if(mode==10)CHECK(putEdit(xrtValueArrayGet(field(changed,"annotations"),0),"needsConfirmation",xrtValueInt(1)));
  if(mode==11)CHECK(putEdit(body,"length",xrtValueString(xrtStrView("-1"))));
  if(mode==12)CHECK(putEdit(first,"eol",xrtValueInt(XIDE_TEXT_EOL_CR)));
  if(mode==13)CHECK(putEdit(first,"annotations",xrtValueArray()));
  if(mode==14)CHECK(putEdit(changed,"needsConfirmation",xrtValueBool(false)));
  /* Remove the second association too so missing annotation ownership cannot
   * hide behind a different correctly associated target. */
  if(mode==13)CHECK(putEdit(second,"annotations",xrtValueArray()));
  writeRecord(marker,changed);xrtValueRelease(changed);p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(!p&&e.code&&bodyFiles(scope)==10);sourceUnchanged(&d);writeRecord(marker,original);p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(p);xideEditSpoolDestroy(p);xrtClearError();
 }
 const xvalue* b=xrtValueArrayGet(field(original,"bodies"),(size_t)asIndex(field(xrtValueArrayGet(field(original,"documents"),0),"baseline")));char* body=casePath(scope,asText(field(b,"ref")));char* hold=casePath(scope,"held.body");wchar_t *wb=xidePathNativeWide(body),*wh=xidePathNativeWide(hold);CHECK(wb&&wh&&MoveFileW(wb,wh));CHECK(!xideEditSpoolOpen(scope,NULL,NULL,&e)&&e.code&&MoveFileW(wh,wb));
 CHECK(writeSource(body,"xxxxxxxxx"));CHECK(!xideEditSpoolOpen(scope,NULL,NULL,&e)&&e.code&&writeSource(body,savedRaw[0]));p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(p);xideEditSpoolDestroy(p);
 CHECK(CreateHardLinkW(wh,wb,NULL));CHECK(!xideEditSpoolOpen(scope,NULL,NULL,&e)&&e.code&&DeleteFileW(wh));free(wb);free(wh);free(body);free(hold);
 char *recovery=casePath(scope,"recovery"),*moved=casePath(scope,"recovery-held");wchar_t *wr=xidePathNativeWide(recovery),*wm=xidePathNativeWide(moved);CHECK(wr&&wm&&MoveFileW(wr,wm)&&junction(recovery,path));CHECK(!xideEditSpoolOpen(scope,NULL,NULL,&e)&&e.code&&RemoveDirectoryW(wr)&&MoveFileW(wm,wr));free(wr);free(wm);free(recovery);free(moved);p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(p);inspect(p,0);xideEditSpoolDestroy(p);xrtValueRelease(original);free(marker);sourceUnchanged(&d);freeData(&d);free(scope);free(path);
}
static void crashCase(const char* root)
{
 char* directory=casePath(root,"crash-child");wchar_t program[32768];CHECK(GetModuleFileNameW(NULL,program,32768));wchar_t* argument=xidePathNativeWide(directory);CHECK(argument);wchar_t command[65536];CHECK(swprintf(command,65536,L"\"%ls\" \"%ls\" --crash",program,argument)>0);free(argument);STARTUPINFOW startup={.cb=sizeof(startup)};PROCESS_INFORMATION process={0};CHECK(CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process));CHECK(WaitForSingleObject(process.hProcess,30000)==WAIT_OBJECT_0);DWORD code=0;CHECK(GetExitCodeProcess(process.hProcess,&code)&&code==83&&CloseHandle(process.hThread)&&CloseHandle(process.hProcess));char* scope=casePath(directory,"scope");xide_model_error e={0};xide_edit_spool* p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(p);inspect(p,0);xideEditSpoolDestroy(p);free(scope);free(directory);
}
static void invalidInputs(const char* root)
{
 char* path=casePath(root,"invalid");data d;init(&d,path);char* scope=casePath(path,"scope");xide_model_error e={0};CHECK(!xideEditSpoolPrepare(d.plan,&d.view,1,"",NULL,0,NULL,NULL,&e)&&e.code==XIDE_MODEL_INVALID);CHECK(!xideEditSpoolOpen("",NULL,NULL,&e)&&e.code==XIDE_MODEL_INVALID);CHECK(!xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,1,NULL,NULL,&e)&&e.code==XIDE_MODEL_MEMORY&&!xideIoExists(scope));CHECK(!xideEditSpoolPrepare(d.plan,NULL,0,scope,NULL,0,NULL,NULL,&e)&&e.code==XIDE_MODEL_INVALID&&!hasMarker(scope));sourceUnchanged(&d);freeData(&d);free(scope);free(path);
}
int main(int argc,char** argv)
{
 CHECK(argc==2||(argc==3&&!strcmp(argv[2],"--crash")));CHECK(xrtDirCreateAll(argv[1]));warmServices(argv[1]);
 if(argc==3){data d;init(&d,argv[1]);char* scope=casePath(argv[1],"scope");xide_model_error e={0};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,NULL,NULL,&e);CHECK(p);inspect(p,1);ExitProcess(83);}
 prepareModes(argv[1]);cancellationCases(argv[1]);damagedRecords(argv[1]);crashCase(argv[1]);invalidInputs(argv[1]);printf("workspace_edit_spool_guard_test passed: %u checks; body-before-marker/flush ordering, uncertain publication, I/O failure, cancellation, damaged records/payloads, aliases/reparse, independent-process exit and invalid inputs\n",checks);return 0;
}
