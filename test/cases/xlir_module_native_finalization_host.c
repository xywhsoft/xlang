#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../build/test/xlir_module_native_finalization.c"

static int opened, closed, order, invalid;
static bool fail_initialization;
void* module_native_open(int64_t id)
{
    ++opened;
    return (void*)(intptr_t)id;
}
bool module_native_should_fail(void) { return fail_initialization; }
void module_native_close(void* handle)
{
    xerror* diagnostic;
    if (handle == NULL) invalid = 1;
    ++closed;
    order = order * 10 + (int)(intptr_t)handle;
    diagnostic = xrtErrorCreate(15, "native-finalizer-test", 200 + (int)(intptr_t)handle, "cleanup diagnostic");
    if (diagnostic == NULL) invalid = 1;
    xrtSetErrorTake(diagnostic);
}

int main(void)
{
    xl_script_module_unit();
    if (!xl_script_module_init(NULL) || opened != 2 || closed != 0) return 1;
    xl_script_module_unit();
    xl_script_module_unit();
    if (invalid || closed != 2 || order != 21 || xrtGetError() == NULL || xrtErrorCode(xrtGetError()) != 202) return 2;
    xrtClearError();
    fail_initialization = true;
    order = 0;
    if (xl_script_module_init(NULL) || opened != 3 || closed != 3 || order != 1 ||
        xrtGetError() == NULL || strcmp(xrtErrorMessage(xrtGetError()),
            "native module initialization failed") != 0) return 3;
    xl_script_module_unit();
    if (closed != 3) return 4;
    xrtClearError();
    fail_initialization = false;
    order = 0;
    if (!xl_script_module_init(NULL)) return 5;
    xl_script_module_unit();
    if (invalid || opened != 5 || closed != 5 || order != 21 || xrtGetError() == NULL || xrtErrorCode(xrtGetError()) != 202) return 6;
    xrtClearError();
    puts("native module finalization: guarded handles, zero storage, reverse order, partial failure and retry ok");
    return 0;
}
