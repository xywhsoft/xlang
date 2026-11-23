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

typedef struct ImageOwner {
    Owner code;
    Owner image;
    bool open;
} ImageOwner;
static int acquireImage(void* data)
{
    ImageOwner* owner=data;
    return owner->open && acquire(&owner->image);
}
static void releaseImage(void* data)
{release(&((ImageOwner*)data)->image);}

static void checkImageAdmission(void)
{
    ImageOwner owner={0};
    XL6_CodeOwnerImageV1 descriptor={
        {{sizeof(descriptor),&owner,acquire,release},UINT64_C(0x584c36434f574e31),&ops},
        UINT64_C(0x584c3643494d4731),acquireImage,releaseImage
    };
    XL6_CodeOwner* code=&descriptor.graph.base;
    XL6_CodeLease* lease=NULL;
    assert(xl6CodeLeaseAcquireImageV1(code,&lease)==XL6_CODE_LEASE_CLOSED && !lease);
    owner.open=true;
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CodeLeaseAcquireImageV1(code,&lease)==XL6_CODE_LEASE_RESOURCE_FAILURE && !lease);
    assert(xrtMemDebugFailTriggered() && owner.image.acquired==1 && owner.image.released==1);
    xrtMemDebugFailClear();
    xrtClearError();

    assert(xl6CodeLeaseAcquireImageV1(code,&lease)==XL6_CODE_LEASE_ACQUIRED && lease);
    xrtownershipref ref=xl6CodeLeaseOwnership(lease);
    size_t refs=0;
    assert(!ref.Ops->Count(ref.Data,&refs) && !owner.code.acquired);
    assert(xl6CodeLeaseDropNativeV1(lease) && owner.image.marked && owner.image.released==2);

    descriptor.graph.base.size=sizeof(descriptor.graph);
    assert(xl6CodeLeaseAcquireImageV1(code,&lease)==XL6_CODE_LEASE_ACQUIRED && lease);
    ref=xl6CodeLeaseOwnership(lease);
    assert(ref.Ops->Count(ref.Data,&refs) && refs==1 && owner.code.acquired==1);
    xl6CodeLeaseRelease(lease);
    assert(owner.code.released==1 && owner.image.acquired==2);
}

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
    checkImageAdmission();
    puts("native code owner: actual RC, marked release tail, image-only admission and legacy fallback");return 0;
}
