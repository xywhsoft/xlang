#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_closure.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* Independent reproducer: no capture runtime, task, Watch or XLIR-generated
 * cleanup participates. Both ordinary terminal release paths must allow a
 * finalizer to wait for a different thread's whole-graph inspection. */
static unsigned finalized, isolated;
static DWORD WINAPI probe(void* unused)
{
    xrtownershipscope freeze={0};DWORD begin=GetTickCount();(void)unused;
    do{
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            assert(xrtOwnershipScopeEnd(&freeze));++isolated;return 0;
        }
        SwitchToThread();
    }while((DWORD)(GetTickCount()-begin)<250);
    return 1;
}
static bool finalize(xvalue* value,void* unused)
{
    (void)unused;assert(value);++finalized;
    HANDLE thread=CreateThread(NULL,0,probe,NULL,0,NULL);
    assert(thread && WaitForSingleObject(thread,5000)==WAIT_OBJECT_0 && CloseHandle(thread));
    return true;
}
static void entry(void) {}
int main(int argc,char** argv)
{
    assert(argc==2);unsigned mode=(unsigned)strtoul(argv[1],NULL,10);assert(mode<2);
    xmemdebugsnapshot before,after;xrtownershipscope freeze={0};
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    xvalue* value=xl6ClassRuntimeV2Get()->CommitTake(xl6ClassRuntimeV2Get()->Create(NULL,finalize));assert(value);
    if(mode==0)xrtValueRelease(value);
    else{
        static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
        static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(xvalue*),1,&slot};
        xvalue** environment=xrtMalloc(sizeof(*environment));assert(environment);*environment=value;
        XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(NULL,entry,NULL,environment,&layout,NULL,NULL,0);assert(closure);
        xl6ClosureRuntimeV1Get()->Release(closure);
    }
    assert(!xrtGetError() && finalized==1);xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    printf("Ordinary release mode %u: finalizers=%u isolated=%u, all threads joined and zero live delta\n",mode,finalized,isolated);
    if(isolated!=1){fputs("Ordinary class/closure release still holds ownership mutation across semantic finalization\n",stderr);return 1;}
    return 0;
}
