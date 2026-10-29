#ifndef XL6_TASK_NET_OWNED_TEST_CLOCK_H
#define XL6_TASK_NET_OWNED_TEST_CLOCK_H
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_TASK_NET
#if defined(XL6_TASK_NET_NATIVE_TIME)
#include "task-net-native.h"
#else
#include "lib/xrt/xrt.h"
#endif
typedef double TestDeadline;
static TestDeadline testDeadline(void) { return xrtTimer()+5.0; }
static bool testExpired(TestDeadline deadline) { return xrtTimer()>=deadline; }
#define TEST_WAIT INT64_C(5000)
#define TEST_DELAY INT64_C(3600000)
typedef xfuture* (*TestRelativeProc)(xnetengine*,uint64,ptr,xcancel*,
    const xtasknetdataownershipv1*,const xfuturepayloadownershipv1*,int64);
_Static_assert(_Generic(&xrtTaskNetForDataPolicyV1,TestRelativeProc:1,default:0),"milliseconds/For ABI");
_Static_assert(_Generic(&xrtTaskNetAfterDataPolicyV1,TestRelativeProc:1,default:0),"milliseconds/After ABI");
#endif
