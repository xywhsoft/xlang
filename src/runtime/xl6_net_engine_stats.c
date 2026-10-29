#include "xl6_net_engine_config_internal.h"
void xl6NetEngineSnapshotDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6NetEngineSnapshotStateV1(const void* raw){return ((const xnetenginestats*)raw)->State;}
#define XL6_NET_ENGINE_STAT(Field,Name) uint64_t xl6NetEngineSnapshot##Field##V1(const void* raw){return ((const xnetenginestats*)raw)->Field;}
#include "xl6_net_engine_stats_fields.inc"
#undef XL6_NET_ENGINE_STAT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetEngineSnapshotHandlePolicyV1Get,xl6NetEngineSnapshotDestroyV1)
