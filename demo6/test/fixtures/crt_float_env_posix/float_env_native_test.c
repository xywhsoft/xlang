#include <math.h>
#include <pthread.h>
#include <stdint.h>

/* Link the real XRT host for thread identity and compatible snapshot storage. */

extern int xfenv_get_round(void);
extern int xfenv_set_round(int mode);
extern int xfenv_get_flags(void);
extern int xfenv_clear_flags(int flags);
extern int xfenv_raise_flags(int flags);
extern void* xfenv_capture(void);
extern void* xfenv_hold(void);
extern int xfenv_snapshot_restore(void* raw);
extern void xfenv_snapshot_destroy(void* raw);

typedef struct restore_probe {
    void* snapshot;
    int restore_status;
    int rounding_before;
    int rounding_after;
} restore_probe;

static void* restore_on_other_thread(void* argument)
{
    restore_probe* probe = (restore_probe*)argument;
    probe->rounding_before = xfenv_get_round();
    probe->restore_status = xfenv_snapshot_restore(probe->snapshot);
    probe->rounding_after = xfenv_get_round();
    return NULL;
}

int main(void)
{
    void* original = xfenv_capture();
    void* snapshot = NULL;
    void* held = NULL;
    pthread_t worker;
    restore_probe probe = {0};
    volatile double input = 2.75;
    int result = 0;

    if (original == NULL) return 1;
    if (!xfenv_set_round(1) || xfenv_get_round() != 1 ||
        nearbyint(input) != 2.0) { result = 2; goto done; }
    if (!xfenv_clear_flags(31) || !xfenv_raise_flags(2) ||
        (xfenv_get_flags() & 2) == 0) { result = 3; goto done; }

    snapshot = xfenv_capture();
    if (snapshot == NULL) { result = 4; goto done; }
    probe.snapshot = snapshot;
    if (pthread_create(&worker, NULL, restore_on_other_thread, &probe) != 0) {
        result = 5; goto done;
    }
    if (pthread_join(worker, NULL) != 0 || probe.restore_status != -1 ||
        probe.rounding_before != probe.rounding_after) {
        result = 6; goto done;
    }
    if (!xfenv_set_round(2) || xfenv_get_round() != 2 ||
        xfenv_snapshot_restore(snapshot) != 1 || xfenv_get_round() != 1) {
        result = 7; goto done;
    }

    held = xfenv_hold();
    if (held == NULL || xfenv_get_flags() != 0 ||
        xfenv_snapshot_restore(held) != 1 ||
        (xfenv_get_flags() & 2) == 0) {
        result = 8; goto done;
    }

done:
    xfenv_snapshot_destroy(held);
    xfenv_snapshot_destroy(snapshot);
    if (xfenv_snapshot_restore(original) != 1 && result == 0) result = 9;
    xfenv_snapshot_destroy(original);
    return result;
}
