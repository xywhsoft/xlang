/* Source-only sidecar for the public API gate. Exact XLIR ABI: destructors
 * return void; strings borrow native storage that callers must copy. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char* native_text(void) { return "native text"; }
const char* native_shared_text(int64_t which)
{
    static char text[32];
    const char* value = which == 1 ? "first" : "second";
    memcpy(text, value, strlen(value) + 1);
    return text;
}
void* native_text_open(void) { return malloc(1); }
void native_text_close(void* self) { free(self); }
const char* native_text_shared(void* self, int64_t which)
{
    static char text[32];
    const char* value = which == 1 ? "member first" : "member second";
    if (!self) exit(2);
    memcpy(text, value, strlen(value) + 1);
    return text;
}
void expect_i64(int64_t actual, int64_t expected)
{
    if (actual != expected) exit(1);
}
typedef struct PublicNativeBox { int64_t value; } PublicNativeBox;
static int64_t closeCount;
void* make_borrowed_box(int64_t value)
{
    PublicNativeBox* box = (PublicNativeBox*)malloc(sizeof(*box));
    if (!box) exit(2);
    box->value = value;
    return box;
}
void borrowed_box_close(void* handle)
{
    if (!handle) exit(3);
    free(handle);
    ++closeCount;
}
int64_t borrowed_box_value(void* handle)
{
    if (!handle) exit(4);
    return ((PublicNativeBox*)handle)->value;
}
int64_t borrowed_close_count(void) { return closeCount; }
