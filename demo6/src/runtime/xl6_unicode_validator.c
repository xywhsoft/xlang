#include "src/runtime/xl6_unicode_internal.h"
#include "src/runtime/xl6_text_thread.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

typedef struct XL6_Utf8Validator {
    XL6_TextThreadOwner thread; /* Checked before any mutable state is accessed. */
    xutf8state state;
    bool finished;
} XL6_Utf8Validator;

static XL6_Utf8Validator* xl6UnicodeValidatorCheck(void* handle)
{
    XL6_Utf8Validator* validator = handle;
    if (!validator) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!xl6TextThreadCheck(&validator->thread)) return NULL;
    return validator;
}
void* xl6UnicodeValidatorCreateV1(void)
{
    XL6_Utf8Validator* validator = xrtCalloc(1, sizeof(*validator));
    if (!validator) return NULL;
    if (!xl6TextThreadInit(&validator->thread)) { xrtFree(validator); return NULL; }
    xrtUtf8StateInit(&validator->state); return validator;
}
void xl6UnicodeValidatorDestroyV1(void* handle)
{
    XL6_Utf8Validator* validator=handle;
    if (validator) { xl6TextThreadDrop(&validator->thread); xrtFree(validator); }
} /* Final retirement may occur on another thread. */
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

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6UnicodeValidatorHandlePolicyV1Get,xl6UnicodeValidatorDestroyV1)
