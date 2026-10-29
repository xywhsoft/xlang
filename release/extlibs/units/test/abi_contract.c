#include <stdint.h>
#include "../include/units_native.h"

/* isPow2 输入使用 XLang int，结果使用 C int。 */
extern int xu_is_pow2(int64_t value);

#define CHECK_TEXT_RESULT(name) \
    _Static_assert(_Generic(&(name), char* (*)(double): 1, default: 0), \
        #name " must return an owned char pointer")

CHECK_TEXT_RESULT(xu_bytes);
CHECK_TEXT_RESULT(xu_bytes_decimal);
CHECK_TEXT_RESULT(xu_duration);
CHECK_TEXT_RESULT(xu_duration_short);
CHECK_TEXT_RESULT(xu_count);
