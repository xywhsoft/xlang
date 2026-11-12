#ifndef XL6_C_INTEGER_ARITHMETIC_H
#define XL6_C_INTEGER_ARITHMETIC_H
#include "xl6_checked_c_integer.h"
#include "xl6_error_text.h"
#include "xl_foreign_error.h"
#include <stddef.h>
#include <wchar.h>

static inline void xl6CIntegerArithmeticFailureV1(
    XL6_CheckedCIntegerStatusV1 status,int line,int column)
{
    const char *kind=status==XL6_C_INTEGER_ZERO_DIVISOR ? "ZeroDivisionError":
        status==XL6_C_INTEGER_OVERFLOW ? "OverflowError":"RangeError";
    const char *message=status==XL6_C_INTEGER_ZERO_DIVISOR ? "C integer division or modulo by zero":
        status==XL6_C_INTEGER_OVERFLOW ? "C integer arithmetic overflow":
        status==XL6_C_INTEGER_SHIFT_COUNT ? "invalid C integer shift":"invalid C integer domain";
    /* The caller owns the language error edge. Do not poison an enclosing
     * foreign scope when this error can still be caught inside the language.
     * A failed allocation leaves NULL here; the separate bool is authoritative. */
    xrtSetErrorTake(xl6ErrorCreateV1(kind,xrtStrView(message),line,column));
}

/* These are typed scalar calls. The target C compiler supplies its actual
 * widths (including LP64/LLP64, char and wchar_t), with no dynamic boxing,
 * profile lookup, payload copy, or per-success heap allocation. */
#define XL6_C_INTEGER_ARITHMETIC(Name,Type,Signed) \
static inline bool xl6C##Name##ArithmeticV1(Type a,Type b,XL6_CheckedCIntegerOpV1 op,int line,int column,Type* out) \
{ \
    XL6_CheckedCIntegerStatusV1 status; \
    if(Signed) { \
        int64_t result=0;status=xl6CheckedCIntegerSignedV1((int64_t)a,(int64_t)b,sizeof(Type)*CHAR_BIT,op,&result); \
        if(status){xl6CIntegerArithmeticFailureV1(status,line,column);return false;} \
        *out=(Type)result;return true; \
    } else { \
        uint64_t result=0;status=xl6CheckedCIntegerUnsignedV1((uint64_t)a,(uint64_t)b,sizeof(Type)*CHAR_BIT,op,&result); \
        if(status){xl6CIntegerArithmeticFailureV1(status,line,column);return false;} \
        *out=(Type)result;return true; \
    } \
} \
static inline bool xl6C##Name##ShiftV1(Type a,uint64_t count,bool right,int line,int column,Type* out) \
{ \
    XL6_CheckedCIntegerStatusV1 status; \
    if(Signed) { \
        int64_t result=0;status=xl6CheckedCIntegerSignedShiftV1((int64_t)a,count,sizeof(Type)*CHAR_BIT,right,&result); \
        if(status){xl6CIntegerArithmeticFailureV1(status,line,column);return false;} \
        *out=(Type)result;return true; \
    } else { \
        uint64_t result=0;status=xl6CheckedCIntegerUnsignedShiftV1((uint64_t)a,count,sizeof(Type)*CHAR_BIT,right,&result); \
        if(status){xl6CIntegerArithmeticFailureV1(status,line,column);return false;} \
        *out=(Type)result;return true; \
    } \
}
XL6_C_INTEGER_ARITHMETIC(Char,char,CHAR_MIN<0)
XL6_C_INTEGER_ARITHMETIC(SChar,signed char,1)
XL6_C_INTEGER_ARITHMETIC(UChar,unsigned char,0)
XL6_C_INTEGER_ARITHMETIC(Short,short,1)
XL6_C_INTEGER_ARITHMETIC(UShort,unsigned short,0)
XL6_C_INTEGER_ARITHMETIC(Int,int,1)
XL6_C_INTEGER_ARITHMETIC(UInt,unsigned int,0)
XL6_C_INTEGER_ARITHMETIC(Long,long,1)
XL6_C_INTEGER_ARITHMETIC(ULong,unsigned long,0)
XL6_C_INTEGER_ARITHMETIC(LongLong,long long,1)
XL6_C_INTEGER_ARITHMETIC(ULongLong,unsigned long long,0)
XL6_C_INTEGER_ARITHMETIC(Size,size_t,0)
XL6_C_INTEGER_ARITHMETIC(SSize,intptr_t,1)
XL6_C_INTEGER_ARITHMETIC(Ptrdiff,ptrdiff_t,1)
XL6_C_INTEGER_ARITHMETIC(Intptr,intptr_t,1)
XL6_C_INTEGER_ARITHMETIC(Uintptr,uintptr_t,0)
XL6_C_INTEGER_ARITHMETIC(Wchar,wchar_t,WCHAR_MIN<0)
#undef XL6_C_INTEGER_ARITHMETIC
#endif
