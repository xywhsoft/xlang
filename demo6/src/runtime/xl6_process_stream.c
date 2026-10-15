#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_io_handle.h"

typedef struct ProcessStream {
    XL6_ProcessLive* process;
    xprocessstream stream;
} ProcessStream;
static bool streamReserve(ProcessStream* context,XL6_ProcessLive* process,xprocessstream stream)
{
    unsigned bit=1u<<(unsigned)stream;
    xl6ProcessLiveLock(process);
    xerrkind error=XERR_NONE;
    if (!(process->pipes&bit)) error=XERR_STATE;
    else if (process->closed&bit) error=XERR_CLOSED;
    else if (process->leased&bit) error=XERR_STATE;
    if (error!=XERR_NONE) {
        xl6ProcessLiveUnlock(process); xrtSetErrorKind(error); return false;
    }
    if (xrtRefRetain(&process->refs)<0) abort();
    process->leased|=bit;
    context->process=process; context->stream=stream;
    xl6ProcessLiveUnlock(process); return true;
}
static void streamRelease(ProcessStream* context)
{
    XL6_ProcessLive* process=context->process;
    xl6ProcessLiveLock(process); process->leased&=~(1u<<(unsigned)context->stream);
    xl6ProcessLiveUnlock(process);
    xl6ProcessDestroyV1(process); xrtFree(context);
}
static bool streamClose(void* data)
{
    ProcessStream* context=data;
    xerror* prior=xrtTakeError();
    bool ok=xl6ProcessCloseV1(context->process,context->stream);
    xerror* error=xrtTakeError();
    streamRelease(context);
    /* Explicit close transports its failure; destruction is no-raise in the
     * owning IO projection. Parent release cannot erase the close cause. */
    xrtSetErrorTake(error ? error : prior);
    if (error) xrtErrorFree(prior);
    return ok;
}
static bool streamRead(void* data,void* buffer,size_t size,size_t* actual)
{
    ProcessStream* context=data;
    int64 count=xrtProcessRead(context->process->native,context->stream,buffer,size);
    if (count<0) return false;
    *actual=(size_t)count; return true;
}
static bool streamWrite(void* data,const void* buffer,size_t size,size_t* actual)
{
    ProcessStream* context=data;
    int64 count=xrtProcessWrite(context->process->native,buffer,size);
    if (count<0) return false;
    *actual=(size_t)count; return true;
}
void* xl6ProcessReaderV1(void* handle,int64_t stream)
{
    XL6_ProcessLive* process=xl6ProcessLiveCheck(handle);
    if (!process) return NULL;
    if (stream!=XPROCESS_STDOUT && stream!=XPROCESS_STDERR) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    xlir_io_reader* reader=xrtCalloc(1,sizeof(*reader)); if (!reader) return NULL;
    ProcessStream* context=xrtMalloc(sizeof(*context));
    if (!context) { xrtFree(reader); return NULL; }
    if (!streamReserve(context,process,(xprocessstream)stream)) {
        xrtFree(context); xrtFree(reader); return NULL;
    }
    const xreaderops ops={.Read=streamRead,.Close=streamClose};
    reader->stream=xrtReaderCreate(&ops,context);
    if (!reader->stream) {
        /* Failed creation does not own context. Roll back the lease/reference,
         * NOT the pipe, so an allocation failure is safely retryable. */
        streamRelease(context); xrtFree(reader); return NULL;
    }
    reader->origin=XL6_IO_ORIGIN_PROCESS;return reader;
}
void* xl6ProcessWriterV1(void* handle)
{
    XL6_ProcessLive* process=xl6ProcessLiveCheck(handle); if (!process) return NULL;
    xlir_io_writer* writer=xrtCalloc(1,sizeof(*writer)); if (!writer) return NULL;
    ProcessStream* context=xrtMalloc(sizeof(*context));
    if (!context) { xrtFree(writer); return NULL; }
    if (!streamReserve(context,process,XPROCESS_STDIN)) {
        xrtFree(context); xrtFree(writer); return NULL;
    }
    /* No buffering: flush is XRT's successful optional no-op. No seek/size. */
    const xwriterops ops={.Write=streamWrite,.Close=streamClose};
    writer->stream=xrtWriterCreate(&ops,context);
    if (!writer->stream) { streamRelease(context); xrtFree(writer); return NULL; }
    writer->origin=XL6_IO_ORIGIN_PROCESS;return writer;
}
