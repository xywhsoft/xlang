#define COBJMACROS
#include "platform/file_association.h"
#include "platform/app_resources.h"
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
const wchar_t* const xideAssociationExtensions[]={L".xlw",L".xlp",L".xl",L".c"};
static const wchar_t* const classes[]={L"xlang.Workspace",L"xlang.Project",L"xlang.Script",L"xlang.CSource"};
static const wchar_t* const names[]={L"xlang 工作空间",L"xlang 工程",L"xlang 脚本",L"C 源文件（xlang IDE）"};
static const wchar_t* const arguments[]={L"--workspace",L"--project",L"--open-file",L"--open-file"};
static const int icons[]={XIDE_RESOURCE_WORKSPACE_ICON,XIDE_RESOURCE_PROJECT_ICON,XIDE_RESOURCE_SCRIPT_ICON,XIDE_RESOURCE_C_ICON};
static const GUID registrationClass={0x591209c7,0x767b,0x42b2,{0x9f,0xba,0x44,0xee,0x46,0x15,0xf2,0xc7}};
static const GUID registrationInterface={0x4e530b0a,0xe611,0x4c77,{0xa3,0xac,0x90,0x31,0xd0,0x22,0x28,0x1b}};
static wchar_t* duplicate(const wchar_t* s){size_t n=(wcslen(s)+1)*sizeof(wchar_t);wchar_t* p=malloc(n);if(p)memcpy(p,s,n);return p;}
static int path(wchar_t out[32768],const wchar_t* prefix,const wchar_t* rest)
{int n=swprintf(out,32768,L"%ls%ls%ls",prefix?prefix:L"",prefix&&*prefix?L"\\":L"",rest);return n>=0 && n<32768;}
static DWORD readValue(const wchar_t* key,const wchar_t* name,DWORD* type,BYTE** bytes,DWORD* size)
{
    HKEY h;LSTATUS code=RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE,&h);if(code)return (DWORD)code;
    DWORD n=0,t=0;code=RegQueryValueExW(h,name,NULL,&t,NULL,&n);
    if(!code && n>65536)code=ERROR_MORE_DATA;
    BYTE* p=!code?calloc((size_t)n+sizeof(wchar_t),1):NULL;if(!code && !p)code=ERROR_NOT_ENOUGH_MEMORY;
    if(!code)code=RegQueryValueExW(h,name,NULL,&t,p,&n);
    RegCloseKey(h);
    if(code){free(p);return (DWORD)code;}*type=t;*bytes=p;*size=n;return 0;
}
static int equalString(const wchar_t* key,const wchar_t* name,const wchar_t* expected)
{
    DWORD type=0,size=0;BYTE* p=NULL;DWORD e=readValue(key,name,&type,&p,&size);
    int equal=!e && type==REG_SZ && size==(wcslen(expected)+1)*sizeof(wchar_t) && !_wcsicmp((const wchar_t*)p,expected);free(p);return equal;
}
static int validInput(const wchar_t* prefix,const wchar_t* exe)
{
    if(!exe || wcslen(exe)>30000 || wcschr(exe,L'"'))return 0;
    int absolute=(wcslen(exe)>3 && exe[1]==L':' && (exe[2]==L'\\' || exe[2]==L'/')) || (exe[0]==L'\\' && exe[1]==L'\\');
    return absolute && (!prefix || (*prefix && wcslen(prefix)<20000));
}
static int probeString(const wchar_t* key,const wchar_t* name,const wchar_t* expected,DWORD* error)
{
    DWORD type=0,size=0;BYTE* bytes=NULL;
    DWORD code=readValue(key,name,&type,&bytes,&size);
    if(code && code!=ERROR_FILE_NOT_FOUND && code!=ERROR_PATH_NOT_FOUND && !*error)*error=code;
    int equal=!code && type==REG_SZ && size==(wcslen(expected)+1)*sizeof(wchar_t) && !_wcsicmp((const wchar_t*)bytes,expected);
    free(bytes);return equal;
}
typedef struct undo {wchar_t *key,*name,*value;BYTE* old;DWORD type,size;int existed;} undo;
typedef struct journal {undo entries[64];size_t count;wchar_t* created[128];size_t createdCount;} journal;
static void deleteEmptyKey(const wchar_t* key)
{
    HKEY handle;DWORD children=0,values=0;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS,&handle))return;
    LSTATUS code=RegQueryInfoKeyW(handle,NULL,NULL,NULL,&children,NULL,NULL,&values,NULL,NULL,NULL,NULL);RegCloseKey(handle);
    /* RegDeleteKey also deletes values; a non-recursive delete alone is unsafe. */
    if(!code && !children && !values)RegDeleteKeyW(HKEY_CURRENT_USER,key);
}
static DWORD createPath(journal* j,const wchar_t* key,HKEY* output)
{
    wchar_t* copy=duplicate(key);if(!copy)return ERROR_NOT_ENOUGH_MEMORY;DWORD code=0;
    for(wchar_t* end=copy;;++end){if(*end && *end!=L'\\')continue;wchar_t ch=*end;*end=0;
        HKEY h;DWORD disposition=0;code=(DWORD)RegCreateKeyExW(HKEY_CURRENT_USER,copy,0,NULL,0,KEY_SET_VALUE|KEY_QUERY_VALUE,NULL,&h,&disposition);
        if(code)break;
        if(disposition==REG_CREATED_NEW_KEY){
            if(j->createdCount==128 || !(j->created[j->createdCount]=duplicate(copy))){RegCloseKey(h);RegDeleteKeyW(HKEY_CURRENT_USER,copy);code=ERROR_NOT_ENOUGH_MEMORY;break;}++j->createdCount;
        }
        *end=ch;if(!ch){*output=h;break;}RegCloseKey(h);
    }free(copy);return code;
}
static DWORD writeValue(journal* j,const wchar_t* key,const wchar_t* name,const wchar_t* value)
{
    if(j->count==64)return ERROR_BUFFER_OVERFLOW;
    undo u={0};u.key=duplicate(key);u.name=duplicate(name?name:L"");u.value=duplicate(value);
    if(!u.key || !u.name || !u.value){free(u.key);free(u.name);free(u.value);return ERROR_NOT_ENOUGH_MEMORY;}
    DWORD code=readValue(key,name,&u.type,&u.old,&u.size);u.existed=code==0;
    if(code && code!=ERROR_FILE_NOT_FOUND && code!=ERROR_PATH_NOT_FOUND)goto failed;
    HKEY h;code=createPath(j,key,&h);if(code)goto failed;
    code=(DWORD)RegSetValueExW(h,name,0,REG_SZ,(const BYTE*)value,(DWORD)((wcslen(value)+1)*sizeof(wchar_t)));RegCloseKey(h);
    if(!code){j->entries[j->count++]=u;return 0;}
failed:free(u.key);free(u.name);free(u.value);free(u.old);return code;
}
static void finish(journal* j,int rollback)
{
    for(size_t i=j->count;i;--i){undo* u=&j->entries[i-1];
        if(rollback && equalString(u->key,u->name,u->value)){HKEY h;if(!RegOpenKeyExW(HKEY_CURRENT_USER,u->key,0,KEY_SET_VALUE,&h)){if(u->existed)RegSetValueExW(h,u->name,0,u->type,u->old,u->size);else RegDeleteValueW(h,u->name);RegCloseKey(h);}}
        free(u->key);free(u->name);free(u->value);free(u->old);
    }
    for(size_t i=j->createdCount;i;--i){if(rollback)deleteEmptyKey(j->created[i-1]);free(j->created[i-1]);}
}
static DWORD probe(const wchar_t* prefix,const wchar_t* exe,xide_association_result* r)
{
    if(!validInput(prefix,exe) || !r)return ERROR_INVALID_PARAMETER;
    memset(r,0,sizeof(*r));
    IApplicationAssociationRegistration* api=NULL;HRESULT initialized=E_FAIL;
    if(!prefix){initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);CoCreateInstance(&registrationClass,NULL,CLSCTX_INPROC_SERVER,&registrationInterface,(void**)&api);}
    wchar_t key[32768],rest[256],command[32768],icon[32768];
    for(int i=0;i<4;++i){
        swprintf(rest,256,L"Software\\Classes\\%ls\\shell\\open\\command",classes[i]);path(key,prefix,rest);
        swprintf(command,32768,L"\"%ls\" %ls \"%%1\"",exe,arguments[i]);r->registered[i]=probeString(key,NULL,command,&r->error);
        swprintf(rest,256,L"Software\\Classes\\%ls\\DefaultIcon",classes[i]);path(key,prefix,rest);swprintf(icon,32768,L"\"%ls\",-%d",exe,icons[i]);r->registered[i]&=probeString(key,NULL,icon,&r->error);
        swprintf(rest,256,L"Software\\Classes\\%ls",classes[i]);path(key,prefix,rest);r->registered[i]&=probeString(key,L"xlangOwner",exe,&r->error);
        swprintf(rest,256,L"Software\\Classes\\%ls\\OpenWithProgids",xideAssociationExtensions[i]);path(key,prefix,rest);r->registered[i]&=probeString(key,classes[i],L"",&r->error);
        path(key,prefix,L"Software\\xlang\\IDE\\Capabilities\\FileAssociations");r->registered[i]&=probeString(key,xideAssociationExtensions[i],classes[i],&r->error);
        path(key,prefix,L"Software\\xlang\\IDE\\Capabilities");r->registered[i]&=probeString(key,L"xlangOwner",exe,&r->error);
        path(key,prefix,L"Software\\RegisteredApplications");r->registered[i]&=probeString(key,L"xlang IDE",L"Software\\xlang\\IDE\\Capabilities",&r->error);
        if(api){LPWSTR current=NULL;HRESULT hr=IApplicationAssociationRegistration_QueryCurrentDefault(api,xideAssociationExtensions[i],AT_FILEEXTENSION,AL_EFFECTIVE,&current);r->defaultKnown[i]=SUCCEEDED(hr) || hr==HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION);r->isDefault[i]=SUCCEEDED(hr) && current && !_wcsicmp(current,classes[i]);CoTaskMemFree(current);}
    }
    if(api)IApplicationAssociationRegistration_Release(api);
    if(SUCCEEDED(initialized))CoUninitialize();
    return r->error;
}
static DWORD registerTypes(const wchar_t* prefix,const wchar_t* exe,unsigned mask)
{
    if(!validInput(prefix,exe) || !mask || mask>15)return ERROR_INVALID_PARAMETER;
    journal* j=calloc(1,sizeof(*j));if(!j)return ERROR_NOT_ENOUGH_MEMORY;DWORD code=0;wchar_t key[32768],rest[256],value[32768];
    for(int i=0;i<4 && !code;++i)if(mask&(1u<<i)){
        swprintf(rest,256,L"Software\\Classes\\%ls",classes[i]);path(key,prefix,rest);code=writeValue(j,key,NULL,names[i]);if(!code)code=writeValue(j,key,L"xlangOwner",exe);
        swprintf(rest,256,L"Software\\Classes\\%ls\\shell\\open\\command",classes[i]);path(key,prefix,rest);swprintf(value,32768,L"\"%ls\" %ls \"%%1\"",exe,arguments[i]);if(!code)code=writeValue(j,key,NULL,value);
        swprintf(rest,256,L"Software\\Classes\\%ls\\DefaultIcon",classes[i]);path(key,prefix,rest);swprintf(value,32768,L"\"%ls\",-%d",exe,icons[i]);if(!code)code=writeValue(j,key,NULL,value);
        swprintf(rest,256,L"Software\\Classes\\%ls\\OpenWithProgids",xideAssociationExtensions[i]);path(key,prefix,rest);if(!code)code=writeValue(j,key,classes[i],L"");
        path(key,prefix,L"Software\\xlang\\IDE\\Capabilities\\FileAssociations");if(!code)code=writeValue(j,key,xideAssociationExtensions[i],classes[i]);
    }
    path(key,prefix,L"Software\\xlang\\IDE\\Capabilities");if(!code)code=writeValue(j,key,L"ApplicationName",L"xlang IDE");if(!code)code=writeValue(j,key,L"ApplicationDescription",L"xlang 工作空间、工程和源文件编辑器");if(!code)code=writeValue(j,key,L"xlangOwner",exe);
    path(key,prefix,L"Software\\RegisteredApplications");if(!code)code=writeValue(j,key,L"xlang IDE",L"Software\\xlang\\IDE\\Capabilities");
    finish(j,code!=0);free(j);if(!code && !prefix)SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);return code;
}
static DWORD deleteOwnedValue(const wchar_t* key,const wchar_t* name,const wchar_t* expected)
{
    DWORD error=0;if(!probeString(key,name,expected,&error))return error;
    HKEY handle;LSTATUS code=RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_SET_VALUE,&handle);
    if(!code){code=RegDeleteValueW(handle,name);RegCloseKey(handle);}
    return code==ERROR_FILE_NOT_FOUND?0:(DWORD)code;
}
static DWORD unregisterTypes(const wchar_t* prefix,const wchar_t* exe,unsigned mask)
{
    if(!validInput(prefix,exe) || !mask || mask>15)return ERROR_INVALID_PARAMETER;
    wchar_t key[32768],rest[256];DWORD error=0;
    for(int i=0;i<4;++i)if(mask&(1u<<i)){
        swprintf(rest,256,L"Software\\Classes\\%ls",classes[i]);path(key,prefix,rest);if(!probeString(key,L"xlangOwner",exe,&error))continue;
        LSTATUS e=RegDeleteTreeW(HKEY_CURRENT_USER,key);if(e && e!=ERROR_FILE_NOT_FOUND){error=(DWORD)e;continue;}
        swprintf(rest,256,L"Software\\Classes\\%ls\\OpenWithProgids",xideAssociationExtensions[i]);path(key,prefix,rest);e=deleteOwnedValue(key,classes[i],L"");if(e && !error)error=(DWORD)e;
        path(key,prefix,L"Software\\xlang\\IDE\\Capabilities\\FileAssociations");e=deleteOwnedValue(key,xideAssociationExtensions[i],classes[i]);if(e && !error)error=(DWORD)e;
    }
    /* Keep shared capabilities while any remaining type is registered. */
    int remains=0;path(key,prefix,L"Software\\xlang\\IDE\\Capabilities\\FileAssociations");
    for(int i=0;i<4;++i)if(equalString(key,xideAssociationExtensions[i],classes[i]))remains=1;
    path(key,prefix,L"Software\\xlang\\IDE\\Capabilities");
    if(!remains && equalString(key,L"xlangOwner",exe)){
        DWORD e=deleteOwnedValue(key,L"ApplicationName",L"xlang IDE");if(e && !error)error=e;
        e=deleteOwnedValue(key,L"ApplicationDescription",L"xlang 工作空间、工程和源文件编辑器");if(e && !error)error=e;
        e=deleteOwnedValue(key,L"xlangOwner",exe);if(e && !error)error=e;
        wchar_t associations[32768];path(associations,prefix,L"Software\\xlang\\IDE\\Capabilities\\FileAssociations");deleteEmptyKey(associations);deleteEmptyKey(key);
        path(key,prefix,L"Software\\RegisteredApplications");e=deleteOwnedValue(key,L"xlang IDE",L"Software\\xlang\\IDE\\Capabilities");if(e && !error)error=e;
    }
    if(!prefix)SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
    return error;
}
void xideAssociationOpenDefaults(void)
{
    HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    HINSTANCE opened=ShellExecuteW(NULL,L"open",L"ms-settings:defaultapps?registeredAppUser=xlang%20IDE",NULL,NULL,SW_SHOWNORMAL);
    if((INT_PTR)opened<=32)ShellExecuteW(NULL,L"open",L"ms-settings:defaultapps",NULL,NULL,SW_SHOWNORMAL);
    if(SUCCEEDED(initialized))CoUninitialize();
}
/* Serialize our own IDE instances so one failed registration cannot roll back
 * a second instance's completed writes. Waiting only happens on a worker. */
static DWORD acquire(HANDLE* mutex)
{
    *mutex=CreateMutexW(NULL,FALSE,L"Local\\xlang.IDE.FileAssociations.v1");
    if(!*mutex)return GetLastError();
    DWORD wait=WaitForSingleObject(*mutex,10000);
    if(wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED)return 0;
    DWORD error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();CloseHandle(*mutex);*mutex=NULL;return error;
}
DWORD xideAssociationProbe(const wchar_t* prefix,const wchar_t* exe,xide_association_result* result)
{
    HANDLE mutex=NULL;DWORD error=acquire(&mutex);if(error)return error;
    error=probe(prefix,exe,result);ReleaseMutex(mutex);CloseHandle(mutex);return error;
}
DWORD xideAssociationRegister(const wchar_t* prefix,const wchar_t* exe,unsigned mask)
{
    HANDLE mutex=NULL;DWORD error=acquire(&mutex);if(error)return error;
    error=registerTypes(prefix,exe,mask);ReleaseMutex(mutex);CloseHandle(mutex);return error;
}
DWORD xideAssociationUnregister(const wchar_t* prefix,const wchar_t* exe,unsigned mask)
{
    HANDLE mutex=NULL;DWORD error=acquire(&mutex);if(error)return error;
    error=unregisterTypes(prefix,exe,mask);ReleaseMutex(mutex);CloseHandle(mutex);return error;
}
DWORD xideAssociationApply(const wchar_t* prefix,const wchar_t* exe,unsigned mask)
{
    if(!validInput(prefix,exe) || mask>15)return ERROR_INVALID_PARAMETER;
    HANDLE mutex=NULL;DWORD error=acquire(&mutex);if(error)return error;
    /* Prepare checked entries before removing unchecked entries. A failed
       registration retains its existing rollback behavior. */
    if(mask)error=registerTypes(prefix,exe,mask);
    unsigned remove=(~mask)&15u;
    if(!error && remove)error=unregisterTypes(prefix,exe,remove);
    ReleaseMutex(mutex);CloseHandle(mutex);return error;
}
