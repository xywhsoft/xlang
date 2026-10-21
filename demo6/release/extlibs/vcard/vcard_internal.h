#ifndef XL_EXTLIB_VCARD_INTERNAL_H
#define XL_EXTLIB_VCARD_INTERNAL_H

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

#define XV_LIMIT (4u * 1024u * 1024u)

typedef struct {
    const char *key;
    const char *wire;
    /* 结构化字段和 URI 已是线值，不再施加 TEXT 转义。 */
    int raw_wire;
} XvProperty;

extern const XvProperty xv_properties[];
extern const size_t xv_property_count;

void xv_error(const char *message);
int xv_ascii_equal(const char *left, size_t length, const char *right);
int xv_wire_chars(xstrview text, int allow_newline);
const XvProperty *xv_property_key(const char *name, size_t length);
const XvProperty *xv_property_wire(const char *name, size_t length);
int xv_append(xbuffer *buffer, const char *text, size_t length);
int xv_literal(xbuffer *buffer, const char *text);

#endif
