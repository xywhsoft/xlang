#ifndef XL6_CGEN_NATIVE_VOID_ABI_H
#define XL6_CGEN_NATIVE_VOID_ABI_H
#include "src/common/xl6_output.h"

/* Instantiate shared runtime bodies under the declared backend ABI. Legacy
 * represents native void as an integer sentinel; XLIR uses actual C void.
 * These template parameters never inspect or rewrite generated function text. */
static void xl6CgenEmitNativeVoidAbiBegin(XL6_Output* out, int xlirAbi)
{
    xl6OutputPuts(xlirAbi
        ? "#define XL6_NATIVE_VOID_RESULT void\n#define XL6_NATIVE_VOID_RETURN() return\n"
        : "#define XL6_NATIVE_VOID_RESULT long long\n#define XL6_NATIVE_VOID_RETURN() return 0LL\n",out);
}

static void xl6CgenEmitNativeVoidAbiEnd(XL6_Output* out)
{
    xl6OutputPuts("\n#undef XL6_NATIVE_VOID_RESULT\n#undef XL6_NATIVE_VOID_RETURN\n",out);
}
#endif
