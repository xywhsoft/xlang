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
#endif
