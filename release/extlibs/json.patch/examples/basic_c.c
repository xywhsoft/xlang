#define XRT_MODULE_ALL
#include "../include/json_patch.h"

#include <stdio.h>
#include <string.h>

static xvalue* parse(const char* text) {
    xstrview source = { text, strlen(text) };
    return xrtJsonParse(source);
}

int main(void) {
    xvalue* original = parse("{\"name\":\"demo\",\"values\":[1,2]}");
    xvalue* operations = parse("[{\"op\":\"add\",\"path\":\"/values/1\",\"value\":9}]");
    xvalue* updated = NULL;
    xvalue* changed = NULL;
    xvalue* unchanged = NULL;
    void* pointer = NULL;
    int64 first, second;
    int ok = 0;

    if (!original || !operations) goto done;
    updated = xjsonpatch_apply(original, operations);
    if (xjsonpatch_has_error()) goto done;
    changed = xjsonpatch_get(updated, "/values/1");
    if (xjsonpatch_has_error() || !xrtValueGetInt(changed, &first)) goto done;
    unchanged = xjsonpatch_get(original, "/values/1");
    if (xjsonpatch_has_error() || !xrtValueGetInt(unchanged, &second)) goto done;
    pointer = xjsonpatch_encode_pointer_len("a/b~c", 5);
    if (!pointer) goto done;

    printf("%lld\n%lld\n%s\n", (long long)first, (long long)second,
        (const char*)pointer);
    ok = 1;
done:
    xrtFree(pointer);
    xrtValueRelease(unchanged);
    xrtValueRelease(changed);
    xrtValueRelease(updated);
    xrtValueRelease(operations);
    xrtValueRelease(original);
    return ok ? 0 : 1;
}

/* 单文件范例直接运行；正式包仍由 manifest 编译原生实现。 */
#include "../json_patch_native.c"
