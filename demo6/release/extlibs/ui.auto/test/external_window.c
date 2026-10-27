#include "../../uia/include/uia.h"
#include <stdio.h>

int main(void)
{
    void* window = x_uia_test_window_create();
    if (window == NULL || !x_uia_test_window_valid(window)) {
        fprintf(stderr, "ui.auto fixture: %s\n", x_uia_last_error());
        if (window != NULL) x_uia_test_window_release(window);
        return 1;
    }

    puts("ready");
    fflush(stdout);
    (void)getchar();

    return x_uia_test_window_release(window) ? 0 : 1;
}
