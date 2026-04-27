#include "xlrt_generated.h"

#if XLANG_DEBUGMODE_ENABLED

static void __xlDebugWriteText(const char* sText)
{
	fputs((sText != NULL) ? sText : "", stdout);
}

const char* xl_debug_text(xvalue value)
{
	return xl_value_to_string(value);
}

void xl_debug_print(xvalue value)
{
	__xlDebugWriteText(xl_value_to_string(value));
}

void xl_debug_println(xvalue value)
{
	__xlDebugWriteText(xl_value_to_string(value));
	__xlDebugWriteText("\n");
}

void xl_trace_write(const char* sText)
{
	__xlDebugWriteText("[trace] ");
	__xlDebugWriteText((sText != NULL) ? sText : "");
	__xlDebugWriteText("\n");
}

#endif
