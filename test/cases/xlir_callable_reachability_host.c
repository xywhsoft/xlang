#include <stdint.h>

#include "../../build/test/xlir_callable_reachability_generated.c"

int64_t xlir_reachability_live_external(int64_t value)
{
    return value + 1;
}

int main(void)
{
    return xlir_reachability_probe() == 42 ? 0 : 1;
}
