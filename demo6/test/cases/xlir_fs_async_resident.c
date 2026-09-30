#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_task.h"
#include <assert.h>
#include <stdio.h>

static XL6_TaskPool* pool_new(void)
{
    XL6_TaskPoolConfig config={2,64,0};
    XL6_TaskPool* pool=xl6TaskRuntimeV1Get()->PoolCreate(&config);
    assert(pool);return pool;
}
static int32 retire_pool(void* data)
{
    bool ok=xl6TaskRuntimeV1Get()->PoolQuiesce(data);
    if(!ok){const xerror* error=xrtGetError();fprintf(stderr,"retire failed: %s\n",error?xrtErrorMessage(error):"no error");}
    assert(ok);return 0;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xfileoptions options; xrtFileOptionsInit(&options);options.Flags=15;
    XL6_TaskPool* pool=pool_new();
    XL6_AsyncFile* file=xl6AsyncFileOpenV1(pool,"build/test/async-resident.bin",&options);
    assert(file && !xl6AsyncFileClosedV1(file));
    xfuture* write=xl6AsyncFileWriteV1(file,0,(const uint8*)"a\0b",3);assert(write);
    // Retire without destroying the language owner first: registry closure must
    // release the native resource credit, so the pool join cannot wait forever.
    assert(xl6TaskRuntimeV1Get()->PoolQuiesce(pool));
    assert(xl6AsyncFileClosedV1(file));
    assert(xrtFutureWait(write)==XWAIT_OK);xfutureresult result={0};assert(xrtFutureResult(write,&result));
    assert(result.State==XFUTURE_RESOLVED && ((xfilechange*)result.Value)->Size==3);
    xrtFutureDestroy(write);
    xfuture* close=xl6AsyncFileCloseV1(file);assert(close && xrtFutureWait(close)==XWAIT_OK);xrtFutureDestroy(close);
    assert(xl6TaskRuntimeV1Get()->PoolDestroy(pool));
    xl6AsyncFileDestroyV1(file); // Last shell reference after module owner release.

    pool=pool_new();XL6_TaskNativeScopeV1 scope={0};
    assert(xl6TaskNativeBeginV1(pool,&scope));
    assert(xl6TaskPoolTryQuiesceV1(pool)==XL6_MODULE_PREPARE_BUSY);
    xl6TaskNativeEndV1(&scope);
    assert(xl6TaskRuntimeV1Get()->PoolQuiesce(pool));
    assert(xl6TaskRuntimeV1Get()->PoolDestroy(pool));
    xrtClearError();

    // Warm the immutable OOM diagnostic before exact transaction accounting.
    xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    xmemdebugsnapshot baseline,final;
    xrtMemDebugSnapshot(&baseline);

    unsigned failures=0,successes=0;
    for(unsigned point=0;point<64;++point){
        pool=pool_new();xfile raw=xrtOpen("build/test/async-resident.bin",15);assert(raw);
        assert(xrtMemDebugFailAfter(point));
        file=xl6AsyncFileAdoptV1(pool,raw);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!file){assert(triggered);++failures;xrtClearError();assert(xrtClose(raw));}
        else{++successes;xl6AsyncFileDestroyV1(file);}
        assert(xl6TaskRuntimeV1Get()->PoolQuiesce(pool));
        assert(xl6TaskRuntimeV1Get()->PoolDestroy(pool));xrtClearError();
    }
    assert(failures && successes);
    for(unsigned round=0;round<64;++round){
        pool=pool_new();
        file=xl6AsyncFileOpenV1(pool,"build/test/async-resident.bin",&options);assert(file);
        write=xl6AsyncFileWriteV1(file,0,(const uint8*)"race",4);assert(write);
        xthread* thread=xrtThreadCreate(retire_pool,pool,0);assert(thread);
        close=xl6AsyncFileCloseV1(file);assert(close && xrtFutureWait(close)==XWAIT_OK);
        assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
        xrtFutureDestroy(write);xrtFutureDestroy(close);
        assert(xl6TaskRuntimeV1Get()->PoolDestroy(pool));xl6AsyncFileDestroyV1(file);
    }
    xrtMemDebugSnapshot(&final);
    assert(final.LiveCount==baseline.LiveCount && final.LiveBytes==baseline.LiveBytes);
    assert(final.InvalidFreeCount==baseline.InvalidFreeCount &&
        final.DoubleFreeCount==baseline.DoubleFreeCount && final.UseAfterFreeCount==baseline.UseAfterFreeCount);
    assert(xrtFileDelete("build/test/async-resident.bin"));
    puts("resident async lifecycle, adoption OOM and retirement races ok");return 0;
}
