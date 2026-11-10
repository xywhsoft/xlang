#ifndef FIXTURE_XGE3_INIT_COST_H
#define FIXTURE_XGE3_INIT_COST_H
#include <xrt_decl.h>
static inline void fixtureInitHeap(size_t* count,size_t* bytes)
{xmemdebugsnapshot value;xrtMemDebugSnapshot(&value);*count=value.LiveCount;*bytes=value.LiveBytes;}
#endif
