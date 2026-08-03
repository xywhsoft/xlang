#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


static void* gFirstValue;
static void* gLateValue;
static void* gNullPromiseValue;
static int gFirstFreeCount;
static int gLateFreeCount;
static int gNullPromiseFreeCount;


static void* testAlloc(size_t size)
{
	return malloc(size);
}


static void testFree(void* value)
{
	if ( value == gFirstValue ) { ++gFirstFreeCount; }
	if ( value == gLateValue ) { ++gLateFreeCount; }
	if ( value == gNullPromiseValue ) { ++gNullPromiseFreeCount; }
	free(value);
}


#define XNET_ALLOC testAlloc
#define XNET_FREE testFree
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"


int main(void)
{
	xfuture* future;
	xpromise* promise;

	if ( !xrtInit() ) { return 1; }
	future = xFutureCreate();
	promise = xPromiseCreate(future);
	if ( future == NULL || promise == NULL ) { return 2; }

	gFirstValue = testAlloc(16u);
	if ( gFirstValue == NULL || !xPromiseResolveOwned(promise, gFirstValue) ) {
		return 3;
	}
	if ( gFirstFreeCount != 0 ) { return 4; }

	gLateValue = testAlloc(16u);
	if ( gLateValue == NULL || xPromiseResolveOwned(promise, gLateValue) ) {
		return 5;
	}
	if ( gLateFreeCount != 1 ) { return 6; }

	gNullPromiseValue = testAlloc(16u);
	if (
		gNullPromiseValue == NULL ||
		xPromiseResolveOwned(NULL, gNullPromiseValue) ||
		gNullPromiseFreeCount != 1
	) {
		return 7;
	}

	xPromiseDestroy(promise);
	xFutureRelease(future);
	if ( gFirstFreeCount != 1 ) { return 8; }
	xrtUnit();
	puts("xrt promise owned contract ok");
	return 0;
}
