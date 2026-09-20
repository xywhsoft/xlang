#ifndef XL_FUTURE_BRIDGE_H
#define XL_FUTURE_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "include/xl.h"



typedef struct xfuture xfuture;
typedef struct xerror xerror;
typedef struct xvalue xvalue;



/* xlang 异步回调使用的私有结果协议，不属于 XRT 公共 ABI。 */
#ifndef XL6_FUTURE_RESULT_TYPEDEF
#define XL6_FUTURE_RESULT_TYPEDEF
typedef struct xl6_future_result {
	int iStatus;
	void* pValue;
	xerror* Error;
	unsigned int iFlags;
	void (*pfnFreeValue)(void* pValue);
} xl6_future_result;



#define XL6_FUTURE_OWN_VALUE 0x00000001u

#define XL6_FUTURE_STATUS_OK 0
#define XL6_FUTURE_STATUS_FAILED (-1)
#define XL6_FUTURE_STATUS_CANCELLED (-2)
#define XL6_FUTURE_STATUS_CLOSED (-3)



typedef int (*xl6_task_thread_proc)(void* pArg, xl6_future_result* pOut);
typedef int (*xl6_future_continue_proc)(
	const xl6_future_result* pIn,
	void* pArg,
	xl6_future_result* pOut
);
typedef void (*xl6_future_finally_proc)(
	const xl6_future_result* pIn,
	void* pArg
);
typedef void (*xl6_future_arg_free_proc)(void* pArg);
/* Returns one newly owned xvalue for a borrowed successful Future payload. */
typedef xvalue* (*xl6_future_payload_box_proc)(const void* pValue);
#endif



XL_API xfuture* xl6FutureRunThread(
	xl6_task_thread_proc pProc,
	void* pArg,
	size_t iStackSize
);

XL_API xfuture* xl6FutureRef(xfuture* pFuture);
XL_API void xl6FutureDestroy(xfuture* pFuture);
XL_API bool xl6FutureWait(xfuture* pFuture);
XL_API bool xl6FutureWaitFor(xfuture* pFuture, long long iTimeoutMs);
XL_API bool xl6FutureWaitUntil(xfuture* pFuture, long long iDeadlineMs);
XL_API bool xl6FutureSetName(xfuture* pFuture, const char* sName);
XL_API const char* xl6FutureName(xfuture* pFuture);
XL_API unsigned long long xl6FutureCreatedMs(xfuture* pFuture);
XL_API unsigned long long xl6FutureCompletedMs(xfuture* pFuture);
XL_API int xl6FuturePendingCount(xfuture* pFuture);
XL_API int xl6FutureState(xfuture* pFuture);
XL_API int xl6FutureStatus(xfuture* pFuture);
XL_API void* xl6FutureValue(xfuture* pFuture);
XL_API const xerror* xl6FutureError(xfuture* pFuture);
XL_API bool xl6FutureCancel(xfuture* pFuture);
XL_API bool xl6FutureLinkCancel(xfuture* pFuture, xfuture* pTarget);

XL_API xfuture* xl6FutureAny(xfuture** pFutures, int iCount);
XL_API xfuture* xl6FutureAll(xfuture** pFutures, int iCount);
XL_API xfuture* xl6FutureRace(xfuture** pFutures, int iCount);
/* Checked language-level aggregate bridges.  They retain every source Future,
 * preserve source failure/cancellation, and set the XRT error slot when the
 * aggregate cannot be created synchronously. */
XL_API xfuture* xl6FutureAllXValues(
	xfuture** pFutures,
	int iCount,
	xl6_future_payload_box_proc pBox
);
XL_API xfuture* xl6FutureRaceChecked(xfuture** pFutures, int iCount);
/* Void aggregate: wait for every source and propagate non-success outcomes. */
XL_API xfuture* xl6FutureAllChecked(xfuture** pFutures, int iCount);
/* A resident mapped aggregate returning an owned language i64 box. */
XL_API xfuture* xl6FutureWhenAnyIndex(xfuture* const* pFutures, size_t iCount);
XL_API int xl6FuturePickIndex(xfuture* pFuture);
XL_API int xl6FutureAllCount(xfuture* pFuture);
XL_API void* xl6FutureAllValue(xfuture* pFuture, int iIndex);

XL_API xfuture* xl6FutureThen(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg
);
XL_API xfuture* xl6FutureThenOwned(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
);
XL_API xfuture* xl6FutureCatchOwned(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
);
XL_API xfuture* xl6FutureContinueOwned(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
);
XL_API xfuture* xl6FutureFinally(
	xfuture* pFuture,
	xl6_future_finally_proc pProc,
	void* pArg
);

#endif
