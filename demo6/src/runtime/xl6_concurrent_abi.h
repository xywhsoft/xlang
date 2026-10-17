#ifndef XL6_CONCURRENT_ABI_H
#define XL6_CONCURRENT_ABI_H
XL6_TEXT_API void* xl6OnceCreateV1(void);
XL6_TEXT_API void xl6OnceDestroyV1(void*);
XL6_TEXT_API void xl6OnceRunV1(void*, struct xlir_closure*);
XL6_TEXT_API bool xl6OnceTryRunV1(void*, struct xlir_closure*);
XL6_TEXT_API void* xl6EventCreateV1(bool manualReset, bool signaled);
XL6_TEXT_API void xl6EventDestroyV1(void*);
XL6_TEXT_API void xl6EventSetV1(void*);
XL6_TEXT_API void xl6EventResetV1(void*);
XL6_TEXT_API bool xl6EventWaitV1(void*, int64_t timeoutMillis);
XL6_TEXT_API bool xl6EventTryWaitV1(void*);
XL6_TEXT_API void* xl6ThreadLocalCreateV1(void);
XL6_TEXT_API void xl6ThreadLocalDestroyV1(void*);
XL6_TEXT_API xvalue* xl6ThreadLocalGetV1(void*);
XL6_TEXT_API void xl6ThreadLocalSetV1(void*, const xvalue*);
XL6_TEXT_API xvalue* xl6ThreadLocalTakeV1(void*);
XL6_TEXT_API bool xl6ThreadLocalHasV1(void*);
XL6_TEXT_API bool xl6ThreadLocalRemoveV1(void*);
XL6_TEXT_API void xl6ThreadLocalCloseV1(void*);
XL6_TEXT_API bool xl6ThreadLocalClosedV1(void*);
XL6_TEXT_API void* xl6CancellationSourceCreateV1(void);
XL6_TEXT_API void* xl6CancellationSourceChildV1(void*);
XL6_TEXT_API void xl6CancellationSourceDestroyV1(void*);
XL6_TEXT_API bool xl6CancellationSourceCancelV1(void*);
XL6_TEXT_API bool xl6CancellationSourceRequestedV1(void*);
XL6_TEXT_API void* xl6CancellationTokenCreateV1(void*);
XL6_TEXT_API void xl6CancellationTokenDestroyV1(void*);
XL6_TEXT_API bool xl6CancellationTokenRequestedV1(void*);
XL6_TEXT_API void xl6CancellationTokenThrowV1(void*);
XL6_TEXT_API void* xl6CancellationRegistrationCreateV1(void*, struct xlir_closure*);
XL6_TEXT_API void xl6CancellationRegistrationDestroyV1(void*);
XL6_TEXT_API void xl6CancellationRegistrationCloseV1(void*);
XL6_TEXT_API bool xl6CancellationRegistrationClosedV1(void*);
XL6_TEXT_API bool xl6CancellationRegistrationTriggeredV1(void*);
XL6_TEXT_API bool xl6CancellationRegistrationFailedV1(void*);
XL6_TEXT_API void xl6CancellationRegistrationCheckV1(void*);
#endif
