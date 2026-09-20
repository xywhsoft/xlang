#include "release/include/xl.h"
#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void module_cleanup_configure(int);
int module_cleanup_complete(void);
void* module_cleanup_open(int64_t);
bool module_cleanup_fail(void);
int64_t module_cleanup_entry(void);
void module_cleanup_close(void*);

int main(void)
{
    XL_Context* ctx;
    XL_Module* module;
    int mode, repeat;
    /* Instrument allocations before the API initializes its runtime pool. */
    assert(xrtMemDebugEnable(true));
    ctx = xlCreateContext(NULL);
    assert(ctx != NULL);
    assert(xlRegisterCFunction(ctx, "module_cleanup_open", (void*)module_cleanup_open));
    assert(xlRegisterCFunction(ctx, "module_cleanup_fail", (void*)module_cleanup_fail));
    assert(xlRegisterCFunction(ctx, "module_cleanup_entry", (void*)module_cleanup_entry));
    assert(xlRegisterCFunction(ctx, "module_cleanup_close", (void*)module_cleanup_close));
    module = xlCompileScriptFileToModule(ctx, "test/cases/xlir_module_cleanup_outcome.xl");
    if (module == NULL) fprintf(stderr, "%s\n", xlGetLastError(ctx));
    assert(module != NULL);
    assert(xlModuleGetSymbol(module, "xl_script_main_checked") != NULL);
    assert(xlModuleGetSymbol(module, "xl_script_module_unit_checked") != NULL);
    for (repeat = 0; repeat < 4; ++repeat) {
        for (mode = 0; mode <= 8; ++mode) {
            module_cleanup_configure(mode); xlClearLastError(ctx);
            assert(xlRunModuleMain(ctx, module, NULL) == (mode <= 1));
            if (mode <= 1) assert(xlGetLastExitCode(ctx) == mode);
            assert(module_cleanup_complete() && xrtGetError() == NULL);
            assert(xlDeactivateModule(ctx, module));
            module_cleanup_configure(mode); xlClearLastError(ctx);
            assert(xlActivateModule(ctx, module, NULL) == (mode != 6 && mode != 7));
            if (mode != 6 && mode != 7) {
                assert(xlDeactivateModule(ctx, module) == (mode <= 1));
                assert(xlDeactivateModule(ctx, module));
            }
            assert(module_cleanup_complete() && xrtGetError() == NULL);
        }
    }
    xlDestroyModule(module); xlDestroyContext(ctx);
    puts("module cleanup DLL API: 72 run/activate/unload/retry cases passed");
    return 0;
}
