/*
 * XLIR owns the generated translation unit and resolves the standard-library
 * facade to the public XRT symbol.  The host only verifies the module-entry
 * ABI as an ordinary C executable.
 */
typedef struct xvalue xvalue;

#include "../../build/test/xlir_typed_array_class_return_generated.c"

int main(void)
{
    return xlang_main((xvalue*)0);
}
