#include "platform/process.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char* utf8(const wchar_t* w)
{
 int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL); char* s=n ? malloc((size_t)n) : NULL;
 if(s && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,s,n,NULL,NULL)) { free(s); s=NULL; } return s;
}
static int sameDirectory(const char* a,const char* b)
{
 wchar_t *wa=xidePathNativeWide(a),*wb=xidePathNativeWide(b); BY_HANDLE_FILE_INFORMATION ia={0},ib={0};
 HANDLE ha=wa ? CreateFileW(wa,FILE_READ_ATTRIBUTES,7,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE;
 HANDLE hb=wb ? CreateFileW(wb,FILE_READ_ATTRIBUTES,7,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE;
 int same=ha!=INVALID_HANDLE_VALUE && hb!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(ha,&ia) && GetFileInformationByHandle(hb,&ib) &&
          ia.dwVolumeSerialNumber==ib.dwVolumeSerialNumber && ia.nFileIndexHigh==ib.nFileIndexHigh && ia.nFileIndexLow==ib.nFileIndexLow;
 if(ha!=INVALID_HANDLE_VALUE) CloseHandle(ha);
 if(hb!=INVALID_HANDLE_VALUE) CloseHandle(hb);
 free(wa); free(wb); return same;
}
static void emit(const char* s) { DWORD written=0; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,(DWORD)strlen(s),&written,NULL); }
static int child(int argc,char** argv)
{
 if(argc!=7) return 2;
 wchar_t cwd[32768]={0}; DWORD n=GetCurrentDirectoryW(32768,cwd); char* directory=n && n<32768 ? utf8(cwd) : NULL;
 int okay=directory && n<MAX_PATH-1 && sameDirectory(directory,argv[2]) && sameDirectory("..",argv[3]) && sameDirectory("\\",argv[4]);
 char drive[]={argv[4][0],':','.',0}; okay=okay && sameDirectory(drive,argv[2]);
 HANDLE file=CreateFileW(L"input.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 char bytes[128]={0}; DWORD received=0;
 okay=okay && file!=INVALID_HANDLE_VALUE && ReadFile(file,bytes,sizeof(bytes)-1,&received,NULL) && !strcmp(bytes,"actual project 中文 😀\n");
 if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
 okay=okay && !strcmp(argv[5],"中文 空格") && !strcmp(argv[6],"quote\"tail\\");
 wchar_t env[128]={0}; GetEnvironmentVariableW(L"XIDE_WORKDIR_TEST",env,128); okay=okay && !wcscmp(env,L"保留 😀");
 if(okay) {
  emit("CWD="); emit(directory); emit("\nIDENTITY=1 PARENT=1 ROOT=1 DRIVE=1 RELATIVE=1 ARGS=1 ENV=1\n");
  DWORD written=0; const char* error="separate stderr 中文\n"; WriteFile(GetStdHandle(STD_ERROR_HANDLE),error,(DWORD)strlen(error),&written,NULL);
  while(ReadFile(GetStdHandle(STD_INPUT_HANDLE),bytes,sizeof(bytes),&received,NULL) && received) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes,received,&written,NULL);
 }
 free(directory); return okay ? 37 : 3;
}
static int waitDone(xide_process* process,xide_process_info* info,char* output,size_t capacity)
{
 ULONGLONG end=GetTickCount64()+10000; size_t used=0;
 for(;;) {
  size_t n=xideProcessRead(process,output+used,capacity-used-1); used+=n; output[used]=0; xideProcessGetInfo(process,info);
  if(info->state==XIDE_PROCESS_FINISHED) {
   while((n=xideProcessRead(process,output+used,capacity-used-1))>0) used+=n;
   output[used]=0; return 1;
  }
  if(GetTickCount64()>end) return 0;
  Sleep(5);
 }
}
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"process_workdir_test:%d: %s; %s; %s\n",__LINE__,#v,error,info.error); failed=1; goto cleanup; } } while(0)
int main(void)
{
 int argc=0; wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&argc); char** argv=wide ? calloc((size_t)argc+1,sizeof(*argv)) : NULL;
 if(!argv) { if(wide) LocalFree(wide); return 2; }
 for(int i=0;i<argc;++i) argv[i]=utf8(wide[i]);
 LocalFree(wide);
 if(argc>=2 && !strcmp(argv[1],"--child")) { int code=child(argc,argv); for(int i=0;i<argc;++i) free(argv[i]); free(argv); return code; }
 int checks=0,failed=0; char error[512]={0},output[8192]={0}; xide_process_info info={0}; xide_process* process=NULL;
 char *parent=NULL,*log=NULL,*missing=NULL; wchar_t before[32768]={0},after[32768]={0}; CHECK(argc==3);
 CHECK(GetCurrentDirectoryW(32768,before)>0); CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,NULL,0)>MAX_PATH);
 parent=xidePathDirectory(argv[1]); log=xidePathResolveLexical(argv[1],"raw.log"); CHECK(parent && log);
 char root[]={argv[1][0],':','/',0}; const char* arguments[]={"--child",argv[1],parent,root,"中文 空格","quote\"tail\\"};
 xprocessenv env={"XIDE_WORKDIR_TEST","保留 😀"};
 xide_process_spec spec={.program=argv[0],.directory=argv[1],.arguments=arguments,.argumentCount=6,.environment=&env,.environmentCount=1,.logPath=log,.interactive=1,.separateStderr=1};
 process=xideProcessStart(&spec,error,sizeof(error)); CHECK(process);
 CHECK(xideProcessWrite(process,"stdin 中文 😀\n",strlen("stdin 中文 😀\n"))); xideProcessCloseInput(process);
 CHECK(waitDone(process,&info,output,sizeof(output))); CHECK(info.started && !info.ioError && !info.cancelled && info.exit.Kind==XPROCESS_EXIT_CODE && info.exit.Code==37);
 CHECK(strstr(output,"IDENTITY=1 PARENT=1 ROOT=1 DRIVE=1 RELATIVE=1 ARGS=1 ENV=1") && strstr(output,"stdin 中文 😀\n"));
 char stderrText[256]={0}; size_t n=xideProcessReadError(process,stderrText,sizeof(stderrText)-1); stderrText[n]=0; CHECK(!strcmp(stderrText,"separate stderr 中文\n"));
 size_t rawSize=0; unsigned char* raw=xideIoReadAllLimit(log,8192,&rawSize); int matches=raw && rawSize==strlen(output) && !memcmp(raw,output,rawSize); xrtFree(raw); CHECK(matches);
 xideProcessDestroy(process); process=NULL; CHECK(GetCurrentDirectoryW(32768,after)>0 && !wcscmp(before,after));
 spec.directory=argv[2]; spec.logPath=NULL; process=xideProcessStart(&spec,error,sizeof(error)); CHECK(process); CHECK(waitDone(process,&info,output,sizeof(output)));
 CHECK(!info.started && info.ioError && strstr(info.error,"working directory is") && strstr(info.error,"UTF-16") && strstr(info.error,"no verified short path") && strstr(info.error,"Windows error 267") && strstr(info.error,"shorter location"));
 xideProcessDestroy(process); process=NULL; missing=xidePathResolveLexical(NULL,"no-such-workdir"); CHECK(missing); spec.directory=missing;
 process=xideProcessStart(&spec,error,sizeof(error)); CHECK(process && waitDone(process,&info,output,sizeof(output)));
 CHECK(!info.started && info.ioError && strstr(info.error,"Windows error 267") && !strstr(info.error,"no verified short path"));
 xideProcessDestroy(process); process=NULL; CHECK(GetCurrentDirectoryW(32768,after)>0 && !wcscmp(before,after));
 printf("Process working-directory checks: %d passed; same physical directory, relative file/parent/root/drive paths, args/environment/stdin/stderr/exit/raw log, genuine no-alias refusal, native errors, unchanged parent CWD\n",checks);
cleanup:
 xideProcessDestroy(process); free(parent); free(log); free(missing); for(int i=0;i<argc;++i) free(argv[i]); free(argv); return failed ? 1 : 0;
}
