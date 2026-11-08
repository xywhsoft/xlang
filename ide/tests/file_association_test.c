/* All writes are under one disposable HKCU subtree, never the real Classes. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
static int writeNumber,failWrite;
static LSTATUS WINAPI testWrite(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE* data,DWORD size)
{
    if(failWrite && ++writeNumber==failWrite)return ERROR_ACCESS_DENIED;
    return RegSetValueExW(key,name,reserved,type,data,size);
}
#define RegSetValueExW testWrite
#include "../src/platform/file_association.c"
#undef RegSetValueExW
static int checks;
#define CHECK(e) do { ++checks; if(!(e)){fprintf(stderr,"file_association_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;} }while(0)
static int seed(const wchar_t* prefix,const wchar_t* suffix,const wchar_t* name,const wchar_t* value)
{
    wchar_t key[32768];HKEY handle;path(key,prefix,suffix);
    if(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,NULL,0,KEY_ALL_ACCESS,NULL,&handle,NULL))return 0;
    LSTATUS code=RegSetValueExW(handle,name,0,REG_SZ,(const BYTE*)value,(DWORD)((wcslen(value)+1)*sizeof(wchar_t)));RegCloseKey(handle);return !code;
}
int main(void)
{
    int failed=0;xide_association_result result={0};wchar_t prefix[256],key[32768];HKEY handle=NULL;
    const wchar_t* exe=L"D:\\程序 带空格\\xlang IDE.exe";
    const wchar_t* moved=L"E:\\new location\\xlang_ide.exe";
    swprintf(prefix,256,L"Software\\xlangIDEAssociationTests\\run-%lu-%llu",(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    CHECK(seed(prefix,L"Software\\Classes\\.c",NULL,L"OtherC.Editor"));
    CHECK(seed(prefix,L"Software\\Classes\\.c\\OpenWithProgids",L"OtherC.Editor",L""));
    CHECK(seed(prefix,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.c\\UserChoice",L"ProgId",L"OtherC.Editor"));
    CHECK(seed(prefix,L"Software\\xlang\\IDE\\Capabilities",L"future",L"preserved"));
    CHECK(xideAssociationProbe(prefix,exe,&result)==0);
    for(int i=0;i<4;++i)CHECK(!result.registered[i] && !result.defaultKnown[i]);
    CHECK(xideAssociationRegister(prefix,L"relative.exe",7)==ERROR_INVALID_PARAMETER);
    CHECK(xideAssociationRegister(prefix,L"D:\\bad\"path.exe",7)==ERROR_INVALID_PARAMETER);
    CHECK(xideAssociationRegister(prefix,exe,16)==ERROR_INVALID_PARAMETER);
    /* Every registration write is a failure boundary; rollback restores prior values. */
    for(int i=1;i<=22;++i){
        failWrite=i;writeNumber=0;CHECK(xideAssociationRegister(prefix,exe,7)==ERROR_ACCESS_DENIED);failWrite=0;
        CHECK(xideAssociationProbe(prefix,exe,&result)==0);
        for(int t=0;t<4;++t)CHECK(!result.registered[t]);
        path(key,prefix,L"Software\\Classes\\xlang.Workspace");CHECK(RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_READ,&handle)==ERROR_FILE_NOT_FOUND);
        path(key,prefix,L"Software\\Classes\\.c");CHECK(equalString(key,NULL,L"OtherC.Editor"));
    }
    CHECK(xideAssociationRegister(prefix,exe,7)==0);
    CHECK(xideAssociationRegister(prefix,exe,7)==0);
    CHECK(xideAssociationProbe(prefix,exe,&result)==0);
    for(int i=0;i<4;++i)CHECK(result.registered[i]==(i<3));
    path(key,prefix,L"Software\\Classes\\xlang.Script\\shell\\open\\command");CHECK(equalString(key,NULL,L"\"D:\\程序 带空格\\xlang IDE.exe\" --open-file \"%1\""));
    path(key,prefix,L"Software\\Classes\\xlang.Workspace\\DefaultIcon");CHECK(equalString(key,NULL,L"\"D:\\程序 带空格\\xlang IDE.exe\",-203"));
    CHECK(xideAssociationProbe(prefix,moved,&result)==0 && !result.registered[0]);
    /* A moved installation cannot remove the registration still owned by the old path. */
    CHECK(xideAssociationUnregister(prefix,moved,7)==0);
    CHECK(xideAssociationProbe(prefix,exe,&result)==0 && result.registered[0]);
    CHECK(xideAssociationRegister(prefix,moved,15)==0);
    CHECK(xideAssociationProbe(prefix,moved,&result)==0);
    for(int i=0;i<4;++i)CHECK(result.registered[i]);
    CHECK(xideAssociationUnregister(prefix,moved,7)==0);
    CHECK(xideAssociationProbe(prefix,moved,&result)==0 && result.registered[3] && !result.registered[0]);
    CHECK(xideAssociationUnregister(prefix,moved,8)==0);
    CHECK(xideAssociationUnregister(prefix,moved,15)==0);
    path(key,prefix,L"Software\\Classes\\.c");CHECK(equalString(key,NULL,L"OtherC.Editor"));
    path(key,prefix,L"Software\\Classes\\.c\\OpenWithProgids");CHECK(equalString(key,L"OtherC.Editor",L""));
    path(key,prefix,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.c\\UserChoice");CHECK(equalString(key,L"ProgId",L"OtherC.Editor"));
    path(key,prefix,L"Software\\RegisteredApplications");CHECK(!equalString(key,L"xlang IDE",L"Software\\xlang\\IDE\\Capabilities"));
    path(key,prefix,L"Software\\xlang\\IDE\\Capabilities");CHECK(equalString(key,L"future",L"preserved"));
    /* Confirm applies the whole desired set, including an empty set. */
    CHECK(xideAssociationApply(prefix,moved,15)==0 && xideAssociationApply(prefix,moved,5)==0);
    CHECK(xideAssociationProbe(prefix,moved,&result)==0);
    for(int i=0;i<4;++i)CHECK(result.registered[i]==!!(5u&(1u<<i)));
    failWrite=1;writeNumber=0;CHECK(xideAssociationApply(prefix,moved,7)==ERROR_ACCESS_DENIED);failWrite=0;
    CHECK(xideAssociationProbe(prefix,moved,&result)==0);
    for(int i=0;i<4;++i)CHECK(result.registered[i]==!!(5u&(1u<<i)));
    CHECK(xideAssociationApply(prefix,moved,0)==0 && xideAssociationProbe(prefix,moved,&result)==0);
    for(int i=0;i<4;++i)CHECK(!result.registered[i]);
    CHECK(xideAssociationApply(prefix,moved,16)==ERROR_INVALID_PARAMETER);
    path(key,prefix,L"Software\\Classes\\.c");CHECK(equalString(key,NULL,L"OtherC.Editor"));
    /* Exercise the real effective-default COM query without any registration. */
    wchar_t actual[32768];CHECK(GetModuleFileNameW(NULL,actual,32768)>0);
    CHECK(xideAssociationProbe(NULL,actual,&result)==0);
cleanup:
    failWrite=0;
    /* prefix is the exact, owned absolute HKCU test key, constructed above. */
    if(wcsncmp(prefix,L"Software\\xlangIDEAssociationTests\\run-",wcslen(L"Software\\xlangIDEAssociationTests\\run-"))==0)RegDeleteTreeW(HKEY_CURRENT_USER,prefix);
    deleteEmptyKey(L"Software\\xlangIDEAssociationTests");
    if(!failed)printf("file_association_test passed: %d checks, 22 rollback boundaries, Unicode/spaces, move repair, C opt-in, unknown metadata and foreign defaults preserved; real read-only COM probe\n",checks);
    return failed;
}
