#ifndef XL6_IR_CGEN_H
#define XL6_IR_CGEN_H

#include "src/ir/xl6_ir.h"

/* Deterministic C emission for lowered XLIR.  This is deliberately separate
 * from the legacy AST/HIR Cgen while feature coverage is migrated. */
int xl6IrCgenVerifyEmittable(const XL6_IrModule* module, XL6_DiagList* diags);
typedef struct XL6_IrCgenOptions {
    /* Emit the stable xlang host/module entry even when the source consists
     * only of executable top-level statements. */
    int emitScriptEntry;
    /* Emit the process entry adapter only for executable artifacts. */
    int emitMainWrapper;
} XL6_IrCgenOptions;
typedef int (*XL6_IrCgenNativeModuleVisitor)(void* context, const char* moduleName);
/* Enumerates the native implementation requirements selected from verified
 * Lowered XLIR. Does not emit C, read AST, or inspect source/identifier text. */
int xl6IrCgenVisitNativeModules(const XL6_IrModule* module, XL6_DiagList* diags,
    const XL6_IrCgenOptions* options, XL6_IrCgenNativeModuleVisitor visitor, void* context);
int xl6IrCgenEmitCEx(XL6_Output* out, const XL6_IrModule* module,
                     XL6_DiagList* diags, const XL6_IrCgenOptions* options);
int xl6IrCgenEmitC(XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags);
/* The public C surface is derived from the same lowered export ABI as the
 * implementation.  It deliberately has no AST/header-model dependency. */
int xl6IrCgenEmitHeader(XL6_Output* out, const XL6_IrModule* module,
                        const char* guardName, XL6_DiagList* diags);

#endif
