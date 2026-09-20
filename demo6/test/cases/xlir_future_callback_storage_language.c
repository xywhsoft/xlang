#include "xlir_future_callback_storage.c"
#define XLIR_GENERATED_MODULE_PREINCLUDED
#define main original_future_language_main
#include "xlir_future_continuation_abi_host.c"
#undef main
#include <assert.h>
int main(void)
{
    xmemdebugsnapshot before,after;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    int result=original_future_language_main();if(result)return result;
    xlir_module_fini();assert(!xrtGetError());xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    puts("Original Future language fixture: all 10 continuation sites, complete cleanup, zero live delta");return 0;
}
