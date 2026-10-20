#include "xl6_value_numeric.h"
/* Collection/text addition has its own dependency root. Numeric bitwise
 * operations never require language Array storage or concatenation support. */
extern bool xl6ArrayIsValueV1(const xvalue*);
extern xvalue* xl6ArrayConcatDynamicV1(const xvalue*,const xvalue*);
extern xvalue* xl6ValueStringConcatV1(const xvalue*,const xvalue*);
xvalue* xl6ValueAddV1(const xvalue* left,const xvalue* right)
{
    if(left&&right&&xrtValueType(left)==XVALUE_STRING&&xrtValueType(right)==XVALUE_STRING)
        return xl6ValueStringConcatV1(left,right);
    if(left&&right&&xl6ArrayIsValueV1(left)&&xl6ArrayIsValueV1(right))
        return xl6ArrayConcatDynamicV1(left,right);
    return xl6ValueNumericBinaryV1(left,right,0);
}
