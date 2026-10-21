#include "platform/startup_stage.h"
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#define STARTUP_BINARY_LIMIT (64u*1024u*1024u)
static const wchar_t* keys[]={L"XLANG_IDE_STARTUP_ORIGIN",L"XLANG_IDE_STARTUP_EXE_SHA256",L"XLANG_IDE_STARTUP_DLL_SHA256"};
int xideStartupDirectoryBytes(const wchar_t* path)
{
    const wchar_t* slash=wcsrchr(path,L'\\');
    return slash ? WideCharToMultiByte(CP_ACP,0,path,(int)(slash-path+1),NULL,0,NULL,NULL) : 0;
}
static wchar_t* sibling(const wchar_t* file,const wchar_t* leaf)
{
    const wchar_t* slash=wcsrchr(file,L'\\'); size_t directory=slash ? (size_t)(slash-file+1) : 0,length=wcslen(leaf);
    if(!directory || directory+length>=32768) {SetLastError(ERROR_FILENAME_EXCED_RANGE);return NULL;}
    wchar_t* result=malloc((directory+length+1)*sizeof(*result));if(!result){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
    memcpy(result,file,directory*sizeof(*result));memcpy(result+directory,leaf,(length+1)*sizeof(*result));return result;
}
static HANDLE readFile(const wchar_t* path)
{
    HANDLE handle=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    BY_HANDLE_FILE_INFORMATION information={0};
    if(handle!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(handle,&information) || (information.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || information.nFileSizeHigh || information.nFileSizeLow>STARTUP_BINARY_LIMIT)) {
        CloseHandle(handle);SetLastError(ERROR_FILE_INVALID);return INVALID_HANDLE_VALUE;
    }
    return handle;
}
static int hash(HANDLE file,wchar_t hex[65],HANDLE output)
{
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE context=NULL;DWORD size=0,returned=0;
    unsigned char *object=NULL,*bytes=NULL,digest[32];LARGE_INTEGER zero={0};int okay=SetFilePointerEx(file,zero,NULL,FILE_BEGIN)!=0;
    okay=okay && BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0 && BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&returned,0)>=0 && size && size<=65536;
    if(okay) {object=malloc(size);bytes=malloc(65536);okay=object && bytes && BCryptCreateHash(algorithm,&context,object,size,NULL,0,0)>=0;}
    size_t total=0;
    while(okay) {
        DWORD count=0,written=0;okay=ReadFile(file,bytes,65536,&count,NULL)!=0;if(!okay || !count)break;
        total+=count;okay=total<=STARTUP_BINARY_LIMIT && BCryptHashData(context,bytes,count,0)>=0;
        if(okay && output!=INVALID_HANDLE_VALUE)okay=WriteFile(output,bytes,count,&written,NULL) && written==count;
    }
    okay=okay && BCryptFinishHash(context,digest,sizeof(digest),0)>=0;
    if(okay)for(int i=0;i<32;++i)swprintf(hex+2*i,3,L"%02x",digest[i]);
    if(context)BCryptDestroyHash(context);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    free(object);free(bytes);
    if(!okay)SetLastError(ERROR_INVALID_DATA);
    return okay;
}
static int matching(const wchar_t* path,const wchar_t* expected)
{
    HANDLE file=readFile(path);wchar_t value[65];int okay=file!=INVALID_HANDLE_VALUE && hash(file,value,INVALID_HANDLE_VALUE) && !wcscmp(value,expected);
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    if(!okay)SetLastError(ERROR_FILE_INVALID);
    return okay;
}
int xideStartupStageOrigin(const wchar_t* actual,wchar_t* origin,DWORD* error)
{
    DWORD lengths[3];for(int i=0;i<3;++i)lengths[i]=GetEnvironmentVariableW(keys[i],NULL,0);
    if(!lengths[0] && !lengths[1] && !lengths[2])return 0;
    wchar_t exe[65],dll[65];int okay=lengths[0]>1 && lengths[0]<=32768 && lengths[1]==65 && lengths[2]==65;
    okay=okay && GetEnvironmentVariableW(keys[0],origin,32768)==lengths[0]-1 && GetEnvironmentVariableW(keys[1],exe,65)==64 && GetEnvironmentVariableW(keys[2],dll,65)==64;
    for(int i=0;okay && i<64;++i)okay=((exe[i]>=L'0' && exe[i]<=L'9') || (exe[i]>=L'a' && exe[i]<=L'f')) && ((dll[i]>=L'0' && dll[i]<=L'9') || (dll[i]>=L'a' && dll[i]<=L'f'));
    wchar_t *sourceLibrary=okay ? sibling(origin,L"xge.dll") : NULL,*actualLibrary=okay ? sibling(actual,L"xge.dll") : NULL;
    okay=okay && sourceLibrary && actualLibrary && xideStartupDirectoryBytes(actual)>0 && xideStartupDirectoryBytes(actual)<256 && xideStartupDirectoryBytes(origin)>=256 && matching(origin,exe) && matching(actual,exe) && matching(sourceLibrary,dll) && matching(actualLibrary,dll);
    free(sourceLibrary);free(actualLibrary);if(okay)return 1;
    origin[0]=0;*error=ERROR_FILE_INVALID;return -1;
}
static int compareEnvironment(const void* a,const void* b)
{
    return _wcsicmp(*(const wchar_t* const*)a,*(const wchar_t* const*)b);
}
int xideStartupStageForgetOrigin(DWORD* error)
{
    for(int i=0;i<3;++i)if(!SetEnvironmentVariableW(keys[i],NULL)){*error=GetLastError();return 0;}
    return 1;
}
static wchar_t* environment(const wchar_t* origin,const wchar_t* exe,const wchar_t* dll)
{
    const wchar_t* values[]={origin,exe,dll};wchar_t* inherited=GetEnvironmentStringsW();if(!inherited)return NULL;
    size_t size=1;
    for(const wchar_t* row=inherited;*row;row+=wcslen(row)+1)size+=wcslen(row)+1;
    for(int i=0;i<3;++i)size+=wcslen(keys[i])+wcslen(values[i])+2;
    wchar_t* result=size<=1024u*1024u ? malloc(size*sizeof(*result)) : NULL;size_t at=0;
    if(result) {
        for(const wchar_t* row=inherited;*row;row+=wcslen(row)+1) {
            int reserved=0;for(int i=0;i<3;++i){size_t n=wcslen(keys[i]);if(!_wcsnicmp(row,keys[i],n) && row[n]==L'=')reserved=1;}
            if(!reserved) {size_t n=wcslen(row)+1;memcpy(result+at,row,n*sizeof(*result));at+=n;}
        }
        for(int i=0;i<3;++i){size_t n=wcslen(keys[i]),v=wcslen(values[i]);memcpy(result+at,keys[i],n*sizeof(*result));at+=n;result[at++]=L'=';memcpy(result+at,values[i],(v+1)*sizeof(*result));at+=v+1;}
        result[at]=0;
        size_t rows=0;for(const wchar_t* row=result;*row;row+=wcslen(row)+1)++rows;
        wchar_t** ordered=malloc(rows*sizeof(*ordered));wchar_t* sorted=malloc(size*sizeof(*sorted));
        if(ordered && sorted) {
            size_t i=0;for(wchar_t* row=result;*row;row+=wcslen(row)+1)ordered[i++]=row;
            qsort(ordered,rows,sizeof(*ordered),compareEnvironment);at=0;
            for(i=0;i<rows;++i){size_t n=wcslen(ordered[i])+1;memcpy(sorted+at,ordered[i],n*sizeof(*sorted));at+=n;}
            sorted[at]=0;free(result);result=sorted;
        } else {free(result);free(sorted);result=NULL;}
        free(ordered);
    }
    FreeEnvironmentStringsW(inherited);if(!result)SetLastError(ERROR_NOT_ENOUGH_MEMORY);return result;
}
static int sameIdentity(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{
    return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow;
}
static void deleteOwned(const wchar_t* path,const BY_HANDLE_FILE_INFORMATION* expected,int directory)
{
    if(!path)return;
    HANDLE handle=CreateFileW(path,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(directory ? FILE_FLAG_BACKUP_SEMANTICS : 0),NULL);
    if(handle==INVALID_HANDLE_VALUE)return;
    BY_HANDLE_FILE_INFORMATION actual={0};FILE_DISPOSITION_INFO disposition={TRUE};
    if(GetFileInformationByHandle(handle,&actual) && !(actual.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && sameIdentity(&actual,expected))SetFileInformationByHandle(handle,FileDispositionInfo,&disposition,sizeof(disposition));
    CloseHandle(handle);
}
void xideStartupStageClear(xide_startup_stage* stage)
{
    xideStartupCacheRelease(stage->cacheLease);stage->cacheLease=NULL;
    for(int i=0;i<2;++i) {if(stage->originals[i] && stage->originals[i]!=INVALID_HANDLE_VALUE)CloseHandle(stage->originals[i]);if(stage->copies[i] && stage->copies[i]!=INVALID_HANDLE_VALUE)CloseHandle(stage->copies[i]);}
    if(stage->created[0])deleteOwned(stage->program,&stage->identities[0],0);
    if(stage->created[1])deleteOwned(stage->library,&stage->identities[1],0);
    if(stage->cacheMarkerIdentity.nNumberOfLinks) {
        wchar_t* marker=sibling(stage->program,L".xide-startup");
        deleteOwned(marker,&stage->cacheMarkerIdentity,0);free(marker);
    }
    if(stage->directoryCreated)deleteOwned(stage->directory,&stage->directoryIdentity,1);
    free(stage->directory);free(stage->program);free(stage->library);free(stage->environment);memset(stage,0,sizeof(*stage));
}
int xideStartupStagePrepare(const wchar_t* original,xide_startup_stage* stage,DWORD* error)
{
    wchar_t temporary[32768],directory[32768],exe[65],dll[65];DWORD length=GetTempPathW(32768,temporary);
    int okay=length && length<32768;wchar_t* sourceLibrary=okay ? sibling(original,L"xge.dll") : NULL;
    if(okay && sourceLibrary) {stage->originals[0]=readFile(original);stage->originals[1]=readFile(sourceLibrary);okay=stage->originals[0]!=INVALID_HANDLE_VALUE && stage->originals[1]!=INVALID_HANDLE_VALUE;} else okay=0;
    int created=0;
    for(int attempt=0;okay && !created && attempt<8;++attempt) {
        unsigned char random[16];okay=BCryptGenRandom(NULL,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0;
        if(!okay)break;
        wchar_t name[64]=L"xlang-ide-start-v2-";for(int i=0;i<16;++i)swprintf(name+19+2*i,3,L"%02x",random[i]);
        int n=swprintf(directory,32768,L"%ls%ls",temporary,name);okay=n>0 && n<32768;
        if(okay) {wchar_t* full=malloc((wcslen(directory)+12)*sizeof(*full));if(full){swprintf(full,wcslen(directory)+12,L"%ls\\ide.exe",directory);okay=xideStartupDirectoryBytes(full)>0 && xideStartupDirectoryBytes(full)<256;if(!okay)SetLastError(ERROR_FILENAME_EXCED_RANGE);free(full);}else {okay=0;SetLastError(ERROR_NOT_ENOUGH_MEMORY);}}
        if(okay) {created=CreateDirectoryW(directory,NULL)!=0;if(!created && GetLastError()!=ERROR_ALREADY_EXISTS)okay=0;}
    }
    okay=okay && created;
    if(okay) {
        stage->directory=_wcsdup(directory);
        if(!stage->directory) {RemoveDirectoryW(directory);SetLastError(ERROR_NOT_ENOUGH_MEMORY);okay=0;}
        HANDLE handle=okay ? CreateFileW(directory,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE;
        if(handle!=INVALID_HANDLE_VALUE){stage->directoryCreated=GetFileInformationByHandle(handle,&stage->directoryIdentity)!=0;CloseHandle(handle);}
        okay=okay && stage->directoryCreated;
        wchar_t sentinel[32768];int n=okay ? swprintf(sentinel,32768,L"%ls\\placeholder",directory) : -1;okay=okay && n>0 && n<32768;
        const wchar_t* leaf=wcsrchr(original,L'\\');stage->program=okay && leaf ? sibling(sentinel,leaf+1) : NULL;stage->library=okay ? sibling(sentinel,L"xge.dll") : NULL;okay=okay && stage->program && stage->library;
    }
    const wchar_t* destinations[]={stage->program,stage->library};wchar_t* digests[]={exe,dll};
    for(int i=0;okay && i<2;++i) {
        HANDLE output=CreateFileW(destinations[i],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,NULL);
        stage->created[i]=output!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(output,&stage->identities[i]);
        okay=stage->created[i] && hash(stage->originals[i],digests[i],output) && FlushFileBuffers(output);
        if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);
        if(okay) {stage->copies[i]=readFile(destinations[i]);wchar_t observed[65];okay=stage->copies[i]!=INVALID_HANDLE_VALUE && hash(stage->copies[i],observed,INVALID_HANDLE_VALUE) && !wcscmp(observed,digests[i]);}
    }
    if(okay){stage->environment=environment(original,exe,dll);okay=stage->environment!=NULL;}
    if(okay){stage->cacheLease=xideStartupCachePublish(stage->directory,stage->program,&stage->cacheMarkerIdentity,error);okay=stage->cacheLease!=NULL;}
    DWORD failure=okay ? 0 : GetLastError();free(sourceLibrary);
    if(!okay){xideStartupStageClear(stage);*error=failure ? failure : ERROR_NOT_ENOUGH_MEMORY;return 0;}return 1;
}
