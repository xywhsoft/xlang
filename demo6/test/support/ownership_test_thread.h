#ifndef XL6_OWNERSHIP_TEST_THREAD_H
#define XL6_OWNERSHIP_TEST_THREAD_H
#include <assert.h>

typedef struct XL6_OwnershipTestThread {
    void (*work)(void*);
    void* data;
} XL6_OwnershipTestThread;

#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI xl6OwnershipTestThreadEntry(void* raw)
{
    XL6_OwnershipTestThread* thread=raw;thread->work(thread->data);return 0;
}
static void xl6OwnershipTestThreadRun(void (*work)(void*),void* data)
{
    XL6_OwnershipTestThread state={work,data};
    HANDLE thread=CreateThread(NULL,0,xl6OwnershipTestThreadEntry,&state,0,NULL);
    assert(thread && WaitForSingleObject(thread,3000)==WAIT_OBJECT_0 && CloseHandle(thread));
}
#else
#include <pthread.h>
#include <time.h>
static void* xl6OwnershipTestThreadEntry(void* raw)
{
    XL6_OwnershipTestThread* thread=raw;thread->work(thread->data);return NULL;
}
static void xl6OwnershipTestThreadRun(void (*work)(void*),void* data)
{
    XL6_OwnershipTestThread state={work,data};pthread_t thread;
    assert(pthread_create(&thread,NULL,xl6OwnershipTestThreadEntry,&state)==0);
#ifdef __linux__
    struct timespec deadline;assert(clock_gettime(CLOCK_REALTIME,&deadline)==0);deadline.tv_sec+=3;
    assert(pthread_timedjoin_np(thread,NULL,&deadline)==0);
#else
    assert(pthread_join(thread,NULL)==0);
#endif
}
#endif
#endif
