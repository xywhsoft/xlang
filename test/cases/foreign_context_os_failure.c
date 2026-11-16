/* Compile the real resident implementation with only its OS key operations
 * fault-injected. Link XRT from the qualified complete runtime, never a mock. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
static DWORD keys[32];
#else
#include <errno.h>
#include <pthread.h>
static pthread_key_t keys[32];
#endif
static unsigned allocations, releases, live, failAt;
static bool failSet;
#if defined(_WIN32)
static DWORD WINAPI allocateKey(PFLS_CALLBACK_FUNCTION callback)
{
    ++allocations;
    if(allocations==failAt)return FLS_OUT_OF_INDEXES;
    DWORD key=FlsAlloc(callback);
    assert(key!=FLS_OUT_OF_INDEXES && live<32);
    keys[live++]=key;
    return key;
}
static BOOL WINAPI releaseKey(DWORD key)
{
    assert(live && keys[live-1]==key);
    BOOL ok=FlsFree(key);assert(ok);--live;++releases;return ok;
}
static BOOL WINAPI setKey(DWORD key,void* value)
{
    if(failSet){failSet=false;return FALSE;}
    return FlsSetValue(key,value);
}
#define FlsAlloc allocateKey
#define FlsFree releaseKey
#define FlsSetValue setKey
#else
static int allocateKey(pthread_key_t* key,void (*destroy)(void*))
{
    ++allocations;
    if(allocations==failAt)return EAGAIN;
    int status=pthread_key_create(key,destroy);
    assert(!status && live<32);keys[live++]=*key;return status;
}
static int releaseKey(pthread_key_t key)
{
    assert(live && keys[live-1]==key);
    int status=pthread_key_delete(key);assert(!status);--live;++releases;return status;
}
static int setKey(pthread_key_t key,const void* value)
{
    if(failSet){failSet=false;return ENOMEM;}
    return pthread_setspecific(key,value);
}
#define pthread_key_create allocateKey
#define pthread_key_delete releaseKey
#define pthread_setspecific setKey
#endif
/* Avoid interposing this private source probe on the complete runtime's own
 * exported context functions. Names change; the implementation does not. */
#define xl6ForeignContextReadyV1 probeReady
#define xl6ForeignContextGetV1 probeGet
#define xl6ForeignContextTryGetV1 probeTryGet
#define xl6ForeignContextSetV1 probeSet
#define xl6ForeignContextCallEnterV1 probeEnter
#define xl6ForeignContextCallContainsV1 probeContains
#define xl6ForeignContextCallOwnsV1 probeOwns
#define xl6ForeignContextCallLeaveV1 probeLeave
#include "src/runtime/xl6_foreign_context.c"

int main(int argc,char** argv)
{
    assert(argc==2);failAt=(unsigned)strtoul(argv[1],NULL,10);
    assert(failAt<=XL6_FOREIGN_CONTEXT_KEY_COUNT_V1);
    assert(!probeTryGet(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1) && !allocations);
    if(failAt){
        assert(!probeReady());
        assert(allocations==failAt && releases==failAt-1 && !live);
        const xerror* error=xrtGetError();assert(error && xrtErrorKind(error)==XERR_MEMORY);
        assert(!probeTryGet(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1));
        assert(xrtGetError()==error && allocations==failAt);
        xrtClearError();failAt=0;
    }
    assert(probeReady());assert(live==XL6_FOREIGN_CONTEXT_KEY_COUNT_V1);
    unsigned readyAllocations=allocations;
    assert(probeReady() && allocations==readyAllocations);
    XL6_ForeignCallContextV1 a={0},b={0},c={0};
    failSet=true;
    assert(!probeEnter(&a,&a) && !a.self && !a.older && !a.newer && !a.owner);
    assert(!probeTryGet(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1));
    assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
    assert(probeEnter(&a,&a) && probeEnter(&b,&b) && probeEnter(&c,&c));
    assert(probeContains(&a) && probeContains(&b) && probeContains(&c));
    assert(probeOwns(&a) && probeOwns(&b) && probeOwns(&c));
    probeLeave(&b);
    assert(!probeContains(&b) && !probeOwns(&b) && a.newer==&c && c.older==&a);
    probeLeave(&c);probeLeave(&a);
    assert(!probeTryGet(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1));
    assert(!xrtGetError() && allocations==readyAllocations);
    printf("FOREIGN_CONTEXT_OS_FAILURE_ROLLBACK_OK slots=%u\n",
        (unsigned)XL6_FOREIGN_CONTEXT_KEY_COUNT_V1);
    return 0;
}
