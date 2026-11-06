#define XRT_MODULE_ALL
#include "com.h"
#include "../test/com_test_fixture.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    void* handle = x_com_attach(x_com_test_object());
    xvalue* args = xrtValueArray();
    xvalue* result = NULL;
    int64 value = 0;
    const char* text;
    int ok = 0;

    if (handle == NULL || args == NULL) { goto done; }
    if (!xrtValueArrayAppendNew(args, xrtValueInt(9)) ||
        !xrtValueArrayAppendNew(args, xrtValueInt(4))) { goto done; }
    result = x_com_invoke_array(handle, "Sub", args);
    if (result == NULL || !xrtValueGetInt(result, &value)) { goto done; }
    text = x_com_get_text(handle, "LongText");
    if (text == NULL || strlen(text) != 6006u) { goto done; }
    printf("%lld\n%zu\n", (long long)value, strlen(text));
    ok = 1;

done:
    xrtValueRelease(result);
    xrtValueRelease(args);
    x_com_release(handle);
    return ok ? 0 : 1;
}
