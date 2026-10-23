#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/process.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define OUTPUT_LIMIT (1024u*1024u)
#define INPUT_LIMIT (64u*1024u)
typedef struct process_output { xide_process* process;int channel; } process_output;
struct xide_process {
    CRITICAL_SECTION lock;
    HANDLE reader,writer,wake,log,errorLog;
    HANDLE controlWake,outputs[2];process_output outputArgs[2];volatile LONG stopReaders;
    xprocess* native;
    char *program,*nativeProgram,*directory,*nativeDirectory,*logPath,*errorLogPath;
    HANDLE directoryPin,aliasPin;
    size_t directoryLength;
    int directoryVerified;
    char** arguments;
    xprocessenv* environment;
    size_t argumentCount,environmentCount;
    unsigned char *output,*input,*stderrBytes;
    size_t outputStart,outputCount,inputCount,stderrStart,stderrCount,outputCapacity;
    ULONGLONG outputProgress;
    int interactive,separateStderr,cancel,closeInput,writerDone,rootExited;
    xide_process_info info;
    ULONGLONG began;
    void (*notify)(void*); void* notifyUser;
};
static void notifyOwner(xide_process* process)
{
    EnterCriticalSection(&process->lock);
    void (*wake)(void*)=process->notify; void* user=process->notifyUser;
    LeaveCriticalSection(&process->lock);
    if(wake) wake(user);
}
void xideProcessSetWake(xide_process* process,void (*wake)(void*),void* user)
{
    if(!process) return;
    EnterCriticalSection(&process->lock); process->notify=wake; process->notifyUser=user; LeaveCriticalSection(&process->lock);
    notifyOwner(process);
}
static char* copy(const char* text)
{
    if(!text) return NULL;
    size_t size=strlen(text)+1; char* value=malloc(size);
    if(value) memcpy(value,text,size);
    return value;
}
static void currentError(char* target,size_t capacity,const char* prefix)
{
    const xerror* error=xrtGetError(); const char* detail=error ? xrtErrorMessage(error) : NULL;
    int32 code=error ? xrtErrorSystemCode(error) : 0;
    if(code) snprintf(target,capacity,"%s%s%s (Windows error %ld)",prefix,detail ? ": " : "",detail ? detail : "",(long)code);
    else snprintf(target,capacity,"%s%s%s",prefix,detail ? ": " : "",detail ? detail : "");
}
/* An existing 8.3 spelling retains the real directory hierarchy, including
   '..' and drive-rooted paths. A newly created junction would change those
   meanings, so this adapter never substitutes one or copies project files. */
static void prepareDirectory(xide_process* process)
{
    if(!process->directory || !*process->directory) return;
    wchar_t* actual=xidePathNativeWide(process->directory); if(!actual) return;
    size_t length=wcslen(actual);
    process->directoryLength=length>=8 && !wcsncmp(actual,L"\\\\?\\UNC\\",8) ? length-6 :
                             length>=4 && !wcsncmp(actual,L"\\\\?\\",4) ? length-4 : length;
    if(process->directoryLength<MAX_PATH-1) { free(actual); return; }
    process->directoryPin=CreateFileW(actual,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    BY_HANDLE_FILE_INFORMATION a={0},b={0};
    if(process->directoryPin==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(process->directoryPin,&a) || !(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) { free(actual); return; }
    process->directoryVerified=1;
    DWORD needed=GetShortPathNameW(actual,NULL,0);
    wchar_t* alias=needed && needed<32760 ? malloc((size_t)needed*sizeof(*alias)) : NULL;
    if(!alias) { free(actual); return; }
    DWORD count=GetShortPathNameW(actual,alias,needed);
    if(!count || count>=needed) goto done;
    if(!wcsncmp(alias,L"\\\\?\\UNC\\",8)) {
        memmove(alias+2,alias+8,(wcslen(alias+8)+1)*sizeof(*alias)); alias[0]=alias[1]=L'\\';
    } else if(!wcsncmp(alias,L"\\\\?\\",4)) memmove(alias,alias+4,(wcslen(alias+4)+1)*sizeof(*alias));
    if(wcslen(alias)>=MAX_PATH-1) goto done;
    process->aliasPin=CreateFileW(alias,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(process->directoryPin==INVALID_HANDLE_VALUE || process->aliasPin==INVALID_HANDLE_VALUE ||
       !GetFileInformationByHandle(process->directoryPin,&a) || !GetFileInformationByHandle(process->aliasPin,&b) ||
       !(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || !(b.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
       a.dwVolumeSerialNumber!=b.dwVolumeSerialNumber || a.nFileIndexHigh!=b.nFileIndexHigh || a.nFileIndexLow!=b.nFileIndexLow) goto done;
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,alias,-1,NULL,0,NULL,NULL);
    process->nativeDirectory=bytes ? malloc((size_t)bytes) : NULL;
    if(process->nativeDirectory && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,alias,-1,process->nativeDirectory,bytes,NULL,NULL)) { free(process->nativeDirectory); process->nativeDirectory=NULL; }
done:
    free(actual); free(alias);
}
static void failure(xide_process* process,const char* message)
{
    EnterCriticalSection(&process->lock);
    process->info.ioError=1;
    if(!*process->info.error) snprintf(process->info.error,sizeof(process->info.error),"%s",message);
    LeaveCriticalSection(&process->lock);
    SetEvent(process->controlWake);
    notifyOwner(process);
}
static void output(xide_process* process,const unsigned char* bytes,size_t size,int errorChannel)
{
 HANDLE log=errorChannel ? process->errorLog : process->log;
 if(log!=INVALID_HANDLE_VALUE) {
  size_t at=0; while(at<size) { DWORD written=0; if(!WriteFile(log,bytes+at,(DWORD)(size-at),&written,NULL) || !written) { failure(process,"Unable to write the raw process log"); break; } at+=written; }
 }
 EnterCriticalSection(&process->lock);
 size_t *count=errorChannel ? &process->stderrCount : &process->outputCount,*start=errorChannel ? &process->stderrStart : &process->outputStart;
 unsigned char* target=errorChannel ? process->stderrBytes : process->output;
 size_t limit=errorChannel ? OUTPUT_LIMIT : process->outputCapacity;
 size_t discard=*count+size>limit ? *count+size-limit : 0;
 if(discard) { *start=(*start+discard)%limit; *count-=discard; if(errorChannel) process->info.stderrDroppedBytes+=discard; else process->info.droppedBytes+=discard; }
 size_t end=(*start+*count)%limit,first=limit-end; if(first>size) first=size;
 memcpy(target+end,bytes,first); memcpy(target,bytes+first,size-first); *count+=size;process->outputProgress+=size;
 LeaveCriticalSection(&process->lock);
 notifyOwner(process);
}
static DWORD WINAPI outputWorker(void* user)
{
    process_output* stream=user;xide_process* process=stream->process;
    int kind=stream->channel ? XPROCESS_STDERR : XPROCESS_STDOUT;unsigned char buffer[16384];
    while(!InterlockedCompareExchange(&process->stopReaders,0,0)) {
        int64 count=xrtProcessRead(process->native,kind,buffer,sizeof(buffer));
        if(count>0) output(process,buffer,(size_t)count,stream->channel);
        else {
            if(count<0 && !InterlockedCompareExchange(&process->stopReaders,0,0)) {
                char message[512];currentError(message,sizeof(message),"Process output failed");failure(process,message);
            }
            break;
        }
    }
    xrtProcessClose(process->native,kind);return 0;
}
static void joinIo(xide_process* process)
{
    HANDLE threads[3];DWORD count=0;
    for(int i=0;i<2;++i) if(process->outputs[i]) threads[count++]=process->outputs[i];
    if(process->writer) threads[count++]=process->writer;
    if(!count) return;
    for(;;) {
        EnterCriticalSection(&process->lock);ULONGLONG before=process->outputProgress;LeaveCriticalSection(&process->lock);
        DWORD result=WaitForMultipleObjects(count,threads,TRUE,2000);
        if(result==WAIT_OBJECT_0) return;
        EnterCriticalSection(&process->lock);int progressed=process->outputProgress!=before;LeaveCriticalSection(&process->lock);
        if(result!=WAIT_TIMEOUT || !progressed) break;
    }
    failure(process,"Process pipes did not close after tree cleanup");
    InterlockedExchange(&process->stopReaders,1);
    /* Cancellation may race the brief gap before a synchronous read. Repeat
       only on this abnormal teardown path until all dedicated workers stop. */
    for(;;) {
        for(DWORD i=0;i<count;++i) CancelSynchronousIo(threads[i]);
        if(WaitForMultipleObjects(count,threads,TRUE,20)==WAIT_OBJECT_0) break;
    }
}
static DWORD WINAPI writeWorker(void* data)
{
    xide_process* process=data; unsigned char buffer[4096];
    for(;;) {
        WaitForSingleObject(process->wake,INFINITE);
        EnterCriticalSection(&process->lock);
        int done=process->cancel || process->rootExited;
        size_t size=process->inputCount; if(size>sizeof(buffer)) size=sizeof(buffer);
        if(!done && size) { memcpy(buffer,process->input,size); memmove(process->input,process->input+size,process->inputCount-size); process->inputCount-=size; }
        int close=process->closeInput && !size;
        if(!process->inputCount) ResetEvent(process->wake);
        LeaveCriticalSection(&process->lock);
        if(!done && size) notifyOwner(process);
        if(done || close) break;
        size_t offset=0;
        while(offset<size) {
            int64 written=xrtProcessWrite(process->native,buffer+offset,size-offset);
            if(written<=0) { char message[512]; currentError(message,sizeof(message),"Program input failed"); failure(process,message); goto finished; }
            offset+=(size_t)written;
        }
        EnterCriticalSection(&process->lock);
        if(process->inputCount || process->closeInput || process->cancel || process->rootExited) SetEvent(process->wake);
        LeaveCriticalSection(&process->lock);
    }
finished:
    xrtProcessClose(process->native,XPROCESS_STDIN);
    EnterCriticalSection(&process->lock); process->writerDone=1; LeaveCriticalSection(&process->lock);
    notifyOwner(process);
    return 0;
}
static DWORD WINAPI readWorker(void* data)
{
    xide_process* process=data; char message[512];
    if(process->logPath) {
        wchar_t* path=xidePathNativeWide(process->logPath);
        process->log=path ? CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
        free(path);
        if(process->log==INVALID_HANDLE_VALUE) { failure(process,"Unable to create the task's raw log"); goto finished; }
    }
    if(process->errorLogPath) {
        wchar_t* path=xidePathNativeWide(process->errorLogPath); process->errorLog=path ? CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE; free(path);
        if(process->errorLog==INVALID_HANDLE_VALUE) { failure(process,"Unable to create the raw stderr log"); goto finished; }
    }
    EnterCriticalSection(&process->lock); int cancelled=process->cancel; LeaveCriticalSection(&process->lock);
    if(cancelled) goto finished;
    prepareDirectory(process);
    xprocessconfig config; xrtProcessConfigInit(&config);
    config.Program=process->nativeProgram ? process->nativeProgram : process->program; config.Args=(const char* const*)process->arguments; config.ArgCount=process->argumentCount;
    config.WorkDir=process->nativeDirectory ? process->nativeDirectory : process->directory; config.Env=process->environment; config.EnvCount=process->environmentCount;
    config.NewGroup=true; config.HideWindow=true;
    config.Stdin.Mode=process->interactive ? XPROCESS_IO_PIPE : XPROCESS_IO_NULL;
    config.Stdout.Mode=XPROCESS_IO_PIPE; config.Stderr.Mode=process->separateStderr ? XPROCESS_IO_PIPE : XPROCESS_IO_MERGE;
    process->native=xrtProcessSpawn(&config);
    if(!process->native) {
        const xerror* error=xrtGetError(); int32 code=error ? xrtErrorSystemCode(error) : 0;
        if((code==ERROR_DIRECTORY || code==ERROR_FILENAME_EXCED_RANGE) && process->directoryVerified && process->directoryLength>=MAX_PATH-1 && !process->nativeDirectory)
            snprintf(message,sizeof(message),"Unable to start process: working directory is %llu UTF-16 characters and has no verified short path (Windows error %ld). Move the project or configured working directory to a shorter location.",(unsigned long long)process->directoryLength,(long)code);
        else currentError(message,sizeof(message),"Unable to start process");
        failure(process,message); goto finished;
    }
    EnterCriticalSection(&process->lock);
    process->info.started=1; process->info.id=xrtProcessId(process->native); process->info.state=XIDE_PROCESS_RUNNING;
    LeaveCriticalSection(&process->lock);
    notifyOwner(process);
    if(process->interactive) {
        process->writer=CreateThread(NULL,0,writeWorker,process,0,NULL);
        if(!process->writer) { failure(process,"Unable to create program-input worker"); xrtProcessKillTree(process->native); }
    }
    for(int channel=0;channel<(process->separateStderr ? 2 : 1);++channel) {
        process->outputArgs[channel]=(process_output){process,channel};
        process->outputs[channel]=CreateThread(NULL,0,outputWorker,process->outputArgs+channel,0,NULL);
        if(!process->outputs[channel]) failure(process,"Unable to create process-output worker");
    }
    HANDLE handles[]={(HANDLE)xrtProcessNative(process->native),process->controlWake};int stopSent=0;
    for(;;) {
        EnterCriticalSection(&process->lock); int stopping=process->cancel || process->info.ioError; LeaveCriticalSection(&process->lock);
        if(stopping && !stopSent) {xrtProcessKillTree(process->native);stopSent=1;}
        DWORD result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
        if(result==WAIT_OBJECT_0) break;
        if(result!=WAIT_OBJECT_0+1) {failure(process,"Unable to wait for process activity");xrtProcessKillTree(process->native);break;}
    }
    /* Publish the SDK's terminal snapshot before querying status. Kill the
       owned descendants so inherited writers cannot hold an output open. */
    if(xrtProcessWait(process->native)!=XWAIT_OK) {currentError(message,sizeof(message),"Unable to wait for process exit");failure(process,message);}
    xrtProcessKillTree(process->native);
    EnterCriticalSection(&process->lock);process->rootExited=1;SetEvent(process->wake);LeaveCriticalSection(&process->lock);
    joinIo(process);
    xprocessstatus status={0};
    if(xrtProcessStatus(process->native,&status)) {
        EnterCriticalSection(&process->lock); process->info.exit=status; LeaveCriticalSection(&process->lock);
    } else { currentError(message,sizeof(message),"Unable to obtain process exit status"); failure(process,message); }
    xrtProcessClose(process->native,XPROCESS_STDOUT);
    if(process->separateStderr) xrtProcessClose(process->native,XPROCESS_STDERR);
    xrtProcessDestroy(process->native); process->native=NULL;
finished:
    if(process->aliasPin!=INVALID_HANDLE_VALUE) { CloseHandle(process->aliasPin); process->aliasPin=INVALID_HANDLE_VALUE; }
    if(process->directoryPin!=INVALID_HANDLE_VALUE) { CloseHandle(process->directoryPin); process->directoryPin=INVALID_HANDLE_VALUE; }
    if(process->errorLog!=INVALID_HANDLE_VALUE) { FlushFileBuffers(process->errorLog); CloseHandle(process->errorLog); process->errorLog=INVALID_HANDLE_VALUE; }
    if(process->log!=INVALID_HANDLE_VALUE) { FlushFileBuffers(process->log); CloseHandle(process->log); process->log=INVALID_HANDLE_VALUE; }
    EnterCriticalSection(&process->lock);
    process->info.durationMs=GetTickCount64()-process->began;
    process->info.cancelled=process->cancel; process->info.state=XIDE_PROCESS_FINISHED;
    LeaveCriticalSection(&process->lock);
    notifyOwner(process);
    return 0;
}
static xide_process* startProcess(const xide_process_spec* spec,size_t maximum,char* error,size_t capacity)
{
    if(error && capacity) *error=0;
    if(!spec || !spec->program || !*spec->program || (spec->argumentCount && !spec->arguments) || (spec->environmentCount && !spec->environment) || (spec->stdoutCapacity && (spec->stdoutCapacity<16384u || spec->stdoutCapacity>maximum))) {
        if(error && capacity) snprintf(error,capacity,"Invalid process request");
        return NULL;
    }
    xide_process* process=calloc(1,sizeof(*process)); if(!process) goto failed;
    InitializeCriticalSection(&process->lock); process->log=process->errorLog=process->directoryPin=process->aliasPin=INVALID_HANDLE_VALUE;
    process->began=GetTickCount64(); process->info.state=XIDE_PROCESS_STARTING;
    process->program=copy(spec->program); process->directory=copy(spec->directory); process->logPath=copy(spec->logPath);
    /* Keep PATH lookup and ordinary argv[0] unchanged. The SDK resolves an
       explicitly named executable beyond MAX_PATH only with its native prefix. */
    int explicitPath=(strlen(spec->program)>=3 && spec->program[1]==':' && (spec->program[2]=='/' || spec->program[2]=='\\')) ||
                     !strncmp(spec->program,"//",2) || !strncmp(spec->program,"\\\\",2);
    if(explicitPath && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,spec->program,-1,NULL,0)>MAX_PATH) {
        process->nativeProgram=xidePathNativeUtf8(spec->program); if(!process->nativeProgram) goto failed;
    }
    process->interactive=spec->interactive; process->separateStderr=!!spec->separateStderr;
    if(process->separateStderr) { process->stderrBytes=malloc(OUTPUT_LIMIT); if(!process->stderrBytes) goto failed; }
    if(process->separateStderr && spec->logPath) { size_t n=strlen(spec->logPath)+8; process->errorLogPath=malloc(n); if(!process->errorLogPath) goto failed; snprintf(process->errorLogPath,n,"%s.stderr",spec->logPath); }
    process->outputCapacity=spec->stdoutCapacity ? spec->stdoutCapacity : OUTPUT_LIMIT;
    process->output=malloc(process->outputCapacity); process->input=malloc(INPUT_LIMIT);
    process->wake=CreateEventW(NULL,TRUE,FALSE,NULL);
    process->controlWake=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!process->program || (spec->directory && !process->directory) || (spec->logPath && !process->logPath) || !process->output || !process->input || !process->wake || !process->controlWake) goto failed;
    process->arguments=calloc(spec->argumentCount+1,sizeof(*process->arguments));
    process->environment=calloc(spec->environmentCount+1,sizeof(*process->environment));
    if(!process->arguments || !process->environment) goto failed;
    process->argumentCount=spec->argumentCount; process->environmentCount=spec->environmentCount;
    for(size_t i=0;i<spec->argumentCount;++i) { process->arguments[i]=copy(spec->arguments[i]); if(!process->arguments[i]) goto failed; }
    for(size_t i=0;i<spec->environmentCount;++i) {
        process->environment[i].Name=copy(spec->environment[i].Name); process->environment[i].Value=copy(spec->environment[i].Value);
        if(!process->environment[i].Name || (spec->environment[i].Value && !process->environment[i].Value)) goto failed;
    }
    process->reader=CreateThread(NULL,0,readWorker,process,0,NULL);
    if(!process->reader) goto failed;
    return process;
failed:
    if(error && capacity) snprintf(error,capacity,"Unable to allocate or start process workers");
    xideProcessDestroy(process); return NULL;
}
xide_process* xideProcessStart(const xide_process_spec* spec,char* error,size_t capacity)
{ return startProcess(spec,16u*1024u*1024u,error,capacity); }
xide_process* xideProcessStartExtended(const xide_process_spec* spec,char* error,size_t capacity)
{ return startProcess(spec,80u*1024u*1024u,error,capacity); }
void xideProcessGetInfo(xide_process* process,xide_process_info* info)
{
    if(!process || !info) return;
    EnterCriticalSection(&process->lock); *info=process->info;
    if(process->cancel && info->state!=XIDE_PROCESS_FINISHED) info->state=XIDE_PROCESS_STOPPING;
    LeaveCriticalSection(&process->lock);
}
static size_t readChannel(xide_process* process,void* data,size_t capacity,int errorChannel)
{
 if(!process || !data || (errorChannel && !process->separateStderr)) return 0;
 EnterCriticalSection(&process->lock); size_t *count=errorChannel ? &process->stderrCount : &process->outputCount,*start=errorChannel ? &process->stderrStart : &process->outputStart; unsigned char* source=errorChannel ? process->stderrBytes : process->output;
 size_t limit=errorChannel ? OUTPUT_LIMIT : process->outputCapacity;
 size_t size=*count; if(size>capacity) size=capacity; size_t first=limit-*start; if(first>size) first=size;
 memcpy(data,source+*start,first); memcpy((unsigned char*)data+first,source,size-first); *start=(*start+size)%limit; *count-=size; LeaveCriticalSection(&process->lock); return size;
}
size_t xideProcessRead(xide_process* p,void* bytes,size_t capacity) { return readChannel(p,bytes,capacity,0); }
size_t xideProcessReadError(xide_process* p,void* bytes,size_t capacity) { return readChannel(p,bytes,capacity,1); }
int xideProcessWrite(xide_process* process,const void* data,size_t size)
{
    if(!process || (!data && size)) return 0;
    EnterCriticalSection(&process->lock);
    int okay=process->interactive && !process->closeInput && !process->cancel && !process->rootExited && process->info.state!=XIDE_PROCESS_FINISHED && size<=INPUT_LIMIT-process->inputCount;
    if(okay && size) { memcpy(process->input+process->inputCount,data,size); process->inputCount+=size; SetEvent(process->wake); }
    LeaveCriticalSection(&process->lock); return okay;
}
int xideProcessInputOpen(xide_process* process)
{
    if(!process) return 0;
    EnterCriticalSection(&process->lock);
    int open=process->interactive && !process->closeInput && !process->cancel && !process->rootExited && process->info.state!=XIDE_PROCESS_FINISHED;
    LeaveCriticalSection(&process->lock); return open;
}
void xideProcessCloseInput(xide_process* process)
{
    if(process) { EnterCriticalSection(&process->lock); process->closeInput=1; SetEvent(process->wake); LeaveCriticalSection(&process->lock); }
}
void xideProcessCancel(xide_process* process)
{
    if(process) { EnterCriticalSection(&process->lock); if(process->info.state!=XIDE_PROCESS_FINISHED) process->cancel=1; SetEvent(process->wake);SetEvent(process->controlWake); LeaveCriticalSection(&process->lock); }
}
void xideProcessDestroy(xide_process* process)
{
    if(!process) return;
    xideProcessCancel(process);
    if(process->reader) { WaitForSingleObject(process->reader,INFINITE); CloseHandle(process->reader); }
    if(process->writer) CloseHandle(process->writer);
    for(int i=0;i<2;++i) if(process->outputs[i]) CloseHandle(process->outputs[i]);
    if(process->wake) CloseHandle(process->wake);
    if(process->controlWake) CloseHandle(process->controlWake);
    for(size_t i=0;i<process->argumentCount;++i) free(process->arguments[i]);
    for(size_t i=0;i<process->environmentCount;++i) { free((void*)process->environment[i].Name); free((void*)process->environment[i].Value); }
    free(process->arguments); free(process->environment); free(process->program); free(process->nativeProgram); free(process->directory); free(process->nativeDirectory); free(process->logPath); free(process->errorLogPath); free(process->stderrBytes); free(process->output); free(process->input);
    DeleteCriticalSection(&process->lock); free(process);
}
