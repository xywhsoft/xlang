#ifndef XL6_IR_CGEN_NATIVE_HEADER_H
#define XL6_IR_CGEN_NATIVE_HEADER_H
#include "xl6_ir_cgen.h"

/* Private compile-time contract for source packages and C sidecars. Includes
 * exact imported storage declarations; these are not public xlang exports. */
int xl6IrCgenEmitNativeContractHeader(XL6_Output* out, const XL6_IrModule* module,
    const char* guardName, XL6_DiagList* diags);
#endif
