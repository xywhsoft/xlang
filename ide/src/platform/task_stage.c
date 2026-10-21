#include "platform/task_stage.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

#define STAGE_MARKER ".xide-stage"
#pragma pack(push,1)
typedef struct stage_inventory {
 uint32_t present,volume,high,low;
 uint64_t size,created,written;
 unsigned char sha256[32];
} stage_inventory;
typedef struct stage_record {
 unsigned char magic[16];
 uint32_t version,state;
 char guid[36];
 uint64_t completed;
 uint32_t rootVolume,rootHigh,rootLow,directoryVolume,directoryHigh,directoryLow;
 unsigned char reserved[36];
 stage_inventory files[10];
} stage_record;
#pragma pack(pop)
_Static_assert(sizeof(stage_inventory)==72 && sizeof(stage_record)==848,"Stable Windows stage ownership record");
static const unsigned char stageMagic[16]={'x','l','a','n','g','-','i','d','e','-','s','t','a','g','e','1'};
struct xide_task_stage {
 HANDLE marker;
 HANDLE lifetime;
 HANDLE* parents;
 size_t parentCount,parentCapacity;
 stage_record record;
 char* path;
 int readonly;
};
static const char* const stageFiles[]={STAGE_MARKER,"request.xlp","compiler-help.log","build.log","run.log","test.log","program.exe","module.dll","publish.tmp","inputs.json","build-invocation.json"};
static int snapshotFiles(xide_task_stage* stage,xide_model_error* error);
/* Kernel object lifetime follows directory identity across parent moves. It
   disappears when the last process handle closes, including process crashes.
   A named mutex serializes creation against cleanup's last-reader check. */
static void leaseName(wchar_t name[128],const BY_HANDLE_FILE_INFORMATION* directory,const wchar_t* suffix)
{
 swprintf(name,128,L"Global\\xlang-ide-stage-%08lx-%08lx-%08lx-%ls",(unsigned long)directory->dwVolumeSerialNumber,(unsigned long)directory->nFileIndexHigh,(unsigned long)directory->nFileIndexLow,suffix);
}
static HANDLE leaseMutex(const BY_HANDLE_FILE_INFORMATION* directory)
{
 wchar_t name[128]; leaseName(name,directory,L"gate"); HANDLE gate=CreateMutexW(NULL,FALSE,name); if(!gate) return NULL;
 DWORD result=WaitForSingleObject(gate,0);
 if(result==WAIT_OBJECT_0 || result==WAIT_ABANDONED) return gate;
 CloseHandle(gate); return NULL;
}
static HANDLE leaseLifetime(const BY_HANDLE_FILE_INFORMATION* directory)
{
 wchar_t name[128]; leaseName(name,directory,L"live"); return CreateEventW(NULL,TRUE,FALSE,name);
}
static int noLifetime(const BY_HANDLE_FILE_INFORMATION* directory)
{
 wchar_t name[128]; leaseName(name,directory,L"live"); HANDLE live=OpenEventW(SYNCHRONIZE,FALSE,name);
 if(live) { CloseHandle(live); return 0; }
 return GetLastError()==ERROR_FILE_NOT_FOUND;
}
static void releaseMutex(HANDLE gate)
{
 if(gate) { ReleaseMutex(gate); CloseHandle(gate); }
}
static int recordGate(HANDLE file,int exclusive,int unlock)
{
 OVERLAPPED gate={0};
 return unlock ? UnlockFileEx(file,0,sizeof(stage_record),0,&gate) : LockFileEx(file,LOCKFILE_FAIL_IMMEDIATELY|(exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0),0,sizeof(stage_record),0,&gate);
}
const char* xideTaskStageDirectory(const xide_task_stage* stage) { return stage ? stage->path : NULL; }
static int fail(xide_model_error* e,int code,const char* text)
{ if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static int guidValid(const char* name)
{
 if(!name || strlen(name)!=36) return 0;
 for(size_t i=0;i<36;++i) {
  if(i==8 || i==13 || i==18 || i==23) { if(name[i]!='-') return 0; }
  else if(!((name[i]>='0' && name[i]<='9') || (name[i]>='a' && name[i]<='f'))) return 0;
 }
 return 1;
}
static HANDLE directoryHandle(const wchar_t* path,DWORD access)
{
 HANDLE h=CreateFileW(path,access,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 BY_HANDLE_FILE_INFORMATION info={0};
 if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) || !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))) { CloseHandle(h); h=INVALID_HANDLE_VALUE; }
 return h;
}
static int appendParent(xide_task_stage* stage,HANDLE h)
{
 if(h==INVALID_HANDLE_VALUE) return 0;
 if(stage->parentCount==stage->parentCapacity) {
  size_t capacity=stage->parentCapacity ? stage->parentCapacity*2 : 16;
  HANDLE* parents=realloc(stage->parents,capacity*sizeof(*parents));
  if(!parents) { CloseHandle(h); return 0; } stage->parents=parents; stage->parentCapacity=capacity;
 }
 stage->parents[stage->parentCount++]=h; return 1;
}
static void releaseParents(xide_task_stage* stage)
{
 for(size_t i=stage->parentCount;i>0;--i) CloseHandle(stage->parents[i-1]);
 free(stage->parents); stage->parents=NULL; stage->parentCount=stage->parentCapacity=0;
}
/* Pin every existing ancestor through the cache root. The caller creates no
   marker or request before rejecting reparse points and replaced parents. */
static int pinChain(xide_task_stage* stage,wchar_t* path)
{
 size_t start=0,length=wcslen(path);
 if(length>=7 && !wcsncmp(path,L"\\\\?\\",4) && path[5]==L':' && path[6]==L'\\') start=7;
 else if(length>=8 && !wcsncmp(path,L"\\\\?\\UNC\\",8)) {
  const wchar_t* server=wcschr(path+8,L'\\'); const wchar_t* share=server ? wcschr(server+1,L'\\') : NULL;
  if(!share) return 0;
  start=(size_t)(share-path)+1;
 } else return 0;
 for(size_t i=start;i<=length;++i) if(path[i]==L'\\' || !path[i]) {
  wchar_t saved=path[i]; path[i]=0;
  HANDLE h=directoryHandle(path,FILE_READ_ATTRIBUTES); path[i]=saved;
  if(!appendParent(stage,h)) return 0;
 }
 return stage->parentCount>0;
}
static int emptyDirectory(const wchar_t* path)
{
 size_t n=wcslen(path); wchar_t* pattern=malloc((n+3)*sizeof(*pattern)); if(!pattern) return 0;
 memcpy(pattern,path,n*sizeof(*pattern)); memcpy(pattern+n,L"\\*",3*sizeof(*pattern));
 WIN32_FIND_DATAW data={0}; HANDLE search=FindFirstFileW(pattern,&data); free(pattern);
 if(search==INVALID_HANDLE_VALUE) return GetLastError()==ERROR_FILE_NOT_FOUND;
 int empty=1;
 do { if(wcscmp(data.cFileName,L".") && wcscmp(data.cFileName,L"..")) { empty=0; break; } } while(FindNextFileW(search,&data));
 DWORD status=GetLastError(); FindClose(search); return empty && status==ERROR_NO_MORE_FILES;
}
static int writeRecord(xide_task_stage* stage)
{
 LARGE_INTEGER start={0}; DWORD count=0;
 if(!recordGate(stage->marker,1,0)) return 0;
 int okay=SetFilePointerEx(stage->marker,start,NULL,FILE_BEGIN) && WriteFile(stage->marker,&stage->record,sizeof(stage->record),&count,NULL) && count==sizeof(stage->record) && SetEndOfFile(stage->marker) && FlushFileBuffers(stage->marker);
 return recordGate(stage->marker,1,1) && okay;
}
void xideTaskStageRelease(xide_task_stage* stage)
{
 if(!stage) return;
 if(stage->marker && stage->marker!=INVALID_HANDLE_VALUE) CloseHandle(stage->marker);
 if(stage->lifetime) CloseHandle(stage->lifetime);
 releaseParents(stage); free(stage->path); free(stage);
}
xide_task_stage* xideTaskStageBegin(const char* directory,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 xide_task_stage* stage=NULL; char *path=NULL,*root=NULL,*marker=NULL; wchar_t *rootWide=NULL,*pathWide=NULL,*markerWide=NULL; int okay=0;
 HANDLE gate=NULL;
 path=directory ? xidePathResolveLexical(NULL,directory) : NULL; root=path ? xidePathDirectory(path) : NULL;
 if(!path || !root || !guidValid(xidePathFilename(path)) || strcmp(xidePathFilename(root),".xlang-ide-build")) { fail(error,XIDE_MODEL_INVALID,"Stage must be a fresh GUID in .xlang-ide-build."); goto done; }
 stage=calloc(1,sizeof(*stage)); rootWide=xidePathNativeWide(root); pathWide=xidePathNativeWide(path);
 if(!stage || !rootWide || !pathWide) { fail(error,XIDE_MODEL_IO,"Unable to allocate task stage ownership."); goto done; }
 stage->path=malloc(strlen(path)+1); if(!stage->path) { fail(error,XIDE_MODEL_IO,"Unable to retain the task stage path."); goto done; } strcpy(stage->path,path);
 if(!xideIoCreateDirectories(root) || !pinChain(stage,rootWide)) { fail(error,XIDE_MODEL_IO,"Cannot pin ordinary task cache ancestors."); goto done; }
 BY_HANDLE_FILE_INFORMATION rootInfo={0},directoryInfo={0};
 if(!GetFileInformationByHandle(stage->parents[stage->parentCount-1],&rootInfo)) { fail(error,XIDE_MODEL_IO,"Cannot identify the task cache root."); goto done; }
 /* CREATE_NEW semantics for the directory: prior artifacts are never reused. */
 if(!CreateDirectoryW(pathWide,NULL)) { fail(error,XIDE_MODEL_CONFLICT,"Task stage already exists or cannot be created; prior files are preserved."); goto done; }
 if(!appendParent(stage,directoryHandle(pathWide,FILE_READ_ATTRIBUTES)) || !GetFileInformationByHandle(stage->parents[stage->parentCount-1],&directoryInfo) || !emptyDirectory(pathWide)) { fail(error,XIDE_MODEL_CONFLICT,"Task stage is not a pinned empty ordinary directory."); goto done; }
 gate=leaseMutex(&directoryInfo); stage->lifetime=gate ? leaseLifetime(&directoryInfo) : NULL;
 if(!gate || !stage->lifetime) { fail(error,XIDE_MODEL_CONFLICT,"Cannot acquire the task stage lifetime lease."); goto done; }
 marker=xidePathResolveLexical(path,STAGE_MARKER); markerWide=marker ? xidePathNativeWide(marker) : NULL;
 if(!markerWide) { fail(error,XIDE_MODEL_IO,"Unable to allocate the task stage lease path."); goto done; }
 stage->marker=CreateFileW(markerWide,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 BY_HANDLE_FILE_INFORMATION info={0};
 if(stage->marker==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(stage->marker,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks!=1) { fail(error,XIDE_MODEL_IO,"Cannot create a unique ordinary task stage lease."); goto done; }
 memcpy(stage->record.magic,stageMagic,sizeof(stageMagic)); stage->record.version=1; stage->record.state=1;
 memcpy(stage->record.guid,xidePathFilename(path),36);
 stage->record.rootVolume=rootInfo.dwVolumeSerialNumber; stage->record.rootHigh=rootInfo.nFileIndexHigh; stage->record.rootLow=rootInfo.nFileIndexLow;
 stage->record.directoryVolume=directoryInfo.dwVolumeSerialNumber; stage->record.directoryHigh=directoryInfo.nFileIndexHigh; stage->record.directoryLow=directoryInfo.nFileIndexLow;
 if(!writeRecord(stage)) { fail(error,XIDE_MODEL_IO,"Cannot publish task stage ownership; files are preserved."); goto done; }
 okay=1;
done:
 releaseMutex(gate);
 free(path); free(root); free(marker); free(rootWide); free(pathWide); free(markerWide);
 if(!okay) { xideTaskStageRelease(stage); stage=NULL; } return stage;
}
int xideTaskStageComplete(xide_task_stage* stage,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 if(!stage || stage->readonly || !stage->marker || stage->marker==INVALID_HANDLE_VALUE) return fail(error,XIDE_MODEL_INVALID,"No writable owned task stage is available.");
 int okay=snapshotFiles(stage,error);
 if(okay) { FILETIME time={0}; GetSystemTimeAsFileTime(&time); stage->record.completed=((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime; stage->record.state=2; okay=writeRecord(stage); }
 /* History keeps the kernel lifetime token. No open disk handles remain to
    obstruct a completed project's parent-directory move. */
 CloseHandle(stage->marker); stage->marker=NULL;
 releaseParents(stage);
 if(!okay) return fail(error,XIDE_MODEL_IO,"Stage completion record failed; cache is preserved.");
 return 1;
}

typedef struct stage_candidate { char guid[37]; stage_record record; } stage_candidate;
static int recordValid(const stage_record* record,const char* guid,const BY_HANDLE_FILE_INFORMATION* root,const BY_HANDLE_FILE_INFORMATION* directory)
{
 unsigned char zero[36]={0}; stage_inventory empty={0};
 for(size_t i=0;i<10;++i) if(record->files[i].present>1 || (!record->files[i].present && memcmp(&record->files[i],&empty,sizeof(empty)))) return 0;
 return !memcmp(record->magic,stageMagic,16) && record->version==1 && record->state==2 && record->completed && !memcmp(record->guid,guid,36) && !memcmp(record->reserved,zero,36) &&
  record->rootVolume==root->dwVolumeSerialNumber && record->rootHigh==root->nFileIndexHigh && record->rootLow==root->nFileIndexLow &&
  record->directoryVolume==directory->dwVolumeSerialNumber && record->directoryHigh==directory->nFileIndexHigh && record->directoryLow==directory->nFileIndexLow;
}
static HANDLE ordinaryFile(const wchar_t* path,DWORD access,DWORD sharing,BY_HANDLE_FILE_INFORMATION* info)
{
 HANDLE h=CreateFileW(path,access,sharing,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,info) || (info->dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)) || info->nNumberOfLinks!=1)) { CloseHandle(h); h=INVALID_HANDLE_VALUE; }
 return h;
}
static int readRecord(HANDLE h,stage_record* record)
{
 LARGE_INTEGER size={0},start={0}; DWORD count=0;
 if(!recordGate(h,0,0)) return 0;
 int okay=GetFileSizeEx(h,&size) && size.QuadPart==sizeof(*record) && SetFilePointerEx(h,start,NULL,FILE_BEGIN) && ReadFile(h,record,sizeof(*record),&count,NULL) && count==sizeof(*record);
 return recordGate(h,0,1) && okay;
}
xide_task_stage* xideTaskStageProtectPath(const char* input,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 if(!input || !*input) return NULL;
 char* resolved=xidePathResolve(NULL,input); if(!resolved) { fail(error,XIDE_MODEL_IO,"Cannot resolve the stage protection path."); return NULL; }
 /* Resolve physical aliases first; ordinary editor files then need no parent
    traversal or marker I/O. A case-insensitive segment check accepts Windows. */
 const char* segment=resolved; int managed=0;
 while((segment=strchr(segment,'/'))!=NULL) { ++segment; if(!_strnicmp(segment,".xlang-ide-build/",17)) { managed=1; break; } }
 if(!managed) { free(resolved); return NULL; }
 char* directory=xidePathIsDirectory(resolved) ? resolved : xidePathDirectory(resolved); if(directory!=resolved) free(resolved);
 char *root=NULL,*marker=NULL; wchar_t *wide=NULL,*markerWide=NULL; xide_task_stage pins={0}; xide_task_stage* reference=NULL; HANDLE gate=NULL;
 while(directory) {
  root=xidePathDirectory(directory);
  if(root && !_stricmp(xidePathFilename(root),".xlang-ide-build")) break;
  if(!root || xidePathLexicalEqual(root,directory)) { free(root); root=NULL; break; }
  free(directory); directory=root; root=NULL;
 }
 if(!root) goto done;
 char guid[37]={0}; const char* leaf=xidePathFilename(directory); if(strlen(leaf)!=36) goto done;
 for(size_t i=0;i<36;++i) guid[i]=leaf[i]>='A' && leaf[i]<='F' ? (char)(leaf[i]-'A'+'a') : leaf[i];
 if(!guidValid(guid)) goto done;
 marker=xidePathResolveLexical(directory,STAGE_MARKER); markerWide=marker ? xidePathNativeWide(marker) : NULL;
 DWORD attrs=markerWide ? GetFileAttributesW(markerWide) : INVALID_FILE_ATTRIBUTES;
 if(attrs==INVALID_FILE_ATTRIBUTES && (GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND)) goto done;
 if(attrs!=INVALID_FILE_ATTRIBUTES && (attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY))) goto done;
 reference=calloc(1,sizeof(*reference)); wide=xidePathNativeWide(root);
 if(!reference || !wide || !pinChain(&pins,wide)) { fail(error,XIDE_MODEL_IO,"Cannot pin a referenced task stage."); goto failed; }
 reference->readonly=1; BY_HANDLE_FILE_INFORMATION markerInfo={0},rootInfo={0},directoryInfo={0}; wchar_t* directoryWide=xidePathNativeWide(directory);
 HANDLE h=directoryWide ? directoryHandle(directoryWide,FILE_READ_ATTRIBUTES) : INVALID_HANDLE_VALUE; free(directoryWide);
 int identified=h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&directoryInfo) && GetFileInformationByHandle(pins.parents[pins.parentCount-1],&rootInfo); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
 if(identified) gate=leaseMutex(&directoryInfo);
 reference->marker=markerWide ? ordinaryFile(markerWide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,&markerInfo) : INVALID_HANDLE_VALUE;
 if(!identified || !gate || reference->marker==INVALID_HANDLE_VALUE) { fail(error,XIDE_MODEL_CONFLICT,"Referenced stage is busy or unavailable; it is preserved."); goto failed; }
 LARGE_INTEGER size={0};
 if(GetFileSizeEx(reference->marker,&size) && size.QuadPart!=sizeof(stage_record)) goto failed;
 if(!readRecord(reference->marker,&reference->record)) { fail(error,XIDE_MODEL_CONFLICT,"Referenced stage metadata is busy or unavailable; it is preserved."); goto failed; }
 stage_record comparison=reference->record; comparison.state=2; if(!comparison.completed) comparison.completed=1;
 /* Unsupported/copy-stale ownership is unmanaged: it remains editable and
    cleanup already refuses it. Only a valid stage needs a lifetime lease. */
 if((reference->record.state!=1 && reference->record.state!=2) || !recordValid(&comparison,guid,&rootInfo,&directoryInfo)) goto failed;
 reference->lifetime=leaseLifetime(&directoryInfo);
 if(!reference->lifetime) { fail(error,XIDE_MODEL_IO,"Cannot retain referenced stage protection."); goto failed; }
 CloseHandle(reference->marker); reference->marker=NULL;
 reference->path=directory; directory=NULL; goto done;
failed:
 xideTaskStageRelease(reference); reference=NULL;
done:
 releaseParents(&pins); releaseMutex(gate); free(directory); free(root); free(marker); free(wide); free(markerWide); return reference;
}
static uint64_t ticks(FILETIME value) { return ((uint64_t)value.dwHighDateTime<<32)|value.dwLowDateTime; }
static int oldEnough(uint64_t now,uint64_t value) { return value && now>=value && now-value>=XIDE_TASK_STAGE_GRACE_TICKS; }
static int candidateOrder(const void* a,const void* b)
{
 const stage_candidate *left=a,*right=b;
 if(left->record.completed!=right->record.completed) return left->record.completed>right->record.completed ? -1 : 1;
 return strcmp(left->guid,right->guid);
}
static int protectedDirectory(const char* directory,const xide_strings* paths)
{
 if(!paths) return 0;
 size_t length=strlen(directory);
 for(size_t i=0;i<paths->count;++i) {
  if(!paths->items[i] || !*paths->items[i]) continue;
  char* path=xidePathResolveLexical(NULL,paths->items[i]);
  if(!path) return 1;
  size_t n=strlen(path); int protected=0;
  if(n==length) protected=xidePathLexicalEqual(path,directory);
  else if(n>length && path[length]=='/') { char saved=path[length]; path[length]=0; protected=xidePathLexicalEqual(path,directory); path[length]=saved; }
  free(path); if(protected) return 1;
 }
 return 0;
}
typedef struct stage_file { HANDLE handle; BY_HANDLE_FILE_INFORMATION info; } stage_file;
static int fileNameIndex(const wchar_t* name)
{
 for(size_t i=0;i<sizeof(stageFiles)/sizeof(*stageFiles);++i) {
  char ascii[80]; size_t n=wcslen(name); if(n>=sizeof(ascii)) return -1;
  int valid=1; for(size_t j=0;j<=n;++j) { if(name[j]>127) { valid=0; break; } ascii[j]=(char)name[j]; }
  if(valid && !strcmp(ascii,stageFiles[i])) return (int)i;
 }
 return -1;
}
static int sameIdentity(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{ return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow; }
static int hashFile(HANDLE handle,unsigned char digest[32],int (*cancelled)(void*),void* user,ULONGLONG deadline)
{
 LARGE_INTEGER start={0}; if(!SetFilePointerEx(handle,start,NULL,FILE_BEGIN)) return 0;
 unsigned char block[65536]; xsha256 hash; xrtSha256Init(&hash);
 for(;;) {
  if((cancelled && cancelled(user)) || GetTickCount64()>deadline) return 0;
  DWORD count=0; if(!ReadFile(handle,block,sizeof(block),&count,NULL)) return 0;
  if(!count) break;
  if(!xrtSha256Update(&hash,block,count)) return 0;
 }
 return xrtSha256Final(&hash,digest);
}
static int inventoryMatches(const stage_inventory* inventory,const BY_HANDLE_FILE_INFORMATION* info)
{
 return inventory->present==1 && inventory->volume==info->dwVolumeSerialNumber && inventory->high==info->nFileIndexHigh && inventory->low==info->nFileIndexLow && inventory->size==(((uint64_t)info->nFileSizeHigh<<32)|info->nFileSizeLow) && inventory->created==ticks(info->ftCreationTime) && inventory->written==ticks(info->ftLastWriteTime);
}
static int snapshotFiles(xide_task_stage* stage,xide_model_error* error)
{
 memset(stage->record.files,0,sizeof(stage->record.files)); wchar_t* wide=xidePathNativeWide(stage->path); if(!wide) return fail(error,XIDE_MODEL_IO,"Cannot inspect completed stage contents.");
 size_t length=wcslen(wide); wchar_t* pattern=malloc((length+3)*sizeof(*pattern)); if(!pattern) { free(wide); return fail(error,XIDE_MODEL_IO,"Cannot allocate completed stage inventory."); }
 memcpy(pattern,wide,length*sizeof(*pattern)); memcpy(pattern+length,L"\\*",3*sizeof(*pattern)); WIN32_FIND_DATAW data={0}; HANDLE search=FindFirstFileW(pattern,&data); free(pattern); free(wide);
 if(search==INVALID_HANDLE_VALUE) return fail(error,XIDE_MODEL_IO,"Cannot enumerate completed stage contents.");
 int okay=1; ULONGLONG deadline=GetTickCount64()+5000;
 do {
  if(!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
  int index=fileNameIndex(data.cFileName); if(index<0 || (data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY))) { okay=0; break; }
  if(!index) continue;
  char* path=xidePathResolveLexical(stage->path,stageFiles[index]); wchar_t* native=path ? xidePathNativeWide(path) : NULL; free(path); BY_HANDLE_FILE_INFORMATION before={0},after={0};
  HANDLE file=native ? ordinaryFile(native,GENERIC_READ,FILE_SHARE_READ,&before) : INVALID_HANDLE_VALUE; free(native);
  if(file==INVALID_HANDLE_VALUE) { okay=0; break; }
  stage_inventory* item=&stage->record.files[index-1]; item->present=1; item->volume=before.dwVolumeSerialNumber; item->high=before.nFileIndexHigh; item->low=before.nFileIndexLow; item->size=((uint64_t)before.nFileSizeHigh<<32)|before.nFileSizeLow; item->created=ticks(before.ftCreationTime); item->written=ticks(before.ftLastWriteTime);
  okay=hashFile(file,item->sha256,NULL,NULL,deadline) && GetFileInformationByHandle(file,&after) && inventoryMatches(item,&after); CloseHandle(file); if(!okay) break;
 } while(FindNextFileW(search,&data));
 DWORD status=GetLastError(); FindClose(search);
 if(!okay || status!=ERROR_NO_MORE_FILES) return fail(error,XIDE_MODEL_IO,"Completed stage has unverified, changed or unavailable contents; it is preserved.");
 return 1;
}
static int removeCandidate(const char* root,const stage_candidate* candidate,const BY_HANDLE_FILE_INFORMATION* rootInfo,uint64_t now,const xide_strings* protectedPaths,int (*cancelled)(void*),void* user,xide_task_stage_stats* stats)
{
 char* path=xidePathResolveLexical(root,candidate->guid); wchar_t* wide=path ? xidePathNativeWide(path) : NULL; HANDLE directory=wide ? directoryHandle(wide,DELETE|FILE_READ_ATTRIBUTES) : INVALID_HANDLE_VALUE;
 stage_file files[sizeof(stageFiles)/sizeof(*stageFiles)]={0}; BY_HANDLE_FILE_INFORMATION directoryInfo={0}; stage_record record={0}; int okay=0,mutated=0; HANDLE gate=NULL;
 if(!path || !wide || directory==INVALID_HANDLE_VALUE || protectedDirectory(path,protectedPaths) || !GetFileInformationByHandle(directory,&directoryInfo) || (directoryInfo.dwFileAttributes&FILE_ATTRIBUTE_READONLY)) goto done;
 gate=leaseMutex(&directoryInfo); if(!gate || !noLifetime(&directoryInfo)) goto done;
 char* marker=xidePathResolveLexical(path,STAGE_MARKER); wchar_t* markerWide=marker ? xidePathNativeWide(marker) : NULL;
 files[0].handle=markerWide ? ordinaryFile(markerWide,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,&files[0].info) : INVALID_HANDLE_VALUE; free(marker); free(markerWide);
 if(files[0].handle==INVALID_HANDLE_VALUE || !readRecord(files[0].handle,&record) || memcmp(&record,&candidate->record,sizeof(record)) || !recordValid(&record,candidate->guid,rootInfo,&directoryInfo) ||
  !oldEnough(now,record.completed) || !oldEnough(now,ticks(files[0].info.ftCreationTime)) || !oldEnough(now,ticks(files[0].info.ftLastWriteTime))) goto done;
 size_t length=wcslen(wide); wchar_t* pattern=malloc((length+3)*sizeof(*pattern)); if(!pattern) goto done;
 memcpy(pattern,wide,length*sizeof(*pattern)); memcpy(pattern+length,L"\\*",3*sizeof(*pattern)); WIN32_FIND_DATAW data={0}; HANDLE search=FindFirstFileW(pattern,&data); free(pattern);
 if(search==INVALID_HANDLE_VALUE) goto done;
 int valid=1;
 do {
  if(!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
  int index=fileNameIndex(data.cFileName);
  if(index<0 || (data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY))) { valid=0; break; }
  if(index==0) continue;
  char* child=xidePathResolveLexical(path,stageFiles[index]); wchar_t* childWide=child ? xidePathNativeWide(child) : NULL;
  files[index].handle=childWide ? ordinaryFile(childWide,GENERIC_READ|DELETE,FILE_SHARE_READ,&files[index].info) : INVALID_HANDLE_VALUE; free(child); free(childWide);
  if(files[index].handle==INVALID_HANDLE_VALUE) { valid=0; break; }
  unsigned char digest[32]; BY_HANDLE_FILE_INFORMATION after={0};
  if(!inventoryMatches(&record.files[index-1],&files[index].info) || !hashFile(files[index].handle,digest,cancelled,user,GetTickCount64()+5000) || memcmp(digest,record.files[index-1].sha256,32) || !GetFileInformationByHandle(files[index].handle,&after) || !inventoryMatches(&record.files[index-1],&after) || !oldEnough(now,ticks(after.ftCreationTime)) || !oldEnough(now,ticks(after.ftLastWriteTime))) { valid=0; break; }
  /* Physical aliases of protected source files also stop cleanup. */
  for(size_t i=0;protectedPaths && i<protectedPaths->count && valid;++i) {
   wchar_t* input=xidePathNativeWide(protectedPaths->items[i]); BY_HANDLE_FILE_INFORMATION inputInfo={0};
   HANDLE h=input ? CreateFileW(input,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE; free(input);
   if(h!=INVALID_HANDLE_VALUE) { if(GetFileInformationByHandle(h,&inputInfo) && sameIdentity(&inputInfo,&files[index].info)) valid=0; CloseHandle(h); }
  }
  if(!valid) break;
 } while(FindNextFileW(search,&data));
 DWORD status=GetLastError(); FindClose(search); if(!valid || status!=ERROR_NO_MORE_FILES) goto done;
 for(size_t i=1;i<sizeof(files)/sizeof(*files);++i) if(!!files[i].handle!=!!record.files[i-1].present) goto done;
 for(size_t i=1;i<sizeof(files)/sizeof(*files);++i) if(files[i].handle && files[i].handle!=INVALID_HANDLE_VALUE) {
  if(cancelled && cancelled(user)) goto done;
  FILE_DISPOSITION_INFO remove={TRUE}; if(!SetFileInformationByHandle(files[i].handle,FileDispositionInfo,&remove,sizeof(remove))) goto done;
  ++stats->deletedFiles; stats->releasedBytes+=((uint64_t)files[i].info.nFileSizeHigh<<32)|files[i].info.nFileSizeLow; mutated=1;
  CloseHandle(files[i].handle); files[i].handle=NULL;
 }
 if(cancelled && cancelled(user)) goto done;
 FILE_DISPOSITION_INFO remove={TRUE};
 if(!SetFileInformationByHandle(files[0].handle,FileDispositionInfo,&remove,sizeof(remove))) goto done;
 ++stats->deletedFiles; stats->releasedBytes+=sizeof(stage_record); mutated=1; CloseHandle(files[0].handle); files[0].handle=NULL;
 if(!SetFileInformationByHandle(directory,FileDispositionInfo,&remove,sizeof(remove))) goto done;
 ++stats->deletedStages; okay=1;
done:
 for(size_t i=0;i<sizeof(files)/sizeof(*files);++i) if(files[i].handle && files[i].handle!=INVALID_HANDLE_VALUE) CloseHandle(files[i].handle);
 if(directory!=INVALID_HANDLE_VALUE) CloseHandle(directory);
 releaseMutex(gate);
 free(path); free(wide); if(!okay) ++stats->skipped;
 return okay ? 1 : mutated ? -1 : 0;
}
int xideTaskStagePrune(const char* cacheRoot,size_t keepCompleted,uint64_t nowTicks,const xide_strings* protectedPaths,int (*cancelled)(void*),void* user,xide_task_stage_stats* stats,xide_model_error* error)
{
 if(error) memset(error,0,sizeof(*error));
 if(stats) memset(stats,0,sizeof(*stats));
 if(!cacheRoot || !stats || !nowTicks || (protectedPaths && protectedPaths->count && !protectedPaths->items)) return fail(error,XIDE_MODEL_INVALID,"Invalid task cache maintenance request.");
 xide_task_stage pins={0}; stage_candidate* candidates=NULL; size_t count=0,capacity=0; char* root=xidePathResolveLexical(NULL,cacheRoot); wchar_t* wide=root ? xidePathNativeWide(root) : NULL; int okay=0; HANDLE search=INVALID_HANDLE_VALUE;
 /* Kernel leases coordinate this Windows host. UNC and mapped network caches
    may have readers on another host, so automatic deletion is disabled there. */
 wchar_t drive[4]={0};
 if(wide && wcslen(wide)>=7 && wide[5]==L':') { drive[0]=wide[4]; drive[1]=L':'; drive[2]=L'\\'; }
 if(wide && (!wcsncmp(wide,L"\\\\?\\UNC\\",8) || (*drive && GetDriveTypeW(drive)==DRIVE_REMOTE))) { fail(error,XIDE_MODEL_INVALID,"Network task caches are preserved; cross-host cleanup leases are unsupported."); goto done; }
 if(!root || !wide || strcmp(xidePathFilename(root),".xlang-ide-build") || !pinChain(&pins,wide)) { fail(error,XIDE_MODEL_INVALID,"Task cache root is not pinned ordinary storage."); goto done; }
 BY_HANDLE_FILE_INFORMATION rootInfo={0}; if(!GetFileInformationByHandle(pins.parents[pins.parentCount-1],&rootInfo) || (rootInfo.dwFileAttributes&FILE_ATTRIBUTE_READONLY)) { fail(error,XIDE_MODEL_IO,"Task cache root is unavailable or read-only."); goto done; }
 size_t length=wcslen(wide); wchar_t* pattern=malloc((length+3)*sizeof(*pattern)); if(!pattern) { fail(error,XIDE_MODEL_IO,"Unable to allocate task cache enumeration."); goto done; }
 memcpy(pattern,wide,length*sizeof(*pattern)); memcpy(pattern+length,L"\\*",3*sizeof(*pattern)); WIN32_FIND_DATAW data={0}; search=FindFirstFileW(pattern,&data); free(pattern);
 if(search==INVALID_HANDLE_VALUE) { if(GetLastError()==ERROR_FILE_NOT_FOUND) okay=1; else fail(error,XIDE_MODEL_IO,"Cannot enumerate task cache."); goto done; }
 do {
  if(!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
  if(cancelled && cancelled(user)) { fail(error,XIDE_MODEL_CONFLICT,"Task cache maintenance cancelled; unvisited data is preserved."); goto done; }
  if(++stats->scanned>20000) { fail(error,XIDE_MODEL_INVALID,"Task cache exceeds the 20000-entry maintenance bound; no deletion started."); goto done; }
  char guid[37]={0}; int valid=wcslen(data.cFileName)==36;
  for(size_t i=0;valid && i<36;++i) { if(data.cFileName[i]>127) valid=0; else guid[i]=(char)data.cFileName[i]; }
  if(!valid || !guidValid(guid) || !(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (data.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY))) { ++stats->skipped; continue; }
  char *directory=xidePathResolveLexical(root,guid),*marker=directory ? xidePathResolveLexical(directory,STAGE_MARKER) : NULL;
  wchar_t *directoryWide=directory ? xidePathNativeWide(directory) : NULL,*markerWide=marker ? xidePathNativeWide(marker) : NULL;
  HANDLE h=directoryWide ? directoryHandle(directoryWide,FILE_READ_ATTRIBUTES) : INVALID_HANDLE_VALUE; BY_HANDLE_FILE_INFORMATION directoryInfo={0},markerInfo={0}; stage_record record={0};
  HANDLE file=markerWide ? ordinaryFile(markerWide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&markerInfo) : INVALID_HANDLE_VALUE;
  int owned=h!=INVALID_HANDLE_VALUE && file!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&directoryInfo) && readRecord(file,&record) && recordValid(&record,guid,&rootInfo,&directoryInfo);
  if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
  if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
  free(directory); free(marker); free(directoryWide); free(markerWide);
  if(!owned) { ++stats->skipped; continue; } ++stats->completed;
  if(count==capacity) { size_t next=capacity ? capacity*2 : 64; stage_candidate* values=realloc(candidates,next*sizeof(*values)); if(!values) { fail(error,XIDE_MODEL_IO,"Unable to retain complete task cache inventory; no deletion started."); goto done; } candidates=values; capacity=next; }
  memcpy(candidates[count].guid,guid,37); candidates[count++].record=record;
 } while(FindNextFileW(search,&data));
 if(GetLastError()!=ERROR_NO_MORE_FILES) { fail(error,XIDE_MODEL_IO,"Task cache enumeration was incomplete; no deletion started."); goto done; }
 FindClose(search); search=INVALID_HANDLE_VALUE; qsort(candidates,count,sizeof(*candidates),candidateOrder);
 for(size_t i=0;i<count;++i) {
  if(cancelled && cancelled(user)) { fail(error,XIDE_MODEL_CONFLICT,"Task cache maintenance cancelled; remaining data is preserved."); goto done; }
  if(i<keepCompleted || !oldEnough(nowTicks,candidates[i].record.completed)) { ++stats->retained; continue; }
  int removed=removeCandidate(root,&candidates[i],&rootInfo,nowTicks,protectedPaths,cancelled,user,stats);
  if(removed<0) { fail(error,XIDE_MODEL_IO,"An old stage was only partially cleaned; remaining files are preserved."); goto done; }
 }
 if(cancelled && cancelled(user)) { fail(error,XIDE_MODEL_CONFLICT,"Task cache maintenance cancelled; remaining data is preserved."); goto done; }
 okay=1;
done:
 if(search!=INVALID_HANDLE_VALUE) FindClose(search);
 releaseParents(&pins); free(root); free(wide); free(candidates); return okay;
}
