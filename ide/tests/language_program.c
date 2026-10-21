/* Controlled compiler/server fixture; never loaded into the IDE process. */
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void writeText(const wchar_t* path,const char* text) { HANDLE f=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,0,NULL); if(f!=INVALID_HANDLE_VALUE) { DWORD n=0; WriteFile(f,text,(DWORD)strlen(text),&n,NULL); CloseHandle(f); } }
static void quote(const char* s) { putchar('"'); while(*s) { if(*s=='"' || *s=='\\') putchar('\\'); if((unsigned char)*s<32) printf("\\u%04x",(unsigned char)*s); else putchar((unsigned char)*s); ++s; } putchar('"'); }
int main(void)
{
 char mode[80]="good"; FILE* f=fopen("language-mode.txt","rb"); if(f) { fgets(mode,sizeof(mode),f); fclose(f); }
 int argc=0; wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc); if(!argv) return 2; int code=0;
 if(argc==3 && !wcscmp(argv[1],L"--compiler")) { if(!strcmp(mode,"server-partial")) fputs("Content-Length: 99\r\n\r\n{}",stdout); else fputs("Content-Length: 2\r\n\r\n[]",stdout); fputs("isolated stderr warning\n",stderr); code=0; }
 else if(argc==2 && !wcscmp(argv[1],L"--help")) {
  if(!strcmp(mode,"unsupported")) puts("xl build <file.xlp> -o output.exe"); else puts("xl check <file.xl> [--json]");
 } else if(argc==4 && !wcscmp(argv[1],L"check")) {
  writeText(L"language-signal.txt","checking"); if(!strcmp(mode,"slow") || !strcmp(mode,"slow-bad")) Sleep(3000);
  if(!strcmp(mode,"change-entry")) writeText(argv[2],"int changed = 0\n");
  if(!strcmp(mode,"change-toolchain")) { wchar_t module[32768]; GetModuleFileNameW(NULL,module,32768); wchar_t* slash=wcsrchr(module,L'\\'); if(slash) { wcscpy(slash+1,L"xlc.dll"); writeText(module,"changed during check"); } }
  if(!strcmp(mode,"stderr-flood")) { char bytes[16384]; memset(bytes,'E',sizeof(bytes)); DWORD n; for(int i=0;i<192;++i) WriteFile(GetStdHandle(STD_ERROR_HANDLE),bytes,sizeof(bytes),&n,NULL); }
  if(!strcmp(mode,"malformed")) fputs("invalid json",stdout);
  else if(!strcmp(mode,"invalid-utf8")) putchar(0xff);
  else if(!strcmp(mode,"oversize")) { for(int i=0;i<1100000;++i) putchar('X'); }
  else if(!strcmp(mode,"crash")) { fputs("compiler failed\n",stderr); code=7; }
  else {
   int bad=!strcmp(mode,"slow-bad") || !strcmp(mode,"bad") || !strcmp(mode,"foreign") || !strcmp(mode,"range") || !strcmp(mode,"wrong-exit");
   printf("{\"ok\":%s,\"phase\":\"sem\",\"diagnostics\":[",bad ? "false" : "true");
   if(bad) { char path[131072]; WideCharToMultiByte(CP_UTF8,0,argv[2],-1,path,sizeof(path),NULL,NULL); fputs("{\"file\":",stdout); quote(!strcmp(mode,"foreign") ? "foreign.xl" : path); printf(",\"line\":1,\"column\":%d,\"message\":\"unknown identifier: 中文😀\"}",!strcmp(mode,"range") ? 9999 : 10); }
   fputs("]}",stdout); code=bad && strcmp(mode,"wrong-exit") ? 1 : 0;
  }
 } else code=2;
 LocalFree(argv); return code;
}
