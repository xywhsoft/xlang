#ifndef XL6_VALUE_DOMAIN_PROJECTION_H
#define XL6_VALUE_DOMAIN_PROJECTION_H
#include "xl6_text_value.h"
/* Consumes an empty, sealed ARRAY<T> metadata witness on both paths. Its
 * complete resident element predicate checks the borrowed source; no live T,
 * queue, synchronization object or thread is created to perform a cast.
 * Only retained-reference domains are admitted. Empty output owns the SAME
 * source, or canonical null on nullable mismatch, after checked cleanup. */
XL6_TEXT_API bool xl6ValueDomainProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,xvalue**);
#endif
