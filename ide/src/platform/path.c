#define _WIN32_WINNT 0x0600
#include "platform/path.h"
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static wchar_t* wide(const char* text)
{
    if (!text) return NULL;
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!size) return NULL;
    wchar_t* result = malloc((size_t)size * sizeof(*result));
    if (result && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, size)) { free(result); result=NULL; }
    return result;
}
static char* utf8(const wchar_t* text)
{
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
    if (!size) return NULL;
    char* result = malloc((size_t)size);
    if (result && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result, size, NULL, NULL)) { free(result); result=NULL; }
    if (result) for (char* i=result; *i; ++i) if (*i=='\\') *i='/';
    return result;
}
static char* duplicate(const char* text)
{
    size_t size=strlen(text)+1; char* result=malloc(size);
    if (result) memcpy(result,text,size);
    return result;
}
static wchar_t* nativeWide(const wchar_t* path)
{
    if(!path || !*path) return NULL;
    DWORD needed=GetFullPathNameW(path,0,NULL,NULL);
    wchar_t* full=needed && needed<32760 ? malloc(((size_t)needed+1)*sizeof(*full)) : NULL;
    if(!full) return NULL;
    DWORD count=GetFullPathNameW(path,needed+1,full,NULL);
    if(!count || count>needed) { free(full); return NULL; }
    for(wchar_t* p=full;*p;++p) if(*p=='/') *p='\\';
    if(!wcsncmp(full,L"\\\\?\\",4)) return full;
    if(!wcsncmp(full,L"\\\\.\\",4)) { free(full); return NULL; }
    int unc=full[0]=='\\' && full[1]=='\\';
    if(!unc && !(count>=3 && full[1]==':' && full[2]=='\\')) { free(full); return NULL; }
    size_t skip=unc ? 2u : 0u,prefix=unc ? 8u : 4u;
    wchar_t* result=malloc((prefix+count-skip+1)*sizeof(*result));
    if(result) { memcpy(result,unc ? L"\\\\?\\UNC\\" : L"\\\\?\\",prefix*sizeof(*result)); memcpy(result+prefix,full+skip,(count-skip+1)*sizeof(*result)); }
    free(full); return result;
}
wchar_t* xidePathNativeWide(const char* path)
{ wchar_t* input=wide(path); wchar_t* result=input ? nativeWide(input) : NULL; free(input); return result; }
char* xidePathNativeUtf8(const char* path)
{
    wchar_t* input=xidePathNativeWide(path);
    int count=input ? WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input,-1,NULL,0,NULL,NULL) : 0;
    char* result=count ? malloc((size_t)count) : NULL;
    if(result && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input,-1,result,count,NULL,NULL)) { free(result); result=NULL; }
    free(input); return result;
}
static int separator(char c) { return c=='/' || c=='\\'; }
static int driveLetter(char c) { return (c>='A' && c<='Z') || (c>='a' && c<='z'); }
static char volumeDrive(const char* path)
{
    if(driveLetter(path[0]) && path[1]==':') return path[0];
    return 0;
}
static size_t rootLength(const char* path)
{
    if(driveLetter(path[0]) && path[1]==':' && path[2]=='/') return 3;
    if(path[0]=='/' && path[1]=='/') {
        size_t start=2;
        if(!strncmp(path,"//?/",4)) {
            if(driveLetter(path[4]) && path[5]==':' && path[6]=='/') return 7;
            if(!strncmp(path+4,"UNC/",4) || !strncmp(path+4,"unc/",4)) start=8;
            else { const char* end=strchr(path+4,'/'); return end ? (size_t)(end-path+1) : strlen(path); }
        }
        const char* server=strchr(path+start,'/'); if(!server) return strlen(path);
        const char* share=strchr(server+1,'/'); return share ? (size_t)(share-path+1) : strlen(path);
    }
    return path[0]=='/' ? 1 : 0;
}
static char* appendPath(const char* base,size_t length,const char* tail)
{
    size_t size=strlen(tail); if(length>SIZE_MAX-size-2) return NULL;
    char* joined=malloc(length+size+2); if(!joined) return NULL;
    memcpy(joined,base,length);
    if(length && !separator(base[length-1])) joined[length++]='/';
    memcpy(joined+length,tail,size+1); return joined;
}
/* A different drive uses the inherited =X: entry, or its root when absent.
   GetFullPathName("X:") in the IDE would incorrectly use the IDE's own CWD
   when that drive is current there but not in the child. */
static char* inheritedDriveDirectory(char drive)
{
    wchar_t name[]={L'=',(wchar_t)toupper((unsigned char)drive),L':',0};
    DWORD count=GetEnvironmentVariableW(name,NULL,0); wchar_t* value=count ? malloc((size_t)count*sizeof(*value)) : NULL;
    char* directory=NULL;
    if(value) {
        DWORD read=GetEnvironmentVariableW(name,value,count);
        if(read>=count) { free(value); return NULL; }
        if(read) { directory=utf8(value); if(!directory) { free(value); return NULL; } }
        free(value);
    } else if(count) return NULL;
    if(directory && driveLetter(directory[0]) && directory[1]==':' && directory[2]=='/' &&
       toupper((unsigned char)directory[0])==toupper((unsigned char)drive)) return directory;
    free(directory); char root[]={drive,':','/',0}; return duplicate(root);
}
static char* resolve(const char* base,const char* path,int canonicalName);
static char* contextualPath(const char* base,const char* path)
{
    if(!base || !*base || (separator(path[0]) && separator(path[1])) ||
       (driveLetter(path[0]) && path[1]==':' && separator(path[2]))) return duplicate(path);
    char* directory=resolve(NULL,base,0); if(!directory) return NULL;
    char* joined=NULL;
    if(separator(path[0])) {
        /* Win32 keeps an extended CWD as a device spelling. Its rooted-relative
           result starts with two separators; it does not borrow the embedded
           drive. Keep the same result as the actual child's GetFullPathName. */
        if(!strncmp(directory,"//?/",4)) joined=appendPath("//",2,path+1);
        else joined=appendPath(directory,rootLength(directory),path+1);
    }
    else if(driveLetter(path[0]) && path[1]==':') {
        char drive=volumeDrive(directory);
        if(!drive || toupper((unsigned char)drive)!=toupper((unsigned char)path[0])) {
            free(directory); directory=inheritedDriveDirectory(path[0]);
        }
        if(directory) joined=appendPath(directory,strlen(directory),path[2] ? path+2 : ".");
    } else joined=appendPath(directory,strlen(directory),path);
    free(directory); return joined;
}
static HANDLE openIdentity(const wchar_t* path)
{
    wchar_t* native=nativeWide(path);
    HANDLE h=native ? CreateFileW(native,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE;
    free(native); return h;
}
static wchar_t* finalName(const wchar_t* path)
{
    HANDLE handle=openIdentity(path);
    if(handle==INVALID_HANDLE_VALUE) return NULL;
    DWORD size=GetFinalPathNameByHandleW(handle,NULL,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    wchar_t* result=size ? malloc(((size_t)size+1)*sizeof(*result)) : NULL;
    if(result && (!GetFinalPathNameByHandleW(handle,result,size+1,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS))) { free(result); result=NULL; }
    CloseHandle(handle);
    if(result && !wcsncmp(result,L"\\\\?\\UNC\\",8)) {
        size_t length=wcslen(result+8); memmove(result+2,result+8,(length+1)*sizeof(*result)); result[0]=result[1]='\\';
    } else if(result && !wcsncmp(result,L"\\\\?\\",4)) memmove(result,result+4,(wcslen(result+4)+1)*sizeof(*result));
    return result;
}
static char* resolve(const char* base, const char* path,int canonicalName)
{
    if(!path || !*path) return NULL;
    char* joined=contextualPath(base,path);
    wchar_t* input=wide(joined); free(joined); if(!input) return NULL;
    DWORD needed=GetFullPathNameW(input,0,NULL,NULL);
    wchar_t* full=needed ? malloc(((size_t)needed+1)*sizeof(*full)) : NULL;
    if(full) { DWORD count=GetFullPathNameW(input,needed+1,full,NULL); if(!count || count>needed) { free(full); full=NULL; } }
    free(input); if(!full) return NULL;
    wchar_t* canonical=canonicalName ? finalName(full) : NULL;
    char* result=utf8(canonical ? canonical : full);
    free(canonical); free(full);
    if(result) {
        size_t size=strlen(result);
        while(size>3 && result[size-1]=='/') result[--size]='\0';
    }
    return result;
}
char* xidePathResolve(const char* base,const char* path) { return resolve(base,path,1); }
char* xidePathResolveLexical(const char* base,const char* path) { return resolve(base,path,0); }
const char* xidePathFilename(const char* path)
{
    if(!path) return "";
    const char* result=path;
    for(const char* i=path;*i;++i) if(*i=='/' || *i=='\\') result=i+1;
    return result;
}
char* xidePathDirectory(const char* path)
{
    if(!path) return NULL;
    const char* name=xidePathFilename(path); size_t size=name==path ? 1 : (size_t)(name-path-1);
    if(name==path) return duplicate(".");
    if(size==2 && path[1]==':') size=3;
    if(size==0) size=1;
    char* result=malloc(size+1); if(result) { memcpy(result,path,size); result[size]='\0'; }
    return result;
}
int xidePathExists(const char* path)
{
    wchar_t* input=xidePathNativeWide(path); if(!input) return 0;
    DWORD attributes=GetFileAttributesW(input); free(input);
    return attributes!=INVALID_FILE_ATTRIBUTES;
}
int xidePathIsDirectory(const char* path)
{
    wchar_t* input=xidePathNativeWide(path); if(!input) return 0;
    DWORD attributes=GetFileAttributesW(input); free(input);
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
}
int xidePathEqual(const char* left,const char* right)
{
    if(!left || !right) return 0;
    if(!strcmp(left,right)) return 1;
    wchar_t *a=wide(left),*b=wide(right); if(!a || !b) { free(a); free(b); return 0; }
    HANDLE first=openIdentity(a),second=openIdentity(b);
    int result=0;
    if(first!=INVALID_HANDLE_VALUE && second!=INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION x,y;
        if(GetFileInformationByHandle(first,&x) && GetFileInformationByHandle(second,&y))
            result=x.dwVolumeSerialNumber==y.dwVolumeSerialNumber && x.nFileIndexHigh==y.nFileIndexHigh && x.nFileIndexLow==y.nFileIndexLow;
    } else {
        /* Missing paths have no physical identity. Keep case distinct rather
         * than incorrectly collapsing a case-sensitive directory's entries. */
        result=CompareStringOrdinal(a,-1,b,-1,FALSE)==CSTR_EQUAL;
    }
    if(first!=INVALID_HANDLE_VALUE) CloseHandle(first);
    if(second!=INVALID_HANDLE_VALUE) CloseHandle(second);
    free(a); free(b); return result;
}
int xidePathLexicalEqual(const char* left,const char* right)
{
    char *a=xidePathResolveLexical(NULL,left),*b=xidePathResolveLexical(NULL,right);
    wchar_t *wa=wide(a),*wb=wide(b);
    int equal=wa && wb && CompareStringOrdinal(wa,-1,wb,-1,TRUE)==CSTR_EQUAL;
    free(a); free(b); free(wa); free(wb); return equal;
}
char* xidePathRelative(const char* base,const char* path)
{
    if(!base || !path) return NULL;
    char *a=xidePathResolve(NULL,base),*b=xidePathResolve(NULL,path);
    if(!a || !b) { free(a); free(b); return NULL; }
    size_t rootA=rootLength(a),rootB=rootLength(b);
    if(rootA!=rootB || strncmp(a,b,rootA)) { free(a); return b; }
    size_t ai=rootA,bi=rootB;
    while(a[ai] && b[bi]) {
        size_t ae=ai,be=bi;
        while(a[ae] && a[ae]!='/') ++ae;
        while(b[be] && b[be]!='/') ++be;
        if(ae-ai!=be-bi || memcmp(a+ai,b+bi,ae-ai)) break;
        ai=a[ae]=='/' ? ae+1 : ae; bi=b[be]=='/' ? be+1 : be;
    }
    size_t parents=0;
    for(size_t i=ai;a[i];++i) if(i==ai || a[i-1]=='/') ++parents;
    size_t rest=strlen(b+bi);
    if(parents>(SIZE_MAX-rest-2)/3) { free(a); free(b); return NULL; }
    char* result=malloc(parents*3+rest+2);
    if(result) {
        char* out=result;
        for(size_t i=0;i<parents;++i) { memcpy(out,"../",3); out+=3; }
        memcpy(out,b+bi,rest+1);
        if(out==result && !rest) { result[0]='.'; result[1]='\0'; }
        else if(!rest && out>result) out[-1]='\0';
    }
    free(a); free(b); return result;
}
int xidePathHasExtension(const char* path,const char* extension)
{
    const char* name=xidePathFilename(path),*dot=strrchr(name,'.');
    if(!dot || !extension) return 0;
    while(*dot && *extension) if(tolower((unsigned char)*dot++)!=tolower((unsigned char)*extension++)) return 0;
    return !*dot && !*extension;
}
char* xideCreateId(const char* prefix)
{
    GUID guid; if(!prefix || FAILED(CoCreateGuid(&guid))) return NULL;
    char buffer[80]; snprintf(buffer,sizeof(buffer),"%s-%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",prefix,
        (unsigned long)guid.Data1,guid.Data2,guid.Data3,guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    return duplicate(buffer);
}
