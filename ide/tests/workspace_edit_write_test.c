#include "workspace_edit_spool_fixture.h"
#include "platform/workspace_edit_write.h"
#include "platform/state_file.h"
static xide_file_snapshot activeSource;static int active,writeMode,eofMode,flushMode,timeMode;static unsigned writeCalls,eofCalls,flushCalls,timeCalls,failAt,stopCall,stopPoint;
extern BOOL (WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
extern BOOL (WINAPI* __real___imp_FlushFileBuffers)(HANDLE);
extern BOOL (WINAPI* __real___imp_SetEndOfFile)(HANDLE);
extern BOOL (WINAPI* __real___imp_SetFileTime)(HANDLE,const FILETIME*,const FILETIME*,const FILETIME*);
static int isSource(HANDLE h){BY_HANDLE_FILE_INFORMATION i={0};return active&&GetFileInformationByHandle(h,&i)&&i.dwVolumeSerialNumber==activeSource.volume&&(((uint64_t)i.nFileIndexHigh<<32)|i.nFileIndexLow)==activeSource.index;}
static BOOL WINAPI probeWrite(HANDLE h,LPCVOID p,DWORD size,LPDWORD wrote,LPOVERLAPPED ov)
{
 if(isSource(h)&&++writeCalls==failAt&&writeMode){int mode=writeMode;writeMode=0;if(mode==1){*wrote=0;SetLastError(ERROR_DISK_FULL);return FALSE;}if(mode==2||mode==3){DWORD actual=0;CHECK(__real___imp_WriteFile(h,p,size/2,&actual,ov)&&actual==size/2);*wrote=mode==2?0:actual;SetLastError(ERROR_DISK_FULL);return mode==3;}if(mode==4||mode==5){unsigned char copy[65536];CHECK(size<=sizeof(copy));memcpy(copy,p,size);if(size)copy[0]^=1;BOOL okay=__real___imp_WriteFile(h,copy,size,wrote,ov);if(mode==5){CHECK(okay);*wrote=0;SetLastError(ERROR_DISK_FULL);return FALSE;}return okay;}}
 return __real___imp_WriteFile(h,p,size,wrote,ov);
}
static BOOL WINAPI probeFlush(HANDLE h){if(isSource(h)&&++flushCalls==failAt&&flushMode){flushMode=0;SetLastError(ERROR_DISK_FULL);return FALSE;}return __real___imp_FlushFileBuffers(h);}
static BOOL WINAPI probeEof(HANDLE h){if(isSource(h)&&++eofCalls==failAt&&eofMode){int mode=eofMode;eofMode=0;if(mode==2)CHECK(__real___imp_SetEndOfFile(h));SetLastError(ERROR_DISK_FULL);return FALSE;}return __real___imp_SetEndOfFile(h);}
static BOOL WINAPI probeTime(HANDLE h,const FILETIME* a,const FILETIME* b,const FILETIME* c){if(isSource(h)&&++timeCalls==failAt&&timeMode){int mode=timeMode;timeMode=0;if(mode==2)CHECK(__real___imp_SetFileTime(h,a,b,c));SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return __real___imp_SetFileTime(h,a,b,c);}
BOOL (WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=probeWrite;
BOOL (WINAPI* __wrap___imp_FlushFileBuffers)(HANDLE)=probeFlush;
BOOL (WINAPI* __wrap___imp_SetEndOfFile)(HANDLE)=probeEof;
BOOL (WINAPI* __wrap___imp_SetFileTime)(HANDLE,const FILETIME*,const FILETIME*,const FILETIME*)=probeTime;
static void reset(void){active=writeMode=eofMode=flushMode=timeMode=0;writeCalls=eofCalls=flushCalls=timeCalls=0;failAt=1;stopCall=stopPoint=0;}
static int stopWrite(void* unused){(void)unused;return ++stopCall==stopPoint;}
static void warmJournal(const char* directory)
{
 /* Independent control without Spool/Write observed13 lazy startup handles
  * on replacing an existing state file, then stable subsequent replacements.
  * Initialize the same real service before component resource accounting. */
 char* path=xidePathResolveLexical(directory,"journal-warm.json");xide_state_file state={0};xide_model_error e={0};CHECK(path&&xideStateOpen(&state,path,"journal-warm",&e));xvalue* root=parseEdit("{\"format\":\"journal-warm\",\"value\":1}");CHECK(root&&xideStateWrite(&state,root,&e)&&xideStateWrite(&state,root,&e));xrtValueRelease(root);xideStateClear(&state);free(path);
}
typedef struct journal {xide_state_file file;const char* path;const char* id;int deny,change;unsigned calls;} journal;
static int intent(void* user,int action)
{
 journal* j=user;++j->calls;if(j->deny==action)return 0;xvalue* root=xrtValueObject();CHECK(root&&putEdit(root,"format",xrtValueString(xrtStrView("xlang-write-test-intent-1")))&&putEdit(root,"package",xrtValueString(xrtStrView(j->id)))&&putEdit(root,"action",xrtValueInt(action)));xide_model_error e={0};CHECK(xideStateWrite(&j->file,root,&e));xrtValueRelease(root);
 if(j->change){wchar_t* path=xidePathNativeWide(j->path);HANDLE h=CreateFileW(path,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);free(path);FILETIME t={123,456};CHECK(h!=INVALID_HANDLE_VALUE&&__real___imp_SetFileTime(h,NULL,NULL,&t)&&CloseHandle(h));}return 1;
}
typedef struct sample {char *source,*scope,*intentPath; xide_edit_spool* spool;xide_edit_file_write* write;journal journal;xide_file_snapshot original;const char* raw;char* expected;} sample;
static void clearSample(sample* s){active=0;xideEditFileWriteDestroy(s->write);xideEditSpoolDestroy(s->spool);xideStateClear(&s->journal.file);free(s->source);free(s->scope);free(s->intentPath);free(s->expected);}
static void bytesEqual(const char* path,const char* body)
{size_t n=0;unsigned char* p=xideIoReadAllLimit(path,16u*1024u*1024u,&n);CHECK(p&&n==strlen(body)&&!memcmp(p,body,n));xrtFree(p);}
static void makeSample(sample* s,const char* directory,const char* raw,const char* replacement)
{
 memset(s,0,sizeof(*s));reset();CHECK(xrtDirCreateAll(directory));s->raw=raw;s->source=xidePathResolveLexical(directory,"write-source.xl");s->scope=xidePathResolveLexical(directory,"scope");s->intentPath=xidePathResolveLexical(s->scope,"file-intent.json");CHECK(s->source&&s->scope&&s->intentPath&&writeSource(s->source,raw));char* uri=xideLspPathToUri(s->source,NULL);xide_text_file decoded={0},encodedText={0};xide_model_error e={0};CHECK(xideTextFileDecode((const unsigned char*)raw,strlen(raw),&decoded,&e));int line=0,column=0,ascii=1;for(const char* p=decoded.text;*p;++p){if(*p=='\n'){++line;column=0;}else{ascii&=(unsigned char)*p<128;++column;}}CHECK(ascii);
 xvalue *root=xrtValueObject(),*docs=xrtValueArray(),*edits=xrtValueArray(),*edit=textEdit(0,0,0,replacement);CHECK(uri&&root&&docs&&edits&&edit&&putEdit(field(edit,"range"),"end",editPosition(line,column))&&xrtValueArrayAppendNew(edits,edit));xvalue* row=documentEdit(uri,0,0,edits);CHECK(row&&diskProof(row,s->source,raw,decoded.text)&&xrtValueArrayAppendNew(docs,row)&&putEdit(root,"documentChanges",docs));xide_workspace_edit_input input={.uri=uri,.text=xlsTextCreate(decoded.text,strlen(decoded.text),8u*1024u*1024u,NULL),.revision=1};enum xide_workspace_edit_error error=0;xide_workspace_edit* plan=xideWorkspaceEditPrepare(root,&input,1,0,0,NULL,NULL,&error);CHECK(plan&&!error);s->spool=xideEditSpoolPrepare(plan,NULL,0,s->scope,NULL,0,NULL,NULL,&e);CHECK(s->spool);xlsTextRelease(input.text);xideWorkspaceEditRelease(plan);xrtValueRelease(root);free(uri);
 CHECK(xideTextFileEncode(replacement,(int)strlen(replacement),decoded.bom,decoded.eol,&encodedText,&e));s->expected=_strdup(!strcmp(decoded.text,replacement)?raw:(char*)encodedText.raw);CHECK(s->expected);xideTextFileClear(&decoded);xideTextFileClear(&encodedText);s->write=xideEditFileWritePrepare(s->spool,0,0,&e);CHECK(s->write&&!e.code);xide_edit_file_status status;CHECK(xideEditFileWriteStatus(s->write,&status)&&status.phase==XIDE_EDIT_FILE_BEFORE&&status.document==0&&status.currentVerified);s->original=status.before;activeSource=status.before;
 CHECK(xideStateOpen(&s->journal.file,s->intentPath,"xlang-write-test-intent-1",&e));s->journal.path=s->source;s->journal.id=asText(field(xideEditSpoolManifest(s->spool),"id"));
}
static char* sub(const char* root,const char* name){char* p=xidePathResolveLexical(root,name);CHECK(p);return p;}
static void basic(const char* root)
{
 const char* raws[]={"a", "abcdef", "", "a", "\xef\xbb\xbf" "a\r\nb\r\n", "same\r\nlast\n"};const char* next[]={"longer中文\nnext\n","x","nonempty","","新\n文\n","same\nlast\n"};
 for(unsigned i=0;i<6;++i){char name[64];snprintf(name,sizeof(name),"basic-%u",i);char* directory=sub(root,name);sample s;makeSample(&s,directory,raws[i],next[i]);xide_model_error e={0};active=1;CHECK(xideEditFileWriteApply(s.write,intent,&s.journal,NULL,NULL,&e)&&!e.code);xide_edit_file_status status;CHECK(xideEditFileWriteStatus(s.write,&status)&&status.phase==XIDE_EDIT_FILE_AFTER&&status.currentVerified);if(status.changed)CHECK(flushCalls==2&&timeCalls==2&&eofCalls==1);bytesEqual(s.source,s.expected);CHECK(!xideEditFileWriteApply(s.write,intent,&s.journal,NULL,NULL,&e)&&e.code==XIDE_MODEL_INVALID);CHECK(xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&!e.code);CHECK(xideEditFileWriteStatus(s.write,&status)&&status.phase==XIDE_EDIT_FILE_RESTORED&&status.currentVerified);bytesEqual(s.source,raws[i]);CHECK(xideWorkspaceEditFilesValidate(xideEditSpoolSources(s.spool),NULL,NULL,&e));if(i==5)CHECK(!writeCalls&&!flushCalls&&!timeCalls&&!eofCalls);char* path=_strdup(s.source);CHECK(path);xide_file_snapshot original=s.original;clearSample(&s);xide_file_snapshot disk={0};CHECK(xideFileSnapshotRead(path,&disk,&e)&&xideFileSnapshotEqual(&disk,&original));free(path);free(directory);}
 /* Actual mixed package: refuse overlays, tiny quotas, and recovered packages;
  * permit separate changed and unchanged closed members with stable pins. */
 char* directory=sub(root,"mixed");data d;init(&d,directory);char* scope=sub(directory,"scope");xide_model_error e={0};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,NULL,NULL,&e);CHECK(p);inspect(p,1);CHECK(!xideEditFileWritePrepare(p,1,0,&e)&&e.code==XIDE_MODEL_INVALID);CHECK(!xideEditFileWritePrepare(p,0,1,&e)&&e.code==XIDE_MODEL_MEMORY);xide_edit_file_write* w=xideEditFileWritePrepare(p,2,0,&e);CHECK(w);xideEditFileWriteDestroy(w);xideEditSpoolDestroy(p);p=xideEditSpoolOpen(scope,NULL,NULL,&e);CHECK(p&&!xideEditFileWritePrepare(p,0,0,&e)&&e.code==XIDE_MODEL_INVALID);xideEditSpoolDestroy(p);freeData(&d);free(scope);free(directory);
}
static void faults(const char* root)
{
 char* raw=malloc(196612);char* next=malloc(196612);CHECK(raw&&next);memset(raw,'a',196611);raw[196611]=0;memset(next,'b',196611);next[196611]=0;
 for(unsigned mode=0;mode<14;++mode){char name[64];snprintf(name,sizeof(name),"fault-%u",mode);char* directory=sub(root,name);sample s;makeSample(&s,directory,raw,(mode==4||mode==5)?"short":next);active=1;failAt=mode<3?2:1;if(mode<3)writeMode=(int)mode+1;if(mode==3)flushMode=1;if(mode==4)eofMode=1;if(mode==5)eofMode=2;if(mode==6)timeMode=1;if(mode==7){timeMode=1;failAt=2;}if(mode==8){timeMode=2;failAt=2;}if(mode==9)s.journal.deny=XIDE_EDIT_FILE_APPLY;if(mode==10){s.journal.change=1;}if(mode==11)writeMode=4;if(mode==12)stopPoint=4;if(mode==13)writeMode=5;
  xide_model_error e={0};CHECK(!xideEditFileWriteApply(s.write,intent,&s.journal,stopWrite,NULL,&e)&&e.code);s.journal.change=0;xide_edit_file_status status;CHECK(xideEditFileWriteStatus(s.write,&status));
  if(mode==9)CHECK(!status.currentVerified&&!status.touched);
  if(mode==10){CHECK(!status.currentVerified&&!status.touched&&!writeCalls&&!xideEditFileWriteRestore(s.write,intent,&s.journal,&e));}
  else if(mode==11||mode==13){CHECK(!xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&e.code==XIDE_MODEL_CONFLICT);CHECK(xideEditFileWriteStatus(s.write,&status)&&status.phase==XIDE_EDIT_FILE_IN_DOUBT);}
  else{CHECK(xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&!e.code);bytesEqual(s.source,raw);CHECK(xideWorkspaceEditFilesValidate(xideEditSpoolSources(s.spool),NULL,NULL,&e));}
  clearSample(&s);free(directory);
 }
 free(raw);free(next);
}
static void restoreGuards(const char* root)
{
 for(unsigned mode=0;mode<5;++mode){char name[64];snprintf(name,sizeof(name),"restore-%u",mode);char* directory=sub(root,name);sample s;makeSample(&s,directory,"original","new");xide_model_error e={0};active=1;CHECK(xideEditFileWriteApply(s.write,intent,&s.journal,NULL,NULL,&e));writeCalls=flushCalls=timeCalls=eofCalls=0;
  if(mode==0)s.journal.deny=XIDE_EDIT_FILE_RESTORE;
  if(mode==1)s.journal.change=1;
  if(mode==2)writeMode=1;
  if(mode==3)flushMode=1;
  if(mode==4)timeMode=1;
  CHECK(!xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&e.code);xide_edit_file_status status;CHECK(xideEditFileWriteStatus(s.write,&status)&&status.phase==XIDE_EDIT_FILE_IN_DOUBT&&status.restoreAttempted&&!status.currentVerified);CHECK(!xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&e.code==XIDE_MODEL_INVALID);clearSample(&s);free(directory);
 }
}
static void cancellationMatrix(const char* root)
{
 char* raw=malloc(196612);char* next=malloc(196612);CHECK(raw&&next);memset(raw,'a',196611);raw[196611]=0;for(size_t i=0;i<196611;++i)next[i]=i%7?'b':'a';next[196611]=0;
 unsigned observed=0;
 for(unsigned point=0;!point||point<=observed;++point){char name[64];snprintf(name,sizeof(name),"cancel-%u",point);char* directory=sub(root,name);sample s;makeSample(&s,directory,raw,next);active=1;stopPoint=point?point:UINT_MAX;xide_model_error e={0};int okay=xideEditFileWriteApply(s.write,intent,&s.journal,stopWrite,NULL,&e);if(!point){CHECK(okay);observed=stopCall;CHECK(observed>2);}else CHECK(!okay&&e.code==XIDE_MODEL_CONFLICT&&stopCall==point);CHECK(xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&!e.code);bytesEqual(s.source,raw);CHECK(xideWorkspaceEditFilesValidate(xideEditSpoolSources(s.spool),NULL,NULL,&e));clearSample(&s);free(directory);}
 char* directory=sub(root,"partial-metadata");sample s;makeSample(&s,directory,raw,next);active=1;writeMode=2;failAt=2;xide_model_error e={0};CHECK(!xideEditFileWriteApply(s.write,intent,&s.journal,NULL,NULL,&e));wchar_t* path=xidePathNativeWide(s.source);HANDLE h=CreateFileW(path,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);free(path);FILETIME changed={456,789};CHECK(h!=INVALID_HANDLE_VALUE&&__real___imp_SetFileTime(h,NULL,NULL,&changed)&&CloseHandle(h));CHECK(!xideEditFileWriteRestore(s.write,intent,&s.journal,&e)&&e.code==XIDE_MODEL_CONFLICT);clearSample(&s);free(directory);free(raw);free(next);printf("write cancellation: %u checkpoints\n",observed);
}
int main(int argc,char** argv)
{
 CHECK(argc==2&&xrtDirCreateAll(argv[1]));warmServices(argv[1]);warmJournal(argv[1]);DWORD handles=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));basic(argv[1]);faults(argv[1]);restoreGuards(argv[1]);cancellationMatrix(argv[1]);DWORD after=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);printf("workspace_edit_write_test passed: %u checks; pinned source writes/restores, BOM/EOL, grow/shrink/empty/noop, durable test intents, partial/short writes, truncation/flush/timestamp failures, cancellation, external changes and handle balance\n",checks);return 0;
}
