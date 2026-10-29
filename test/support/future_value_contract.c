#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    int64_t* payload=xrtMalloc(sizeof(*payload)); assert(payload); *payload=42;
    assert(xl6PayloadResolveOwned(promise,payload,XL6_FUTURE_PAYLOAD_VALUE));
    xvalue* value=xl6PayloadBoxFutureV1(future,123); assert(value);
    xfuture* output=NULL;
    assert(xl6PayloadReadFutureV1(value,123,XL6_FUTURE_VALUE_NONNULL,&output) && output==future);
    assert(!xl6PayloadReadFutureV1(value,123,XL6_FUTURE_VALUE_NONNULL,&output) && output==future);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    xrtFutureDestroy(output); output=NULL;
    assert(!xl6PayloadReadFutureV1(value,124,XL6_FUTURE_VALUE_NONNULL,&output) && !output);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    assert(xl6PayloadReadFutureV1(value,124,XL6_FUTURE_VALUE_TRY,&output) && !output && !xrtGetError());
    assert(xl6PayloadReadFutureV1(xrtValueNull(),123,XL6_FUTURE_VALUE_NULLABLE,&output) && !output);
    assert(xl6PayloadReadFutureV1(xrtValueNull(),123,XL6_FUTURE_VALUE_TRY,&output) && !output);
    assert(!xl6PayloadReadFutureV1(value,123,3,&output) && !output); xrtClearError();
    xvalue* fake=xrtValueObject(); assert(fake && xrtValueTypeIdBind(fake,123));
    assert(!xl6PayloadReadFutureV1(fake,123,XL6_FUTURE_VALUE_NONNULL,&output) && !output);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); xrtValueRelease(fake);
    /* A successful read retains without allocation, even at fail-after-zero. */
    xl6ProductMemoryBegin(0);
    assert(xl6PayloadReadFutureV1(value,123,XL6_FUTURE_VALUE_NONNULL,&output) && output);
    assert(!xl6ProductMemoryEnd()); xrtFutureDestroy(output); output=NULL; xl6ProductMemoryVerify();
    size_t point=0;
    for (;point<1000;++point) {
        xl6ProductMemoryBegin(point);
        xvalue* boxed=xl6PayloadBoxFutureV1(future,123); bool hit=xl6ProductMemoryEnd();
        assert(hit ? !boxed && xrtErrorKind(xrtGetError())==XERR_MEMORY : boxed && !xrtGetError());
        xrtValueRelease(boxed); xrtClearError(); xl6ProductMemoryVerify(); if(!hit)break;
    }
    assert(point<1000); printf("native Future boxing prefixes=%zu first-no-hit=%zu balanced\n",point+1,point);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); xl6ErrorScopeCaptureFailure(&scope);
    assert(!xl6PayloadReadFutureV1(value,124,XL6_FUTURE_VALUE_NONNULL,&output));
    xl6_error_result failure=xl6ErrorScopeEnd(&scope); assert(!failure.ok && !failure.error); xrtClearError();
    /* The read owner outlives every producer/transport reference. */
    assert(xl6PayloadReadFutureV1(value,123,XL6_FUTURE_VALUE_NONNULL,&output));
    xrtValueRelease(value); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    assert(*(const int64_t*)xrtFutureValue(output)==42); xrtFutureDestroy(output); assert(!xrtGetError());
    return 0;
}
