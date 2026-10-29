#include "src/runtime/xl6_text_value.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
static DWORD WINAPI wrong_thread(void* control)
#else
#include <pthread.h>
static void* wrong_thread(void* control)
#endif
{
    xl6ObjectPoolControlEnterV1(control);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();
    return 0;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    void* state=xl6ObjectPoolControlCreateV1(); assert(state);
#if defined(_WIN32)
    HANDLE thread=CreateThread(NULL,0,wrong_thread,state,0,NULL);assert(thread);
    assert(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0);assert(CloseHandle(thread));
#else
    pthread_t thread;assert(!pthread_create(&thread,NULL,wrong_thread,state));assert(!pthread_join(thread,NULL));
#endif
    xl6ObjectPoolControlEnterV1(state);assert(!xrtGetError());
    xl6ObjectPoolControlCloseV1(state);assert(xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError(); assert(!xl6ObjectPoolControlClosedV1(state));
    xl6ObjectPoolControlLeaveV1(state);
    xl6ObjectPoolControlCloseV1(state);assert(!xrtGetError() && xl6ObjectPoolControlClosedV1(state));
    xl6ObjectPoolControlEnterV1(state);assert(xrtErrorKind(xrtGetError())==XERR_CLOSED);
    xrtClearError();xl6ObjectPoolControlDestroyV1(state);
    bool enabled=xl6MemoryStatisticsEnabledV1();xl6MemoryStatsEnableV1(true);
    void* snapshot=xl6MemoryStatsCaptureV1();assert(snapshot && xl6MemoryStatsEnabledV1(snapshot));
    assert(xl6MemoryStatsClassCountV1(snapshot)==64);
    xl6MemoryStatsEnableV1(false);assert(xl6MemoryStatsEnabledV1(snapshot));
    xl6MemoryStatsClassCallsV1(snapshot,-1);assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
    xrtClearError();xl6MemoryStatsClassBytesV1(snapshot,64);assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
    xrtClearError();xl6MemoryStatsDestroyV1(snapshot);xl6MemoryStatsEnableV1(enabled);
    xstrview text=xl6MemoryReportTextV1(0);assert(!text.Data && xrtGetError());xrtClearError();
    text=xl6MemoryReportJsonV1(16777216);assert(text.Data && text.Size);xrtFree((void*)text.Data);
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    typedef void* (*Capture)(void);typedef void (*Drop)(void*);
    Capture capture[]={xl6MemoryStatsCaptureV1,xl6MemoryDebugCaptureV1,xl6ObjectPoolControlCreateV1};
    Drop drop[]={xl6MemoryStatsDestroyV1,xl6MemoryDebugSnapshotDestroyV1,xl6ObjectPoolControlDestroyV1};
    for(size_t family=0;family<3;++family) {
        size_t failures=0;bool complete=false;
        for(size_t point=0;point<128;++point) {
            xl6ProductMemoryBegin(point);void* value=capture[family]();
            bool hit=xl6ProductMemoryEnd();
            if(hit){assert(!value);++failures;}else{assert(value);complete=true;}
            if(value)drop[family](value);
            xrtClearError();xl6ProductMemoryVerify();if(complete)break;
        }
        assert(complete && failures);
        printf("factory %zu: %zu complete allocation failures\n",family,failures);
    }
    puts("foundation native: checked admission, immutable snapshots, thread refusal and heap balanced");
    return 0;
}
