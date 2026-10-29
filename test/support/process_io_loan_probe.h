/* Deterministic close/IO race at the OS-call boundary; the native process
 * implementation is real. Private pipes/socketpairs replace child endpoints,
 * not the process algorithm. No scheduling sleeps or GUI launches. */
#if defined(_WIN32)
#include <windows.h>
static BOOL WINAPI loanReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI loanWriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI loanDuplicate(HANDLE,HANDLE,HANDLE,LPHANDLE,DWORD,BOOL,DWORD);
static BOOL WINAPI loanCloseHandle(HANDLE);
#define ReadFile loanReadFile
#define WriteFile loanWriteFile
#define DuplicateHandle loanDuplicate
#define CloseHandle loanCloseHandle
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <errno.h>
static ssize_t loanRead(int,void*,size_t);
static ssize_t loanSend(int,const void*,size_t,int);
static int loanFcntl(int,int,...);
static int loanClose(int);
#define read loanRead
#define send loanSend
#define fcntl loanFcntl
#define close loanClose
#endif
#define XRT_MODULE_PROCESS
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include XRT_PROCESS_IO_LOAN_HEADER
#if defined(_WIN32)
#undef ReadFile
#undef WriteFile
#undef DuplicateHandle
#undef CloseHandle
typedef HANDLE loanhandle;
#define LOAN_INVALID INVALID_HANDLE_VALUE
#else
#undef read
#undef send
#undef fcntl
#undef close
typedef int loanhandle;
#define LOAN_INVALID (-1)
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    xmutex lock;
    xcond changed;
    int pauseMode;
    bool stopped,go,duplicate,flags,denyDuplicate,failIo,failRelease,clobberClose;
    bool shortWrite,eofIo,interruptOnce;
    unsigned duplicates,releases,calls;
    loanhandle original,operation;
} probe;
static void operationBoundary(loanhandle handle,int mode)
{
    ++probe.calls;
    probe.operation=handle;
    if(probe.pauseMode!=mode)return;
    assert(xrtMutexLock(&probe.lock));
    probe.duplicate=handle!=probe.original;
#if defined(_WIN32)
    DWORD flags=0;
    probe.flags=GetHandleInformation(handle,&flags) && !(flags&HANDLE_FLAG_INHERIT);
#else
    probe.flags=(fcntl(handle,F_GETFD)&FD_CLOEXEC)!=0;
#endif
    probe.stopped=true;assert(xrtCondBroadcast(&probe.changed));
    while(!probe.go)assert(xrtCondWaitFor(&probe.changed,&probe.lock,UINT64_C(10000000))==XWAIT_OK);
    assert(xrtMutexUnlock(&probe.lock));
}
#if defined(_WIN32)
static BOOL WINAPI loanReadFile(HANDLE h,LPVOID p,DWORD n,LPDWORD done,LPOVERLAPPED over)
{
    operationBoundary(h,1);
    if(probe.eofIo){*done=77;SetLastError(ERROR_HANDLE_EOF);return FALSE;}
    if(probe.failIo){SetLastError(ERROR_READ_FAULT);return FALSE;}
    return ReadFile(h,p,n,done,over);
}
static BOOL WINAPI loanWriteFile(HANDLE h,LPCVOID p,DWORD n,LPDWORD done,LPOVERLAPPED over)
{
    operationBoundary(h,2);
    if(probe.shortWrite && n>2)n=2;
    if(probe.failIo){SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return WriteFile(h,p,n,done,over);
}
static BOOL WINAPI loanDuplicate(HANDLE src,HANDLE h,HANDLE dst,LPHANDLE result,DWORD access,BOOL inherit,DWORD options)
{
    ++probe.duplicates;
    if(probe.denyDuplicate){*result=NULL;SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    return DuplicateHandle(src,h,dst,result,access,inherit,options);
}
static BOOL WINAPI loanCloseHandle(HANDLE h)
{
    if(h==probe.operation)++probe.releases;
    BOOL result=CloseHandle(h);
    if(probe.failRelease && h==probe.operation){probe.failRelease=false;SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    if(probe.clobberClose)SetLastError(ERROR_INVALID_HANDLE);
    return result;
}
static void pairCreate(loanhandle pair[2],bool writing)
{(void)writing;assert(CreatePipe(&pair[0],&pair[1],NULL,0));}
static void endpointClose(loanhandle h){assert(CloseHandle(h));}
static void putByte(loanhandle h,unsigned char value)
{DWORD count=0;assert(WriteFile(h,&value,1,&count,NULL) && count==1);}
static bool getByte(loanhandle h,unsigned char* value)
{DWORD count=0;return ReadFile(h,value,1,&count,NULL) && count==1;}
static bool handleClosed(loanhandle h){DWORD flags;return !GetHandleInformation(h,&flags) && GetLastError()==ERROR_INVALID_HANDLE;}
#else
static ssize_t loanRead(int fd,void* p,size_t n)
{
    operationBoundary(fd,1);
    if(probe.interruptOnce){probe.interruptOnce=false;errno=EINTR;return -1;}
    if(probe.failIo){errno=EIO;return -1;}
    return read(fd,p,n);
}
static ssize_t loanSend(int fd,const void* p,size_t n,int flags)
{
    operationBoundary(fd,2);
    if(probe.interruptOnce){probe.interruptOnce=false;errno=EINTR;return -1;}
    if(probe.failIo){errno=EIO;return -1;}
    if(probe.shortWrite && n>2)n=2;
    return send(fd,p,n,flags);
}
static int loanFcntl(int fd,int command,...)
{
    if(command==F_GETFD || command==F_GETFL)return fcntl(fd,command);
    va_list args;va_start(args,command);int value=va_arg(args,int);va_end(args);
    if(command==F_DUPFD
#if defined(F_DUPFD_CLOEXEC)
        || command==F_DUPFD_CLOEXEC
#endif
        ){
        ++probe.duplicates;
        if(probe.denyDuplicate){errno=EMFILE;return -1;}
    }
    return fcntl(fd,command,value);
}
static int loanClose(int fd)
{
    if(fd==probe.operation)++probe.releases;
    int result=close(fd);
    if(probe.failRelease && fd==probe.operation){probe.failRelease=false;errno=EIO;return -1;}
    if(probe.clobberClose)errno=EBADF;
    return result;
}
static void pairCreate(loanhandle pair[2],bool writing)
{assert((writing ? socketpair(AF_UNIX,SOCK_STREAM,0,pair) : pipe(pair))==0);}
static void endpointClose(loanhandle h){assert(close(h)==0);}
static void putByte(loanhandle h,unsigned char value){assert(write(h,&value,1)==1);}
static bool getByte(loanhandle h,unsigned char* value)
{return recv(h,value,1,MSG_DONTWAIT)==1;}
static bool handleClosed(loanhandle h){return fcntl(h,F_GETFD)<0 && errno==EBADF;}
#endif
typedef struct operation {
    xprocess* process;
    xprocessstream stream;
    bool writing;
    unsigned char byte;
    int64 result;
    xerrkind kind;
    int code,system;
} operation;
static int32 perform(ptr data)
{
    operation* op=data;
    op->result=op->writing ? xrtProcessWrite(op->process,&op->byte,1)
        : xrtProcessRead(op->process,op->stream,&op->byte,1);
    const xerror* error=xrtGetError();
    op->kind=xrtErrorKind(error);op->code=xrtErrorCode(error);op->system=xrtErrorSystemCode(error);
    xrtClearError();return 0;
}
static void processInit(xprocess* process,loanhandle h,xprocessstream stream)
{
    memset(process,0,sizeof(*process));assert(xrtMutexInit(&process->Lock));
    process->Stdin=process->Stdout=process->Stderr=LOAN_INVALID;
    if(stream==XPROCESS_STDIN)process->Stdin=h;
    else if(stream==XPROCESS_STDOUT)process->Stdout=h;
    else process->Stderr=h;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount);
    assert(before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool closeRace(xprocessstream stream)
{
    bool writing=stream==XPROCESS_STDIN;loanhandle endpoints[2];pairCreate(endpoints,writing);
#if defined(_WIN32)
    loanhandle parent=endpoints[writing ? 1 : 0],peer=endpoints[writing ? 0 : 1];
#else
    loanhandle parent=endpoints[0],peer=endpoints[1];
#endif
    xprocess process;processInit(&process,parent,stream);
    if(!writing)putByte(peer,'R');
    probe.original=parent;probe.operation=LOAN_INVALID;probe.pauseMode=writing?2:1;
    probe.stopped=probe.go=probe.duplicate=probe.flags=false;
    operation op={.process=&process,.stream=stream,.writing=writing,.byte='R'};
    xthread* thread=xrtThreadCreate(perform,&op,0);assert(thread);
    assert(xrtMutexLock(&probe.lock));
    while(!probe.stopped)assert(xrtCondWaitFor(&probe.changed,&probe.lock,UINT64_C(10000000))==XWAIT_OK);
    assert(xrtMutexUnlock(&probe.lock));
    assert(xrtProcessClose(&process,stream));assert(xrtProcessClose(&process,stream));
#if !defined(_WIN32)
    /* Reuse precisely the old descriptor, not merely close it. The broken
     * implementation now accesses a foreign endpoint deterministically. */
    loanhandle foreign[2];pairCreate(foreign,writing);
    assert(dup2(foreign[0],parent)==parent);
    if(foreign[0]!=parent)endpointClose(foreign[0]);
    if(!writing)putByte(foreign[1],'F');
#endif
    assert(xrtMutexLock(&probe.lock));probe.go=true;
    assert(xrtCondBroadcast(&probe.changed));assert(xrtMutexUnlock(&probe.lock));
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    probe.pauseMode=0;
    bool ok=probe.duplicate && probe.flags && op.result==1 && op.kind==XERR_NONE;
    if(!writing)ok=ok && op.byte=='R';
    else {unsigned char value=0;if(op.result==1)ok=ok && getByte(peer,&value) && value=='R';}
    if(probe.duplicate)ok=ok && handleClosed(probe.operation);
    printf("stream %d close race: duplicate=%d flags=%d result=%lld byte=%u kind=%d safe=%d\n",
        stream,probe.duplicate,probe.flags,(long long)op.result,op.byte,op.kind,ok);
#if !defined(_WIN32)
    endpointClose(parent);endpointClose(foreign[1]);
#endif
    endpointClose(peer);assert(xrtMutexUnit(&process.Lock));return ok;
}
static bool failureCases(bool writing)
{
    loanhandle endpoints[2];pairCreate(endpoints,writing);
#if defined(_WIN32)
    loanhandle parent=endpoints[writing ? 1 : 0],peer=endpoints[writing ? 0 : 1];
    int duplicateError=ERROR_NOT_ENOUGH_MEMORY,ioError=writing?ERROR_WRITE_FAULT:ERROR_READ_FAULT;
#else
    loanhandle parent=endpoints[0],peer=endpoints[1];
    int duplicateError=EMFILE,ioError=EIO;
#endif
    xprocessstream stream=writing?XPROCESS_STDIN:XPROCESS_STDOUT;
    xprocess process;processInit(&process,parent,stream);
    operation op={.process=&process,.stream=stream,.writing=writing,.byte='R'};
    if(!writing)putByte(peer,'R');
    probe.denyDuplicate=true;perform(&op);probe.denyDuplicate=false;
    bool duplicateOk=op.result<0 && op.system==duplicateError
        && op.code==(writing?XPROCESS_ERROR_WRITE:XPROCESS_ERROR_READ);
    probe.failIo=probe.clobberClose=true;perform(&op);probe.failIo=probe.clobberClose=false;
    bool errorOk=op.result<0 && op.system==ioError
        && op.code==(writing?XPROCESS_ERROR_WRITE:XPROCESS_ERROR_READ)
        && handleClosed(probe.operation);
    if(!writing)putByte(peer,'R');
    probe.failRelease=true;perform(&op);
    bool releaseOk=op.result<0 && op.code==XPROCESS_ERROR_CLOSE && !probe.failRelease
        && handleClosed(probe.operation);
    probe.failRelease=false;
    assert(xrtProcessClose(&process,stream));
    perform(&op);
    bool closedOk=op.result<0 && op.kind==XERR_CLOSED;
    endpointClose(peer);assert(xrtMutexUnit(&process.Lock));
    printf("stream %d failure guards: duplicate=%d original-error=%d release=%d closed=%d\n",
        stream,duplicateOk,errorOk,releaseOk,closedOk);
    return duplicateOk && errorOk && releaseOk && closedOk;
}
/* Short IO and EOF are real pipe operations; short writes, EINTR and the
 * Windows EOF byte-count clobber are explicit OS-boundary seams. */
static void successCases(bool writing)
{
    loanhandle endpoints[2];pairCreate(endpoints,writing);
#if defined(_WIN32)
    loanhandle parent=endpoints[writing?1:0],peer=endpoints[writing?0:1];
#else
    loanhandle parent=endpoints[0],peer=endpoints[1];
#endif
    xprocessstream stream=writing?XPROCESS_STDIN:XPROCESS_STDOUT;
    xprocess process;processInit(&process,parent,stream);
    unsigned char data[4]={'A',0,'B',255};
    probe.duplicates=probe.releases=probe.calls=0;
    xrtClearError();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert((writing?xrtProcessWrite(&process,data,0):xrtProcessRead(&process,stream,data,0))==0);
    assert(probe.duplicates==0 && probe.releases==0 && probe.calls==0);
    if(!writing){putByte(peer,'A');putByte(peer,0);}
    probe.shortWrite=writing;
#if !defined(_WIN32)
    probe.interruptOnce=true;
#endif
    int64 result=writing?xrtProcessWrite(&process,data,sizeof(data))
        :xrtProcessRead(&process,stream,data,sizeof(data));
    probe.shortWrite=false;
    assert(result==2 && !xrtGetError() && probe.duplicates==1 && probe.releases==1);
#if !defined(_WIN32)
    assert(!probe.interruptOnce && probe.calls==2);
#else
    assert(probe.calls==1);
#endif
    assert(handleClosed(probe.operation));
    if(writing){
        unsigned char first=0,second=1;
        assert(getByte(peer,&first) && getByte(peer,&second) && first=='A' && second==0);
    }else assert(data[0]=='A' && data[1]==0);
    balanced(&before); /* Duplicate/release has no XRT payload allocations. */
    endpointClose(peer);
    if(!writing){
        assert(xrtProcessRead(&process,stream,data,sizeof(data))==0 && !xrtGetError());
        assert(handleClosed(probe.operation));
#if defined(_WIN32)
        probe.eofIo=true;
        assert(xrtProcessRead(&process,stream,data,sizeof(data))==0 && !xrtGetError());
        probe.eofIo=false;assert(handleClosed(probe.operation));
#elif defined(XRT_FEATURE_PROCESS_TERMINAL)
        process.Terminal=true;probe.failIo=true;
        assert(xrtProcessRead(&process,stream,data,sizeof(data))==0 && !xrtGetError());
        process.Terminal=false;probe.failIo=false;assert(handleClosed(probe.operation));
#endif
        balanced(&before);
    }else{
        assert(xrtProcessWrite(&process,data,sizeof(data))<0);
        assert(xrtErrorCode(xrtGetError())==XPROCESS_ERROR_WRITE && handleClosed(probe.operation));
        xrtClearError();
    }
    assert(xrtProcessClose(&process,stream));assert(xrtMutexUnit(&process.Lock));
    printf("stream %d success guards: zero, short binary IO, EOF/peer close, loan released, no payload allocations\n",stream);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));assert(xrtMutexInit(&probe.lock));assert(xrtCondInit(&probe.changed));
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    bool ok=true;
    for(int stream=XPROCESS_STDIN;stream<=XPROCESS_STDERR;++stream)ok=closeRace((xprocessstream)stream) && ok;
    ok=failureCases(false) && ok;ok=failureCases(true) && ok;
    successCases(false);successCases(true);
    assert(xrtCondUnit(&probe.changed));assert(xrtMutexUnit(&probe.lock));
    xrtClearError();balanced(&before);
    printf("process IO loan: 3 deterministic close races, 8 failure guards, short/EOF/zero/peer guards, balanced; safe=%d\n",ok);
    return ok ? 0 : 1;
}
