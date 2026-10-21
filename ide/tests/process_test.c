#include "platform/process.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"process_test:%d: %s; %s\n",__LINE__,#test,info.error); failed=1; goto cleanup; } } while(0)
static char* utf8(const wchar_t* text)
{
    int size=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL); char* value=malloc((size_t)size);
    if(value) WideCharToMultiByte(CP_UTF8,0,text,-1,value,size,NULL,NULL);
    return value;
}
static void emit(const char* value)
{
    DWORD written; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),value,(DWORD)strlen(value),&written,NULL);
}
static int child(int count,wchar_t** arguments)
{
    if(!wcscmp(arguments[2],L"flood")) {
        char bytes[16384]; memset(bytes,'X',sizeof(bytes)); DWORD written;
        for(int i=0;i<192;++i) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes,sizeof(bytes),&written,NULL);
        return 0;
    }
    if(!wcscmp(arguments[2],L"separate")) {
        emit("PROTO-only\n"); char bytes[16384]; memset(bytes,'E',sizeof(bytes)); DWORD written;
        for(int i=0;i<192;++i) WriteFile(GetStdHandle(STD_ERROR_HANDLE),bytes,sizeof(bytes),&written,NULL);
        emit("PROTO-end\n"); return 0;
    }
    if(!wcscmp(arguments[2],L"sleep")) { Sleep(60000); return 0; }
    if(!wcscmp(arguments[2],L"tree")) {
        wchar_t self[32768],command[32768]; GetModuleFileNameW(NULL,self,32768);
        if(swprintf(command,32768,L"\"%ls\" child sleep",self)<0) return 2;
        STARTUPINFOW startup={0}; startup.cb=sizeof(startup); PROCESS_INFORMATION process={0};
        if(!CreateProcessW(NULL,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) return 3;
        char line[64]; snprintf(line,sizeof(line),"CHILD=%lu\n",process.dwProcessId); emit(line);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); Sleep(60000); return 0;
    }
    if(!wcscmp(arguments[2],L"echo")) {
        for(int i=3;i<count;++i) { char* value=utf8(arguments[i]); emit("ARG["); emit(value); emit("]\n"); free(value); }
        wchar_t environment[256],directory[32768]; GetEnvironmentVariableW(L"XIDE_PROCESS_TEST",environment,256); GetCurrentDirectoryW(32768,directory);
        char* value=utf8(environment); emit("ENV["); emit(value); emit("]\n"); free(value);
        value=utf8(directory); emit("DIR["); emit(value); emit("]\n"); free(value);
        const char* stderrText="STDERR-合并\n"; DWORD written; WriteFile(GetStdHandle(STD_ERROR_HANDLE),stderrText,(DWORD)strlen(stderrText),&written,NULL);
        char input[1024]; DWORD size;
        while(ReadFile(GetStdHandle(STD_INPUT_HANDLE),input,sizeof(input),&size,NULL) && size) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),input,size,&written,NULL);
        return 7;
    }
    return 4;
}
static int waitDone(xide_process* process,xide_process_info* info,char* output,size_t capacity,size_t* count,unsigned timeout)
{
    ULONGLONG began=GetTickCount64();
    for(;;) {
        char scratch[16384]; size_t size=xideProcessRead(process,scratch,sizeof(scratch));
        if(output && *count+size<capacity) { memcpy(output+*count,scratch,size); *count+=size; output[*count]=0; }
        xideProcessGetInfo(process,info);
        if(info->state==XIDE_PROCESS_FINISHED) {
            while((size=xideProcessRead(process,scratch,sizeof(scratch)))>0) {
                if(output && *count+size<capacity) { memcpy(output+*count,scratch,size); *count+=size; output[*count]=0; }
            }
            return 1;
        }
        if(GetTickCount64()-began>timeout) return 0;
        Sleep(5);
    }
}
int main(int argc,char** argv)
{
    int count=0; wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    if(count>=3 && !wcscmp(arguments[1],L"child")) { int result=child(count,arguments); LocalFree(arguments); return result; }
    int failed=0; xide_process_info info={0}; xide_process* process=NULL;
    char *self=NULL,*directory=NULL,*log=NULL,*bad=NULL; unsigned char* logBytes=NULL; HANDLE descendant=NULL;
    char error[512]={0},output[32768]={0}; size_t outputSize=0,logSize=0;
    CHECK(argc==2 && arguments); self=utf8(arguments[0]); LocalFree(arguments); arguments=NULL;
    directory=xidePathResolve(argv[1],"中文 进程"); CHECK(directory && xrtDirCreateAll(directory));
    log=xidePathResolve(directory,"完整 日志.bin"); bad=xidePathResolve(directory,"missing-program.exe"); CHECK(log && bad);
    const char* echoArguments[]={"child","echo","中文 空格","","a\"b","ends\\","back\\\"quote"};
    xprocessenv environment[]={ {"XIDE_PROCESS_TEST","继承环境 空格"} };
    xide_process_spec request={.program=self,.directory=directory,.arguments=echoArguments,.argumentCount=7,.environment=environment,.environmentCount=1,.logPath=log,.interactive=1};
    process=xideProcessStart(&request,error,sizeof(error)); CHECK(process);
    CHECK(xideProcessWrite(process,"INPUT-你好\n",strlen("INPUT-你好\n"))); xideProcessCloseInput(process);
    CHECK(waitDone(process,&info,output,sizeof(output),&outputSize,8000));
    CHECK(info.started && !info.ioError && !info.cancelled && info.exit.Kind==XPROCESS_EXIT_CODE && info.exit.Code==7);
    CHECK(strstr(output,"ARG[中文 空格]\n") && strstr(output,"ARG[]\n") && strstr(output,"ARG[a\"b]\n") && strstr(output,"ARG[ends\\]\n") && strstr(output,"ARG[back\\\"quote]\n"));
    CHECK(strstr(output,"ENV[继承环境 空格]") && strstr(output,"中文 进程") && strstr(output,"STDERR-合并") && strstr(output,"INPUT-你好"));
    logBytes=xrtFileReadAll(log,&logSize); CHECK(logBytes && logSize==outputSize && !memcmp(logBytes,output,logSize)); xrtFree(logBytes); logBytes=NULL;
    xideProcessDestroy(process); process=NULL;
    request.program=bad; request.logPath=NULL; process=xideProcessStart(&request,error,sizeof(error)); CHECK(process);
    outputSize=0; CHECK(waitDone(process,&info,NULL,0,&outputSize,8000)); CHECK(!info.started && info.ioError && *info.error);
    xideProcessDestroy(process); process=NULL;
    const char* floodArguments[]={"child","flood"}; request.program=self; request.arguments=floodArguments; request.argumentCount=2; request.interactive=0; request.logPath=log;
    process=xideProcessStart(&request,error,sizeof(error)); CHECK(process);
    ULONGLONG deadline=GetTickCount64()+8000;
    do { Sleep(5); xideProcessGetInfo(process,&info); } while(info.state!=XIDE_PROCESS_FINISHED && GetTickCount64()<deadline);
    CHECK(info.state==XIDE_PROCESS_FINISHED && !info.ioError && info.droppedBytes==2u*1024u*1024u);
    logBytes=xrtFileReadAll(log,&logSize); CHECK(logBytes && logSize==3u*1024u*1024u); xrtFree(logBytes); logBytes=NULL;
    xideProcessDestroy(process); process=NULL;
    request.stdoutCapacity=4u*1024u*1024u; process=xideProcessStart(&request,error,sizeof(error)); CHECK(process); deadline=GetTickCount64()+8000;
    do { Sleep(5); xideProcessGetInfo(process,&info); } while(info.state!=XIDE_PROCESS_FINISHED && GetTickCount64()<deadline);
    CHECK(info.state==XIDE_PROCESS_FINISHED && !info.ioError && !info.droppedBytes);
    size_t fullCount=0,part=0; while((part=xideProcessRead(process,output,sizeof(output)))>0) { for(size_t i=0;i<part;++i) CHECK(output[i]=='X'); fullCount+=part; } CHECK(fullCount==3u*1024u*1024u);
    xideProcessDestroy(process); process=NULL; request.stdoutCapacity=1; CHECK(!xideProcessStart(&request,error,sizeof(error))); request.stdoutCapacity=17u*1024u*1024u; CHECK(!xideProcessStart(&request,error,sizeof(error))); request.stdoutCapacity=0;
    request.stdoutCapacity=81u*1024u*1024u; CHECK(!xideProcessStartExtended(&request,error,sizeof(error)));
    request.stdoutCapacity=80u*1024u*1024u; process=xideProcessStartExtended(&request,error,sizeof(error)); CHECK(process); deadline=GetTickCount64()+8000;
    do { Sleep(5); xideProcessGetInfo(process,&info); } while(info.state!=XIDE_PROCESS_FINISHED && GetTickCount64()<deadline);
    CHECK(info.state==XIDE_PROCESS_FINISHED && !info.ioError && !info.droppedBytes);
    fullCount=0; while((part=xideProcessRead(process,output,sizeof(output)))>0) { for(size_t i=0;i<part;++i) CHECK(output[i]=='X'); fullCount+=part; } CHECK(fullCount==3u*1024u*1024u);
    xideProcessDestroy(process); process=NULL; request.stdoutCapacity=0;
    const char* separatedArguments[]={"child","separate"}; request.arguments=separatedArguments; request.separateStderr=1;
    process=xideProcessStart(&request,error,sizeof(error)); CHECK(process); deadline=GetTickCount64()+8000;
    do { Sleep(5); xideProcessGetInfo(process,&info); } while(info.state!=XIDE_PROCESS_FINISHED && GetTickCount64()<deadline);
    CHECK(info.state==XIDE_PROCESS_FINISHED && !info.ioError && !info.droppedBytes && info.stderrDroppedBytes==2u*1024u*1024u);
    outputSize=xideProcessRead(process,output,sizeof(output)-1); output[outputSize]=0; CHECK(!strcmp(output,"PROTO-only\nPROTO-end\n"));
    logBytes=xrtFileReadAll(log,&logSize); CHECK(logBytes && logSize==outputSize && !memcmp(logBytes,output,logSize)); xrtFree(logBytes); logBytes=NULL;
    char stderrPath[32768]; CHECK(snprintf(stderrPath,sizeof(stderrPath),"%s.stderr",log)>0); logBytes=xrtFileReadAll(stderrPath,&logSize); CHECK(logBytes && logSize==3u*1024u*1024u); for(size_t i=0;i<logSize;++i) CHECK(logBytes[i]=='E'); xrtFree(logBytes); logBytes=NULL;
    size_t stderrCount=0,n=0; while((n=xideProcessReadError(process,output,sizeof(output)))>0) stderrCount+=n; CHECK(stderrCount==1024u*1024u);
    xideProcessDestroy(process); process=NULL; request.separateStderr=0;
    const char* treeArguments[]={"child","tree"}; request.arguments=treeArguments; request.logPath=NULL;
    process=xideProcessStart(&request,error,sizeof(error)); CHECK(process); outputSize=0; output[0]=0; deadline=GetTickCount64()+8000;
    while(!strstr(output,"CHILD=") && GetTickCount64()<deadline) { size_t size=xideProcessRead(process,output+outputSize,sizeof(output)-outputSize-1); outputSize+=size; output[outputSize]=0; Sleep(5); }
    CHECK(strstr(output,"CHILD=")); DWORD childId=(DWORD)strtoul(strstr(output,"CHILD=")+6,NULL,10);
    descendant=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,childId); CHECK(descendant);
    xideProcessCancel(process); CHECK(waitDone(process,&info,NULL,0,&outputSize,4000)); CHECK(info.cancelled && info.started);
    CHECK(WaitForSingleObject(descendant,1000)==WAIT_OBJECT_0); CloseHandle(descendant); descendant=NULL;
    xideProcessDestroy(process); process=NULL;
    const char* blockedArguments[]={"child","sleep"}; request.arguments=blockedArguments; request.interactive=1;
    process=xideProcessStart(&request,error,sizeof(error)); CHECK(process); char input[65536]; memset(input,'I',sizeof(input));
    CHECK(xideProcessWrite(process,input,sizeof(input))); Sleep(100); xideProcessCancel(process);
    CHECK(waitDone(process,&info,NULL,0,&outputSize,4000) && info.cancelled);
    xideProcessDestroy(process); process=NULL;
    puts("process_test passed: Unicode/quoting/environment/cwd, merged and separated streams, independent bounded stderr/raw logs, input, raw log and bounded output, spawn failure, blocked-input cancellation and descendant cleanup.");
cleanup:
    if(descendant) CloseHandle(descendant);
    xideProcessDestroy(process); xrtFree(logBytes); free(self); free(directory); free(log); free(bad); if(arguments) LocalFree(arguments); return failed;
}
