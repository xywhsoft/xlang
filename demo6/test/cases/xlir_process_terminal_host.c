/* Actual selected terminal backend, adapters and private child. No terminal
 * emulation, shell, fault seam or success skip. Join the private native waiter
 * only to finish the test's physical heap/OS-handle ledger deterministically. */
#define XRT_MODULE_PROCESS_TERMINAL
#define XRT_MODULE_IO
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_process_command.c"
#include "src/runtime/xl6_process_terminal_config.c"
#include "src/runtime/xl6_process_terminal_live.c"
#include "src/runtime/xl6_process_live.c"
#include "src/runtime/xl6_process_stream.c"
#include "src/runtime/xl6_process_status.c"
#include <assert.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif
static unsigned terminalHandles(void)
{
#if defined(_WIN32)
    DWORD count;assert(GetProcessHandleCount(GetCurrentProcess(),&count));return count;
#else
    DIR* directory=opendir("/proc/self/fd");assert(directory);unsigned count=0;
    while(readdir(directory))++count;
    assert(!closedir(directory));return count;
#endif
}
static void terminalBalanced(const xmemdebugsnapshot* before,unsigned descriptors,unsigned platformRetention)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    if(after.LiveCount!=before->LiveCount || after.LiveBytes!=before->LiveBytes || terminalHandles()!=descriptors+platformRetention)
        fprintf(stderr,"terminal ledger live=%zu/%zu bytes=%zu/%zu handles=%u/%u\n",
            after.LiveCount,before->LiveCount,after.LiveBytes,before->LiveBytes,terminalHandles(),descriptors);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount==before->DoubleFreeCount && after.InvalidFreeCount==before->InvalidFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount && terminalHandles()==descriptors+platformRetention);
}
/* Windows 10 inbox ConPTY retains a terminated conhost process handle even
 * in the direct Win32 API control on this host. Measure, report and compare
 * that platform behavior; never claim global handle balance or patch opaque
 * HPCON storage. Linux's descriptor ledger remains exactly zero growth. */
static unsigned terminalPlatformControl(void)
{
#if defined(_WIN32)
    xprocessterminalapi api;assert(__xrtProcessTerminalApiLoad(&api));
    unsigned before=terminalHandles();
    HANDLE inputRead,inputWrite,outputRead,outputWrite,terminal;
    assert(CreatePipe(&inputRead,&inputWrite,NULL,0) && CreatePipe(&outputRead,&outputWrite,NULL,0));
    assert(SUCCEEDED(api.Create((COORD){80,24},inputRead,outputWrite,0,&terminal)));
    assert(CloseHandle(inputRead) && CloseHandle(outputWrite));api.Close(terminal);
    assert(CloseHandle(inputWrite) && CloseHandle(outputRead));
    unsigned after=terminalHandles();assert(after>=before);return after-before;
#else
    return 0;
#endif
}
static xstrview terminalView(const char* text){return (xstrview){text,strlen(text)};}
static XL6_ProcessCommand* terminalCommand(const char* child,const char* mode)
{
    xvalue* args=xrtValueArray();assert(args);
    assert(xrtValueArrayAppendNew(args,xrtValueString(terminalView(mode))));
    XL6_ProcessCommand* command=xl6ProcessCommandDirectV1(terminalView(child),args,-1,-1);
    xrtValueRelease(args);assert(command);return command;
}
static void terminalRetire(xprocess* observer)
{
    assert(xrtProcessWait(observer)==XWAIT_OK);
    assert(xrtThreadWait(observer->Waiter)==XWAIT_OK);xrtProcessDestroy(observer);
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true) && xrtProcessTerminalSupported());
    /* Initialize process-wide Windows loader/console caches before per-session
     * comparison; no allocation failure is enabled in these explicit controls. */
    XL6_ProcessCommand* warm=terminalCommand(argv[1],"exit");
    XL6_ProcessLive* warmProcess=xl6ProcessSpawnV1(warm);assert(warmProcess);
    xprocess* warmNative=xrtProcessRef(warmProcess->native);assert(warmNative);
    xl6ProcessDestroyV1(warmProcess);terminalRetire(warmNative);xl6ProcessCommandDestroyV1(warm);
    (void)terminalPlatformControl();
    unsigned platformRetention=terminalPlatformControl();
    assert(terminalPlatformControl()==platformRetention);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned descriptors=terminalHandles();
    xl6ProcessCommandTerminalV1(NULL,true);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    XL6_ProcessCommand* command=terminalCommand(argv[1],"terminal");
    const int64_t invalid[][2]={{0,24},{80,-1},{32768,24},{80,INT64_MAX}};
    uint32 columns=command->config.Columns,rows=command->config.Rows;
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i){
        xl6ProcessCommandTerminalSizeV1(command,invalid[i][0],invalid[i][1]);
        assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(command->config.Columns==columns && command->config.Rows==rows);
    }
    xl6ProcessCommandTerminalSizeV1(command,1,32767);
    xl6ProcessCommandTerminalSizeV1(command,32767,1);assert(!xrtGetError());
    xl6ProcessCommandTerminalV1(command,true);xl6ProcessCommandTerminalSizeV1(command,80,24);
    XL6_ProcessLive* process=xl6ProcessSpawnV1(command);assert(process);
    xprocess* observer=xrtProcessRef(process->native);assert(observer);
#if defined(_WIN32)
    HANDLE ownedProcess=observer->Process,ownedInput=observer->Stdin,ownedOutput=observer->Stdout;
#endif
    xl6ProcessCommandDestroyV1(command);
    assert(!xl6ProcessResizeV1(NULL,80,24) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i){
        assert(!xl6ProcessResizeV1(process,invalid[i][0],invalid[i][1]));
        assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    }
    assert(xl6ProcessResizeV1(process,96,32));
    assert(!xl6ProcessReaderV1(process,2) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xlir_io_reader* output=xl6ProcessReaderV1(process,1);assert(output);
    xlir_io_writer* input=xl6ProcessWriterV1(process);assert(input);
    assert(!xl6ProcessReaderV1(process,1) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    size_t actual=0;assert(xrtWriterWriteFull(input->stream,"hello\r\n",7,&actual) && actual==7);
    assert(xrtWriterFlush(input->stream));
    /* Stream leases keep the Process alive independently of its language owner. */
    xl6ProcessDestroyV1(process);
    char data[16384];size_t used=0;
    do {assert(used<sizeof(data)-1 && xrtReaderRead(output->stream,data+used,sizeof(data)-1-used,&actual));used+=actual;}while(actual);
    data[used]=0;
    assert(strstr(data,"tty:1:1:1") && strstr(data,"terminal-stderr"));
    assert(strstr(data,"size:96:32") && strstr(data,"reply:hello"));
    assert(xrtWriterDestroy(input->stream));xrtFree(input);
    assert(xrtReaderDestroy(output->stream));xrtFree(output);
    terminalRetire(observer);xrtClearError();
#if defined(_WIN32)
    DWORD flags;
    assert(!GetHandleInformation(ownedProcess,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
    assert(!GetHandleInformation(ownedInput,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
    assert(!GetHandleInformation(ownedOutput,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
#endif
    terminalBalanced(&before,descriptors,platformRetention);

    /* Dimension validation does not fabricate support for ordinary pipes. */
    xrtMemDebugSnapshot(&before);descriptors=terminalHandles();
    command=terminalCommand(argv[1],"exit");process=xl6ProcessSpawnV1(command);assert(process);
    observer=xrtProcessRef(process->native);assert(observer);
    assert(!xl6ProcessResizeV1(process,80,24) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6ProcessDestroyV1(process);terminalRetire(observer);xl6ProcessCommandDestroyV1(command);
    xrtClearError();terminalBalanced(&before,descriptors,0);
    printf("terminal: real tty/merged stderr, child-observed resize, independent stream leases; 13 guards, heap/owned handles balanced; raw Win32 platform retention=%u, no extra handles\n",platformRetention);
    return 0;
}
