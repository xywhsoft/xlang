#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
uint64_t prepare_live_count(void) { xmemdebugsnapshot snapshot; xrtMemDebugSnapshot(&snapshot); return snapshot.LiveCount; }
uint64_t prepare_live_bytes(void) { xmemdebugsnapshot snapshot; xrtMemDebugSnapshot(&snapshot); return snapshot.LiveBytes; }
