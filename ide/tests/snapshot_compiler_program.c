/* Controlled compiler responses for snapshot validation and cancellation. */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char** argv)
{
 char mode[128]={0}; FILE* f=fopen("snapshot-mode.txt","rb"); if(f) { fread(mode,1,sizeof(mode)-1,f); fclose(f); }
 if(argc==2 && !strcmp(argv[1],"--help")) { printf(strstr(mode,"unsupported") ? "xl check <file.xl> [--json]\n" : "xl check <file.xl> [--json] [--source-overlay <path> <snapshot>] [--source-report]%s%s\n",strstr(mode,"indexed") ? " [--semantic-index]" : "",strstr(mode,"completionfake") ? " [--completion-atlas <path> <offset>]" : strstr(mode,"completion") ? " [--completion-at <path> <offset>]" : ""); return 0; }
 if(argc<2 || strcmp(argv[1],"check")) return 2;
 HANDLE held=INVALID_HANDLE_VALUE;
 if(strstr(mode,"hold")) {
  int count=0; wchar_t** words=CommandLineToArgvW(GetCommandLineW(),&count);
  if(words) { for(int i=1;i+2<count;++i) if(!wcscmp(words[i],L"--source-overlay")) { held=CreateFileW(words[i+2],GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); break; } LocalFree(words); }
  if(held==INVALID_HANDLE_VALUE) return 2;
 }
 f=fopen("snapshot-signal.txt","wb"); if(f) { fputs(held!=INVALID_HANDLE_VALUE ? "held" : "started",f); fclose(f); }
 if(strstr(mode,"slow")) Sleep(1500);
 if(held!=INVALID_HANDLE_VALUE) CloseHandle(held);
 f=fopen("snapshot-response.json","rb"); if(!f) return 2;
 char bytes[8192]; size_t n; while((n=fread(bytes,1,sizeof(bytes),f))!=0) fwrite(bytes,1,n,stdout); fclose(f);
 return strstr(mode,"exit0") ? 0 : strstr(mode,"exit2") ? 2 : 1;
}
