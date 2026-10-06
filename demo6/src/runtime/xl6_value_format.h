#ifndef XL6_VALUE_FORMAT_H
#define XL6_VALUE_FORMAT_H
#include "src/runtime/xl6_error_value.h"
/* Same display contract as xl6ValueTextV1, but writes directly into a buffer:
 * borrowed strings and stack-formatted scalars have no intermediate copies. */
XL6_TEXT_API bool xl6ValueTextAppendV1(xstrbuf*, const xvalue*);
/* Private language print argument pack. All formatting succeeds before the
 * first console write; a failure is reported, never turned into empty text. */
XL6_TEXT_API bool xl6_xrt_print(xvalue*, bool newline);
#endif
