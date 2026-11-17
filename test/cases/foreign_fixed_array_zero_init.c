#define XRT_MODULE_ALL
#include <xrt_decl.h>
#include "foreign_fixed_array_zero_init.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureFixedZeroInts(const int* values)
{for(unsigned i=0;i<3;++i)assert(values[i]==0);}
void fixtureFixedZeroPoints(const fixed_zero_point_t* values)
{for(unsigned i=0;i<3;++i)assert(values[i].x==0 && values[i].y==0 && values[i].z==0);}
void fixtureFixedZeroRecord(const fixed_zero_record_t* value)
{
 for(unsigned i=0;i<7;++i)assert(value->bytes[i]==0);
 for(unsigned i=0;i<5;++i)assert(value->tags[i]==0);
 fixtureFixedZeroPoints(value->points);
}
void fixtureFixedZeroHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureFixedZeroHeapEnd(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(before.AllocCount==after.AllocCount && before.ReallocCount==after.ReallocCount);
 assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
 puts("FIXED_ARRAY_ZERO_HOT_ALLOCATIONS=0");
}
