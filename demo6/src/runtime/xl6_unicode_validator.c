#include "src/runtime/xl6_unicode_internal.h"

typedef struct XL6_Utf8Validator {
    uint64_t ownerThread; /* Checked before any mutable state is accessed. */
    xutf8state state;
    bool finished;
} XL6_Utf8Validator;

static XL6_Utf8Validator* xl6UnicodeValidatorCheck(void* handle)
{
    XL6_Utf8Validator* validator = handle;
    if (!validator) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (validator->ownerThread != xrtThreadCurrentId()) { xrtSetErrorKind(XERR_STATE); return NULL; }
    return validator;
}
void* xl6UnicodeValidatorCreateV1(void)
{
    XL6_Utf8Validator* validator = xrtCalloc(1, sizeof(*validator));
    if (!validator) return NULL;
    validator->ownerThread = xrtThreadCurrentId();
    xrtUtf8StateInit(&validator->state); return validator;
}
void xl6UnicodeValidatorDestroyV1(void* handle)
{ xrtFree(handle); } /* Final retirement may occur on another thread. */
int64_t xl6UnicodeValidatorFeedV1(void* handle, const xvalue* data, bool final, int64_t size)
{
    XL6_Utf8Validator* validator = xl6UnicodeValidatorCheck(handle);
    xstrview text;
    if (!validator || !xl6UnicodeBytes(data, size, &text)) return XUTF_INVALID;
    if (validator->finished) { xrtSetErrorKind(XERR_STATE); return XUTF_INVALID; }
    /* XRT uses size_t. Keep all exposed offsets/total in the language int
     * domain, and reject overflow before committing the chunk or final bit. */
    if (text.Size > (uint64_t)INT64_MAX - validator->state.Total) {
        xrtSetErrorKind(XERR_RANGE); return XUTF_INVALID;
    }
    xutfstatus status = xrtUtf8StateFeed(&validator->state, text, final);
    if (final) validator->finished = true;
    return (int64_t)status;
}
void xl6UnicodeValidatorResetV1(void* handle)
{
    XL6_Utf8Validator* validator = xl6UnicodeValidatorCheck(handle);
    if (!validator) return;
    xrtUtf8StateInit(&validator->state); validator->finished = false;
}
int64_t xl6UnicodeValidatorErrorV1(void* handle)
{
    XL6_Utf8Validator* validator = xl6UnicodeValidatorCheck(handle);
    if (!validator) return -1;
    size_t error = xrtUtf8StateError(&validator->state);
    return error == XRT_NPOS ? -1 : (int64_t)error;
}
int64_t xl6UnicodeValidatorTotalV1(void* handle)
{
    XL6_Utf8Validator* validator = xl6UnicodeValidatorCheck(handle);
    return validator ? (int64_t)validator->state.Total : 0;
}
bool xl6UnicodeValidatorFinishedV1(void* handle)
{
    XL6_Utf8Validator* validator = xl6UnicodeValidatorCheck(handle);
    return validator && validator->finished;
}
