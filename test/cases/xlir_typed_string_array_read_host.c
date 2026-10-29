#include <string.h>

/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_typed_string_array_read_generated.c"

int main(void)
{
    xlir_result_string call;
    char* result;
    int ok;
    if ( !xlir_module_init() ) return 1;
    call = xlir_fn_1();
    if ( !call.ok ) return 1;
    result = call.value;
    ok = result != NULL && strcmp(result, "newtail") == 0;
    xrtFree(result);
    return ok ? 0 : 1;
}
