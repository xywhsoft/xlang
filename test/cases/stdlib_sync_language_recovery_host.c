/* Real generated language cleanup. No native provider or physical outcome
 * is replaced: a held native lock must produce a checked failure and a record. */
#include "test/support/module_product_memory.h"
#include "xl_sync_retirement.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef XLIR_GENERATED_MODULE
#error "An actual generated language module is required"
#endif
#include XLIR_GENERATED_MODULE
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xl_script_module_init(NULL));
    xvalue* arguments=xrtValueArray(); assert(arguments);
    int code=-99;
    assert(!xl_script_main_checked(arguments,&code) && code==1);
    xrtValueRelease(arguments);
    assert(xrtGetError());
#ifdef SYNC_PRIMARY
    assert(strstr(xrtErrorMessage(xrtGetError()),"sync original assertion"));
#endif
    XL_SyncRetirementV1 row; size_t count=0;
    assert(xlSyncRetirementSnapshotCheckedV1(&row,1,&count) && count==1);
    const xerror* primary=xrtGetError();
    assert(!xlSyncRetirementRetryCheckedV1(row.token) && xrtGetError()==primary);
    xvalue* repair=NULL;
    assert(xlSyncRetirementAcquireCheckedV1(row.token,&repair) && repair && xrtGetError()==primary);
    assert(xlSyncRetirementUnlockCheckedV1(repair,SYNC_UNLOCK_MODE) && xrtGetError()==primary);
    assert(xlSyncRetirementRetryCheckedV1(row.token) && xrtGetError()==primary);
    xrtValueRelease(repair); xrtClearError();
    assert(xl_script_module_unit_checked());
    assert(xlSyncRetirementSnapshotCheckedV1(NULL,0,&count) && count==0);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("actual language held cleanup: checked failure, explicit repair/retry and lifetimes balanced");
    return 0;
}
