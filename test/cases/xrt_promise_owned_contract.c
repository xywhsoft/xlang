#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define XRT_MODULE_FUTURE
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"


static void* gFirstValue;
static void* gLateValue;
static void* gNullPromiseValue;
static int gFirstFreeCount;
static int gLateFreeCount;
static int gNullPromiseFreeCount;



/* 为所有权契约测试分配独立值。 */
static void* testAlloc(size_t size)
{
	return malloc(size);
}



/* 记录每个测试值被析构的次数。 */
static void testDestroy(ptr pValue, ptr pData)
{
	(void)pData;
	if ( pValue == gFirstValue ) { ++gFirstFreeCount; }
	if ( pValue == gLateValue ) { ++gLateFreeCount; }
	if ( pValue == gNullPromiseValue ) { ++gNullPromiseFreeCount; }
	free(pValue);
}



/* 验证 Promise 成功接管值，失败时仍由调用方持有值。 */
int main(void)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( (pFuture == NULL) || (pPromise == NULL) ) { return 2; }

	gFirstValue = testAlloc(16u);
	if ( (gFirstValue == NULL) || !xrtPromiseResolveOwned(
		pPromise,
		gFirstValue,
		testDestroy,
		NULL
	) ) {
		return 3;
	}
	if ( gFirstFreeCount != 0 ) { return 4; }

	gLateValue = testAlloc(16u);
	if ( (gLateValue == NULL) || xrtPromiseResolveOwned(
		pPromise,
		gLateValue,
		testDestroy,
		NULL
	) ) {
		return 5;
	}
	if ( gLateFreeCount != 0 ) { return 6; }
	testDestroy(gLateValue, NULL);
	if ( gLateFreeCount != 1 ) { return 7; }

	gNullPromiseValue = testAlloc(16u);
	if ( (gNullPromiseValue == NULL) || xrtPromiseResolveOwned(
		NULL,
		gNullPromiseValue,
		testDestroy,
		NULL
	) || (gNullPromiseFreeCount != 0) ) {
		return 8;
	}
	testDestroy(gNullPromiseValue, NULL);
	if ( gNullPromiseFreeCount != 1 ) { return 9; }

	xrtPromiseDestroy(pPromise);
	if ( gFirstFreeCount != 0 ) { return 10; }
	xrtFutureDestroy(pFuture);
	if ( gFirstFreeCount != 1 ) { return 11; }
	puts("xrt promise owned contract ok");
	return 0;
}
