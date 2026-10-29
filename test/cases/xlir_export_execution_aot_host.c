/* Includes compiler-emitted C, never a handwritten export or copied body.
 * Code is statically resident in this lane; the real unmapping lane is the
 * public API/TCC fixture. Borrowed code addresses do not own this capability. */
#include XL6_EXECUTION_GENERATED_C
#include "test/support/module_product_memory.h"
#include "release/include/xrt_decl.h"
#include <assert.h>

static size_t pins, acquisitions, releases;
static bool open = true;
static XL6_CodeOwner owner;
static int acquire(void* context)
{ assert(context == &pins); if (!open) return 0; ++pins; ++acquisitions; return 1; }
static void release(void* context)
{ assert(context == &pins && pins); --pins; ++releases; }
int64_t export_execution_probe(int64_t mode)
{
    xrtownershipscope freeze = {0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(pins == 1 && xl6CodeOwnerScopeActive(&owner) && !xl6CodeOwnerScopeForeign(&owner));
    assert(xrtOwnershipScopeEnd(&freeze));
    if (mode) xl6ErrorScopePropagateFailure((xl6_error_result){false, NULL});
    return 35;
}
static int64_t nested(void)
{
    size_t before = acquisitions;
    open = false;
    assert(scope_probe(0) == 42 && scope_pure(42) == 42 && acquisitions == before);
    return 42;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    owner = (XL6_CodeOwner){sizeof(owner), &pins, acquire, release};
    assert(xl_script_bind_code_owner(&owner));
    for (int mode = 0; mode < 2; ++mode) for (int kind = 0; kind < 3; ++kind) {
        xl6_foreign_error_scope scope = {0}; assert(xl6ForeignCallBegin(&scope));
        xvalue* value = NULL; int64_t scalar = 0;
        if (!kind) scalar = scope_probe(mode);
        else if (kind == 1) value = xl6_export_scope_value(mode);
        else scope_void(mode);
        xl6_foreign_error_result result = xl6ForeignCallEnd(&scope);
        assert(result.ok == !mode && !result.error && !pins);
        if (mode) assert(!scalar && !value);
        else if (!kind) assert(scalar == 42);
        else if (kind == 1) { int64 n = 0; assert(xrtValueGetInt(value, &n) && n == 7); }
        xrtValueRelease(value);
    }
    assert(scope_reenter(nested) == 42 && !pins && !open);
    xl6_foreign_error_scope scope = {0}; assert(xl6ForeignCallBegin(&scope));
    assert(scope_pure(42) == 0); /* No active accepted scope: Close refuses. */
    xl6_foreign_error_result result = xl6ForeignCallEnd(&scope);
    assert(!result.ok && !result.error && !pins && acquisitions == releases);
    /* Fragment C has no public script adapter; run its actual lowered
     * finalizer (this fixture has only one trivial integer global). */
    xlir_module_fini(); assert(xlir_module_initialized == 0);
    xrtClearError(); xl6ProductMemoryVerify();
    puts("generated C exports: six result/error vectors, closed nested inheritance, closed refusal and balanced capability/heap");
    return 0;
}
