#include <stdio.h>
#include "dinov3.h"

int main(void)
{
    if (!dinov3_available_native()) {
        fprintf(stderr, "dinov3 ONNX bridge: %s\n", dinov3_last_error_native());
        return 1;
    }
    puts("dinov3 C bridge load: PASS");
    return 0;
}
