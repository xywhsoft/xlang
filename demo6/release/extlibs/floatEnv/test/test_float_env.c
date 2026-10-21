#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#include "xrt.h"
#include "../include/float_env_native.h"
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "floatEnv C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int restore_from_other_thread(void* snapshot)
{
    return xfenv_snapshot_restore(snapshot) == -1 ? 0 : 1;
}

static int hold_from_other_thread(void* snapshot)
{
    return xfenv_snapshot_hold(snapshot) == -1 ? 0 : 1;
}

static int change_other_thread(void* unused)
{
    (void)unused;
    return xfenv_set_round(2) == 1 && xfenv_get_round() == 2 &&
        xfenv_clear_flags(31) == 1 && xfenv_raise_flags(4) == 1 &&
        (xfenv_get_flags() & 4) != 0 ? 0 : 1;
}

int main(void)
{
    void* original = xfenv_capture();
    void* nested;
    void* held;
    xthread* thread;
    CHECK(original != NULL);
    CHECK(xfenv_set_round(-1) == 0);
    CHECK(xfenv_set_round(4) == 0);
    CHECK(xfenv_clear_flags(-1) == 0);
    CHECK(xfenv_raise_flags(32) == 0);

    CHECK(xfenv_clear_flags(31) == 1);
    CHECK(xfenv_get_flags() == 0);
    CHECK(xfenv_raise_flags(2) == 1);
    CHECK((xfenv_get_flags() & 2) != 0);
    CHECK(xfenv_set_round(1) == 1);
    CHECK(xfenv_get_round() == 1);

    nested = xfenv_capture();
    CHECK(nested != NULL);
    CHECK(xfenv_set_round(2) == 1);
    CHECK(xfenv_get_round() == 2);
    CHECK(xfenv_snapshot_restore(nested) == 1);
    CHECK(xfenv_get_round() == 1);
    CHECK(xfenv_set_round(3) == 1);
    CHECK(xfenv_snapshot_restore(nested) == 1);
    CHECK(xfenv_get_round() == 1);
    xfenv_snapshot_destroy(nested);

    held = xfenv_hold();
    CHECK(held != NULL);
    CHECK(xfenv_get_flags() == 0);
    CHECK(xfenv_raise_flags(1) == 1);
    CHECK(xfenv_snapshot_restore(held) == 1);
    CHECK(xfenv_get_flags() == 2);
    xfenv_snapshot_destroy(held);

    held = xfenv_capture();
    CHECK(held != NULL);
    thread = xrtThreadCreate(hold_from_other_thread, held, 0);
    CHECK(thread != NULL);
    CHECK(xrtThreadWait(thread) == XWAIT_OK);
    CHECK(xrtThreadExitCode(thread) == 0);
    xrtThreadDestroy(thread);
    CHECK(xfenv_get_flags() == 2);
    CHECK(xfenv_snapshot_hold(held) == 1);
    CHECK(xfenv_get_flags() == 0);
    CHECK(xfenv_raise_flags(8) == 1);
    CHECK(xfenv_snapshot_restore(held) == 1);
    CHECK(xfenv_get_flags() == 2);
    xfenv_snapshot_destroy(held);

    thread = xrtThreadCreate(change_other_thread, NULL, 0);
    CHECK(thread != NULL);
    CHECK(xrtThreadWait(thread) == XWAIT_OK);
    CHECK(xrtThreadExitCode(thread) == 0);
    xrtThreadDestroy(thread);
    CHECK(xfenv_get_round() == 1 && xfenv_get_flags() == 2);

    /* 环境是线程局部的，外线程只能检查，不能恢复此快照。 */
    thread = xrtThreadCreate(restore_from_other_thread, original, 0);
    CHECK(thread != NULL);
    CHECK(xrtThreadWait(thread) == XWAIT_OK);
    CHECK(xrtThreadExitCode(thread) == 0);
    xrtThreadDestroy(thread);
    CHECK(xfenv_snapshot_restore(original) == 1);
    xfenv_snapshot_destroy(original);

    puts("floatEnv C boundaries: PASS");
    return 0;
}
