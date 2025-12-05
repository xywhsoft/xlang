# Thread 线程管理库

> 多线程创建和管理功能，包含线程、互斥体、信号量、条件变量

[English](api-thread.en.md) | [中文](api-thread.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [数据类型](#数据类型)
- [线程管理](#线程管理)
- [互斥体](#互斥体)
- [信号量](#信号量)
- [条件变量](#条件变量)
- [使用场景](#使用场景)
- [注意事项](#注意事项)

---

## 常量定义

### 线程状态

```c
#define XRT_THREAD_STOPPED      0    // 已停止
#define XRT_THREAD_RUNNING      1    // 运行中
#define XRT_THREAD_SUSPENDED    2    // 已挂起
```

### 等待返回值

```c
#define XRT_WAIT_OK            0    // 等待成功
#define XRT_WAIT_TIMEOUT       1    // 等待超时
#define XRT_WAIT_ERROR        -1    // 等待错误
```

---

## 数据类型

### xthread_struct

线程数据结构。

```c
typedef struct {
    ptr Handle;                    // 线程句柄
    uint32 TID;                    // 线程ID
    uint32 (*Proc)(ptr param);     // 用户回调函数
    ptr Param;                     // 用户参数
    volatile int StopFlag;         // 停止信号标志
} xthread_struct, *xthread;
```

### xmutex_struct

互斥体数据结构。

```c
typedef struct {
    ptr Handle;                    // 互斥体句柄
} xmutex_struct, *xmutex;
```

### xsem_struct

信号量数据结构。

```c
typedef struct {
    ptr Handle;                    // 信号量句柄
} xsem_struct, *xsem;
```

### xcond_struct

条件变量数据结构。

```c
typedef struct {
    ptr Handle;                    // 条件变量句柄
} xcond_struct, *xcond;
```

---

## 线程管理

### xrtThreadCreate

创建并启动新线程。

```c
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
```

**参数：**
- `pProc` - 线程函数指针
- `pParam` - 传递给线程的参数
- `iStackSize` - 栈大小（字节，0 使用系统默认值）

**返回值：** 成功返回线程对象指针，失败返回 `NULL`

---

### xrtThreadDestroy

销毁线程对象（不终止线程，仅释放管理结构）。

```c
XXAPI void xrtThreadDestroy(xthread pThread);
```

**参数：**
- `pThread` - 线程对象

---

### xrtThreadWait

等待线程结束（阻塞）。

```c
XXAPI void xrtThreadWait(xthread pThread);
```

---

### xrtThreadWaitTimeout

等待线程结束（带超时）。

```c
XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout);
```

**参数：**
- `pThread` - 线程对象
- `iTimeout` - 超时时间（毫秒）

**返回值：** `XRT_WAIT_OK`/`XRT_WAIT_TIMEOUT`/`XRT_WAIT_ERROR`

---

### xrtThreadStop

发送停止信号。线程需要主动检查 `xrtThreadShouldStop`。

```c
XXAPI void xrtThreadStop(xthread pThread);
```

---

### xrtThreadShouldStop

检查是否应该停止（线程内调用）。

```c
XXAPI bool xrtThreadShouldStop(xthread pThread);
```

**返回值：** `TRUE` 表示应该停止

---

### xrtThreadKill

强制终止线程。

```c
XXAPI bool xrtThreadKill(xthread pThread);
```

**警告：** 危险操作，可能导致资源泄漏，不建议使用。

---

### xrtThreadSuspend

挂起线程。

```c
XXAPI bool xrtThreadSuspend(xthread pThread);
```

**说明：** 仅 Windows 支持，Linux/macOS 返回 `FALSE`

---

### xrtThreadResume

恢复线程。

```c
XXAPI bool xrtThreadResume(xthread pThread);
```

**说明：** 仅 Windows 支持，Linux/macOS 返回 `FALSE`

---

### xrtThreadGetState

获取线程状态。

```c
XXAPI int xrtThreadGetState(xthread pThread);
```

**返回值：** `XRT_THREAD_STOPPED`/`XRT_THREAD_RUNNING`/`XRT_THREAD_SUSPENDED`

---

### xrtThreadGetExitCode

获取线程退出码。

```c
XXAPI uint32 xrtThreadGetExitCode(xthread pThread);
```

---

### xrtThreadGetCurrentId

获取当前线程ID。

```c
XXAPI uint32 xrtThreadGetCurrentId();
```

---

### xrtThreadYield

让出当前线程的时间片。

```c
XXAPI void xrtThreadYield();
```

---

## 互斥体

### xrtMutexCreate

创建互斥体。

```c
XXAPI xmutex xrtMutexCreate();
```

**返回值：** 成功返回互斥体对象，失败返回 `NULL`

---

### xrtMutexDestroy

销毁互斥体。

```c
XXAPI void xrtMutexDestroy(xmutex pMutex);
```

---

### xrtMutexInit / xrtMutexUnit

初始化/释放互斥体（对自维护结构体指针使用）。

```c
XXAPI void xrtMutexInit(xmutex pMutex);
XXAPI void xrtMutexUnit(xmutex pMutex);
```

---

### xrtMutexLock

锁定互斥体（阻塞）。

```c
XXAPI void xrtMutexLock(xmutex pMutex);
```

---

### xrtMutexTryLock

尝试锁定互斥体（非阻塞）。

```c
XXAPI bool xrtMutexTryLock(xmutex pMutex);
```

**返回值：** `TRUE` 表示锁定成功

---

### xrtMutexUnlock

解锁互斥体。

```c
XXAPI void xrtMutexUnlock(xmutex pMutex);
```

---

## 信号量

### xrtSemCreate

创建信号量。

```c
XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue);
```

**参数：**
- `iInitValue` - 初始值
- `iMaxValue` - 最大值

**返回值：** 成功返回信号量对象，失败返回 `NULL`

---

### xrtSemDestroy

销毁信号量。

```c
XXAPI void xrtSemDestroy(xsem pSem);
```

---

### xrtSemInit / xrtSemUnit

初始化/释放信号量（对自维护结构体指针使用）。

```c
XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue);
XXAPI void xrtSemUnit(xsem pSem);
```

---

### xrtSemWait

等待信号量（阻塞，计数减1）。

```c
XXAPI void xrtSemWait(xsem pSem);
```

---

### xrtSemTryWait

尝试等待信号量（非阻塞）。

```c
XXAPI bool xrtSemTryWait(xsem pSem);
```

---

### xrtSemWaitTimeout

等待信号量（带超时）。

```c
XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout);
```

**返回值：** `XRT_WAIT_OK`/`XRT_WAIT_TIMEOUT`/`XRT_WAIT_ERROR`

---

### xrtSemPost

释放信号量（计数加1）。

```c
XXAPI bool xrtSemPost(xsem pSem);
```

---

### xrtSemPostMultiple

释放信号量（计数加N）。

```c
XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount);
```

---

## 条件变量

### xrtCondCreate

创建条件变量。

```c
XXAPI xcond xrtCondCreate();
```

---

### xrtCondDestroy

销毁条件变量。

```c
XXAPI void xrtCondDestroy(xcond pCond);
```

---

### xrtCondInit / xrtCondUnit

初始化/释放条件变量。

```c
XXAPI void xrtCondInit(xcond pCond);
XXAPI void xrtCondUnit(xcond pCond);
```

---

### xrtCondWait

等待条件变量（需要先锁定互斥体）。

```c
XXAPI void xrtCondWait(xcond pCond, xmutex pMutex);
```

---

### xrtCondWaitTimeout

等待条件变量（带超时）。

```c
XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout);
```

---

### xrtCondSignal

唤醒一个等待的线程。

```c
XXAPI void xrtCondSignal(xcond pCond);
```

---

### xrtCondBroadcast

唤醒所有等待的线程。

```c
XXAPI void xrtCondBroadcast(xcond pCond);
```

---

## 使用场景

### 1. 基本线程使用

```c
#include "xrt.h"
#include <stdio.h>

// 线程函数
static uint32 WorkerThread(ptr param)
{
    xthread self = (xthread)param;
    
    for (int i = 0; i < 10; i++) {
        // 检查停止信号
        if (xrtThreadShouldStop(self)) {
            printf("线程收到停止信号\n");
            return 1;
        }
        printf("工作中: %d\n", i);
        xrtSleep(100);
    }
    return 0;
}

int main() {
    xrtInit();
    
    xthread t = xrtThreadCreate(WorkerThread, NULL, 0);
    // 传递自身给线程函数
    t->Param = t;
    
    // 等待线程结束
    xrtThreadWait(t);
    xrtThreadDestroy(t);
    
    xrtUnit();
    return 0;
}
```

### 2. 互斥体保护共享数据

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
    
    // 创建多个线程
    xthread threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = xrtThreadCreate(CounterThread, NULL, 0);
    }
    
    // 等待所有线程
    for (int i = 0; i < 4; i++) {
        xrtThreadWait(threads[i]);
        xrtThreadDestroy(threads[i]);
    }
    
    printf("最终计数: %d (期望: 4000)\n", g_Counter);
    
    xrtMutexDestroy(g_Mutex);
    xrtUnit();
    return 0;
}
```

### 3. 生产者-消费者模式

```c
#include "xrt.h"
#include <stdio.h>

#define BUFFER_SIZE 5

static int g_Buffer[BUFFER_SIZE];
static int g_Index = 0;
static xsem g_SemEmpty = NULL;  // 空槽位数量
static xsem g_SemFull = NULL;   // 数据数量
static xmutex g_Mutex = NULL;

static uint32 ProducerThread(ptr param)
{
    for (int i = 1; i <= 10; i++) {
        xrtSemWait(g_SemEmpty);  // 等待空槽位
        
        xrtMutexLock(g_Mutex);
        g_Buffer[g_Index++] = i;
        printf("生产: %d\n", i);
        xrtMutexUnlock(g_Mutex);
        
        xrtSemPost(g_SemFull);   // 通知有新数据
    }
    return 0;
}

static uint32 ConsumerThread(ptr param)
{
    for (int i = 0; i < 10; i++) {
        xrtSemWait(g_SemFull);   // 等待数据
        
        xrtMutexLock(g_Mutex);
        int value = g_Buffer[--g_Index];
        printf("消费: %d\n", value);
        xrtMutexUnlock(g_Mutex);
        
        xrtSemPost(g_SemEmpty);  // 通知有空槽位
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

### 4. 条件变量等待通知

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
        printf("线程 %d 等待中...\n", id);
        xrtCondWait(g_Cond, g_Mutex);
    }
    printf("线程 %d 被唤醒\n", id);
    xrtMutexUnlock(g_Mutex);
    
    return 0;
}

int main() {
    xrtInit();
    
    g_Cond = xrtCondCreate();
    g_Mutex = xrtMutexCreate();
    
    // 创建多个等待线程
    xthread waiters[3];
    for (int i = 0; i < 3; i++) {
        waiters[i] = xrtThreadCreate(WaiterThread, (ptr)(intptr)(i + 1), 0);
    }
    
    xrtSleep(500);
    
    // 唤醒所有线程
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

## 注意事项

### 1. 线程安全

XRT 库的大部分函数**不是线程安全**的，特别是：

- `xCore` 全局变量（`iRet`, `LastError` 等）
- 临时内存（`xrtTempMemory`）
- JSON 解析器

使用互斥体保护共享数据：

```c
xmutex mutex = xrtMutexCreate();

// 线稍安全的访问
xrtMutexLock(mutex);
// 访问共享数据...
xrtMutexUnlock(mutex);

xrtMutexDestroy(mutex);
```

### 2. 停止线程的正确方式

推荐使用停止信号而非强制终止：

```c
// ✅ 推荐：使用停止信号
static uint32 SafeThread(ptr param) {
    xthread self = (xthread)param;
    while (!xrtThreadShouldStop(self)) {
        // 工作...
    }
    return 0;
}

// 发送停止信号
xrtThreadStop(thread);
xrtThreadWait(thread);

// ❌ 不推荐：强制终止
xrtThreadKill(thread);  // 可能导致资源泄漏
```

### 3. 资源释放顺序

```c
xthread t = xrtThreadCreate(func, param, 0);
if (t) {
    // 1. 等待线程完成
    xrtThreadWait(t);
    // 2. 销毁线程对象
    xrtThreadDestroy(t);
}
```

### 4. 平台差异

| 功能 | Windows | Linux/macOS |
|------|---------|-------------|
| 线程挂起/恢复 | 支持 | 不支持 |
| 带超时的线程等待 | 支持 | 部分支持 |
| 条件变量 | 完全支持 | 完全支持 |

### 5. 编译链接

在 Linux/macOS 上需要链接 pthread 库：

```bash
gcc -o program program.c -lpthread
```

---

## 相关文档

- [Time 时间处理库](api-time.md) - 包含 `xrtSleep` 函数
- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#thread-线程管理库)

</div>
