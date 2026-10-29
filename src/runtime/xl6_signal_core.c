#include "src/runtime/xl6_text_internal.h"
#include <string.h>

/* Language int is 64-bit; xsignal is a native enum. Never let narrowing turn
 * an invalid code into a valid (and possibly process-terminating) signal.
 * Queries preserve XRT's false/UNKNOWN convention for unknown codes. */
static bool signalCode(int64_t value, xsignal* code)
{
    if (value < INT_MIN || value > INT_MAX) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.signal", XSIGNAL_ERROR_CODE,
            "Signal code does not fit the native enum");
        return false;
    }
    *code = (xsignal)value;
    return true;
}

bool xl6SignalSupportedV1(int64_t code)
{ return code >= INT_MIN && code <= INT_MAX && xrtSignalSupported((xsignal)code); }

xstrview xl6SignalNameV1(int64_t code)
{
    const char* name = code >= INT_MIN && code <= INT_MAX
        ? xrtSignalName((xsignal)code) : "UNKNOWN";
    /* Only XRT's static ASCII CString crosses this boundary. The language
     * result is independent owned data with an explicit byte length. */
    return xl6TextOwn(xrtStrDup(name), strlen(name));
}

bool xl6SignalIgnoreV1(int64_t value)
{ xsignal code; return signalCode(value, &code) && xrtSignalIgnore(code); }

bool xl6SignalRestoreV1(int64_t value)
{ xsignal code; return signalCode(value, &code) && xrtSignalRestore(code); }

bool xl6SignalRestoreAllV1(void)
{ return xrtSignalRestoreAll(); }

bool xl6SignalRaiseV1(int64_t value)
{ xsignal code; return signalCode(value, &code) && xrtSignalRaise(code); }

uint64_t xl6SignalCountV1(int64_t value)
{ xsignal code; return signalCode(value, &code) ? xrtSignalCount(code) : 0; }

bool xl6SignalReceivedV1(int64_t value)
{ xsignal code; return signalCode(value, &code) && xrtSignalReceived(code); }

void xl6SignalClearV1(int64_t value)
{ xsignal code; if (signalCode(value, &code)) (void)xrtSignalClear(code); }

bool xl6SignalHealthyV1(void)
{ return xrtSignalHealthy(); }

void xl6SignalResetV1(void)
{ (void)xrtSignalShutdown(); }
