#include "../include/uia.h"
#include <stdio.h>

int main(void)
{
    void* window = x_uia_test_window_create();
    void* root = window ? x_uia_from_hwnd(x_uia_test_window_hwnd(window)) : NULL;
    int invalid = !x_uia_element_wait_window_closed(NULL, 25) &&
        x_uia_last_error()[0] != '\0';
    int timed_out = invalid && root ? !x_uia_element_wait_window_closed(root, 25) &&
        x_uia_last_error()[0] == '\0' : 0;
    int closed = timed_out ? x_uia_element_close_window(root) : 0;
    int waited = closed ? x_uia_element_wait_window_closed(root, 1000) &&
        x_uia_last_error()[0] == '\0' : 0;
    int closed_handle = waited && x_uia_test_window_close(window) &&
        x_uia_test_window_hwnd(window) == NULL;
    int focus_rejected = closed_handle && !x_uia_test_window_focus_edit(window) &&
        x_uia_last_error()[0] != '\0';
    if (!invalid || !timed_out || !closed || !waited || !closed_handle || !focus_rejected) {
        fprintf(stderr, "invalid=%d timeout=%d close=%d wait=%d handle=%d focus=%d: %s\n",
            invalid, timed_out, closed, waited, closed_handle, focus_rejected, x_uia_last_error());
    }
    if (root) x_uia_element_release(root);
    if (window) x_uia_test_window_release(window);
    x_uia_shutdown();
    if (invalid && timed_out && closed && waited && closed_handle && focus_rejected) puts("uia close/wait ok");
    return invalid && timed_out && closed && waited && closed_handle && focus_rejected ? 0 : 1;
}
