#ifndef XL_API_LOCK_H
#define XL_API_LOCK_H
/* Private host lifecycle lock, one instance in the owning API translation
 * unit. Read-only admission uses TryLock BEFORE a nonblocking XRT Freeze. */
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION xl__apiLock;
static volatile LONG xl__apiLockState = 0;

static void xlApiEnsureLock(void)
{
    LONG state;

    state = InterlockedCompareExchange(&xl__apiLockState, 1, 0);
    if ( state == 0 ) {
        InitializeCriticalSection(&xl__apiLock);
        InterlockedCompareExchange(&xl__apiLockState, 2, 1);
        return;
    }
    while ( xl__apiLockState != 2 ) {
        Sleep(0);
    }
}

static void xlApiLock(void)
{
    xlApiEnsureLock();
    EnterCriticalSection(&xl__apiLock);
}

static void xlApiUnlock(void)
{
    LeaveCriticalSection(&xl__apiLock);
}
static int xlApiTryLock(void)
{
    xlApiEnsureLock();
    return TryEnterCriticalSection(&xl__apiLock) != 0;
}

#else
#include <pthread.h>
static pthread_mutex_t xl__apiLock;
static pthread_once_t xl__apiLockOnce = PTHREAD_ONCE_INIT;

static void xlApiInitLock(void)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&xl__apiLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void xlApiLock(void)
{
    pthread_once(&xl__apiLockOnce, xlApiInitLock);
    pthread_mutex_lock(&xl__apiLock);
}

static void xlApiUnlock(void)
{
    pthread_mutex_unlock(&xl__apiLock);
}
static int xlApiTryLock(void)
{
    pthread_once(&xl__apiLockOnce, xlApiInitLock);
    return pthread_mutex_trylock(&xl__apiLock) == 0;
}

#endif
#endif
