#ifndef XL6_LIST_METHODS_H
#define XL6_LIST_METHODS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_LIST_METHODS_API __declspec(dllexport)
#else
#define XL6_LIST_METHODS_API
#endif
/* Compiler-private List operations, shared by sync/async/generator Cgen.
 * Accept the legacy sparse wire carrier OR a genuine projected native List;
 * this is a staged source cutover boundary, not generic HANDLE dispatch.
 * Receiver/item borrowed. Take and CountOrNull return owned Values. Missing
 * keys are normal null/false, failure is exceptional and latches the enclosing
 * checked scope even when no diagnostic could be allocated. CountIfPresent
 * only publishes on success; these outputs are not language byref parameters.
 * Trim retains the requested number of empty pool pages; negative is RANGE. */
XL6_LIST_METHODS_API int64_t xl6ListValueCountV1(const xvalue*);
XL6_LIST_METHODS_API bool xl6ListValueCountIfPresentV1(const xvalue*,int64_t*);
XL6_LIST_METHODS_API xvalue* xl6ListValueCountOrNullV1(const xvalue*);
XL6_LIST_METHODS_API bool xl6ListValueRemoveV1(xvalue*,int64_t);
XL6_LIST_METHODS_API bool xl6ListValueClearV1(xvalue*);
XL6_LIST_METHODS_API int64_t xl6ListValueTrimV1(xvalue*,int64_t);
XL6_LIST_METHODS_API xvalue* xl6ListValueTakeV1(xvalue*,int64_t);
XL6_LIST_METHODS_API bool xl6ListValueContainsV1(const xvalue*,const xvalue*);
#endif
