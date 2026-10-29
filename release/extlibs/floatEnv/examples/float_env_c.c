#include "../include/float_env_native.h"
#include <stdio.h>

int main(void)
{
    void* original = xfenv_capture();
    void* held = NULL;
    int result = 1;
    if (original == NULL || !xfenv_clear_flags(31) || !xfenv_set_round(1)) goto done;
    printf("round mode: %d\n", xfenv_get_round());

    if (!xfenv_raise_flags(2)) goto done;
    printf("flags: %d\n", xfenv_get_flags());

    held = xfenv_hold();
    if (held == NULL) goto done;
    printf("held flags: %d\n", xfenv_get_flags());
    if (xfenv_snapshot_restore(held) != 1) goto done;
    printf("restored flags: %d\n", xfenv_get_flags());

    result = 0;
done:
    xfenv_snapshot_destroy(held);
    if (original != NULL) {
        if (xfenv_snapshot_restore(original) != 1) result = 1;
        xfenv_snapshot_destroy(original);
    }
    if (result == 0) puts("floatEnv example: PASS");
    return result;
}

/* 单文件 TCC 运行时补入实现；正式包仍单独编译原生源码。 */
#ifdef __TINYC__
#include "../float_env_native.c"
#endif
