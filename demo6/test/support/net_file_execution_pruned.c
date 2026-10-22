#include "net-file-execution-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_execution_net_file.h"
#include <assert.h>
#include <stdio.h>
static void waitFuture(xfuture* future)
{assert(future);double end=xrtTimer()+10;while(xrtFutureState(future)==XFUTURE_PENDING){assert(xrtTimer()<end);xrtThreadYield();}}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.NodeCacheBytes=0;
#if defined(_WIN32)
    config.Backend=XNET_PORT_IOCP;
#else
    config.Backend=XNET_PORT_URING;
#endif
    XL6_ExecutionResourceV1* engine=xl6ExecutionNetEngineCreateV1(source,&config);assert(engine&&xl6ExecutionNetEngineStartV1(engine));
    xfileoptions options;xrtFileOptionsInit(&options);options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE;
    XL6_ExecutionResourceV1* file=xl6ExecutionNetFileCreateV1(source,engine,0,"net-file-pruned.bin",&options);assert(file);
    xfuture* future=xl6ExecutionNetFileWriteV1(file,0,XRT_BYTES_LITERAL("a\0b"),NULL);waitFuture(future);
    assert(xrtFutureState(future)==XFUTURE_RESOLVED&&(uintptr_t)xrtFutureValue(future)==3);xrtFutureDestroy(future);
    future=xl6ExecutionNetFileReadV1(file,0,8,NULL);waitFuture(future);assert(xrtFutureState(future)==XFUTURE_RESOLVED);
    xbuffer* bytes=xrtFutureValue(future);assert(bytes&&bytes->Size==3&&bytes->Data[0]=='a'&&!bytes->Data[1]&&bytes->Data[2]=='b');xrtFutureDestroy(future);
    future=xl6ExecutionNetFileCloseSourceV1(file);waitFuture(future);assert(xrtFutureState(future)==XFUTURE_RESOLVED);xrtFutureDestroy(future);
    xl6ExecutionResourceReleaseV1(file);xl6ExecutionResourceReleaseV1(engine);
    assert(xl6ExecutionSourceCloseV1(source));double end=xrtTimer()+10;
    while(source){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&source);assert(result.ok);if(result.ready)break;assert(xrtTimer()<end);xrtThreadYield();}
    assert(remove("net-file-pruned.bin")==0);xrtClearError();assert(xrtMemDebugReset());
    puts("NetFile pruned: actual write/read/close; no default task pool or helper executor");return 0;
}
