#include "../../extlibs/op/include/op.h"

#include <stdio.h>
#include <string.h>
#include "xrt.h"

int main(void)
{
    char *screen = xop_get_screen_size();
    void *context;

    if (screen == NULL || strchr(screen, 'x') == NULL) {
        xrtFree(screen);
        return 1;
    }
    context = xop_create();
    if (context == NULL || xop_is_bind(context) != 0) {
        xop_destroy(context);
        xrtFree(screen);
        return 2;
    }
    printf("screen: %s\n", screen);
    printf("bound: %lld\n", xop_is_bind(context));
    xop_destroy(context);
    xrtFree(screen);
    return 0;
}
