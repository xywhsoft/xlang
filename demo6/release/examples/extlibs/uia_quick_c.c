#include "../../extlibs/uia/include/uia.h"
#include <stdio.h>

int main(void)
{
    void* window = x_uia_test_window_create();
    void* edit = NULL;
    int ok = 0;

    if (window == NULL || !x_uia_test_window_valid(window)) goto cleanup;
    edit = x_uia_from_hwnd(x_uia_test_window_edit_hwnd(window));
    if (edit == NULL || !x_uia_element_set_value(edit, "hello")) goto cleanup;

    printf("uia version: %s\n", x_uia_version());
    printf("edit text: %s\n", x_uia_element_text(edit));
    ok = 1;

cleanup:
    if (!ok) fprintf(stderr, "uia: %s\n", x_uia_last_error());
    if (edit != NULL) x_uia_element_release(edit);
    if (window != NULL) x_uia_test_window_release(window);
    x_uia_shutdown();
    return ok ? 0 : 1;
}
