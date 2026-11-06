#ifndef XGE_STATIC_XRT_CONTRACT_H
#define XGE_STATIC_XRT_CONTRACT_H
#include "xge_host_xrt.h"
#include <wchar.h>
#define XGE_STATIC_XRT_HEADER_SHA256 "4c02cd11accbd06c4e5de725b1f19bacbf44a92e4791a3819ba45540bc6e4f5d"
struct XgeStaticScalarAlignment {char c;void*p;double d;};
struct XgeStaticViewAlignment {char c;xstrview v;};
#if defined(_WIN64) && defined(__x86_64__)
#define XGE_STATIC_XRT_TARGET 1u
#elif defined(__linux__) && defined(__x86_64__)
#define XGE_STATIC_XRT_TARGET 2u
#else
#error "Static XGE target ABI has not been qualified"
#endif
#define XGE_STATIC_XRT_LOCAL_CONTRACT_INIT {sizeof(xge_host_xrt_contract_t),1u, \
 ((uint64_t)XGE_STATIC_XRT_TARGET|((uint64_t)sizeof(void*)<<8)|((uint64_t)sizeof(size_t)<<12)| \
 ((uint64_t)sizeof(long)<<16)|((uint64_t)sizeof(wchar_t)<<20)|((uint64_t)sizeof(double)<<24)| \
 ((uint64_t)sizeof(int)<<28)|((uint64_t)offsetof(struct XgeStaticScalarAlignment,p)<<32)| \
 ((uint64_t)offsetof(struct XgeStaticScalarAlignment,d)<<40)), \
 ((uint64_t)sizeof(xstrview)|((uint64_t)offsetof(struct XgeStaticViewAlignment,v)<<16)| \
 ((uint64_t)sizeof(xrtresourcelimits)<<32)),XGE_STATIC_XRT_HEADER_SHA256}
extern const xge_host_xrt_contract_t xgeStaticXrtContractV1;
#endif
