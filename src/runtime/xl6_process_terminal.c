#include "src/runtime/xl6_text_value.h"

/* Native probe reports unavailable as false, not as a fabricated error.
 * It is intentionally not declared pure: platform capability can change. */
bool xl6ProcessTerminalSupportedV1(void)
{return xrtProcessTerminalSupported();}
