#ifndef XL6_CHECKED_C_INTEGER_H
#define XL6_CHECKED_C_INTEGER_H
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/* Pure arithmetic, independent of error allocation or a language frame. No
 * failure publishes an output. Width belongs to the execution domain; shift
 * counts are kept as a full unsigned carrier, so negative signed counts
 * convert to large invalid counts rather than narrowing into valid values. */
typedef enum XL6_CheckedCIntegerStatusV1 {
    XL6_C_INTEGER_OK=0, XL6_C_INTEGER_ZERO_DIVISOR,
    XL6_C_INTEGER_OVERFLOW, XL6_C_INTEGER_SHIFT_COUNT,
    XL6_C_INTEGER_INVALID_DOMAIN
} XL6_CheckedCIntegerStatusV1;
typedef enum XL6_CheckedCIntegerOpV1 {
    XL6_C_INTEGER_ADD, XL6_C_INTEGER_SUB, XL6_C_INTEGER_MUL,
    XL6_C_INTEGER_DIV, XL6_C_INTEGER_MOD
} XL6_CheckedCIntegerOpV1;

static inline uint64_t xl6CIntegerMagnitudeV1(int64_t value)
{ return value<0 ? (uint64_t)(-(value+1))+UINT64_C(1) : (uint64_t)value; }

static inline XL6_CheckedCIntegerStatusV1 xl6CheckedCIntegerSignedV1(
    int64_t a,int64_t b,unsigned width,XL6_CheckedCIntegerOpV1 op,int64_t *out)
{
    int64_t low,high,result;
    if(!out || !width || width>64)return XL6_C_INTEGER_INVALID_DOMAIN;
    high=width==64 ? INT64_MAX:(int64_t)((UINT64_C(1)<<(width-1))-1);
    low=-high-1;
    if(a<low || a>high || b<low || b>high)return XL6_C_INTEGER_INVALID_DOMAIN;
    switch(op) {
    case XL6_C_INTEGER_ADD:
        if((b>0 && a>high-b) || (b<0 && a<low-b))return XL6_C_INTEGER_OVERFLOW;
        result=a+b;break;
    case XL6_C_INTEGER_SUB:
        if((b>0 && a<low+b) || (b<0 && a>high+b))return XL6_C_INTEGER_OVERFLOW;
        result=a-b;break;
    case XL6_C_INTEGER_MUL: {
        bool negative=(a<0)!=(b<0);
        uint64_t left=xl6CIntegerMagnitudeV1(a),right=xl6CIntegerMagnitudeV1(b);
        uint64_t limit=(uint64_t)high+(negative ? UINT64_C(1):UINT64_C(0));
        if(right && left>limit/right)return XL6_C_INTEGER_OVERFLOW;
        uint64_t magnitude=left*right;
        if(negative && magnitude==(uint64_t)INT64_MAX+UINT64_C(1))result=INT64_MIN;
        else result=negative ? -(int64_t)magnitude:(int64_t)magnitude;
        break;
    }
    case XL6_C_INTEGER_DIV:
    case XL6_C_INTEGER_MOD:
        if(!b)return XL6_C_INTEGER_ZERO_DIVISOR;
        if(a==low && b==-1)return XL6_C_INTEGER_OVERFLOW;
        result=op==XL6_C_INTEGER_DIV ? a/b:a%b;break;
    default:return XL6_C_INTEGER_INVALID_DOMAIN;
    }
    *out=result;return XL6_C_INTEGER_OK;
}

static inline XL6_CheckedCIntegerStatusV1 xl6CheckedCIntegerUnsignedV1(
    uint64_t a,uint64_t b,unsigned width,XL6_CheckedCIntegerOpV1 op,uint64_t *out)
{
    uint64_t high,result;
    if(!out || !width || width>64)return XL6_C_INTEGER_INVALID_DOMAIN;
    high=width==64 ? UINT64_MAX:(UINT64_C(1)<<width)-1;
    if(a>high || b>high)return XL6_C_INTEGER_INVALID_DOMAIN;
    switch(op) {
    case XL6_C_INTEGER_ADD:
        if(a>high-b)return XL6_C_INTEGER_OVERFLOW;
        result=a+b;break;
    case XL6_C_INTEGER_SUB:
        if(a<b)return XL6_C_INTEGER_OVERFLOW;
        result=a-b;break;
    case XL6_C_INTEGER_MUL:
        if(b && a>high/b)return XL6_C_INTEGER_OVERFLOW;
        result=a*b;break;
    case XL6_C_INTEGER_DIV:
    case XL6_C_INTEGER_MOD:
        if(!b)return XL6_C_INTEGER_ZERO_DIVISOR;
        result=op==XL6_C_INTEGER_DIV ? a/b:a%b;break;
    default:return XL6_C_INTEGER_INVALID_DOMAIN;
    }
    *out=result;return XL6_C_INTEGER_OK;
}

static inline XL6_CheckedCIntegerStatusV1 xl6CheckedCIntegerSignedShiftV1(
    int64_t a,uint64_t count,unsigned width,bool right,int64_t *out)
{
    int64_t high,low,result;
    if(!out || !width || width>64)return XL6_C_INTEGER_INVALID_DOMAIN;
    high=width==64 ? INT64_MAX:(int64_t)((UINT64_C(1)<<(width-1))-1);
    low=-high-1;
    if(a<low || a>high)return XL6_C_INTEGER_INVALID_DOMAIN;
    if(count>=width)return XL6_C_INTEGER_SHIFT_COUNT;
    if(right) {
        /* Portable arithmetic right shift without a signed-negative shift. */
        result=a<0 ? -1-((-1-a)>>(unsigned)count):a>>(unsigned)count;
    } else {
        if(a<0)return XL6_C_INTEGER_SHIFT_COUNT;
        if(a>(high>>(unsigned)count))return XL6_C_INTEGER_OVERFLOW;
        result=(int64_t)((uint64_t)a<<(unsigned)count);
    }
    *out=result;return XL6_C_INTEGER_OK;
}

static inline XL6_CheckedCIntegerStatusV1 xl6CheckedCIntegerUnsignedShiftV1(
    uint64_t a,uint64_t count,unsigned width,bool right,uint64_t *out)
{
    uint64_t high,result;
    if(!out || !width || width>64)return XL6_C_INTEGER_INVALID_DOMAIN;
    high=width==64 ? UINT64_MAX:(UINT64_C(1)<<width)-1;
    if(a>high)return XL6_C_INTEGER_INVALID_DOMAIN;
    if(count>=width)return XL6_C_INTEGER_SHIFT_COUNT;
    if(!right && a>(high>>(unsigned)count))return XL6_C_INTEGER_OVERFLOW;
    result=right ? a>>(unsigned)count:a<<(unsigned)count;
    *out=result;return XL6_C_INTEGER_OK;
}
#endif
