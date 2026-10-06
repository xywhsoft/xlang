#include "src/runtime/xl6_array_default.h"
_Static_assert(_Generic(&xl6ArrayResizeDefaultV1,
    bool (*)(xvalue*,int64_t,unsigned int): 1, default: 0),
    "Array resize has one borrowed receiver and a verified wire default hint");
