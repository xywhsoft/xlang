#include "xrt.h"
#include <assert.h>
#include <string.h>

static xerror* remembered;

void error_probe_remember(xerror* value)
{
    assert(!remembered && value);
    remembered = xrtErrorRef(value);
}

bool error_probe_matches(xerror* value)
{
    return value == remembered && value &&
        strcmp(xrtErrorMessage(value), "original diagnostic") == 0;
}

bool error_probe_null(xerror* value)
{
    return value == NULL;
}

void error_probe_forget(void)
{
    assert(remembered && strcmp(xrtErrorMessage(remembered), "original diagnostic") == 0);
    xrtErrorFree(remembered);
    remembered = NULL;
}

void error_probe_fail(xerror* value)
{
    assert(error_probe_matches(value));
    xrtSetErrorInfo(XERR_RANGE, "extlib.error.probe", 1, "native probe rejected");
}
