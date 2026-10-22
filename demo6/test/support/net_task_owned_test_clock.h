#ifndef XL6_TASK_NET_OWNED_TEST_CLOCK_H
#define XL6_TASK_NET_OWNED_TEST_CLOCK_H
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_TASK_NET
#if defined(XL6_TASK_NET_NATIVE_TIME)
#include "task-net-native.h"
typedef double TestDeadline;
static TestDeadline testDeadline(void) { return xrtTimer()+5.0; }
static bool testExpired(TestDeadline deadline) { return xrtTimer()>=deadline; }
#define TEST_WAIT 5000
#define TEST_DELAY INT64_C(3600000)
typedef xfuture* (*TestRelativeProc)(xnetengine*,uint64,ptr,xcancel*,
    const xtasknetdataownershipv1*,const xfuturepayloadownershipv1*,int64);
_Static_assert(_Generic(&xrtTaskNetForDataPolicyV1,TestRelativeProc:1,default:0),"native milliseconds/For ABI");
#else
#include "lib/xrt/xrt.h"
typedef xdeadline TestDeadline;
static TestDeadline testDeadline(void) { return xrtDeadlineAfter(5000000); }
static bool testExpired(TestDeadline deadline) { return xrtDeadlineExpired(deadline); }
#define TEST_WAIT UINT64_C(5000000)
#define TEST_DELAY UINT64_C(3600000000)
typedef xfuture* (*TestRelativeProc)(xnetengine*,uint64,ptr,xcancel*,
    const xtasknetdataownershipv1*,const xfuturepayloadownershipv1*,uint64);
typedef xfuture* (*TestAbsoluteProc)(xnetengine*,uint64,ptr,xcancel*,
    const xtasknetdataownershipv1*,const xfuturepayloadownershipv1*,xdeadline);
_Static_assert(_Generic(&xrtTaskNetUntilDataPolicyV1,TestAbsoluteProc:1,default:0),"pinned microseconds/Until ABI");
#endif
_Static_assert(_Generic(&xrtTaskNetAfterDataPolicyV1,TestRelativeProc:1,default:0),"relative time ABI");
#endif

