#define XRT_MODULE_ALL
#include "com.h"

#include <stdio.h>

int main(void)
{
    void* handle = x_com_create("Scripting.Dictionary");
    xvalue* args = NULL;
    xvalue* names = NULL;
    xvalue* result = NULL;
    bool exists = false;
    int ok = 0;

    if (handle == NULL) {
        fprintf(stderr, "Scripting.Dictionary unavailable: %s\n", x_com_last_error());
        return 2;
    }
    args = xrtValueArray();
    if (args == NULL ||
        !xrtValueArrayAppendNew(args, xrtValueString((xstrview){"key", 3u})) ||
        !xrtValueArrayAppendNew(args, xrtValueInt(42))) { goto done; }
    result = x_com_invoke_array(handle, "Add", args);
    xrtValueRelease(result);
    result = NULL;
    xrtValueRelease(args);
    args = xrtValueArray();
    if (args == NULL || !xrtValueArrayAppendNew(args,
        xrtValueString((xstrview){"key", 3u}))) { goto done; }
    result = x_com_invoke_array(handle, "Exists", args);
    if (result == NULL || !xrtValueGetBool(result, &exists) || !exists ||
        x_com_get_int(handle, "Count") != 1) { goto done; }
    xrtValueRelease(result);
    result = NULL;
    xrtValueRelease(args);
    args = xrtValueArray();
    names = xrtValueArray();
    if (args == NULL || names == NULL ||
        !xrtValueArrayAppendNew(args, xrtValueString((xstrview){"named", 5u})) ||
        !xrtValueArrayAppendNew(args, xrtValueInt(17)) ||
        !xrtValueArrayAppendNew(names, xrtValueString((xstrview){"Key", 3u})) ||
        !xrtValueArrayAppendNew(names, xrtValueString((xstrview){"Item", 4u}))) { goto done; }
    result = x_com_invoke_named(handle, "Add", args, names);
    if (x_com_last_error()[0] != '\0' || x_com_get_int(handle, "Count") != 2) {
        fprintf(stderr, "named Add failed: %s\n", x_com_last_error());
        goto done;
    }
    printf("%lld\n%d\n", (long long)x_com_get_int(handle, "Count"), exists ? 1 : 0);
    ok = 1;

done:
    xrtValueRelease(result);
    xrtValueRelease(args);
    xrtValueRelease(names);
    x_com_release(handle);
    return ok ? 0 : 1;
}
