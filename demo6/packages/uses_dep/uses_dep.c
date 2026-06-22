#include <stdint.h>

int64_t xl6_dep_value(void);

int64_t xl6_uses_dep_add(int64_t value)
{
    return xl6_dep_value() + value;
}
