#ifndef XL6_CGEN_NATIVE_VOID_ABI_H
#define XL6_CGEN_NATIVE_VOID_ABI_H
#include "src/common/xl6_output.h"

/* Instantiate component templates with XLIR's canonical native-void ABI. */
static void xl6CgenEmitNativeVoidAbiBegin(XL6_Output* out)
{
    xl6OutputPuts(
        "#define XL6_NATIVE_VOID_RESULT void\n#define XL6_NATIVE_VOID_RETURN() return\n",
        out);
}

static void xl6CgenEmitNativeVoidAbiEnd(XL6_Output* out)
{
    xl6OutputPuts("\n#undef XL6_NATIVE_VOID_RESULT\n#undef XL6_NATIVE_VOID_RETURN\n",out);
}
#endif
