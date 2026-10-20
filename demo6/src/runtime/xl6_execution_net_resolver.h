#ifndef XL6_EXECUTION_NET_RESOLVER_H
#define XL6_EXECUTION_NET_RESOLVER_H
#include "xl6_execution_resource.h"
struct xnetresolverconfig;
struct xnetresolverstats;
struct xfuture;
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionNetResolverCreateV1(XL6_ExecutionSourceV1*,const struct xnetresolverconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionNetResolverWaitV1(XL6_ExecutionResourceV1*,int64_t,bool*);
XL6_CODE_LEASE_API bool xl6ExecutionNetResolverClearV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionNetResolverConfigV1(XL6_ExecutionResourceV1*,struct xnetresolverconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionNetResolverStatsV1(XL6_ExecutionResourceV1*,struct xnetresolverstats*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionNetResolverQueryV1(XL6_ExecutionResourceV1*,const char*,int);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionNetResolverFamilyV1Get(void);
#endif
