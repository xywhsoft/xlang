#ifndef XL_EXTLIB_ICAL_INTERNAL_H
#define XL_EXTLIB_ICAL_INTERNAL_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

#define XI_LIMIT (8u * 1024u * 1024u)

typedef struct {
    const char *key;
    const char *wire;
    int is_text;
} XiProperty;

extern const XiProperty xi_properties[];
extern const size_t xi_property_count;

void xi_error(const char *message);
int xi_ascii_equal(const char *left, size_t length, const char *right);
const XiProperty *xi_property_key(const char *key, size_t length);
const XiProperty *xi_property_wire(const char *name, size_t length);
int xi_is_time_property(const XiProperty *property);
int xi_wire_chars(xstrview value, int allow_newline);
int xi_append(xbuffer *buffer, const char *text, size_t length);
int xi_literal(xbuffer *buffer, const char *text);

#endif
