#include "xrt.h"

#include <stdio.h>
#include <string.h>

static int32 add_two(const xfuture_result* input, ptr arg, xfuture_result* output)
{
    int64* value = (int64*)arg;

    if ( input == NULL || input->pValue == NULL || value == NULL || output == NULL ) {
        return XRT_NET_ERROR;
    }
    memset(output, 0, sizeof(*output));
    *value = *(int64*)input->pValue + 2;
    output->pValue = value;
    return XRT_NET_OK;
}

static int32 recover(const xfuture_result* input, ptr arg, xfuture_result* output)
{
    int64* value = (int64*)arg;

    if ( input == NULL || input->iStatus == XRT_NET_OK || value == NULL || output == NULL ) {
        return XRT_NET_ERROR;
    }
    memset(output, 0, sizeof(*output));
    *value = 77;
    output->pValue = value;
    return XRT_NET_OK;
}

static int32 cleanup(const xfuture_result* input, ptr arg, xfuture_result* output)
{
    int* count = (int*)arg;

    (void)input;
    (void)output;
    if ( count == NULL ) {
        return XRT_NET_ERROR;
    }
    *count += 1;
    return XRT_NET_OK;
}

int main(void)
{
    xfuture* source = NULL;
    xfuture* result = NULL;
    xpromise* promise = NULL;
    int64 sourceValue = 20;
    int64 mappedValue = 0;
    int64 recoveredValue = 0;
    int finalized = 0;
    int exitCode = 1;

    xrtInit();

    source = xFutureCreate();
    promise = xPromiseCreate(source);
    result = xFutureThenSourceEx(source, add_two, &mappedValue, NULL);
    if ( source == NULL || promise == NULL || result == NULL ||
         !xPromiseResolve(promise, &sourceValue) || !xFutureWait(result) ) {
        goto cleanup_then;
    }
    printf("then=%lld\n", (long long)*(int64*)xFutureValue(result));

cleanup_then:
    xFutureRelease(result);
    xPromiseDestroy(promise);
    xFutureRelease(source);
    if ( mappedValue != 22 ) {
        goto cleanup_runtime;
    }

    source = xFutureCreate();
    promise = xPromiseCreate(source);
    result = xFutureCatchSourceEx(source, recover, &recoveredValue, NULL);
    if ( source == NULL || promise == NULL || result == NULL ||
         !xPromiseReject(promise, XRT_NET_ERROR, (str)"boom") || !xFutureWait(result) ) {
        goto cleanup_catch;
    }
    printf("catch=%lld\n", (long long)*(int64*)xFutureValue(result));

cleanup_catch:
    xFutureRelease(result);
    xPromiseDestroy(promise);
    xFutureRelease(source);
    if ( recoveredValue != 77 ) {
        goto cleanup_runtime;
    }

    source = xFutureCreate();
    promise = xPromiseCreate(source);
    result = xFutureFinallyResultSourceEx(source, cleanup, &finalized, NULL);
    if ( source == NULL || promise == NULL || result == NULL ||
         !xPromiseResolve(promise, &sourceValue) || !xFutureWait(result) ) {
        goto cleanup_finally;
    }
    printf("finally=%lld\n", (long long)*(int64*)xFutureValue(result));
    printf("finalized=%d\n", finalized);
    exitCode = finalized == 1 ? 0 : 1;

cleanup_finally:
    xFutureRelease(result);
    xPromiseDestroy(promise);
    xFutureRelease(source);

cleanup_runtime:
    xrtUnit();
    return exitCode;
}
