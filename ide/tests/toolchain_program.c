#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(const char* path,const char* text) { FILE* f=fopen(path,"wb"); if(f) { fwrite(text,1,strlen(text),f); fclose(f); } }
static void count(const char* path) { int n=0; FILE* f=fopen(path,"rb"); if(f) { fscanf(f,"%d",&n); fclose(f); } f=fopen(path,"wb"); if(f) { fprintf(f,"%d",n+1); fclose(f); } }
static void sibling(void) { wchar_t exe[32768]; DWORD n=GetModuleFileNameW(NULL,exe,32768); if(n && n<32768) { wchar_t* slash=wcsrchr(exe,L'\\'); if(slash) { wcscpy(slash+1,L"xl.dll"); HANDLE f=CreateFileW(exe,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,0,NULL); if(f!=INVALID_HANDLE_VALUE) { DWORD written; WriteFile(f,"changed",7,&written,NULL); CloseHandle(f); } } } }
int main(void)
{
 int argc=0; wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc); if(!argv) return 2;
 char mode[80]={0}; FILE* f=fopen("profile-mode.txt","rb"); if(f) { fgets(mode,sizeof(mode),f); fclose(f); }
 int exitCode=0;
 if(argc==2 && !wcscmp(argv[1],L"--help")) {
  count("profile-count.txt"); put("profile-signal.txt","started");
  if(!strcmp(mode,"slow")) Sleep(40000);
  else if(!strcmp(mode,"pause")) Sleep(500);
  if(!strcmp(mode,"change")) sibling();
  if(!strcmp(mode,"oversize")) { for(int i=0;i<70000;++i) putchar('x'); }
  else if(!strcmp(mode,"invalid")) { putchar(0xff); }
  else if(!strcmp(mode,"nul")) { fputs("xl build <file.xlp> -o output.exe",stdout); putchar(0); fputs("junk",stdout); }
  else if(!strcmp(mode,"empty")) { /* No output. */ }
  else if(!strcmp(mode,"misleading")) puts("The compiler does not support build -o.\nxl run <file.xl> [--link library]\nxl build <file.xlp> -o output.exe [--linking name]");
  else puts("fixture compiler help\n  xl build <file.xlp> -o output.exe [--inc-path dir] [--lib-path dir] [--link lib] [--res file] [-shared] [--save-temps] [--emit-header] [--backend xlir]\nxl run <file.xl|file.c|file.xlp>\nxl check <file.xl> [--json]\nxl lex <file.xl>\nxl parse <file.xl>\nxl ir <file.xl>\nxl cgen <file.xl>\nxl abi <file.xl> [--json]\nxl impdef <file.dll> -o output.def");
  if(!strcmp(mode,"nonzero")) exitCode=5;
 } else if(argc>1 && !wcscmp(argv[1],L"build")) {
  count("profile-builds.txt"); if(!strcmp(mode,"build-change")) sibling();
  wchar_t exe[32768]; GetModuleFileNameW(NULL,exe,32768); int produced=0;
  for(int i=2;i+1<argc;++i) if(!wcscmp(argv[i],L"-o")) produced=CopyFileW(exe,argv[i+1],FALSE);
  if(!produced) exitCode=6;
 }
 LocalFree(argv); return exitCode;
}
