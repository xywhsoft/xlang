#include <limits.h>

#define XRT_MODULE_TASK_GROUP
#include "lib/xrt/xrt.h"

#include "xl_task_group_bridge.h"



/* xlang 句柄为新版任务组补充可后绑定的父 Future 取消关系。 */
struct xl6_task_group {
	xmutex Lock;
	xtaskgroup* Group;
	xcancelwatch* ParentWatch;
	bool BindingParent;
	uint64 Reaped;
};



/* 把语言层毫秒相对时间转换为 XRT 微秒。 */
static uint64 __xl6TaskGroupTimeout(long long iMilliseconds)
{
	if ( iMilliseconds <= 0 ) {
		return 0;
	}
	if ( (unsigned long long)iMilliseconds >
		(UINT64_MAX / UINT64_C(1000)) ) {
		return UINT64_MAX;
	}
	return (uint64)iMilliseconds * UINT64_C(1000);
}



/* 使用一个已经创建的新版任务组构造语言句柄。 */
static xl6_task_group* __xl6TaskGroupWrap(xtaskgroup* pGroup)
{
	xl6_task_group* pResult;

	if ( pGroup == NULL ) {
		return NULL;
	}
	pResult = (xl6_task_group*)xrtCalloc(1, sizeof(xl6_task_group));
	if ( pResult == NULL ) {
		xrtTaskGroupDestroy(pGroup);
		return NULL;
	}
	if ( !xrtMutexInit(&pResult->Lock) ) {
		xrtTaskGroupDestroy(pGroup);
		xrtFree(pResult);
		return NULL;
	}
	pResult->Group = pGroup;
	return pResult;
}



/* 在句柄锁内返回仍然有效的底层任务组。 */
static xtaskgroup* __xl6TaskGroupGet(xl6_task_group* pGroup)
{
	xtaskgroup* pResult;

	if ( pGroup == NULL ) {
		return NULL;
	}
	(void)xrtMutexLock(&pGroup->Lock);
	pResult = pGroup->Group;
	(void)xrtMutexUnlock(&pGroup->Lock);
	return pResult;
}



/* 父 Future 请求取消时同步取消仍然存活的任务组。 */
static void __xl6TaskGroupParentCancelled(void* pData)
{
	xl6_task_group* pGroup = (xl6_task_group*)pData;
	xtaskgroup* pCurrent;

	(void)xrtMutexLock(&pGroup->Lock);
	pCurrent = pGroup->Group;
	if ( pCurrent != NULL ) {
		(void)xrtTaskGroupCancel(pCurrent);
	}
	(void)xrtMutexUnlock(&pGroup->Lock);
}



/* 创建一个独立任务组。 */
XL_API xl6_task_group* xl6TaskGroupCreate(void)
{
	return __xl6TaskGroupWrap(xrtTaskGroupCreate(NULL));
}



/* 创建由父组跟踪的结构化子组。 */
XL_API xl6_task_group* xl6TaskGroupChild(xl6_task_group* pParent)
{
	xtaskgroup* pGroup = __xl6TaskGroupGet(pParent);

	return pGroup != NULL ?
		__xl6TaskGroupWrap(xrtTaskGroupChild(pGroup, NULL)) : NULL;
}



/* 注销父取消观察并销毁任务组句柄。 */
XL_API void xl6TaskGroupDestroy(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent;
	xcancelwatch* pWatch;

	if ( pGroup == NULL ) {
		return;
	}
	(void)xrtMutexLock(&pGroup->Lock);
	pCurrent = pGroup->Group;
	pWatch = pGroup->ParentWatch;
	pGroup->Group = NULL;
	pGroup->ParentWatch = NULL;
	(void)xrtMutexUnlock(&pGroup->Lock);
	xrtCancelUnwatch(pWatch);
	xrtTaskGroupDestroy(pCurrent);
	(void)xrtMutexUnit(&pGroup->Lock);
	xrtFree(pGroup);
}



/* 停止接纳新任务。 */
XL_API bool xl6TaskGroupClose(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return (pCurrent != NULL) && xrtTaskGroupClose(pCurrent);
}



/* 停止接纳新任务并请求全部活动任务取消。 */
XL_API bool xl6TaskGroupCancel(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return (pCurrent != NULL) && xrtTaskGroupCancel(pCurrent);
}



/* 让父 Future 的取消令牌驱动当前任务组取消。 */
XL_API bool xl6TaskGroupBindParent(
	xl6_task_group* pGroup,
	xfuture* pParent
)
{
	xcancel* pCancel;
	xcancelwatch* pWatch;
	bool bAvailable;

	if ( (pGroup == NULL) || (pParent == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	/* Reserve before registering: a cancelled second parent must not run its
	 * synchronous callback and cancel this group before we reject the bind. */
	(void)xrtMutexLock(&pGroup->Lock);
	bAvailable = pGroup->Group != NULL && pGroup->ParentWatch == NULL && !pGroup->BindingParent;
	if (bAvailable) pGroup->BindingParent = true;
	(void)xrtMutexUnlock(&pGroup->Lock);
	if (!bAvailable) { xrtSetErrorKind(XERR_STATE); return false; }
	pCancel = xrtFutureCancelToken(pParent);
	if ( pCancel == NULL ) {
		goto failed;
	}
	pWatch = xrtCancelWatch(
		pCancel,
		__xl6TaskGroupParentCancelled,
		pGroup
	);
	xrtCancelDestroy(pCancel);
	if ( pWatch == NULL ) {
		goto failed;
	}
	(void)xrtMutexLock(&pGroup->Lock);
	bAvailable = (pGroup->Group != NULL) &&
		(pGroup->ParentWatch == NULL);
	if ( bAvailable ) {
		pGroup->ParentWatch = pWatch;
	}
	pGroup->BindingParent = false;
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( !bAvailable ) {
		xrtCancelUnwatch(pWatch);
		xrtSetErrorKind(XERR_STATE);
	}
	return bAvailable;
failed:
	(void)xrtMutexLock(&pGroup->Lock);
	pGroup->BindingParent = false;
	(void)xrtMutexUnlock(&pGroup->Lock);
	return false;
}



/* 跟踪一个普通 Future。 */
XL_API bool xl6TaskGroupAdd(
	xl6_task_group* pGroup,
	xfuture* pFuture
)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return (pCurrent != NULL) && xrtTaskGroupAdd(pCurrent, pFuture);
}



/* 返回当前活动任务数量。 */
XL_API int xl6TaskGroupCount(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);
	xtaskgroupstats tStats;

	if ( (pCurrent == NULL) || !xrtTaskGroupGet(pCurrent, &tStats) ) {
		return -1;
	}
	return tStats.Active <= (size_t)INT_MAX ? (int)tStats.Active : INT_MAX;
}



/* 返回自上次调用后由新版任务组自动摘除的完成项数量。 */
XL_API int xl6TaskGroupReap(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent;
	xtaskgroupstats tStats;
	uint64 iCount;

	if ( pGroup == NULL ) {
		return -1;
	}
	(void)xrtMutexLock(&pGroup->Lock);
	pCurrent = pGroup->Group;
	if ( (pCurrent == NULL) || !xrtTaskGroupGet(pCurrent, &tStats) ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		return -1;
	}
	iCount = tStats.Completed >= pGroup->Reaped ?
		tStats.Completed - pGroup->Reaped : 0;
	pGroup->Reaped = tStats.Completed;
	(void)xrtMutexUnlock(&pGroup->Lock);
	return iCount <= (uint64)INT_MAX ? (int)iCount : INT_MAX;
}



/* 返回新增引用后的稳定完成 Future。 */
XL_API xfuture* xl6TaskGroupFuture(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return pCurrent != NULL ? xrtTaskGroupFuture(pCurrent) : NULL;
}



/* 关闭并永久等待全部活动任务。 */
XL_API bool xl6TaskGroupJoin(xl6_task_group* pGroup)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return (pCurrent != NULL) &&
		(xrtTaskGroupWait(pCurrent) == XWAIT_OK);
}



/* 关闭并在相对毫秒超时内等待全部活动任务。 */
XL_API bool xl6TaskGroupJoinFor(
	xl6_task_group* pGroup,
	long long iTimeoutMs
)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return (pCurrent != NULL) &&
		(xrtTaskGroupWaitFor(
			pCurrent,
			__xl6TaskGroupTimeout(iTimeoutMs)
		) == XWAIT_OK);
}



/* 关闭并等待到语言层单调毫秒截止时间。 */
XL_API bool xl6TaskGroupJoinUntil(
	xl6_task_group* pGroup,
	long long iDeadlineMs
)
{
	xtaskgroup* pCurrent = __xl6TaskGroupGet(pGroup);

	return (pCurrent != NULL) &&
		(xrtTaskGroupWaitUntil(
			pCurrent,
			(xdeadline)__xl6TaskGroupTimeout(iDeadlineMs)
		) == XWAIT_OK);
}
