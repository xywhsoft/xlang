#include "src/runtime/xl6_process_internal.h"
#include "src/runtime/xl6_fs_file.h"
#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#endif

static void processFileSystemError(int32_t code)
{
    xerrordesc error={.Kind=XERR_IO,.Domain="xlang.process.file",.Code=code,
        .Operation="redirect",.Message="cannot duplicate process redirection handle",.SystemCode=code};
    xerror* owned=xrtErrorBuild(&error);
    if(owned)xrtSetErrorTake(owned);
    else if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);
}
void xl6ProcessCommandFileV1(void* handle,int64_t stream,void* fileHandle)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    if(stream<XPROCESS_STDIN || stream>XPROCESS_STDERR){xrtSetErrorKind(XERR_RANGE);return;}
    if(command->config.Terminal){xrtSetErrorKind(XERR_UNSUPPORTED);return;}
    xlir_fs_file* owner=fileHandle;xfile file=xlir_fs_file_get(owner);if(!file)return;
    uint64_t capability=stream==XPROCESS_STDIN ? XFILE_CAP_READ : XFILE_CAP_WRITE;
    if(!(xrtFileCapabilities(file)&capability)){xrtSetErrorKind(XERR_UNSUPPORTED);return;}
    if(!xlir_fs_file_ref(owner))return;
    xprocessio io=xrtProcessFile(file);
    intptr_t copy=-1;
    if(io.Handle!=-1){
#if defined(_WIN32)
        HANDLE duplicate=NULL;
        if(DuplicateHandle(GetCurrentProcess(),(HANDLE)io.Handle,GetCurrentProcess(),
            &duplicate,0,FALSE,DUPLICATE_SAME_ACCESS))copy=(intptr_t)duplicate;
        else processFileSystemError((int32_t)GetLastError());
#elif defined(F_DUPFD_CLOEXEC)
        int duplicate;
        do{duplicate=fcntl((int)io.Handle,F_DUPFD_CLOEXEC,STDERR_FILENO+1);}while(duplicate<0 && errno==EINTR);
        if(duplicate>=0)copy=duplicate;else processFileSystemError(errno);
#else
        /* No racy dup-then-CLOEXEC fallback while another thread may spawn. */
        xrtSetErrorKind(XERR_UNSUPPORTED);
#endif
    }
    xerror* error=xrtTakeError();xlir_fs_file_drop(owner);
    xrtClearError();xrtSetErrorTake(error);if(copy==-1)return;
    io.Handle=copy;
    xprocessio* target=stream==XPROCESS_STDIN ? &command->config.Stdin :
        stream==XPROCESS_STDOUT ? &command->config.Stdout : &command->config.Stderr;
    XL6_ProcessIoLease old=command->stdio[stream];
    *target=io;command->stdio[stream]=(XL6_ProcessIoLease){copy,true};
    xl6ProcessIoLeaseDrop(&old);
}
