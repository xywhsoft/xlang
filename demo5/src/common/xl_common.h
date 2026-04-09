#ifndef XL_COMMON_H
#define XL_COMMON_H

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xl_config.h"
#include "xl_rc.h"

#define XL_UNUSED(x) (void)(x)
#define XL_COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif
