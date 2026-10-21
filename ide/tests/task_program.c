#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
static char* utf8(const wchar_t* text)
{
    int count=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL); char* value=malloc((size_t)count);
    if(value) WideCharToMultiByte(CP_UTF8,0,text,-1,value,count,NULL,NULL);
    return value;
}
static void emit(const char* text)
{
    DWORD written; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)strlen(text),&written,NULL);
}
int main(void)
{
    int count=0; wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!arguments) return 1;
    for(int i=1;i<count;++i) { char* text=utf8(arguments[i]); if(!text) return 2; emit("ARG["); emit(text); emit("]\n"); free(text); }
    LocalFree(arguments);
    wchar_t current[32768],mode[128]={0}; GetCurrentDirectoryW(32768,current); GetEnvironmentVariableW(L"XIDE_TASK_MODE",mode,128);
    char* directory=utf8(current); if(!directory) return 3; emit("DIR["); emit(directory); emit("]\n"); free(directory);
    emit("RUN_STARTED\n");
    if(!wcscmp(mode,L"flood")) {
        const char* line="中文😀\n"; char bytes[11264]; size_t length=strlen(line);
        for(size_t i=0;i<sizeof(bytes);i+=length) memcpy(bytes+i,line,length);
        for(int i=0;i<48;++i) { DWORD written; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes,sizeof(bytes),&written,NULL); }
        unsigned char tail[]={0xff,0,'\n'}; DWORD written; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),tail,sizeof(tail),&written,NULL); return 0;
    }
    if(!wcscmp(mode,L"wait")) { Sleep(60000); return 0; }
    if(!wcscmp(mode,L"fail")) return 13;
    char input[1024]; DWORD size,written;
    while(ReadFile(GetStdHandle(STD_INPUT_HANDLE),input,sizeof(input),&size,NULL) && size) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),input,size,&written,NULL);
    return 0;
}
