#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_lease.h"
#include "test/support/ownership_test_thread.h"
#include <assert.h>
#include <stdio.h>

typedef struct Owner { unsigned pins,acquires,releases; } Owner;
static unsigned runs;
static int acquire(void* data){Owner* o=data;++o->pins;++o->acquires;return 1;}
static void worker(void* unused)
{
    xrtownershipscope mutation={0};(void)unused;
    assert(xrtOwnershipMutationBegin(&mutation));xvalue* v=xrtValueInt(175);assert(v);xrtValueRelease(v);
    assert(xrtOwnershipScopeEnd(&mutation));
}
static void release(void* data)
{
    Owner* o=data;xrtownershipscope freeze={0};
    assert(o->pins==1 && xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipScopeEnd(&freeze));
    xl6OwnershipTestThreadRun(worker,NULL);
    --o->pins;++o->releases;++runs;
    xrtSetErrorKind(XERR_ARGUMENT); /* must not replace the caller's error */
}
int main(void)
{
    xmemdebugsnapshot before,after;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    for(unsigned round=0;round<100;++round)for(unsigned scoped=0;scoped<2;++scoped){
        Owner owner={0};XL6_CodeOwner descriptor={sizeof(descriptor),&owner,acquire,release};
        XL6_CodeLease* lease=NULL;XL6_CodeScope scope={0};
        assert(xl6CodeLeaseAcquire(&descriptor,&lease)==XL6_CODE_LEASE_ACQUIRED && lease);
        if(scoped)xl6CodeScopeEnter(&scope,lease);
        xrtSetErrorKind(XERR_STATE);const xerror* error=xrtGetError();
        xl6CodeLeaseRelease(lease);
        if(scoped){assert(owner.pins==1 && !owner.releases);xl6CodeScopeLeave(&scope);}
        assert(!owner.pins && owner.acquires==1 && owner.releases==1 && xrtGetError()==error);xrtClearError();
    }
    xrtMemDebugSnapshot(&after);assert(runs==200 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    puts("Code lease retirement tails: 100 direct and 100 scoped final releases, exclusive graph freezes and native worker joins, exact diagnostic and allocator balance");return 0;
}
