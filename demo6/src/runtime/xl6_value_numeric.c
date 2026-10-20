#include "xl6_value_numeric.h"
#include "xl6_scalar_domain.h"
#include "xl6_error_text.h"
#include <string.h>

static xvalue* failure(const char* kind,const char* message)
{ xrtSetErrorTake(xl6ErrorCreateV1(kind,xrtStrView(message),0,0));return NULL; }
static int64_t signedBits(uint64_t bits)
{ int64_t result;memcpy(&result,&bits,sizeof(result));return result; }
typedef struct Number {
    uint64_t identity,bits;
    uint32_t domain;
    xvaluetype type;
    int64_t integer;
    double real;
} Number;
/* A TypeId without an authenticated domain cannot be guessed to be a C
 * scalar, enum or ordinary integer. No fallback may discard nominal identity. */
static bool read(const xvalue* value,Number* number)
{
    if (!value) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    number->type=xrtValueType(value);number->identity=xrtValueTypeId(value);
    number->domain=xrtValueScalarDomainV1(value);
    if ((number->identity || number->domain) && !xl6ScalarDomainAdmitsV1(value,number->domain)) {
        xrtSetErrorKind(XERR_TYPE);return false;
    }
    switch(number->type) {
    case XVALUE_BOOL: {bool flag;if(!xrtValueGetBool(value,&flag))return false;
        number->integer=flag?1:0;number->bits=(uint64_t)number->integer;return true;}
    case XVALUE_INT:
        if(!xrtValueGetInt(value,&number->integer))return false;
        number->bits=(uint64_t)number->integer;return true;
    case XVALUE_UINT:return xrtValueGetUInt(value,&number->bits);
    case XVALUE_FLOAT:return xrtValueGetFloat(value,&number->real);
    default:return true; /* Existing primitive unsupported-result rule below. */
    }
}
static bool numeric(xvaluetype type)
{return type==XVALUE_BOOL||type==XVALUE_INT||type==XVALUE_UINT||type==XVALUE_FLOAT;}
static bool enumeration(const Number* n)
{return (n->domain&XL6_SCALAR_DOMAIN_FAMILY_MASK)==XL6_SCALAR_DOMAIN_ENUM_V1;}
static unsigned width(unsigned rule)
{
    switch(rule) {
    case XL6_VALUE_ADMIT_I8:case XL6_VALUE_ADMIT_U8:return 8;
    case XL6_VALUE_ADMIT_I16:case XL6_VALUE_ADMIT_U16:return 16;
    case XL6_VALUE_ADMIT_I32:case XL6_VALUE_ADMIT_U32:return 32;
    case XL6_VALUE_ADMIT_I64:case XL6_VALUE_ADMIT_U64:return 64;
    default:return 0;
    }
}
static xvalue* enumResult(const Number* source,uint64_t bits)
{
    unsigned rule=source->domain&255u,bitsWide=width(rule);
    if (!bitsWide) return failure("TypeError","invalid numeric enum domain");
    uint64_t mask=bitsWide==64?UINT64_MAX:(UINT64_C(1)<<bitsWide)-1;
    bits&=mask;
    if(rule<=XL6_VALUE_ADMIT_I64 && (bits&(UINT64_C(1)<<(bitsWide-1)))) bits|=~mask;
    xvalue* result=rule==XL6_VALUE_ADMIT_U64 ? xrtValueUInt(bits) : xrtValueInt(signedBits(bits));
    if (!result || xrtValueScalarDomainBindV1(result,source->identity,source->domain)) return result;
    xerror* error=xrtTakeError();xrtValueRelease(result);xrtClearError();xrtSetErrorTake(error);return NULL;
}
static double real(const Number* n)
{return n->type==XVALUE_FLOAT?n->real:n->type==XVALUE_UINT?(double)n->bits:(double)n->integer;}

xvalue* xl6ValueNumericBinaryV1(const xvalue* left,const xvalue* right,unsigned op)
{
    if(op>9)return failure("TypeError","invalid dynamic binary opcode");
    Number a={0},b={0};if(!read(left,&a)||!read(right,&b))return NULL;
    if(enumeration(&a)||enumeration(&b)) {
        if(!enumeration(&a)||!enumeration(&b)||a.identity!=b.identity||a.domain!=b.domain||op<5||op>7)
            return failure("TypeError","numeric enum operators require the same enum and bitwise operation");
        return enumResult(&a,op==5?a.bits&b.bits:op==6?a.bits|b.bits:a.bits^b.bits);
    }
    if(!numeric(a.type)||!numeric(b.type))return xrtValueNull();
    if(op>=5) {
        if(a.type==XVALUE_FLOAT||b.type==XVALUE_FLOAT)return xrtValueNull();
        uint64_t bits=op==5?a.bits&b.bits:op==6?a.bits|b.bits:op==7?a.bits^b.bits:0;
        bool unsignedResult=a.type==XVALUE_UINT||b.type==XVALUE_UINT;
        if(op>=8) {
            if((b.type!=XVALUE_UINT&&b.integer<0)||b.bits>=64)
                return failure("RangeError","shift count is outside the left operand width");
            bits=op==8?a.bits<<(unsigned)b.bits:a.bits>>(unsigned)b.bits;
        }
        return unsignedResult?xrtValueUInt(bits):xrtValueInt(signedBits(bits));
    }
    if(a.type==XVALUE_FLOAT||b.type==XVALUE_FLOAT) {
        double x=real(&a),y=real(&b);
        return op==0?xrtValueFloat(x+y):op==1?xrtValueFloat(x-y):op==2?xrtValueFloat(x*y):
            op==3&&y!=0.0?xrtValueFloat(x/y):xrtValueNull();
    }
    if(op<3) {
        uint64_t bits=op==0?a.bits+b.bits:op==1?a.bits-b.bits:a.bits*b.bits;
        return a.type==XVALUE_UINT||b.type==XVALUE_UINT?xrtValueUInt(bits):xrtValueInt(signedBits(bits));
    }
    if(b.bits==0)return failure("ZeroDivisionError",op==3?"integer division by zero":"integer modulo by zero");
    if(a.type==XVALUE_UINT||b.type==XVALUE_UINT)
        return xrtValueUInt(op==3?a.bits/b.bits:a.bits%b.bits);
    if(a.integer==INT64_MIN&&b.integer==-1)
        return failure("OverflowError",op==3?"integer division overflow":"integer modulo overflow");
    return xrtValueInt(op==3?a.integer/b.integer:a.integer%b.integer);
}
xvalue* xl6ValueNumericUnaryV1(const xvalue* value,unsigned op)
{
    if(op>2)return failure("TypeError","invalid dynamic unary opcode");
    Number n={0};if(!read(value,&n))return NULL;
    if(enumeration(&n))return op==2?enumResult(&n,~n.bits):
        failure("TypeError","numeric enums only support bitwise complement");
    if(!numeric(n.type))return xrtValueNull();
    if(n.type==XVALUE_FLOAT)return op==0?xrtValueFloat(n.real):op==1?xrtValueFloat(-n.real):xrtValueNull();
    uint64_t bits=op==0?n.bits:op==1?UINT64_C(0)-n.bits:~n.bits;
    return n.type==XVALUE_UINT?xrtValueUInt(bits):xrtValueInt(signedBits(bits));
}
