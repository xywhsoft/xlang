# Thread Management Library

> Multi-threading creation and management, including threads, mutexes, semaphores, condition variables

[English](api-thread.en.md) | [中文](api-thread.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constants](#constants)
- [Data Types](#data-types)
- [Thread Management](#thread-management)
- [Mutex](#mutex)
- [Semaphore](#semaphore)
- [Condition Variable](#condition-variable)
- [Use Cases](#use-cases)
- [Notes](#notes)

---

## Constants

### Thread States

```c
#define XRT_THREAD_STOPPED      0    // Stopped
#define XRT_THREAD_RUNNING      1    // Running
#define XRT_THREAD_SUSPENDED    2    // Suspended
```

### Wait Return Values

```c
#define XRT_WAIT_OK            0    // Wait successful
#define XRT_WAIT_TIMEOUT       1    // Wait timeout
#define XRT_WAIT_ERROR        -1    // Wait error
```

---

## Data Types

### xthread_struct

Thread data structure.

```c
typedef struct {
    ptr Handle;                    // Thread handle
    uint32 TID;                    // Thread ID
    uint32 (*Proc)(ptr param);     // User callback function
    ptr Param;                     // User parameter
    volatile int StopFlag;         // Stop signal flag
} xthread_struct, *xthread;
```

### xmutex_struct

Mutex data structure.

```c
typedef struct {
    ptr Handle;                    // Mutex handle
} xmutex_struct, *xmutex;
```

### xsem_struct

Semaphore data structure.

```c
typedef struct {
    ptr Handle;                    // Semaphore handle
} xsem_struct, *xsem;
```

### xcond_struct

Condition variable data structure.

```c
typedef struct {
    ptr Handle;                    // Condition variable handle
} xcond_struct, *xcond;
```

---

## Thread Management

### xrtThreadCreate

Create and start a new thread.

```c
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
```

**Parameters:**
- `pProc` - Thread function pointer
- `pParam` - Parameter to pass to thread
- `iStackSize` - Stack size (bytes, 0 uses system default)

**Returns:** Thread object pointer on success, `NULL` on failure

---

### xrtThreadDestroy

Destroy thread object (does not terminate thread, only releases management structure).

```c
XXAPI void xrtThreadDestroy(xthread pThread);
```

---

### xrtThreadWait

Wait for thread to finish (blocking).

```c
XXAPI void xrtThreadWait(xthread pThread);
```

---

### xrtThreadWaitTimeout

Wait for thread to finish (with timeout).

```c
XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout);
```

**Parameters:**
- `pThread` - Thread object
- `iTimeout` - Timeout in milliseconds

**Returns:** `XRT_WAIT_OK`/`XRT_WAIT_TIMEOUT`/`XRT_WAIT_ERROR`

---

### xrtThreadStop

Send stop signal. Thread must check `xrtThreadShouldStop`.

```c
XXAPI void xrtThreadStop(xthread pThread);
```

---

### xrtThreadShouldStop

Check if thread should stop (called from within thread).

```c
XXAPI bool xrtThreadShouldStop(xthread pThread);
```

**Returns:** `TRUE` if should stop

---

### xrtThreadKill

Forcefully terminate thread.

```c
XXAPI bool xrtThreadKill(xthread pThread);
```

**Warning:** Dangerous operation, may cause resource leaks. Not recommended.

---

### xrtThreadSuspend

Suspend thread.

```c
XXAPI bool xrtThreadSuspend(xthread pThread);
```

**Note:** Only supported on Windows, returns `FALSE` on Linux/macOS

---

### xrtThreadResume

Resume thread.

```c
XXAPI bool xrtThreadResume(xthread pThread);
```

**Note:** Only supported on Windows, returns `FALSE` on Linux/macOS

---

### xrtThreadGetState

Get thread state.

```c
XXAPI int xrtThreadGetState(xthread pThread);
```

**Returns:** `XRT_THREAD_STOPPED`/`XRT_THREAD_RUNNING`/`XRT_THREAD_SUSPENDED`

---

### xrtThreadGetExitCode

Get thread exit code.

```c
XXAPI uint32 xrtThreadGetExitCode(xthread pThread);
```

---

### xrtThreadGetCurrentId

Get current thread ID.

```c
XXAPI uint32 xrtThreadGetCurrentId();
```

---

### xrtThreadYield

Yield current thread's time slice.

```c
XXAPI void xrtThreadYield();
```

---

## Mutex

### xrtMutexCreate

Create mutex.

```c
XXAPI xmutex xrtMutexCreate();
```

**Returns:** Mutex object on success, `NULL` on failure

---

### xrtMutexDestroy

Destroy mutex.

```c
XXAPI void xrtMutexDestroy(xmutex pMutex);
```

---

### xrtMutexInit / xrtMutexUnit

Initialize/release mutex (for self-maintained struct pointers).

```c
XXAPI void xrtMutexInit(xmutex pMutex);
XXAPI void xrtMutexUnit(xmutex pMutex);
```

---

### xrtMutexLock

Lock mutex (blocking).

```c
XXAPI void xrtMutexLock(xmutex pMutex);
```

---

### xrtMutexTryLock

Try to lock mutex (non-blocking).

```c
XXAPI bool xrtMutexTryLock(xmutex pMutex);
```

**Returns:** `TRUE` if lock successful

---

### xrtMutexUnlock

Unlock mutex.

```c
XXAPI void xrtMutexUnlock(xmutex pMutex);
```

---

## Semaphore

### xrtSemCreate

Create semaphore.

```c
XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue);
```

**Parameters:**
- `iInitValue` - Initial value
- `iMaxValue` - Maximum value

**Returns:** Semaphore object on success, `NULL` on failure

---

### xrtSemDestroy

Destroy semaphore.

```c
XXAPI void xrtSemDestroy(xsem pSem);
```

---

### xrtSemInit / xrtSemUnit

Initialize/release semaphore (for self-maintained struct pointers).

```c
XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue);
XXAPI void xrtSemUnit(xsem pSem);
```

---

### xrtSemWait

Wait for semaphore (blocking, count decreases by 1).

```c
XXAPI void xrtSemWait(xsem pSem);
```

---

### xrtSemTryWait

Try to wait for semaphore (non-blocking).

```c
XXAPI bool xrtSemTryWait(xsem pSem);
```

---

### xrtSemWaitTimeout

Wait for semaphore (with timeout).

```c
XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout);
```

**Returns:** `XRT_WAIT_OK`/`XRT_WAIT_TIMEOUT`/`XRT_WAIT_ERROR`

---

### xrtSemPost

Release semaphore (count increases by 1).

```c
XXAPI bool xrtSemPost(xsem pSem);
```

---

### xrtSemPostMultiple

Release semaphore (count increases by N).

```c
XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount);
```

---

## Condition Variable

### xrtCondCreate

Create condition variable.

```c
XXAPI xcond xrtCondCreate();
```

---

### xrtCondDestroy

Destroy condition variable.

```c
XXAPI void xrtCondDestroy(xcond pCond);
```

---

### xrtCondInit / xrtCondUnit

Initialize/release condition variable.

```c
XXAPI void xrtCondInit(xcond pCond);
XXAPI void xrtCondUnit(xcond pCond);
```

---

### xrtCondWait

Wait for condition variable (must lock mutex first).

```c
XXAPI void xrtCondWait(xcond pCond, xmutex pMutex);
```

---

### xrtCondWaitTimeout

Wait for condition variable (with timeout).

```c
XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout);
```

---

### xrtCondSignal

Wake one waiting thread.

```c
XXAPI void xrtCondSignal(xcond pCond);
```

---

### xrtCondBroadcast

Wake all waiting threads.

```c
XXAPI void xrtCondBroadcast(xcond pCond);
```

---

## Use Cases

### 1. Basic Thread Usage

```c
#include "xrt.h"
#include <stdio.h>

static uint32 WorkerThread(ptr param)
{
    xthread self = (xthread)param;
    
    for (int i = 0; i < 10; i++) {
        if (xrtThreadShouldStop(self)) {
            printf("Thread received stop signal\n");
            return 1;
        }
        printf("Working: %d\n", i);
        xrtSleep(100);
    }
    return 0;
}

int main() {
    xrtInit();
    
    xthread t = xrtThreadCreate(WorkerThread, NULL, 0);
    t->Param = t;  // Pass self to thread function
    
    xrtThreadWait(t);
    xrtThreadDestroy(t);
    
    xrtUnit();
    return 0;
}
```

### 2. Mutex Protecting Shared Data

```c
#include "xrt.h"
#include <stdio.h>

static int g_Counter = 0;
static xmutex g_Mutex = NULL;

static uint32 CounterThread(ptr param)
{
    for (int i = 0; i < 1000; i++) {
        xrtMutexLock(g_Mutex);
        g_Counter++;
        xrtMutexUnlock(g_Mutex);
    }
    return 0;
}

int main() {
    xrtInit();
    
    g_Mutex = xrtMutexCreate();
    
    xthread threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = xrtThreadCreate(CounterThread, NULL, 0);
    }
    
    for (int i = 0; i < 4; i++) {
        xrtThreadWait(threads[i]);
        xrtThreadDestroy(threads[i]);
    }
    
    printf("Final count: %d (expected: 4000)\n", g_Counter);
    
    xrtMutexDestroy(g_Mutex);
    xrtUnit();
    return 0;
}
```

### 3. Producer-Consumer Pattern

```c
#include "xrt.h"
#include <stdio.h>

#define BUFFER_SIZE 5

static int g_Buffer[BUFFER_SIZE];
static int g_Index = 0;
static xsem g_SemEmpty = NULL;
static xsem g_SemFull = NULL;
static xmutex g_Mutex = NULL;

static uint32 ProducerThread(ptr param)
{
    for (int i = 1; i <= 10; i++) {
        xrtSemWait(g_SemEmpty);
        
        xrtMutexLock(g_Mutex);
        g_Buffer[g_Index++] = i;
        printf("Produced: %d\n", i);
        xrtMutexUnlock(g_Mutex);
        
        xrtSemPost(g_SemFull);
    }
    return 0;
}

static uint32 ConsumerThread(ptr param)
{
    for (int i = 0; i < 10; i++) {
        xrtSemWait(g_SemFull);
        
        xrtMutexLock(g_Mutex);
        int value = g_Buffer[--g_Index];
        printf("Consumed: %d\n", value);
        xrtMutexUnlock(g_Mutex);
        
        xrtSemPost(g_SemEmpty);
    }
    return 0;
}

int main() {
    xrtInit();
    
    g_SemEmpty = xrtSemCreate(BUFFER_SIZE, BUFFER_SIZE);
    g_SemFull = xrtSemCreate(0, BUFFER_SIZE);
    g_Mutex = xrtMutexCreate();
    
    xthread producer = xrtThreadCreate(ProducerThread, NULL, 0);
    xthread consumer = xrtThreadCreate(ConsumerThread, NULL, 0);
    
    xrtThreadWait(producer);
    xrtThreadWait(consumer);
    
    xrtThreadDestroy(producer);
    xrtThreadDestroy(consumer);
    xrtSemDestroy(g_SemEmpty);
    xrtSemDestroy(g_SemFull);
    xrtMutexDestroy(g_Mutex);
    
    xrtUnit();
    return 0;
}
```

### 4. Condition Variable Wait/Notify

```c
#include "xrt.h"
#include <stdio.h>

static xcond g_Cond = NULL;
static xmutex g_Mutex = NULL;
static volatile int g_Ready = 0;

static uint32 WaiterThread(ptr param)
{
    int id = (int)(intptr)param;
    
    xrtMutexLock(g_Mutex);
    while (!g_Ready) {
        printf("Thread %d waiting...\n", id);
        xrtCondWait(g_Cond, g_Mutex);
    }
    printf("Thread %d awakened\n", id);
    xrtMutexUnlock(g_Mutex);
    
    return 0;
}

int main() {
    xrtInit();
    
    g_Cond = xrtCondCreate();
    g_Mutex = xrtMutexCreate();
    
    xthread waiters[3];
    for (int i = 0; i < 3; i++) {
        waiters[i] = xrtThreadCreate(WaiterThread, (ptr)(intptr)(i + 1), 0);
    }
    
    xrtSleep(500);
    
    xrtMutexLock(g_Mutex);
    g_Ready = 1;
    xrtCondBroadcast(g_Cond);
    xrtMutexUnlock(g_Mutex);
    
    for (int i = 0; i < 3; i++) {
        xrtThreadWait(waiters[i]);
        xrtThreadDestroy(waiters[i]);
    }
    
    xrtCondDestroy(g_Cond);
    xrtMutexDestroy(g_Mutex);
    xrtUnit();
    return 0;
}
```

---

## Notes

### 1. Thread Safety

Most XRT library functions are **not thread-safe**, especially:

- `xCore` global variable (`iRet`, `LastError`, etc.)
- Temporary memory (`xrtTempMemory`)
- JSON parser

Use mutex to protect shared data:

```c
xmutex mutex = xrtMutexCreate();

// Thread-safe access
xrtMutexLock(mutex);
// Access shared data...
xrtMutexUnlock(mutex);

xrtMutexDestroy(mutex);
```

### 2. Correct Way to Stop Threads

Recommend using stop signal instead of forceful termination:

```c
// ✅ Recommended: Use stop signal
static uint32 SafeThread(ptr param) {
    xthread self = (xthread)param;
    while (!xrtThreadShouldStop(self)) {
        // Work...
    }
    return 0;
}

// Send stop signal
xrtThreadStop(thread);
xrtThreadWait(thread);

// ❌ Not recommended: Force terminate
xrtThreadKill(thread);  // May cause resource leaks
```

### 3. Resource Release Order

```c
xthread t = xrtThreadCreate(func, param, 0);
if (t) {
    // 1. Wait for thread to complete
    xrtThreadWait(t);
    // 2. Destroy thread object
    xrtThreadDestroy(t);
}
```

### 4. Platform Differences

| Feature | Windows | Linux/macOS |
|---------|---------|-------------|
| Thread suspend/resume | Supported | Not supported |
| Thread wait with timeout | Supported | Partially supported |
| Condition variable | Fully supported | Fully supported |

### 5. Compilation and Linking

On Linux/macOS, link the pthread library:

```bash
gcc -o program program.c -lpthread
```

---

## Related Documentation

- [Time Processing Library](api-time.en.md) - Contains `xrtSleep` function
- [Base Functions Library](api-base.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#thread-management-library)

</div>
