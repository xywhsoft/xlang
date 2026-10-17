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
#endif
