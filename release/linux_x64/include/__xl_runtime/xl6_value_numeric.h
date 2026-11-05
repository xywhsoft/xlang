#ifndef XL6_VALUE_NUMERIC_H
#define XL6_VALUE_NUMERIC_H
#include "xl6_text_value.h"
/* Borrow operands, return one owned box; NULL is checked failure. The numeric
 * opcodes are the closed existing XLIR binary0..9 / unary0..2 ABI. Primitive
 * unsupported operations keep the established language null-result rule;
 * nominal enum operations instead require their exact authenticated domain. */
XL6_TEXT_API xvalue* xl6ValueNumericBinaryV1(const xvalue*,const xvalue*,unsigned);
XL6_TEXT_API xvalue* xl6ValueNumericUnaryV1(const xvalue*,unsigned);
XL6_TEXT_API xvalue* xl6ValueAddV1(const xvalue*,const xvalue*);
#endif
