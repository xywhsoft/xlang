#ifndef XLS_SIGNATURE_HELP_WIRE_H
#define XLS_SIGNATURE_HELP_WIRE_H
#include "analysis/signature.h"
#include <xrt.h>
enum { XLS_SIGNATURE_LABEL_OFFSETS=1, XLS_SIGNATURE_ACTIVE_PARAMETER=2 };
/* Pure wire/schema adapter. No disk work or semantic cache mutation. The
 * caller validates captured inputs and current document identity separately.
 * Returned values own every string; unavailable native facts become null. */
int xlsSignatureHelpCapabilities(const xvalue* capabilities,unsigned* flags);
int xlsSignatureHelpContextValid(const xvalue* context);
xvalue* xlsSignatureHelpValue(const xls_signature_help* result,unsigned flags);
#endif
