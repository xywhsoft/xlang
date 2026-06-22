#include <stdint.h>

int64_t xl6_dep_value_v2(void);

int64_t xl6_uses_dep_v2_add(int64_t value)
{
    return xl6_dep_value_v2() + value;
}
