/* Controlled external application/launcher, not a substitute XRT provider.
 * Windows is a headless GUI-subsystem executable: no visible console/window.
 * POSIX is installed only as this test's private PATH entry named xdg-open. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command,int show)
{
    wchar_t marker[32768],expected[32768],actual[32768];
    (void)instance;(void)previous;(void)show;
    DWORD length=GetEnvironmentVariableW(L"XL6_ASSOCIATION_MARKER",marker,32768);
    DWORD target=GetEnvironmentVariableW(L"XL6_ASSOCIATION_TARGET",expected,32768);
    DWORD module=GetModuleFileNameW(NULL,actual,32768);
    if(!length||length>=32768||!target||target>=32768||!module||module>=32768||
        wcscmp(expected,actual)||*command)return 17;
    HANDLE file=CreateFileW(marker,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 18;
    const char stamp[]="controlled application reached\n";DWORD written=0;
    int ok=WriteFile(file,stamp,(DWORD)(sizeof(stamp)-1),&written,NULL)&&written==sizeof(stamp)-1;
    if(!CloseHandle(file))ok=0;
    return ok?7:19;
}
#else
int main(int argc,char** argv)
{
    const char* marker=getenv("XL6_ASSOCIATION_MARKER");
    const char* expected=getenv("XL6_ASSOCIATION_TARGET");
    if(!marker||!expected||argc!=2||strcmp(expected,argv[1]))return 17;
    FILE* file=fopen(marker,"wx");
    if(!file)return 18;
    const char stamp[]="controlled application reached\n";
    int ok=fwrite(stamp,1,sizeof(stamp)-1,file)==sizeof(stamp)-1;
    if(fclose(file))ok=0;
    return ok?7:19;
}
#endif
