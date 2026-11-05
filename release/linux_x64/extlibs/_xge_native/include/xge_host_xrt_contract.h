/* Generated; include after matching XRT declarations. */
#ifndef XGE_HOST_XRT_CONTRACT_H
#define XGE_HOST_XRT_CONTRACT_H
#include "xge_host_xrt.h"
#include "xge_host_xrt_fingerprint.h"
#include <wchar.h>
#if defined(_WIN64) && defined(__x86_64__)
#define XGE_HOST_XRT_TARGET 1u
#elif defined(__linux__) && defined(__x86_64__)
#define XGE_HOST_XRT_TARGET 2u
#else
#error "XGE hosted XRT has not been validated for this target ABI"
#endif
struct XgeHostXrtScalarAlignment { char c; void* p; double d; };
struct XgeHostXrtViewAlignment { char c; xstrview v; };
#define XGE_HOST_XRT_DATA_MODEL ((uint64_t)XGE_HOST_XRT_TARGET | \
 ((uint64_t)sizeof(void*)<<8) | ((uint64_t)sizeof(size_t)<<12) | \
 ((uint64_t)sizeof(long)<<16) | ((uint64_t)sizeof(wchar_t)<<20) | \
 ((uint64_t)sizeof(double)<<24) | ((uint64_t)sizeof(int)<<28) | \
 ((uint64_t)offsetof(struct XgeHostXrtScalarAlignment,p)<<32) | \
 ((uint64_t)offsetof(struct XgeHostXrtScalarAlignment,d)<<40))
#define XGE_HOST_XRT_CORE_LAYOUT ((uint64_t)sizeof(xstrview) | \
 ((uint64_t)offsetof(struct XgeHostXrtViewAlignment,v)<<16) | \
 ((uint64_t)sizeof(xrtresourcelimits)<<32))
#define XGE_HOST_XRT_LOCAL_CONTRACT_INIT {sizeof(xge_host_xrt_contract_t), 1u, \
 XGE_HOST_XRT_DATA_MODEL, XGE_HOST_XRT_CORE_LAYOUT, XGE_HOST_XRT_HEADER_SHA256}
#endif
