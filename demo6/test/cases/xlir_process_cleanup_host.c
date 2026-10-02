/* Process execution is resident, not a generated-C algorithm to intercept.
 * The real runtime must release the complete module-owned object graph. */
#include <__xl_runtime/xruntime_decl.h>
#include <assert.h>
#include <stdio.h>
#include XLIR_GENERATED_MODULE
int main(void)
{
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    bool initialized = xlir_module_init();
    if (!initialized || xrtGetError()) fprintf(stderr, "module init kind=%d message=%s\n",
        (int)xrtErrorKind(xrtGetError()), xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
    assert(initialized && !xrtGetError());
    xlir_module_fini();
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    return 0;
}
