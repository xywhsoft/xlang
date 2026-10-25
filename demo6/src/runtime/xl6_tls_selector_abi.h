#ifndef XL6_TLS_SELECTOR_ABI_H
#define XL6_TLS_SELECTOR_ABI_H
#include "xl6_runtime_export.h"
#include "xl6_native_class_abi.h"
#include "xl6_tls_server_request.h"
struct xlir_closure;
XL6_TEXT_API void* xl6TlsServerChoiceCreateV1(void*,uint64_t);
XL6_TEXT_API void* xl6TlsServerChoiceWithProtocolIndexV1(void*,int64_t);
XL6_TEXT_API void xl6TlsServerChoiceDestroyV1(void*);
XL6_TEXT_API void* xl6TlsServerChoiceIdentityV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerChoiceCookieV1(const void*);
XL6_TEXT_API bool xl6TlsServerChoiceHasProtocolIndexV1(const void*);
XL6_TEXT_API int64_t xl6TlsServerChoiceProtocolIndexV1(const void*);
XL6_TEXT_API void* xl6TlsServerContextSelectorCreateV1(XL6_NativeClassCallbackContextV1,void*,struct xlir_closure*);
XL6_TEXT_API void* xl6TlsServerContextWithSelectorV1(XL6_NativeClassCallbackContextV1,void*,struct xlir_closure*);
XL6_TEXT_API bool xl6TlsServerContextHasSelectorV1(void*);
#endif
