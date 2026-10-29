/* Offline parity of the exact embedded core/runtime products, not a DLL test. */
#if DECLARATIONS_FIRST
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#else
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_MEMORY_DEBUG
#if TEST_RUNTIME
#define XRUNTIME_IMPLEMENTATION
#include "xruntime.h"
#else
#define XRT_IMPLEMENTATION
#include "xrt.h"
#endif
#endif
#include <assert.h>
#include <stdio.h>

#ifndef SNAPSHOT_PROVIDER
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const uint64_t identity=UINT64_C(0x123456789ABCDEF0);
    const uint32_t domain=UINT32_C(0xFEDCBA98);
    xvalue* scalar=xrtValueInt(254);assert(scalar);
    assert(xrtMemDebugFailAfter(0));
    assert(xrtValueScalarDomainBindV1(scalar,identity,domain)&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xrtValueTypeId(scalar)==identity&&xrtValueScalarDomainV1(scalar)==domain);
    xvalue* copy=xrtValueClone(scalar);assert(copy==scalar);
    assert(xrtValueScalarDomainBindV1(copy,identity,domain));
    assert(!xrtValueScalarDomainBindV1(copy,identity,domain-1)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xrtValueRelease(copy);
    assert(!xrtValueTypeIdRebind(scalar,identity+1)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xvalue* array=xrtValueArray();assert(array&&xrtValueArrayAppend(array,scalar));
    copy=xrtValueDeepClone(array);assert(copy);
    const xvalue* item=xrtValueArrayGet(copy,0);
    assert(item&&xrtValueTypeId(item)==identity&&xrtValueScalarDomainV1(item)==domain);
    #if !DECLARATIONS_FIRST
    assert(sizeof(void*)!=8||(sizeof(xvalue)==88&&offsetof(xvalue,TypeId)==16&&offsetof(xvalue,Data)==64));
    #endif
    xrtValueRelease(copy);xrtValueRelease(array);xrtValueRelease(scalar);xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    puts("Embedded scalar domain: exact metadata, clone, checked refusal, 64-bit ABI and zero live delta");
    return 0;
}
#endif
