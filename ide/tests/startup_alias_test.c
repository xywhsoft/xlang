#include "platform/startup_alias.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static void json(const wchar_t* text)
{
    int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);char* bytes=length ? malloc((size_t)length) : NULL;
    if(!bytes || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,bytes,length,NULL,NULL))exit(3);
    putchar('"');for(int i=0;i<length-1;++i) {unsigned char c=(unsigned char)bytes[i];if(c=='"' || c=='\\')printf("\\%c",c);else if(c<32)printf("\\u%04x",c);else putchar(c);}putchar('"');free(bytes);
}
int main(void)
{
    wchar_t desktop[256];DWORD size=0;
    if(!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&size) || wcsncmp(desktop,L"XlangStartupAlias-",18))return 4;
    int code=0;unsigned long error=0;int handoff=xideStartupAliasMaybe(&code,&error);
    if(handoff>0)return code;
    if(handoff<0){fprintf(stderr,"startup alias failed: %lu\n",error);return 5;}
    wchar_t module[32768];DWORD length=GetModuleFileNameW(NULL,module,32768);if(!length || length>=32768)return 6;
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 7;
    wchar_t current[32768],environment[128];if(!GetCurrentDirectoryW(32768,current) || !GetEnvironmentVariableW(L"XIDE_STARTUP_ALIAS_TEST",environment,128))return 8;
    printf("{\"pid\":%lu,\"module\":",GetCurrentProcessId());json(module);printf(",\"resourceModule\":");json(xideStartupResourceModule(module));printf(",\"internalContextPresent\":%s",GetEnvironmentVariableW(L"XLANG_IDE_STARTUP_ORIGIN",NULL,0) || GetEnvironmentVariableW(L"XLANG_IDE_STARTUP_EXE_SHA256",NULL,0) || GetEnvironmentVariableW(L"XLANG_IDE_STARTUP_DLL_SHA256",NULL,0) ? "true" : "false");printf(",\"desktop\":");json(desktop);printf(",\"cwd\":");json(current);printf(",\"environment\":");json(environment);printf(",\"arguments\":[");
    int stream=0;
    for(int i=1;i<argc;++i){if(i>1)putchar(',');json(argv[i]);if(!wcsncmp(argv[i],L"--exit=",7))code=(int)wcstol(argv[i]+7,NULL,10);if(!wcscmp(argv[i],L"--stream-check"))stream=1;}
    printf("]");
    if(stream) {
        char input[128]={0};wchar_t wide[128];DWORD received=0;
        if(!ReadFile(GetStdHandle(STD_INPUT_HANDLE),input,sizeof(input)-1,&received,NULL) || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input,-1,wide,128))return 9;
        printf(",\"stdin\":");json(wide);fputs("startup-stream-check\n",stderr);fflush(stderr);
    }
    printf("}\n");fflush(stdout);LocalFree(argv);return code;
}
