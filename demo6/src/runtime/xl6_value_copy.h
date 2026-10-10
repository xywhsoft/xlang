#ifndef XL6_VALUE_COPY_H
#define XL6_VALUE_COPY_H
#include "xl6_text_value.h"
/* Borrowed input, independent owned container shell. Genuine native Array
 * and List/Set owners keep their sealed layout/admission/projection. Other
 * payloads retain XRT's declared shallow-copy semantics. No tag-based handle
 * reinterpretation, registry or source-language callback. */
XL6_TEXT_API xvalue* xl6ValueCopyV1(const xvalue*);
XL6_TEXT_API xvalue* xl6ValueCloneGraphV1(const xvalue*);
/* Open graphs may contain both List and Set. This separately selected
 * provider component does not enlarge closed Array/List-only programs. */
XL6_TEXT_API xvalue* xl6ValueCloneCollectionsV1(const xvalue*);
XL6_TEXT_API xvalue* xl6ValueCloneSetGraphV1(const xvalue*);
#endif
