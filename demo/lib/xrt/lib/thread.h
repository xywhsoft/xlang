


// 跨平台头文件
#if !defined(_WIN32) && !defined(_WIN64)
	#include <pthread.h>
	#include <semaphore.h>
	#include <signal.h>
#endif

// TCC 编译器兼容性：定义 CONDITION_VARIABLE 相关
#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		// TCC 编译器可能缺少 CONDITION_VARIABLE 定义
		#ifndef CONDITION_VARIABLE_INIT
			typedef struct { PVOID Ptr; } CONDITION_VARIABLE, *PCONDITION_VARIABLE;
			#define CONDITION_VARIABLE_INIT {0}
			
			// 声明函数（从 kernel32.dll 加载）
			void WINAPI InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
			void WINAPI WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
			void WINAPI WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable);
			BOOL WINAPI SleepConditionVariableCS(
				PCONDITION_VARIABLE ConditionVariable,
				PCRITICAL_SECTION CriticalSection,
				DWORD dwMilliseconds
			);
		#endif
	#endif
#endif



/* ================================ 线程管理 ================================ */

// 创建线程
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize)
{
	xthread pThread = xrtMalloc(sizeof(xthread_struct));
	if ( !pThread ) return NULL;
	
	pThread->Proc = pProc;
	pThread->Param = pParam;
	pThread->StopFlag = 0;
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows 方案
		DWORD iThreadID;
		pThread->Handle = CreateThread(NULL, iStackSize, pProc, pParam, 0, &iThreadID);
		pThread->TID = iThreadID;
		if ( !pThread->Handle ) {
			xrtFree(pThread);
			return NULL;
		}
	#else
		// POSIX 方案
		pthread_t tid;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if ( iStackSize > 0 ) {
			pthread_attr_setstacksize(&attr, iStackSize);
		}
		int ret = pthread_create(&tid, &attr, (void*(*)(void*))pProc, pParam);
		pthread_attr_destroy(&attr);
		if ( ret != 0 ) {
			xrtFree(pThread);
			return NULL;
		}
		pThread->Handle = (ptr)tid;
		pThread->TID = (uint32)tid;
	#endif
	
	return pThread;
}



// 销毁线程对象（不终止线程，仅释放管理结构）
XXAPI void xrtThreadDestroy(xthread pThread)
{
	if ( pThread ) {
		#if defined(_WIN32) || defined(_WIN64)
			if ( pThread->Handle ) {
				CloseHandle(pThread->Handle);
			}
		#else
			// POSIX: pthread_detach 让线程结束后自动释放资源
			if ( pThread->Handle ) {
				pthread_detach((pthread_t)pThread->Handle);
			}
		#endif
		xrtFree(pThread);
	}
}



// 等待线程结束
XXAPI void xrtThreadWait(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject(pThread->Handle, INFINITE);
	#else
		pthread_join((pthread_t)pThread->Handle, NULL);
	#endif
}



// 等待线程结束（带超时，毫秒）
XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout)
{
	if ( !pThread || !pThread->Handle ) return XRT_WAIT_ERROR;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD ret = WaitForSingleObject(pThread->Handle, iTimeout);
		if ( ret == WAIT_OBJECT_0 ) return XRT_WAIT_OK;
		if ( ret == WAIT_TIMEOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#else
		// POSIX 没有带超时的 join，使用轮询方式
		struct timespec ts;
		ts.tv_sec = iTimeout / 1000;
		ts.tv_nsec = (iTimeout % 1000) * 1000000;
		
		#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
			#if __GLIBC_PREREQ(2, 34)
				// glibc 2.34+ 支持 pthread_timedjoin_np
				struct timespec abstime;
				clock_gettime(CLOCK_REALTIME, &abstime);
				abstime.tv_sec += ts.tv_sec;
				abstime.tv_nsec += ts.tv_nsec;
				if ( abstime.tv_nsec >= 1000000000 ) {
					abstime.tv_sec++;
					abstime.tv_nsec -= 1000000000;
				}
				int ret = pthread_timedjoin_np((pthread_t)pThread->Handle, NULL, &abstime);
				if ( ret == 0 ) return XRT_WAIT_OK;
				if ( ret == ETIMEDOUT ) return XRT_WAIT_TIMEOUT;
				return XRT_WAIT_ERROR;
			#endif
		#endif
		
		// 回退方案：轮询检查
		uint32 elapsed = 0;
		while ( elapsed < iTimeout ) {
			int ret = pthread_tryjoin_np((pthread_t)pThread->Handle, NULL);
			if ( ret == 0 ) return XRT_WAIT_OK;
			if ( ret != EBUSY ) return XRT_WAIT_ERROR;
			xrtSleep(10);
			elapsed += 10;
		}
		return XRT_WAIT_TIMEOUT;
	#endif
}



// 发送停止信号
XXAPI void xrtThreadStop(xthread pThread)
{
	if ( pThread ) {
		pThread->StopFlag = 1;
	}
}



// 检查是否应该停止
XXAPI bool xrtThreadShouldStop(xthread pThread)
{
	if ( pThread ) {
		return pThread->StopFlag != 0;
	}
	return FALSE;
}



// 强制终止线程
XXAPI bool xrtThreadKill(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return TerminateThread(pThread->Handle, 0) != 0;
	#else
		return pthread_cancel((pthread_t)pThread->Handle) == 0;
	#endif
}



// 挂起线程
XXAPI bool xrtThreadSuspend(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return SuspendThread(pThread->Handle) != (DWORD)-1;
	#else
		// POSIX 不直接支持挂起线程，返回失败
		// 可以通过信号量或条件变量实现类似功能
		return FALSE;
	#endif
}



// 恢复线程
XXAPI bool xrtThreadResume(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return ResumeThread(pThread->Handle) != (DWORD)-1;
	#else
		// POSIX 不直接支持恢复线程，返回失败
		return FALSE;
	#endif
}



// 获取线程状态
XXAPI int xrtThreadGetState(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return XRT_THREAD_STOPPED;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD exitCode;
		if ( GetExitCodeThread(pThread->Handle, &exitCode) ) {
			if ( exitCode == STILL_ACTIVE ) {
				// 检查是否挂起
				DWORD suspendCount = SuspendThread(pThread->Handle);
				if ( suspendCount != (DWORD)-1 ) {
					ResumeThread(pThread->Handle);
					if ( suspendCount > 0 ) {
						return XRT_THREAD_SUSPENDED;
					}
				}
				return XRT_THREAD_RUNNING;
			}
			return XRT_THREAD_STOPPED;
		}
		return XRT_THREAD_STOPPED;
	#else
		// POSIX: 尝试 join 检查状态
		int ret = pthread_tryjoin_np((pthread_t)pThread->Handle, NULL);
		if ( ret == 0 ) {
			return XRT_THREAD_STOPPED;
		}
		if ( ret == EBUSY ) {
			return XRT_THREAD_RUNNING;
		}
		return XRT_THREAD_STOPPED;
	#endif
}



// 获取线程退出码
XXAPI uint32 xrtThreadGetExitCode(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return 0;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD exitCode = 0;
		GetExitCodeThread(pThread->Handle, &exitCode);
		return (uint32)exitCode;
	#else
		// POSIX: 需要通过 pthread_join 获取
		return 0;
	#endif
}



// 获取当前线程ID
XXAPI uint32 xrtThreadGetCurrentId()
{
	#if defined(_WIN32) || defined(_WIN64)
		return GetCurrentThreadId();
	#else
		return (uint32)pthread_self();
	#endif
}



// 让出当前线程的时间片
XXAPI void xrtThreadYield()
{
	#if defined(_WIN32) || defined(_WIN64)
		SwitchToThread();
	#else
		sched_yield();
	#endif
}



/* ================================ 互斥体 ================================ */

// 创建互斥体
XXAPI xmutex xrtMutexCreate()
{
	xmutex pMutex = xrtMalloc(sizeof(xmutex_struct));
	if ( !pMutex ) return NULL;
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->Handle = xrtMalloc(sizeof(CRITICAL_SECTION));
		if ( !pMutex->Handle ) {
			xrtFree(pMutex);
			return NULL;
		}
		InitializeCriticalSection((CRITICAL_SECTION*)pMutex->Handle);
	#else
		pMutex->Handle = xrtMalloc(sizeof(pthread_mutex_t));
		if ( !pMutex->Handle ) {
			xrtFree(pMutex);
			return NULL;
		}
		pthread_mutex_init((pthread_mutex_t*)pMutex->Handle, NULL);
	#endif
	
	return pMutex;
}



// 销毁互斥体
XXAPI void xrtMutexDestroy(xmutex pMutex)
{
	if ( pMutex ) {
		xrtMutexUnit(pMutex);
		xrtFree(pMutex);
	}
}



// 初始化互斥体
XXAPI void xrtMutexInit(xmutex pMutex)
{
	if ( !pMutex ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->Handle = xrtMalloc(sizeof(CRITICAL_SECTION));
		if ( pMutex->Handle ) {
			InitializeCriticalSection((CRITICAL_SECTION*)pMutex->Handle);
		}
	#else
		pMutex->Handle = xrtMalloc(sizeof(pthread_mutex_t));
		if ( pMutex->Handle ) {
			pthread_mutex_init((pthread_mutex_t*)pMutex->Handle, NULL);
		}
	#endif
}



// 释放互斥体
XXAPI void xrtMutexUnit(xmutex pMutex)
{
	if ( !pMutex || !pMutex->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		DeleteCriticalSection((CRITICAL_SECTION*)pMutex->Handle);
	#else
		pthread_mutex_destroy((pthread_mutex_t*)pMutex->Handle);
	#endif
	
	xrtFree(pMutex->Handle);
	pMutex->Handle = NULL;
}



// 锁定互斥体
XXAPI void xrtMutexLock(xmutex pMutex)
{
	if ( !pMutex || !pMutex->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection((CRITICAL_SECTION*)pMutex->Handle);
	#else
		pthread_mutex_lock((pthread_mutex_t*)pMutex->Handle);
	#endif
}



// 尝试锁定互斥体
XXAPI bool xrtMutexTryLock(xmutex pMutex)
{
	if ( !pMutex || !pMutex->Handle ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return TryEnterCriticalSection((CRITICAL_SECTION*)pMutex->Handle) != 0;
	#else
		return pthread_mutex_trylock((pthread_mutex_t*)pMutex->Handle) == 0;
	#endif
}



// 解锁互斥体
XXAPI void xrtMutexUnlock(xmutex pMutex)
{
	if ( !pMutex || !pMutex->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection((CRITICAL_SECTION*)pMutex->Handle);
	#else
		pthread_mutex_unlock((pthread_mutex_t*)pMutex->Handle);
	#endif
}



/* ================================ 信号量 ================================ */

// 创建信号量
XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue)
{
	xsem pSem = xrtMalloc(sizeof(xsem_struct));
	if ( !pSem ) return NULL;
	
	#if defined(_WIN32) || defined(_WIN64)
		pSem->Handle = CreateSemaphoreW(NULL, iInitValue, iMaxValue, NULL);
		if ( !pSem->Handle ) {
			xrtFree(pSem);
			return NULL;
		}
	#else
		pSem->Handle = xrtMalloc(sizeof(sem_t));
		if ( !pSem->Handle ) {
			xrtFree(pSem);
			return NULL;
		}
		if ( sem_init((sem_t*)pSem->Handle, 0, iInitValue) != 0 ) {
			xrtFree(pSem->Handle);
			xrtFree(pSem);
			return NULL;
		}
	#endif
	
	return pSem;
}



// 销毁信号量
XXAPI void xrtSemDestroy(xsem pSem)
{
	if ( pSem ) {
		xrtSemUnit(pSem);
		xrtFree(pSem);
	}
}



// 初始化信号量
XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue)
{
	if ( !pSem ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		pSem->Handle = CreateSemaphoreW(NULL, iInitValue, iMaxValue, NULL);
	#else
		pSem->Handle = xrtMalloc(sizeof(sem_t));
		if ( pSem->Handle ) {
			if ( sem_init((sem_t*)pSem->Handle, 0, iInitValue) != 0 ) {
				xrtFree(pSem->Handle);
				pSem->Handle = NULL;
			}
		}
	#endif
}



// 释放信号量
XXAPI void xrtSemUnit(xsem pSem)
{
	if ( !pSem || !pSem->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		CloseHandle(pSem->Handle);
	#else
		sem_destroy((sem_t*)pSem->Handle);
		xrtFree(pSem->Handle);
	#endif
	
	pSem->Handle = NULL;
}



// 等待信号量
XXAPI void xrtSemWait(xsem pSem)
{
	if ( !pSem || !pSem->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject(pSem->Handle, INFINITE);
	#else
		sem_wait((sem_t*)pSem->Handle);
	#endif
}



// 尝试等待信号量
XXAPI bool xrtSemTryWait(xsem pSem)
{
	if ( !pSem || !pSem->Handle ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return WaitForSingleObject(pSem->Handle, 0) == WAIT_OBJECT_0;
	#else
		return sem_trywait((sem_t*)pSem->Handle) == 0;
	#endif
}



// 等待信号量（带超时）
XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout)
{
	if ( !pSem || !pSem->Handle ) return XRT_WAIT_ERROR;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD ret = WaitForSingleObject(pSem->Handle, iTimeout);
		if ( ret == WAIT_OBJECT_0 ) return XRT_WAIT_OK;
		if ( ret == WAIT_TIMEOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#else
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += iTimeout / 1000;
		ts.tv_nsec += (iTimeout % 1000) * 1000000;
		if ( ts.tv_nsec >= 1000000000 ) {
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000;
		}
		int ret = sem_timedwait((sem_t*)pSem->Handle, &ts);
		if ( ret == 0 ) return XRT_WAIT_OK;
		if ( errno == ETIMEDOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#endif
}



// 释放信号量
XXAPI bool xrtSemPost(xsem pSem)
{
	if ( !pSem || !pSem->Handle ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return ReleaseSemaphore(pSem->Handle, 1, NULL) != 0;
	#else
		return sem_post((sem_t*)pSem->Handle) == 0;
	#endif
}



// 释放信号量（计数加N）
XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount)
{
	if ( !pSem || !pSem->Handle || iCount == 0 ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return ReleaseSemaphore(pSem->Handle, iCount, NULL) != 0;
	#else
		// POSIX 不支持一次释放多个，循环调用
		for ( uint32 i = 0; i < iCount; i++ ) {
			if ( sem_post((sem_t*)pSem->Handle) != 0 ) {
				return FALSE;
			}
		}
		return TRUE;
	#endif
}



/* ================================ 条件变量 ================================ */

// 创建条件变量
XXAPI xcond xrtCondCreate()
{
	xcond pCond = xrtMalloc(sizeof(xcond_struct));
	if ( !pCond ) return NULL;
	
	#if defined(_WIN32) || defined(_WIN64)
		pCond->Handle = xrtMalloc(sizeof(CONDITION_VARIABLE));
		if ( !pCond->Handle ) {
			xrtFree(pCond);
			return NULL;
		}
		InitializeConditionVariable((CONDITION_VARIABLE*)pCond->Handle);
	#else
		pCond->Handle = xrtMalloc(sizeof(pthread_cond_t));
		if ( !pCond->Handle ) {
			xrtFree(pCond);
			return NULL;
		}
		pthread_cond_init((pthread_cond_t*)pCond->Handle, NULL);
	#endif
	
	return pCond;
}



// 销毁条件变量
XXAPI void xrtCondDestroy(xcond pCond)
{
	if ( pCond ) {
		xrtCondUnit(pCond);
		xrtFree(pCond);
	}
}



// 初始化条件变量
XXAPI void xrtCondInit(xcond pCond)
{
	if ( !pCond ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		pCond->Handle = xrtMalloc(sizeof(CONDITION_VARIABLE));
		if ( pCond->Handle ) {
			InitializeConditionVariable((CONDITION_VARIABLE*)pCond->Handle);
		}
	#else
		pCond->Handle = xrtMalloc(sizeof(pthread_cond_t));
		if ( pCond->Handle ) {
			pthread_cond_init((pthread_cond_t*)pCond->Handle, NULL);
		}
	#endif
}



// 释放条件变量
XXAPI void xrtCondUnit(xcond pCond)
{
	if ( !pCond || !pCond->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows CONDITION_VARIABLE 不需要显式销毁
	#else
		pthread_cond_destroy((pthread_cond_t*)pCond->Handle);
	#endif
	
	xrtFree(pCond->Handle);
	pCond->Handle = NULL;
}



// 等待条件变量
XXAPI void xrtCondWait(xcond pCond, xmutex pMutex)
{
	if ( !pCond || !pCond->Handle || !pMutex || !pMutex->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		SleepConditionVariableCS((CONDITION_VARIABLE*)pCond->Handle, 
			(CRITICAL_SECTION*)pMutex->Handle, INFINITE);
	#else
		pthread_cond_wait((pthread_cond_t*)pCond->Handle, 
			(pthread_mutex_t*)pMutex->Handle);
	#endif
}



// 等待条件变量（带超时）
XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout)
{
	if ( !pCond || !pCond->Handle || !pMutex || !pMutex->Handle ) {
		return XRT_WAIT_ERROR;
	}
	
	#if defined(_WIN32) || defined(_WIN64)
		if ( SleepConditionVariableCS((CONDITION_VARIABLE*)pCond->Handle, 
				(CRITICAL_SECTION*)pMutex->Handle, iTimeout) ) {
			return XRT_WAIT_OK;
		}
		if ( GetLastError() == ERROR_TIMEOUT ) {
			return XRT_WAIT_TIMEOUT;
		}
		return XRT_WAIT_ERROR;
	#else
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += iTimeout / 1000;
		ts.tv_nsec += (iTimeout % 1000) * 1000000;
		if ( ts.tv_nsec >= 1000000000 ) {
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000;
		}
		int ret = pthread_cond_timedwait((pthread_cond_t*)pCond->Handle, 
			(pthread_mutex_t*)pMutex->Handle, &ts);
		if ( ret == 0 ) return XRT_WAIT_OK;
		if ( ret == ETIMEDOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#endif
}



// 唤醒一个等待的线程
XXAPI void xrtCondSignal(xcond pCond)
{
	if ( !pCond || !pCond->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable((CONDITION_VARIABLE*)pCond->Handle);
	#else
		pthread_cond_signal((pthread_cond_t*)pCond->Handle);
	#endif
}



// 唤醒所有等待的线程
XXAPI void xrtCondBroadcast(xcond pCond)
{
	if ( !pCond || !pCond->Handle ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable((CONDITION_VARIABLE*)pCond->Handle);
	#else
		pthread_cond_broadcast((pthread_cond_t*)pCond->Handle);
	#endif
}


