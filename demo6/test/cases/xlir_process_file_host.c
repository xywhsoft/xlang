/* Real selected native closure. No callback lifetime, artificial allocator,
 * sleeps, OS fault seam, shell, GUI, network or user files. Invalid native
 * handles below intentionally exercise the real DuplicateHandle/fcntl failure. */
#if !defined(XL6_PROCESS_PRODUCT_OBSERVER)
#define XRT_MODULE_PROCESS_FILE
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VFS_MEMORY
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#define XRT_MODULE_ONCE
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.c"
#include "src/runtime/xl6_native_handle.c"
#include "src/runtime/xl6_process_command.c"
#include "src/runtime/xl6_process_terminal_config.c"
#include "src/runtime/xl6_process_file.c"
#endif
#include <assert.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static unsigned handles(void)
{
#if defined(_WIN32)
    DWORD count;assert(GetProcessHandleCount(GetCurrentProcess(),&count));return count;
#else
    DIR* directory=opendir("/proc/self/fd");assert(directory);unsigned count=0;
    while(readdir(directory))++count;
    assert(!closedir(directory));return count;
#endif
}
static void balanced(const xmemdebugsnapshot* before,unsigned descriptors)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    if(after.LiveCount!=before->LiveCount || after.LiveBytes!=before->LiveBytes)
        fprintf(stderr,"ledger mismatch: live %zu/%zu bytes %zu/%zu handles %u/%u\n",
            after.LiveCount,before->LiveCount,after.LiveBytes,before->LiveBytes,handles(),descriptors);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount==before->DoubleFreeCount && after.InvalidFreeCount==before->InvalidFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount && descriptors==handles());
}
static xlir_fs_file* temporary(void)
{
    xlir_fs_file* result=xlir_fs_file_alloc();assert(result);
    result->file=xrtFileTemp(NULL,"xl-process-file-",".tmp",&result->tempPath);assert(result->file);
    result->deleteOnClose=true;return result;
}
static void closeFile(xlir_fs_file* file)
{
    assert(xrtAtomic32Load(&file->refs,XMEMORY_ACQUIRE)==1);
    assert(xlir_fs_temp_remove(file));xfile raw=file->file;file->file=NULL;
    assert(xrtClose(raw));xlir_fs_file_drop(file);
}
static bool valid(intptr_t handle)
{
#if defined(_WIN32)
    DWORD flags;return GetHandleInformation((HANDLE)handle,&flags)!=0;
#else
    return fcntl((int)handle,F_GETFD)>=0;
#endif
}
static void noInherit(intptr_t handle)
{
#if defined(_WIN32)
    DWORD flags;assert(GetHandleInformation((HANDLE)handle,&flags) && !(flags&HANDLE_FLAG_INHERIT));
#else
    assert(handle>STDERR_FILENO && (fcntl((int)handle,F_GETFD)&FD_CLOEXEC));
#endif
}
typedef struct WrongOwner{void* command;void* file;} WrongOwner;
static int32 otherThread(void* data)
{
    WrongOwner* probe=data;
    xl6ProcessCommandFileV1(probe->command,1,probe->file);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();return 0;
}
static unsigned errorPrefix(XL6_ProcessCommand* command,xlir_fs_file* invalid)
{
    intptr_t previous=command->config.Stdout.Handle;
    for(size_t point=0;point<32;++point){
        unsigned descriptors=handles();xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));xl6ProcessCommandFileV1(command,1,invalid);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();const xerror* error=xrtGetError();assert(error);
        if(hit)assert(xrtErrorKind(error)==XERR_MEMORY);
        else{
            assert(xrtErrorKind(error)==XERR_IO && xrtErrorSystemCode(error)!=0);
            assert(!strcmp(xrtErrorDomain(error),"xlang.process.file"));
        }
        assert(command->config.Stdout.Handle==previous && valid(previous));
        assert(xrtAtomic32Load(&invalid->refs,XMEMORY_ACQUIRE)==1);
        xrtClearError();balanced(&before,descriptors);if(!hit)return (unsigned)point;
    }
    abort();
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    XL6_ProcessCommand* command=xl6ProcessCommandShellV1((xstrview){"unused",6},-1);assert(command);
    xlir_fs_file* first=temporary();xlir_fs_file* second=temporary();
    unsigned baseHandles=handles();xmemdebugsnapshot base;xrtMemDebugSnapshot(&base);
    assert(xrtMemDebugFailAfter(0));xl6ProcessCommandFileV1(command,1,first);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(!xrtGetError());
    intptr_t previous=command->config.Stdout.Handle;assert(command->stdio[1].owned);noInherit(previous);
    assert(xrtAtomic32Load(&first->refs,XMEMORY_ACQUIRE)==1);
    xl6ProcessCommandFileV1(command,1,second);assert(!xrtGetError() && !valid(previous));
    previous=command->config.Stdout.Handle;
    xl6ProcessCommandTerminalV1(command,true);
    assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED && !command->config.Terminal);xrtClearError();
    xl6ProcessCommandFileV1(command,3,first);assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xl6ProcessCommandFileV1(command,1,NULL);assert(xrtErrorKind(xrtGetError())==XERR_CLOSED);xrtClearError();
    assert(command->config.Stdout.Handle==previous);
    WrongOwner probe={command,first};xthread* thread=xrtThreadCreate(otherThread,&probe,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);assert(!xrtGetError());
    xvfs vfs=xrtVfsCreate();xvfsmemory memory=xrtVfsMemoryCreate();assert(vfs && memory);
    assert(xrtVfsMemoryPutCopy(memory,"item","a\0b",3) && xrtVfsMemorySeal(memory));
    xvfsmount mount=xrtVfsMemoryMount(vfs,"/m",0,(xvfscase)0,memory,0);assert(mount);
    xfileoptions options;xrtFileOptionsInit(&options);options.Flags=XFILE_READ;
    xlir_fs_file* virtualFile=xlir_fs_file_alloc();assert(virtualFile);
    virtualFile->file=xrtVfsOpen(vfs,"/m/item",&options);assert(virtualFile->file);
    xl6ProcessCommandFileV1(command,0,virtualFile);
    assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);xrtClearError();
    xl6ProcessCommandFileV1(command,1,virtualFile);
    assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);xrtClearError();
    xlir_fs_file_drop(virtualFile);xrtVfsMountDestroy(mount);xrtVfsMemoryDestroy(memory);xrtVfsDestroy(vfs);
    // Real OS failure without modifying production OS primitives or allocators.
    xlir_fs_file* invalid=temporary();intptr_t raw=xrtFileNative(invalid->file);
    assert(xlir_fs_temp_remove(invalid));
#if defined(_WIN32)
    assert(CloseHandle((HANDLE)raw));
#else
    assert(!close((int)raw));
#endif
    unsigned failures=errorPrefix(command,invalid);
    xlir_fs_file_drop(invalid);xrtClearError();
    xl6ProcessCommandStdioV1(command,1,XPROCESS_IO_NULL);assert(!valid(previous));
    balanced(&base,baseHandles);
    // A closed original does not invalidate an owned input duplicate at spawn.
    const unsigned char binary[]={'a',0,0xe4,0xbd,0xa0,'b'};
    assert(xrtWriteFull(first->file,binary,sizeof(binary),NULL));assert(xrtSeek(first->file,0,XSEEK_START,NULL));
    xvalue* args=xrtValueArray();assert(args && xrtValueArrayAppendNew(args,xrtValueString((xstrview){"echo",4})));
    XL6_ProcessCommand* child=xl6ProcessCommandDirectV1((xstrview){argv[1],strlen(argv[1])},args,-1,-1);assert(child);
    xrtValueRelease(args);xl6ProcessCommandFileV1(child,0,first);xl6ProcessCommandFileV1(child,1,second);
    xlir_fs_file* errors=temporary();xl6ProcessCommandFileV1(child,2,errors);assert(!xrtGetError());
    closeFile(first);xprocess* native=xrtProcessSpawn(&child->config);assert(native);
    xl6ProcessCommandDestroyV1(child);
    assert(xrtProcessWait(native)==XWAIT_OK && xrtThreadWait(native->Waiter)==XWAIT_OK);
    xprocessstatus status;assert(xrtProcessStatus(native,&status) && status.Code==0);
    xrtProcessDestroy(native);unsigned char output[8];size_t size=0;
    assert(xrtReadAtFull(second->file,0,output,sizeof(binary),&size) && size==sizeof(binary) && !memcmp(output,binary,size));
    assert(xrtReadAtFull(errors->file,0,output,3,&size) && size==3 && !memcmp(output,"E\0R",size));
    closeFile(second);closeFile(errors);
    xrtSetErrorKind(XERR_IO);const xerror* ambient=xrtGetError();
    xl6ProcessCommandDestroyV1(command);assert(xrtGetError()==ambient);xrtClearError();
    assert(xrtMemDebugReset());assert(xrtMemDebugEnable(false));
    printf("process file: independent non-inheritable handles, replacement/rollback, original close, binary child, 5 guards; OS error prefix %u failures + 1 first no-hit, heap/handles balanced\n",failures);
    return 0;
}
