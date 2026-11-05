#ifndef XGE_HOST_XRT_H
#define XGE_HOST_XRT_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(XGE_DLL)
# if defined(XGE_BUILD_DLL)
#  define XGE_HOST_API __declspec(dllexport)
# else
#  define XGE_HOST_API __declspec(dllimport)
# endif
#else
# define XGE_HOST_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*xge_host_xrt_proc)(void);
typedef xge_host_xrt_proc (*xge_host_xrt_resolve_proc)(void* user, const char* name);

typedef struct xge_host_xrt_contract_t {
    uint32_t size, revision;
    uint64_t dataModel, coreLayout;
    const char* headerSha256;
} xge_host_xrt_contract_t;

typedef struct xge_host_xrt_requirements_t {
    xge_host_xrt_contract_t contract;
    size_t count;
    const char* const* names;
} xge_host_xrt_requirements_t;

enum {
    XGE_HOST_XRT_OK = 0,
    XGE_HOST_XRT_INVALID_CONTRACT = -1,
    XGE_HOST_XRT_MISSING_SYMBOL = -2,
    XGE_HOST_XRT_DIFFERENT_RUNTIME = -3,
    XGE_HOST_XRT_BUSY = -4
};

/* Hosted builds only. These functions do not call XRT or allocate memory.
 * Bind BEFORE any other XGE/XUI call, with no concurrent XGE/XUI execution.
 * The host owns the runtime and must keep it resident until XGE is unloaded.
 * resolve must return exact C function addresses and return normally without
 * calling XGE/XUI. It is used only during Bind, and is not retained.
 * All required addresses are validated before any slot is published. A
 * repeated Bind succeeds only with the same contract and every same address;
 * rebinding to another runtime is rejected. No unbind/rebind is provided. */
XGE_HOST_API const xge_host_xrt_requirements_t* xgeHostXrtDescribe(void);
XGE_HOST_API int xgeHostXrtBind(const xge_host_xrt_contract_t* contract,
    xge_host_xrt_resolve_proc resolve, void* user, const char** missingName);
XGE_HOST_API xge_host_xrt_proc xgeHostXrtRuntimeIdentity(void);

#ifdef __cplusplus
}
#endif
#endif
