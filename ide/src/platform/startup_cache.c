#include "platform/startup_cache.h"
#include <aclapi.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#define MARKER L".xide-startup"
#define PREFIX L"xlang-ide-start-v2-"
#define BINARY_LIMIT (64u*1024u*1024u)
#define HASH_BUDGET (128ULL*1024*1024)
typedef struct cache_file {
    DWORD volume,high,low,size;
    ULONGLONG created,written;
    unsigned char digest[32];
} cache_file;
typedef struct cache_record {
    unsigned char magic[16];
    DWORD version,bytes,sidBytes,reserved;
    DWORD rootVolume,rootHigh,rootLow,directoryVolume,directoryHigh,directoryLow;
    DWORD markerVolume,markerHigh,markerLow;
    ULONGLONG created;
    wchar_t program[260];
    unsigned char sid[SECURITY_MAX_SID_SIZE];
    cache_file files[2];
} cache_record;
static const unsigned char magic[16]={'x','l','a','n','g','-','s','t','a','r','t','-','v','2',0,0};
struct xide_startup_cache_lease { HANDLE marker,directory,root; };
static xide_startup_cache_lease* childLease;
static ULONGLONG ticks(FILETIME value) {return ((ULONGLONG)value.dwHighDateTime<<32)|value.dwLowDateTime;}
static int identity(const BY_HANDLE_FILE_INFORMATION* info,DWORD volume,DWORD high,DWORD low)
{return info->dwVolumeSerialNumber==volume && info->nFileIndexHigh==high && info->nFileIndexLow==low;}
static wchar_t* join(const wchar_t* directory,const wchar_t* leaf)
{
    size_t n=wcslen(directory),m=wcslen(leaf);int slash=n && directory[n-1]!=L'\\';
    if(!n || n+m+(size_t)slash>=32768)return NULL;
    wchar_t* value=malloc((n+m+(size_t)slash+1)*sizeof(*value));if(!value)return NULL;
    memcpy(value,directory,n*sizeof(*value));if(slash)value[n++]=L'\\';memcpy(value+n,leaf,(m+1)*sizeof(*value));return value;
}
static wchar_t* parent(const wchar_t* path)
{
    const wchar_t* end=wcsrchr(path,L'\\');if(!end || end-path<3)return NULL;
    size_t n=(size_t)(end-path);wchar_t* result=malloc((n+1)*sizeof(*result));if(result){memcpy(result,path,n*sizeof(*result));result[n]=0;}return result;
}
static int currentUser(unsigned char sid[SECURITY_MAX_SID_SIZE],DWORD* length)
{
    HANDLE token=NULL;DWORD n=0;int okay=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)!=0;
    if(okay)GetTokenInformation(token,TokenUser,NULL,0,&n);
    TOKEN_USER* user=n && n<=4096 ? malloc(n) : NULL;
    okay=okay && user && GetTokenInformation(token,TokenUser,user,n,&n) && IsValidSid(user->User.Sid);
    if(okay){*length=GetLengthSid(user->User.Sid);okay=*length<=SECURITY_MAX_SID_SIZE && CopySid(SECURITY_MAX_SID_SIZE,sid,user->User.Sid);}
    free(user);if(token)CloseHandle(token);return okay;
}
static int ownerMatches(HANDLE file,const unsigned char* user)
{
    PSID owner=NULL;PSECURITY_DESCRIPTOR security=NULL;HANDLE token=NULL;DWORD n=0;
    int okay=GetSecurityInfo(file,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&owner,NULL,NULL,NULL,&security)==ERROR_SUCCESS && owner && IsValidSid(owner);
    if(okay && EqualSid(owner,(PSID)user)){LocalFree(security);return 1;}
    int queried=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)!=0;
    if(queried)GetTokenInformation(token,TokenOwner,NULL,0,&n);
    TOKEN_OWNER* expected=n && n<=4096 ? malloc(n) : NULL;
    okay=okay && queried && expected && GetTokenInformation(token,TokenOwner,expected,n,&n) && EqualSid(owner,expected->Owner);
    free(expected);if(token)CloseHandle(token);if(security)LocalFree(security);return okay;
}
static HANDLE ordinary(const wchar_t* path,DWORD access,int directory,BY_HANDLE_FILE_INFORMATION* info)
{
    HANDLE file=CreateFileW(path,access|READ_CONTROL,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(directory ? FILE_FLAG_BACKUP_SEMANTICS : 0),NULL);
    if(file!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(file,info) || (info->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || !!(info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory || (!directory && info->nNumberOfLinks!=1))){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
    return file;
}
static int digest(HANDLE file,const BY_HANDLE_FILE_INFORMATION* info,cache_file* value,ULONGLONG* budget)
{
    if(info->nFileSizeHigh || info->nFileSizeLow>BINARY_LIMIT || (budget && *budget+info->nFileSizeLow>HASH_BUDGET))return 0;
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE context=NULL;DWORD size=0,n=0;unsigned char* object=NULL;unsigned char* data=NULL;LARGE_INTEGER zero={0};
    int okay=SetFilePointerEx(file,zero,NULL,FILE_BEGIN) && BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0 && BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&n,0)>=0 && size && size<=65536;
    if(okay){object=malloc(size);data=malloc(65536);okay=object && data && BCryptCreateHash(algorithm,&context,object,size,NULL,0,0)>=0;}
    ULONGLONG total=0;
    while(okay){DWORD count=0;okay=ReadFile(file,data,65536,&count,NULL);if(!okay || !count)break;total+=count;if(budget)*budget+=count;okay=total<=BINARY_LIMIT && BCryptHashData(context,data,count,0)>=0;}
    okay=okay && total==info->nFileSizeLow && BCryptFinishHash(context,value->digest,sizeof(value->digest),0)>=0;
    if(okay){value->volume=info->dwVolumeSerialNumber;value->high=info->nFileIndexHigh;value->low=info->nFileIndexLow;value->size=info->nFileSizeLow;value->created=ticks(info->ftCreationTime);value->written=ticks(info->ftLastWriteTime);}
    if(context)BCryptDestroyHash(context);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    free(object);free(data);return okay;
}
void xideStartupCacheRelease(xide_startup_cache_lease* lease)
{
    if(!lease)return;
    if(lease->marker && lease->marker!=INVALID_HANDLE_VALUE)CloseHandle(lease->marker);
    if(lease->directory && lease->directory!=INVALID_HANDLE_VALUE)CloseHandle(lease->directory);
    if(lease->root && lease->root!=INVALID_HANDLE_VALUE)CloseHandle(lease->root);
    free(lease);
}
static int nameValid(const wchar_t* directory)
{
    const wchar_t* name=wcsrchr(directory,L'\\');if(!name)return 0;++name;
    size_t prefix=wcslen(PREFIX);if(wcslen(name)!=prefix+32 || wcsncmp(name,PREFIX,prefix))return 0;
    for(size_t i=prefix;i<prefix+32;++i)if(!((name[i]>=L'0' && name[i]<=L'9') || (name[i]>=L'a' && name[i]<=L'f')))return 0;
    return 1;
}
static int inventory(const wchar_t* directory,const wchar_t* program)
{
    wchar_t* pattern=join(directory,L"*");if(!pattern)return 0;
    WIN32_FIND_DATAW row={0};HANDLE search=FindFirstFileW(pattern,&row);free(pattern);if(search==INVALID_HANDLE_VALUE)return 0;
    int counts[3]={0},okay=1;
    do {
        if(!wcscmp(row.cFileName,L".") || !wcscmp(row.cFileName,L".."))continue;
        if(row.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)){okay=0;break;}
        int index=!_wcsicmp(row.cFileName,MARKER) ? 0 : (!_wcsicmp(row.cFileName,program) ? 1 : (!_wcsicmp(row.cFileName,L"xge.dll") ? 2 : -1));
        if(index<0 || ++counts[index]!=1){okay=0;break;}
    } while(FindNextFileW(search,&row));
    DWORD error=GetLastError();FindClose(search);return okay && error==ERROR_NO_MORE_FILES && counts[0]==1 && counts[1]==1 && counts[2]==1;
}
xide_startup_cache_lease* xideStartupCachePublish(const wchar_t* directory,const wchar_t* program,BY_HANDLE_FILE_INFORMATION* markerIdentity,DWORD* error)
{
    if(!directory || !program || !markerIdentity){if(error)*error=ERROR_INVALID_PARAMETER;return NULL;}
    memset(markerIdentity,0,sizeof(*markerIdentity));
    cache_record record={0};BY_HANDLE_FILE_INFORMATION rootInfo={0},directoryInfo={0};wchar_t* root=parent(directory);wchar_t* marker=join(directory,MARKER);
    xide_startup_cache_lease* lease=calloc(1,sizeof(*lease));const wchar_t* leaf=wcsrchr(program,L'\\');int okay=lease && root && marker && leaf && wcslen(leaf+1)<260 && nameValid(directory) && currentUser(record.sid,&record.sidBytes);
    if(okay){lease->root=ordinary(root,FILE_READ_ATTRIBUTES,1,&rootInfo);lease->directory=ordinary(directory,FILE_READ_ATTRIBUTES,1,&directoryInfo);okay=lease->root!=INVALID_HANDLE_VALUE && lease->directory!=INVALID_HANDLE_VALUE && ownerMatches(lease->directory,record.sid);}
    if(okay){memcpy(record.magic,magic,sizeof(magic));record.version=2;record.bytes=sizeof(record);wcscpy(record.program,leaf+1);record.rootVolume=rootInfo.dwVolumeSerialNumber;record.rootHigh=rootInfo.nFileIndexHigh;record.rootLow=rootInfo.nFileIndexLow;record.directoryVolume=directoryInfo.dwVolumeSerialNumber;record.directoryHigh=directoryInfo.nFileIndexHigh;record.directoryLow=directoryInfo.nFileIndexLow;record.created=ticks(directoryInfo.ftCreationTime);}
    for(int i=0;okay && i<2;++i){wchar_t* path=join(directory,i ? L"xge.dll" : leaf+1);BY_HANDLE_FILE_INFORMATION info={0};HANDLE file=path ? ordinary(path,GENERIC_READ,0,&info) : INVALID_HANDLE_VALUE;okay=file!=INVALID_HANDLE_VALUE && ownerMatches(file,record.sid) && digest(file,&info,&record.files[i],NULL);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);free(path);}
    if(okay){HANDLE output=CreateFileW(marker,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_OPEN_REPARSE_POINT,NULL);DWORD count=0;okay=output!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(output,markerIdentity);if(okay){record.markerVolume=markerIdentity->dwVolumeSerialNumber;record.markerHigh=markerIdentity->nFileIndexHigh;record.markerLow=markerIdentity->nFileIndexLow;}okay=okay && WriteFile(output,&record,sizeof(record),&count,NULL) && count==sizeof(record) && FlushFileBuffers(output);if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);}
    if(okay){BY_HANDLE_FILE_INFORMATION info={0};lease->marker=ordinary(marker,GENERIC_READ,0,&info);okay=lease->marker!=INVALID_HANDLE_VALUE && identity(&info,markerIdentity->dwVolumeSerialNumber,markerIdentity->nFileIndexHigh,markerIdentity->nFileIndexLow) && inventory(directory,record.program);}
    free(root);free(marker);
    if(!okay){if(error)*error=GetLastError() ? GetLastError() : ERROR_FILE_INVALID;xideStartupCacheRelease(lease);return NULL;}return lease;
}
static int readRecord(HANDLE marker,cache_record* record,const BY_HANDLE_FILE_INFORMATION* info)
{
    DWORD n=0;unsigned char sid[SECURITY_MAX_SID_SIZE];DWORD length=0;
    if(info->nFileSizeHigh || info->nFileSizeLow!=sizeof(*record) || !ReadFile(marker,record,sizeof(*record),&n,NULL) || n!=sizeof(*record))return 0;
    if(memcmp(record->magic,magic,sizeof(magic)) || record->version!=2 || record->bytes!=sizeof(*record) || record->reserved || !record->sidBytes || record->sidBytes>SECURITY_MAX_SID_SIZE || !record->program[0] || record->program[259] || wcspbrk(record->program,L"\\/:") || !wcscmp(record->program,L".") || !wcscmp(record->program,L"..") || !_wcsicmp(record->program,MARKER) || !_wcsicmp(record->program,L"xge.dll"))return 0;
    return identity(info,record->markerVolume,record->markerHigh,record->markerLow) && currentUser(sid,&length) && length==record->sidBytes && IsValidSid(record->sid) && GetLengthSid(record->sid)==length && EqualSid(sid,record->sid) && ownerMatches(marker,sid);
}
static void releaseChild(void) {xideStartupCacheRelease(childLease);childLease=NULL;}
int xideStartupCacheChildHold(const wchar_t* program,DWORD* error)
{
    if(!program){if(error)*error=ERROR_INVALID_PARAMETER;return 0;}
    wchar_t* directory=parent(program);wchar_t* root=directory ? parent(directory) : NULL;wchar_t* marker=directory ? join(directory,MARKER) : NULL;xide_startup_cache_lease* lease=calloc(1,sizeof(*lease));cache_record record={0};BY_HANDLE_FILE_INFORMATION a={0},d={0},m={0};int okay=!childLease && lease && root && marker && nameValid(directory);
    if(okay){lease->root=ordinary(root,FILE_READ_ATTRIBUTES,1,&a);lease->directory=ordinary(directory,FILE_READ_ATTRIBUTES,1,&d);lease->marker=ordinary(marker,GENERIC_READ,0,&m);okay=lease->root!=INVALID_HANDLE_VALUE && lease->directory!=INVALID_HANDLE_VALUE && lease->marker!=INVALID_HANDLE_VALUE && readRecord(lease->marker,&record,&m) && identity(&a,record.rootVolume,record.rootHigh,record.rootLow) && identity(&d,record.directoryVolume,record.directoryHigh,record.directoryLow) && record.created==ticks(d.ftCreationTime) && !_wcsicmp(wcsrchr(program,L'\\')+1,record.program) && inventory(directory,record.program);}
    for(int i=0;okay && i<2;++i){wchar_t* path=join(directory,i ? L"xge.dll" : record.program);BY_HANDLE_FILE_INFORMATION info={0};cache_file value={0};HANDLE file=path ? ordinary(path,GENERIC_READ,0,&info) : INVALID_HANDLE_VALUE;okay=file!=INVALID_HANDLE_VALUE && digest(file,&info,&value,NULL) && !memcmp(&value,&record.files[i],sizeof(value));if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);free(path);}
    free(root);free(directory);free(marker);
    if(!okay){xideStartupCacheRelease(lease);if(error)*error=ERROR_FILE_INVALID;return 0;}
    if(atexit(releaseChild)){xideStartupCacheRelease(lease);if(error)*error=ERROR_NOT_ENOUGH_MEMORY;return 0;}
    childLease=lease;return 1;
}
static int disposition(HANDLE file) {FILE_DISPOSITION_INFO info={TRUE};return SetFileInformationByHandle(file,FileDispositionInfo,&info,sizeof(info))!=0;}
int xideStartupCachePrune(const wchar_t* root,ULONGLONG now,xide_startup_cache_stats* stats)
{
    xide_startup_cache_stats local={0};if(!stats)stats=&local;memset(stats,0,sizeof(*stats));if(!root)return 0;BY_HANDLE_FILE_INFORMATION rootInfo={0};HANDLE rootHandle=ordinary(root,FILE_READ_ATTRIBUTES,1,&rootInfo);if(rootHandle==INVALID_HANDLE_VALUE)return 0;
    wchar_t* pattern=join(root,PREFIX L"*");WIN32_FIND_DATAW row={0};HANDLE search=pattern ? FindFirstFileW(pattern,&row) : INVALID_HANDLE_VALUE;free(pattern);
    if(search!=INVALID_HANDLE_VALUE)do {
        if(stats->scanned>=64 || stats->hashedBytes>=HASH_BUDGET)break;
        ++stats->scanned;wchar_t* directory=join(root,row.cFileName);wchar_t* marker=directory ? join(directory,MARKER) : NULL;BY_HANDLE_FILE_INFORMATION d={0},m={0};HANDLE dh=INVALID_HANDLE_VALUE,mh=INVALID_HANDLE_VALUE,files[2]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};cache_record record={0};int okay=directory && marker && nameValid(directory);
        if(okay){dh=ordinary(directory,DELETE|FILE_READ_ATTRIBUTES,1,&d);mh=ordinary(marker,GENERIC_READ|DELETE,0,&m);okay=dh!=INVALID_HANDLE_VALUE && mh!=INVALID_HANDLE_VALUE && readRecord(mh,&record,&m) && ownerMatches(dh,record.sid) && identity(&rootInfo,record.rootVolume,record.rootHigh,record.rootLow) && identity(&d,record.directoryVolume,record.directoryHigh,record.directoryLow) && record.created==ticks(d.ftCreationTime);}
        int recent=okay && (record.created>now || now-record.created<XIDE_STARTUP_CACHE_GRACE);if(recent)okay=0;
        if(okay)okay=inventory(directory,record.program);
        for(int i=0;okay && i<2;++i){wchar_t* path=join(directory,i ? L"xge.dll" : record.program);BY_HANDLE_FILE_INFORMATION info={0};cache_file value={0};files[i]=path ? ordinary(path,GENERIC_READ|DELETE,0,&info) : INVALID_HANDLE_VALUE;free(path);okay=files[i]!=INVALID_HANDLE_VALUE && ownerMatches(files[i],record.sid) && digest(files[i],&info,&value,&stats->hashedBytes) && !memcmp(&value,&record.files[i],sizeof(value));}
        /* Validate every member before any deletion. Handles deny replacement;
           unknown concurrent additions keep the directory nonempty. */
        if(okay)okay=inventory(directory,record.program);
        if(okay){okay=disposition(files[0]) && disposition(files[1]);if(okay)okay=disposition(mh);}
        for(int i=0;i<2;++i)if(files[i]!=INVALID_HANDLE_VALUE)CloseHandle(files[i]);
        if(mh!=INVALID_HANDLE_VALUE)CloseHandle(mh);
        if(okay)okay=disposition(dh);
        if(dh!=INVALID_HANDLE_VALUE)CloseHandle(dh);
        if(okay)++stats->removed;else {++stats->retained;if(!recent)++stats->rejected;}
        free(directory);free(marker);
    } while(FindNextFileW(search,&row));
    if(search!=INVALID_HANDLE_VALUE)FindClose(search);
    CloseHandle(rootHandle);return 1;
}
void xideStartupCacheMaintain(void)
{
    wchar_t root[32768];DWORD n=GetTempPathW(32768,root);FILETIME now;GetSystemTimeAsFileTime(&now);
    if(n && n<32768){while(n>3 && root[n-1]==L'\\')root[--n]=0;xideStartupCachePrune(root,ticks(now),NULL);}
}
