#include "export_enum_abi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) && defined(XL6_USE_SHARED)
#include <windows.h>
#ifndef XL6_TEST_ENUM_DLL
#define XL6_TEST_ENUM_DLL "export_enum_abi.dll"
#endif
#endif

/* Current closed XLIR ABI: numeric enums are scalars; tagged enums/classes
 * are owned xvalue carriers. Metadata is tested by CLI ABI, not old structs.
 * These calls never use private enum keys or assume a payload layout. */
int main(void)
{
    bool (*begin)(xl6_foreign_error_scope*)=xl6ForeignCallBegin;
    xl6_foreign_error_result (*end)(xl6_foreign_error_scope*)=xl6ForeignCallEnd;
    void (*freeError)(xerror*)=xrtErrorFree;
#if defined(_WIN32) && defined(XL6_USE_SHARED)
    /* Independent DLLs have independent TLS error stacks. Never let import
     * name collisions silently bracket the module with xl.dll's scope. */
    HMODULE module=GetModuleHandleA(XL6_TEST_ENUM_DLL); assert(module);
    FARPROC address=GetProcAddress(module,"xl6ForeignCallBegin"); assert(address);
    _Static_assert(sizeof(address)==sizeof(begin),"Windows function pointer ABI"); memcpy(&begin,&address,sizeof(begin));
    address=GetProcAddress(module,"xl6ForeignCallEnd"); assert(address); memcpy(&end,&address,sizeof(end));
    address=GetProcAddress(module,"xrtErrorFree"); assert(address); memcpy(&freeError,&address,sizeof(freeError));
#endif
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    xvalue* packet=enum_make_packet(42); assert(packet && xrtValueTypeId(packet));
    uint64_t packetId=xrtValueTypeId(packet);
    xvalue* copy=xrtValueClone(packet); assert(copy && copy!=packet && xrtValueTypeId(copy)==packetId);
    xvalue* retained=xrtValueRetain(packet); assert(retained==packet);
    assert(enum_packet_value(packet)==42 && enum_packet_value(copy)==42);
    xrtValueRelease(packet);
    assert(enum_packet_value(retained)==42); xrtValueRelease(retained);
    assert(enum_packet_value(copy)==42);

    char input[]={'a',0,(char)0xE4,(char)0xBD,(char)0xA0,0};
    xvalue* failure=enum_make_failure((xlir_string){input,5});
    assert(failure && xrtValueTypeId(failure) && xrtValueTypeId(failure)!=packetId);
    xvalue* failureCopy=xrtValueClone(failure); assert(failureCopy && failureCopy!=failure);
    input[0]='z'; /* C caller may change its buffer after the value copy. */
    assert(enum_failure_code(failure)==503 && enum_failure_code(failureCopy)==503);
    assert(enum_failure_matches(failureCopy,(xlir_string){"a\0你",5}));
    assert(!enum_failure_matches(failureCopy,(xlir_string){"a",1}));
    xrtValueRelease(failure);
    assert(enum_failure_matches(failureCopy,(xlir_string){"a\0你",5}));

    xvalue* values=xrtValueArray(); assert(values);
    for (int64_t i=10;i<=30;i+=10) assert(xrtValueArrayAppendNew(values,xrtValueInt(i)));
    xvalue* inspector=enum_make_inspector(); assert(inspector && xrtValueTypeId(inspector));
    xvalue* batch=enum_make_batch(values); assert(batch && xrtValueTypeId(batch));
    assert(enum_batch_count(batch)==3 && xrtValueCount(values)==3);
    assert(enum_inspector_packet_value(inspector,copy)==42 && enum_packet_value(copy)==42);
    assert(enum_inspector_batch_count(inspector,values)==3 && xrtValueCount(values)==3);
    /* Containers remain reference objects. No obsolete byval clone contract. */
    assert(xrtValueArrayAppendNew(values,xrtValueInt(40)));
    assert(enum_batch_count(batch)==4);
    xrtValueRelease(values); assert(enum_batch_count(batch)==4);

    assert(enum_status(200)==200 && enum_status(201)==201 && enum_status(404)==404);
    assert(enum_kind_int(9)==10);
    assert(enum_kind_string((xlir_string){"你",3})==1);
    assert(enum_kind_string((xlir_string){"a\0你",5})==3);
    xrtSetErrorKind(XERR_MEMORY); const xerror* prior=xrtGetError(); assert(prior);
    xl6_foreign_error_scope scope={0}; assert(begin(&scope));
    assert(enum_packet_value(copy)==42);
    xl6_foreign_error_result outcome=end(&scope);
    assert(outcome.ok && !outcome.error && xrtGetError()==prior); xrtClearError();
    assert(begin(&scope));
    enum_fail(); outcome=end(&scope);
    assert(!outcome.ok && outcome.error); freeError(outcome.error); xrtClearError();
    /* Recovery is proved with another valid call, not a NULL diagnostic. */
    assert(enum_packet_value(copy)==42 && !xrtGetError());
    xrtValueRelease(batch); xrtValueRelease(inspector); xrtValueRelease(failureCopy); xrtValueRelease(copy);
    xrtMemDebugSnapshot(&after);
#ifndef XL6_USE_SHARED
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
        after.UseAfterFreeCount==before.UseAfterFreeCount);
#endif
#if defined(_WIN32) && defined(XL6_USE_SHARED)
    assert(xl_script_runtime_retire_checked());
#endif
    puts("enum closed C ABI: identity, ownership, exact text, reference containers and foreign errors passed");
    return 0;
}
