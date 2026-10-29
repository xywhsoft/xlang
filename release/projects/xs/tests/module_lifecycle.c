#include "xl.h"

#include <stdio.h>

typedef int (*xs_test_module_read_proc)(void);

int main(void)
{
    XL_ContextOptions options;
    XL_Context* context;
    XL_Module* module;
    xs_test_module_read_proc read_value;
    int result;

    result = 0;
    options = xlDefaultContextOptions();
    options.workDir = ".";
    context = xlCreateContext(&options);
    if ( context == NULL ) {
        return 1;
    }
    module = xlCompileScriptFileToModule(
        context,
        "projects/xs/tests/handlers/module_lifecycle.xl");
    if ( module == NULL ) {
        fprintf(stderr, "%s\n", xlGetLastError(context));
        xlDestroyContext(context);
        return 2;
    }

    if ( !xlActivateModule(context, module, NULL) ) {
        result = 3;
        goto cleanup;
    }
    read_value = (xs_test_module_read_proc)xlModuleGetSymbol(module, "xs_test_module_read");
    if ( read_value == NULL || read_value() != 41 || read_value() != 42 ) {
        result = 4;
        goto cleanup;
    }
    if ( xlRunModuleMain(context, module, NULL) ) {
        result = 5;
        goto cleanup;
    }
    if ( !xlDeactivateModule(context, module) || !xlDeactivateModule(context, module) ) {
        result = 6;
        goto cleanup;
    }
    if ( !xlActivateModule(context, module, NULL) || read_value() != 41 ) {
        result = 7;
        goto cleanup;
    }

cleanup:
    xlDestroyModule(module);
    xlDestroyContext(context);
    if ( result == 0 ) {
        puts("xs2 module lifecycle test ok");
    }
    return result;
}
