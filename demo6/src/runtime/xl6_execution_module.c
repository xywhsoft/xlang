#include "include/xruntime_decl.h"
#include "xl6_execution_module_abi.h"
#include "xl6_execution_resource.h"
bool xl6ExecutionModuleInitializeV1(XL6_ExecutionSourceV1** slot)
{
    if(!slot||*slot){xrtSetErrorKind(XERR_STATE);return false;}
    *slot=xl6ExecutionSourceCreateV1();return *slot!=NULL;
}
XL6_ModulePrepareResult xl6ExecutionModulePrepareV1(XL6_ExecutionSourceV1* source)
{
    /* Stop new source admission only. Still-owned globals are released by
     * the verified IR finalizer; waiting for them here would deadlock it. */
    return !source||xl6ExecutionSourceCloseV1(source)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
XL6_ModuleRetireResult xl6ExecutionModuleRetireV1(XL6_ExecutionSourceV1** slot,bool wait)
{
    if(!slot){xrtSetErrorKind(XERR_ARGUMENT);return XL6_MODULE_RETIRE_FAILED;}
    if(!*slot)return XL6_MODULE_RETIRE_READY;
    if(!xl6ExecutionSourceCloseV1(*slot))return XL6_MODULE_RETIRE_FAILED;
    xdeadline deadline=wait?xrtDeadlineAfter(UINT64_C(5000000)):0;
    for(;;){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(slot);
        if(!result.ok)return XL6_MODULE_RETIRE_FAILED;
        if(result.ready)return XL6_MODULE_RETIRE_READY;
        if(!wait||xrtDeadlineExpired(deadline))return XL6_MODULE_RETIRE_BUSY;
        xrtSleep(1);
    }
}
