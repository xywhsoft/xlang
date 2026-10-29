#ifndef XL6_COMMON_H
#define XL6_COMMON_H

#include "src/common/xl6_xrt.h"

#include <stddef.h>
#include <string.h>

#ifndef XL6_ARRAY_COUNT
#define XL6_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#endif
