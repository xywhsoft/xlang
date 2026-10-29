#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void generator_close_prepare(void), generator_close_finish(void);
extern void generator_close_case(int, int, int);
extern int generator_close_cases_failed(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int mode, limit, form, executions = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    generator_close_prepare();
    for (form = 0; form < 4; ++form) for (mode = 0; mode < (form == 0 ? 2 : form == 2 ? 3 : 1); ++mode)
        for (limit = 1; limit <= (form <= 1 ? 5 : 1); ++limit) {
            if (!smoke) xrtMemDebugSnapshot(&before);
            generator_close_case(form, mode, limit); ++executions;
            if (!smoke) continuation_owner_fault_balanced(&before);
        }
    generator_close_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("generator close routes: %d executions, %d semantic failures; memory checks %s\n",
        executions, generator_close_cases_failed(), smoke ? "disabled" : "balanced");
    return generator_close_cases_failed() ? 1 : 0;
}
