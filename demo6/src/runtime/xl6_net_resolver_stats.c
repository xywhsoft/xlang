#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
void xl6NetResolverSnapshotDestroyV1(void* raw){xrtFree(raw);}
#define XL6_NET_RESOLVER_STAT(Field,Name) \
uint64_t xl6NetResolverSnapshot##Field##V1(const void* raw){return ((const xnetresolverstats*)raw)->Field;}
#include "xl6_net_resolver_stats_fields.inc"
#undef XL6_NET_RESOLVER_STAT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetResolverSnapshotHandlePolicyV1Get,xl6NetResolverSnapshotDestroyV1)
