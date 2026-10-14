#ifndef XL6_CONTAINER_PROJECTION_H
#define XL6_CONTAINER_PROJECTION_H
#include "xl6_text_value.h"

/* Compiler-private resident callbacks and fresh zeroed metadata. No callback,
 * schema or code lease from a generated program can enter this boundary.
 * Capture owns immutable metadata independently of witness storage; failure
 * may leave owned metadata for Drop. Match is allocation/error-transparent,
 * validates genuine private owner ops, and never reads mutable element slots.
 * Drop clears all metadata and reports cleanup failures through error scopes. */
typedef struct XL6_ContainerProjectionOpsV1 {
    bool (*Capture)(void*,const xvalue*);
    bool (*Match)(const void*,const xvalue*);
    void (*Drop)(void*);
} XL6_ContainerProjectionOpsV1;
/* Consumes witness on both paths, borrows source. Empty output owns the SAME
 * source, or canonical null on nullable mismatch, only after checked cleanup.
 * Failed native NULL and nullable mismatch are different outcomes. */
XL6_TEXT_API bool xl6ContainerProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,
    void*,const XL6_ContainerProjectionOpsV1*,xvalue**);
XL6_TEXT_API bool xl6ListProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,xvalue**);
XL6_TEXT_API bool xl6SetProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,xvalue**);
XL6_TEXT_API bool xl6DictProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,xvalue**);
XL6_TEXT_API bool xl6TreeProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,xvalue**);
#endif
