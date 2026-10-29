#include "src/runtime/xl6_text_internal.h"
#include <string.h>

/* The OS environment is a CString boundary, not a language string store.
 * Copy the selected view once to establish termination; never inspect past
 * its declared end or silently discard an embedded NUL. */
static char* environmentInput(xstrview text, int64_t size, bool name)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    if (memchr(text.Data, 0, text.Size) ||
        (name && (!text.Size || memchr(text.Data, '=', text.Size)))) {
        xrtSetErrorInfo(XERR_VALUE, "xrt.environment",
            name ? XENV_ERROR_NAME : XENV_ERROR_VALUE,
            name ? "Invalid environment variable name" : "Environment value contains NUL");
        return NULL;
    }
    return xrtStrDupView(text);
}

xstrview xl6EnvGetV1(xstrview name, int64_t size)
{
    char* key = environmentInput(name, size, true);
    char* value = NULL;
    if (!key) return xl6TextFailure;
    bool ok = xrtEnvLookup(key, &value);
    xrtFree(key);
    if (!ok || !value) return xl6TextFailure;
    /* Only this owned OS/XRT CString result uses strlen. Input language values
     * always retain their explicit lengths, including zero-sized selections. */
    return xl6TextOwn(value, strlen(value));
}

bool xl6EnvHasV1(xstrview name, int64_t size)
{
    xstrview value = xl6EnvGetV1(name, size);
    bool present = value.Data != NULL;
    xrtFree((void*)value.Data);
    return present;
}

void xl6EnvSetV1(xstrview name, xstrview value, int64_t nameSize, int64_t valueSize)
{
    char* key = environmentInput(name, nameSize, true);
    if (!key) return;
    char* text = environmentInput(value, valueSize, false);
    if (text) { (void)xrtEnvSet(key, text); xrtFree(text); }
    xrtFree(key);
}

void xl6EnvRemoveV1(xstrview name, int64_t size)
{
    char* key = environmentInput(name, size, true);
    if (key) { (void)xrtEnvRemove(key); xrtFree(key); }
}
