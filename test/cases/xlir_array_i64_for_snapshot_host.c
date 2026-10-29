#include <stdint.h>
#ifndef XLIR_ARRAY_FOR_GENERATED_SOURCE
#define XLIR_ARRAY_FOR_GENERATED_SOURCE "../../build/test/xlir_array_i64_for_abi_generated.c"
#endif
#include XLIR_ARRAY_FOR_GENERATED_SOURCE
#include "xruntime_decl.h"
#include <assert.h>
#include <stdio.h>

void expect_i64(int64_t actual,int64_t expected) { assert(actual==expected); }
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (unsigned iteration=0;iteration<2;++iteration) {
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        assert(xlir_module_init() && !xrtGetError());
        xlir_result_void retired=xlir_module_fini_body();
        assert(retired.ok && !xrtGetError() && xlir_module_initialized==0);
        xrtMemDebugSnapshot(&after);
        assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
            after.UseAfterFreeCount==before.UseAfterFreeCount);
    }
    puts("Array for: 11 snapshot/transfer loops, two language lifecycles and memory balance");
    return 0;
}
