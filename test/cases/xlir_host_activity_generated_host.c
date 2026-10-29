/* Actual compiler-emitted module, statically resident GCC execution lane.
 * Independent from real TCC/unmapping and shipping AOT acceptance. */
#include XL6_ACTIVITY_GENERATED_C
#include "test/support/module_product_memory.h"
#include "release/include/xrt_decl.h"
#include <assert.h>

static unsigned depth, pins, acquired, released, phases[4];
static bool open = true;
static XL6_CodeOwner owner;
extern void __real_xl6CodeActivityEnterV1(XL6_CodeActivityV1*, const XL6_CodeOwner*);
extern void __real_xl6CodeActivityLeaveV1(XL6_CodeActivityV1*);
void __wrap_xl6CodeActivityEnterV1(XL6_CodeActivityV1* scope, const XL6_CodeOwner* capability)
{ __real_xl6CodeActivityEnterV1(scope, capability); ++depth; }
void __wrap_xl6CodeActivityLeaveV1(XL6_CodeActivityV1* scope)
{ assert(depth); --depth; __real_xl6CodeActivityLeaveV1(scope); }
static int acquire(void* context)
{ assert(context == &pins); if (!open) return 0; ++pins; ++acquired; return 1; }
static void release(void* context)
{ assert(context == &pins && pins); --pins; ++released; }
int64_t activity_probe(int64_t phase)
{
    assert(depth && phase > 0 && phase < 4); ++phases[phase];
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&owner) && !xl6CodeOwnerScopeForeign(&owner));
    assert(xrtOwnershipScopeEnd(&freeze)); return 42;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    owner = (XL6_CodeOwner){sizeof(owner), &pins, acquire, release};
    assert(xl_script_bind_code_owner(&owner) && xl_script_module_init(NULL) && !depth);
    assert(phases[1] == 1 && acquired == 1 && pins == 1);
    int code = -1; assert(xl_script_main_checked(NULL, &code) && !depth);
#ifdef XL6_ACTIVITY_SYNTHETIC
    assert(code == 0 && phases[2] == 1);
#else
    assert(code == 42 && phases[2] == 1);
#endif
    assert(xl_script_module_retirement_v2()->prepare() == XL6_MODULE_PREPARE_READY && !depth);
    open = false; assert(xrtMemDebugFailAfter(0));
    assert(xl_script_module_unit_checked() && !depth);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(phases[3] == 1 && !pins && acquired == 1 && released == 1);
    xrtClearError(); xl6ProductMemoryVerify();
    puts("generated host activity: initializer/main/finalizer, prepare, closed zero-allocation cleanup and balanced capability/heap");
    return 0;
}
