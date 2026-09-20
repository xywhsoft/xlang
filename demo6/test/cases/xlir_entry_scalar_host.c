/* The XLIR emitter owns the opaque runtime-boundary declaration. */
#include "../../build/test/xlir_entry_scalar_generated.c"

int main(void)
{
    return xlang_main((xvalue*)0) == 42 ? 0 : 1;
}
