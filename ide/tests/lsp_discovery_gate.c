/* Test-only link wrapper. Holds a real opened directory until the fixture
 * releases it, proving owner responsiveness and the physical lease barrier. */
#include "app_xrt.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static volatile LONG chosen;
extern xdir __real_xrtDirOpen(cstr,uint32);
xdir __wrap_xrtDirOpen(cstr path,uint32 flags)
{
 xdir directory=__real_xrtDirOpen(path,flags);
 const char* prefix=getenv("XLANG_TEST_WORKSPACE_GATE");
 if(directory && prefix && *prefix && InterlockedCompareExchange(&chosen,1,0)==0){
  wchar_t entered[4096],released[4096];char name[4096];
  int n=snprintf(name,sizeof(name),"%s.entered",prefix);
  if(n>0 && (size_t)n<sizeof(name) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,entered,4096)){
   n=snprintf(name,sizeof(name),"%s.release",prefix);
   if(n>0 && (size_t)n<sizeof(name) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,released,4096)){
    HANDLE file=CreateFileW(entered,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE){char value[128];DWORD written=0;int size=snprintf(value,sizeof(value),"{\"thread\":%lu,\"realDirectoryOpened\":true}\n",GetCurrentThreadId());
     int okay=size>0 && (size_t)size<sizeof(value) && WriteFile(file,value,(DWORD)size,&written,NULL) && written==(DWORD)size && FlushFileBuffers(file);CloseHandle(file);
     if(okay){ULONGLONG start=GetTickCount64();while(GetFileAttributesW(released)==INVALID_FILE_ATTRIBUTES && GetTickCount64()-start<30000)Sleep(5);}
    }
   }
  }
 }
 return directory;
}
