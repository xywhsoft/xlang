#include "src/runtime/xl6_value_format.h"

bool xl6_xrt_print(xvalue* values, bool newline)
{
    xstrbuf output;
    bool ready = true;
    xrtStrBufInit(&output);
    if (values && xrtValueType(values) == XVALUE_ARRAY) {
        size_t count = xrtValueCount(values);
        for (size_t i = 0; ready && i < count; ++i) {
            if (i) ready = xrtStrBufAppend(&output, xrtStrView(" "));
            if (ready) ready = xl6ValueTextAppendV1(&output, xrtValueArrayGet(values, i));
        }
    } else ready = xl6ValueTextAppendV1(&output, values);
    if (ready) ready = newline ? xrtConsoleWriteLine(XCONSOLE_STDOUT, xrtStrBufView(&output)) :
        xrtConsoleWrite(XCONSOLE_STDOUT, xrtStrBufView(&output));
    xrtStrBufFree(&output);
    return ready;
}
