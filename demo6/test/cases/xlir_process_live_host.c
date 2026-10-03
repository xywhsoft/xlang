/* Real private child + white-box waiter retirement. Public wait is deliberately
 * NOT a waiter join; this fixture retains a native observer to finish its heap
 * ledger deterministically. No sleeps, shell, network, GUI or user files. */
#define XRT_MODULE_PROCESS
#define XRT_MODULE_IO
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
#include "src/runtime/xl6_process_live.c"
#include "src/runtime/xl6_process_stream.c"
#include "src/runtime/xl6_process_status.c"
#include <assert.h>
#include <stdio.h>

static xstrview view(const char* text) { return (xstrview){text,strlen(text)}; }
static XL6_ProcessCommand* command(const char* child,const char* mode,const char* value)
{
    xvalue* args=xrtValueArray(); assert(args);
    assert(xrtValueArrayAppendNew(args,xrtValueString(view(mode))));
    if (value) assert(xrtValueArrayAppendNew(args,xrtValueString(view(value))));
    XL6_ProcessCommand* result=xl6ProcessCommandDirectV1(view(child),args,-1,-1);
    xrtValueRelease(args); assert(result); return result;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void retire(xprocess* observer)
{
    assert(xrtProcessWait(observer)==XWAIT_OK);
    assert(xrtThreadWait(observer->Waiter)==XWAIT_OK);
    xrtProcessDestroy(observer);
}
static void readerDrop(xlir_io_reader* reader)
{ assert(xrtReaderDestroy(reader->stream)); xrtFree(reader); }
static void writerDrop(xlir_io_writer* writer)
{ assert(xrtWriterDestroy(writer->stream)); xrtFree(writer); }
static unsigned factorySweep(XL6_ProcessLive* process,bool reader)
{
    unsigned failures=0;
    for (size_t point=0;point<16;++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        void* projection=reader ? xl6ProcessReaderV1(process,1) : xl6ProcessWriterV1(process);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (!hit) {
            assert(projection && !xrtGetError());
            if (reader) readerDrop(projection); else writerDrop(projection);
            balanced(&before); return failures;
        }
        assert(!projection && xrtErrorKind(xrtGetError())==XERR_MEMORY);
        xrtClearError(); ++failures;
        assert(process->refs==1 && !process->leased && !process->closed);
        balanced(&before);
    }
    abort();
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    XL6_ProcessCommand* plan=command(argv[1],"echo",NULL);
    /* Wrapper-only failures are before spawn. Native spawn's full allocation
     * prefix is already exercised separately; these are not OS fault seams. */
    for (size_t point=0;point<2;++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point)); assert(!xl6ProcessSpawnV1(plan));
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(xrtErrorKind(xrtGetError())==XERR_MEMORY); xrtClearError(); balanced(&before);
    }
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    XL6_ProcessLive* process=xl6ProcessSpawnV1(plan); assert(process);
    xprocess* observer=xrtProcessRef(process->native); assert(observer);
    assert(xl6ProcessIdV1(process)>0 && xl6ProcessRunningV1(process) && xl6ProcessStateV1(process)==0);
    assert(!xl6ProcessWaitReadyV1(process,0) && !xrtGetError());
    assert(!xl6ProcessWaitReadyV1(process,-2) && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    assert(!xl6ProcessWaitReadyV1(process,INT64_MAX) && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    assert(!xl6ProcessLiveStatusV1(process) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(!xl6ProcessReaderV1(process,0) && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    xlir_io_reader* output=xl6ProcessReaderV1(process,1); assert(output);
    xlir_io_reader* errors=xl6ProcessReaderV1(process,2); assert(errors);
    xlir_io_writer* input=xl6ProcessWriterV1(process); assert(input && process->refs==4);
    assert(!xl6ProcessReaderV1(process,1) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(!xl6ProcessWriterV1(process) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(!xrtReaderCanSeek(output->stream) && !xrtReaderCanSize(output->stream));
    assert(!xrtWriterCanSeek(input->stream) && !xrtWriterCanSize(input->stream));
    const unsigned char binary[]={'a',0,0xe4,0xbd,0xa0,'b'};
    size_t written=0;
    assert(xrtWriterWriteFull(input->stream,binary,sizeof(binary),&written) && written==sizeof(binary) && xrtWriterFlush(input->stream));
    const xerror* ambient; xrtSetErrorKind(XERR_IO); ambient=xrtGetError();
    xl6ProcessDestroyV1(process); assert(xrtGetError()==ambient); xrtClearError();
    /* The language Process is gone; all three stream operations still work. */
    writerDrop(input);
    unsigned char data[16]; size_t actual=0;
    assert(xrtReaderReadFull(output->stream,data,sizeof(binary),&actual) && actual==sizeof(binary) && !memcmp(data,binary,sizeof(binary)));
    assert(xrtReaderRead(output->stream,data,sizeof(data),&actual) && actual==0);
    assert(xrtReaderReadFull(errors->stream,data,3,&actual) && actual==3 && !memcmp(data,"E\0R",3));
    readerDrop(output); readerDrop(errors); retire(observer); balanced(&before);

    unsigned counts[2];
    for (unsigned i=0;i<2;++i) {
        XL6_ProcessCommand* zero=command(argv[1],"exit","7");
        xrtMemDebugSnapshot(&before);
        process=xl6ProcessSpawnV1(zero); assert(process); observer=xrtProcessRef(process->native); assert(observer);
        assert(xl6ProcessWaitReadyV1(process,-1)); assert(xrtThreadWait(observer->Waiter)==XWAIT_OK);
        counts[i]=factorySweep(process,i==0);
        assert(!xl6ProcessCloseV1(process,3) && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
        assert(xl6ProcessCloseV1(process,i==0?1:0));
        assert(!(i==0 ? xl6ProcessReaderV1(process,1) : xl6ProcessWriterV1(process)) && xrtErrorKind(xrtGetError())==XERR_CLOSED); xrtClearError();
        assert(xrtMemDebugFailAfter(0)); assert(!xl6ProcessLiveStatusV1(process));
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); assert(xrtErrorKind(xrtGetError())==XERR_MEMORY); xrtClearError();
        void* status=xl6ProcessLiveStatusV1(process); assert(status && xl6ProcessStatusCodeV1(status)==7);
        xl6ProcessDestroyV1(process); retire(observer);
        assert(xl6ProcessStatusKindV1(status)==XPROCESS_EXIT_CODE && xl6ProcessStatusCodeV1(status)==7);
        xl6ProcessStatusDestroyV1(status); balanced(&before); xl6ProcessCommandDestroyV1(zero);
    }
    process=xl6ProcessSpawnV1(plan); assert(process); observer=xrtProcessRef(process->native); assert(observer);
    xl6ProcessDestroyV1(process); assert(xrtProcessState(observer)==XPROCESS_RUNNING);
    assert(xrtProcessClose(observer,XPROCESS_STDIN)); retire(observer);
    XL6_ProcessCommand* nullIo=command(argv[1],"exit","0");
    xl6ProcessCommandStdioV1(nullIo,1,XPROCESS_IO_NULL);
    process=xl6ProcessSpawnV1(nullIo); assert(process); observer=xrtProcessRef(process->native); assert(observer);
    assert(!xl6ProcessReaderV1(process,1) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    xl6ProcessDestroyV1(process); retire(observer); xl6ProcessCommandDestroyV1(nullIo);
    xl6ProcessCommandDestroyV1(plan);
    printf("live process: exact binary streams survive parent drop; unique leases, timeout/range/state, independent status; reader/writer OOM %u/%u failures + 2 first no-hit, balanced\n",counts[0],counts[1]);
    return 0;
}
