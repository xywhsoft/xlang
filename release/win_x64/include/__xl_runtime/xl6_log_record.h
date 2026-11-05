#ifndef XL6_LOG_RECORD_H
#define XL6_LOG_RECORD_H
#include "xl6_log.h"

/* Private resident representation. Never projected from a language object or
 * exposed in generated C. All views below have independent owned backing. */
typedef struct xl6_log_record {
    xlogrecord record;
    xlogfield* fields;
    size_t capacity;
} xl6_log_record;
bool xl6LogTextConfig(xlogtextconfig*, int64_t style, int64_t flags, int64_t offset);
bool xl6LogJsonConfig(xlogjsonconfig*, int64_t flags, int64_t escape,
    int64_t fields, int64_t nonfinite, int64_t depth);
#endif
