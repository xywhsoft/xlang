#define XRT_MODULE_FUTURE_CONTINUE
#include "xrt.h"

#include <stdio.h>



/* 把成功结果加二后发布到下一个 Future。 */
static void addTwo(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	int64* pValue = (int64*)pData;

	if ( (pInput == NULL) || (pInput->Value == NULL) ||
		(pOutput == NULL) || (pValue == NULL) ) {
		(void)xrtPromiseClose(pOutput);
		return;
	}
	*pValue = *(const int64*)pInput->Value + 2;
	(void)xrtPromiseResolve(pOutput, pValue);
}



/* 把失败结果恢复为固定整数。 */
static void recover(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	int64* pValue = (int64*)pData;

	if ( (pInput == NULL) || (pInput->State != XFUTURE_FAILED) ||
		(pOutput == NULL) || (pValue == NULL) ) {
		(void)xrtPromiseClose(pOutput);
		return;
	}
	*pValue = 77;
	(void)xrtPromiseResolve(pOutput, pValue);
}



/* 记录 Finally 回调执行次数，原 Future 结果由 XRT 自动透传。 */
static void cleanup(const xfutureresult* pInput, ptr pData)
{
	int* pCount = (int*)pData;

	(void)pInput;
	if ( pCount != NULL ) {
		*pCount += 1;
	}
}



/* 演示 Then、Catch 和 Finally 的稳定组合契约。 */
int main(void)
{
	xfuture* pSource = NULL;
	xfuture* pResult = NULL;
	xpromise* pPromise = NULL;
	xerror* pError = NULL;
	int64 iSourceValue = 20;
	int64 iMappedValue = 0;
	int64 iRecoveredValue = 0;
	int iFinalized = 0;
	int iExitCode = 1;

	pPromise = xrtPromiseCreate(&pSource, NULL);
	pResult = xrtFutureThen(pSource, addTwo, &iMappedValue);
	if ( (pSource == NULL) || (pPromise == NULL) || (pResult == NULL) ||
		!xrtPromiseResolve(pPromise, &iSourceValue) ||
		(xrtFutureWait(pResult) != XWAIT_OK) ) {
		goto cleanup_then;
	}
	printf("then=%lld\n", (long long)*(const int64*)xrtFutureValue(pResult));

cleanup_then:
	xrtFutureDestroy(pResult);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	if ( iMappedValue != 22 ) {
		goto cleanup_runtime;
	}

	pSource = NULL;
	pResult = NULL;
	pPromise = xrtPromiseCreate(&pSource, NULL);
	pResult = xrtFutureCatch(pSource, recover, &iRecoveredValue);
	pError = xrtErrorCreate(XERR_INTERNAL, "example.future", 1, "boom");
	if ( (pSource == NULL) || (pPromise == NULL) || (pResult == NULL) ||
		(pError == NULL) || !xrtPromiseReject(pPromise, pError) ||
		(xrtFutureWait(pResult) != XWAIT_OK) ) {
		goto cleanup_catch;
	}
	printf("catch=%lld\n", (long long)*(const int64*)xrtFutureValue(pResult));

cleanup_catch:
	xrtErrorFree(pError);
	xrtFutureDestroy(pResult);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	if ( iRecoveredValue != 77 ) {
		goto cleanup_runtime;
	}

	pSource = NULL;
	pResult = NULL;
	pPromise = xrtPromiseCreate(&pSource, NULL);
	pResult = xrtFutureFinally(pSource, cleanup, &iFinalized);
	if ( (pSource == NULL) || (pPromise == NULL) || (pResult == NULL) ||
		!xrtPromiseResolve(pPromise, &iSourceValue) ||
		(xrtFutureWait(pResult) != XWAIT_OK) ) {
		goto cleanup_finally;
	}
	printf("finally=%lld\n", (long long)*(const int64*)xrtFutureValue(pResult));
	printf("finalized=%d\n", iFinalized);
	iExitCode = (iFinalized == 1) ? 0 : 1;

cleanup_finally:
	xrtFutureDestroy(pResult);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);

cleanup_runtime:
	return iExitCode;
}
