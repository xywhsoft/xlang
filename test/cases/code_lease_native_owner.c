/* Resident native-owner hooks retain exact RC and mark final native tails. */
#define XRT_MODULE_ALL
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_foreign_callback_scope.h"
#include "src/common/xl6_xrt.h"
#include <assert.h>
#include <stdio.h>
typedef struct Owner {int acquired,released,marked;} Owner;
static int acquire(void* data){++((Owner*)data)->acquired;return 1;}
static void release(void* data)
{Owner* owner=data;++owner->released;owner->marked=xl6ForeignCallbackActiveV1();}
static bool count(const void* data,size_t* output)
{const Owner* owner=data;*output=(size_t)(owner->acquired-owner->released);return true;}
static bool trace(const void* data,xrtownershipvisitor visit,void* user)
{(void)data;(void)visit;(void)user;return true;}
static const xrtownershipops ops={count,trace};
int main(void)
{
    assert(xrtMemDebugEnable(true));
    XL6_ForeignCallbackScopeV1 warm={0};assert(xl6ForeignCallbackEnterV1(&warm));
    assert(xl6ForeignCallbackLeaveV1(&warm));
    Owner owner={0};XL6_CodeOwnerGraph code={{sizeof(code),&owner,acquire,release},UINT64_C(0x584c36434f574e31),&ops};
    XL6_CodeLease* lease=NULL;assert(xl6CodeLeaseAcquire(&code.base,&lease)==XL6_CODE_LEASE_ACQUIRED && lease);
    xrtownershipref ref=xl6CodeLeaseOwnership(lease);size_t refs=0;
    assert(ref.Ops->Count(ref.Data,&refs) && refs==1);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CodeLeaseHoldNativeV1(lease));
    assert(ref.Ops->Count(ref.Data,&refs) && refs==2);
    assert(xl6CodeLeaseDropNativeV1(lease) && !owner.released && !xl6ForeignCallbackActiveV1());
    assert(xl6CodeLeaseDropNativeV1(lease));
    assert(owner.acquired==1 && owner.released==1 && owner.marked && !xl6ForeignCallbackActiveV1());
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xl6CodeLeaseHoldNativeV1(NULL) && xl6CodeLeaseDropNativeV1(NULL));
    puts("native code owner: actual RC, marked release tail, zero warmed allocations");return 0;
}
