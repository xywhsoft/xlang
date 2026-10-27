#ifndef XL6_CLASS_TEXT_H
#define XL6_CLASS_TEXT_H
#include "xl6_error_value.h"

/* Closed, borrowed metadata emitted from the verified XLIR class layout.
 * Fields are exactly the declared derive-string partition, in layout order.
 * No generated formatter or callback is stored by the resident runtime. */
typedef struct XL6_ClassTextSchemaV1 {
    xstrview Name;
    int64_t RuntimeTypeTag;
    uint64_t TypeId;
    const xstrview* Fields;
    size_t FieldCount;
} XL6_ClassTextSchemaV1;

/* Owned exact UTF-8 bytes with a trailing NUL sentinel; {NULL,0} on failure.
 * The input and schema are borrowed only for this call. Null prints as null.
 * Non-null values must carry both the exact nominal identity and class tag,
 * as well as every selected field. Tags alone are only module-local. */
XL6_TEXT_API xstrview xl6ClassValueTextV1(const xvalue*, const XL6_ClassTextSchemaV1*);
struct XL6_ValueGraphPlanV1;
XL6_TEXT_API xstrview xl6ClassValueTextNativeV1(const xvalue*, const XL6_ClassTextSchemaV1*, const struct XL6_ValueGraphPlanV1*);
#endif
