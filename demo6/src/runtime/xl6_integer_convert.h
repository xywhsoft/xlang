#ifndef XL6_INTEGER_CONVERT_H
#define XL6_INTEGER_CONVERT_H
#include "xl6_text_value.h"
#include "xl6_integer_convert_abi.h"
/* Borrow exact-length inputs. Publish output only on checked success; false
 * remains authoritative when allocating an error diagnostic fails. */
XL6_TEXT_API bool xl6IntegerTextConvertV1(xstrview, unsigned, void*);
XL6_TEXT_API bool xl6IntegerValueConvertV1(const xvalue*, unsigned, uint64_t, void*);
#endif
