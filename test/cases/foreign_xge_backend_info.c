#include "foreign_xge_backend_info.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char platformName[]="legacy-platform";
static xmemdebugsnapshot before;
static double started;

static int no_op(void* user)
{
    (void)user;
    return 0;
}

void backendInstallLegacy(void)
{
    const xge_platform_backend_t platform={.iType=XGE_PLATFORM_BACKEND_CUSTOM,.sName=platformName,.poll=no_op};
    const xge_graphics_backend_t graphics={.iType=XGE_GPU_BACKEND_CUSTOM,.begin=no_op};
    assert(xgePlatformBackendSet(&platform)==XGE_OK);
    assert(xgeGraphicsBackendSet(&graphics)==XGE_OK);
}

void backendChangeLegacyName(void)
{
    platformName[0]='L';
}

typedef struct BackendRefs {
    xge_platform_backend_ref platform;
    xge_graphics_backend_ref graphics;
} BackendRefs;

static int window_sample(void* window,void* user);

static int32 check_other_thread(void* raw)
{
    const BackendRefs* refs=raw;
    xge_backend_info_t info={.iType=-1,.sName="canary",.iCallbackMask=79};
    bool visited=true;
    assert(xgePlatformBackendInfoGet(refs->platform,&info)==XGE_ERROR_BUSY);
    assert(xgeGraphicsBackendInfoGet(refs->graphics,&info)==XGE_ERROR_BUSY);
    assert(xgePlatformBackendCurrentInfo(&info)==XGE_ERROR_INVALID_STATE);
    assert(xgeGraphicsBackendCurrentInfo(&info)==XGE_ERROR_INVALID_STATE);
    assert(info.iType==-1 && strcmp(info.sName,"canary")==0 && info.iCallbackMask==79);
    assert(xgePlatformVisitNativeHandle(window_sample,NULL,&visited)==XGE_ERROR_INVALID_STATE && visited);
    return 0;
}

void backendCheckOtherThread(xge_platform_backend_ref platform,xge_graphics_backend_ref graphics)
{
    BackendRefs refs={platform,graphics};
    xthread* worker=xrtThreadCreate(check_other_thread,&refs,0);
    assert(worker && xrtThreadWait(worker)==XWAIT_OK);
    xrtThreadDestroy(worker);
}

int backendWindowMatches(void* window)
{
    return window && window==xgePlatformNativeHandle();
}

static int window_sample(void* window,void* user)
{
    (void)user;
    assert(backendWindowMatches(window));
    return XGE_OK;
}

static int window_refuse(void* window,void* user)
{
    (void)window;(void)user;
    return 1;
}

void backendCheckWindowAbi(void)
{
    bool visited=true;
    assert(xgePlatformVisitNativeHandle(window_refuse,NULL,&visited)==XGE_ERROR_CALLBACK_FAILED && visited);
    visited=false;
    assert(xgePlatformVisitNativeHandle(window_sample,NULL,&visited)==XGE_OK && visited);
}

void backendFailNextAllocation(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(0));
}

int backendClearFailure(void)
{
    int triggered=xrtMemDebugFailTriggered() ? 1 : 0;
    xrtMemDebugFailClear();
    return triggered;
}

void backendBeginMeasure(void)
{
    backendFailNextAllocation();
    xrtMemDebugSnapshot(&before);
    started=xrtTimer();
}

void backendEndMeasure(int language)
{
    double elapsed=xrtTimer()-started;
    assert(backendClearFailure()==0);
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount);
    assert(after.ReallocCount==before.ReallocCount && after.LiveCount==before.LiveCount);
    printf("XGE_WINDOW_SAMPLE language=%d count=1000 alloc=0 free=0 realloc=0 wallMs=%.3f\n",
        language,elapsed*1000.0);
}

void backendMeasureNative(void)
{
    bool visited=false;
    assert(xgePlatformVisitNativeHandle(window_sample,NULL,&visited)==XGE_OK && visited);
    backendBeginMeasure();
    for(int i=0;i<1000;++i)
        assert(xgePlatformVisitNativeHandle(window_sample,NULL,&visited)==XGE_OK && visited);
    backendEndMeasure(0);
}
