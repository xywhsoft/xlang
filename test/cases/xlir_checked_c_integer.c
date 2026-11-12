#include "src/runtime/xl6_checked_c_integer.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static uint64_t checks;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"checked C integer failure at line %d\n",__LINE__);exit(1);} } while(0)
static uint64_t mask(unsigned width)
{ return width==64 ? UINT64_MAX:(UINT64_C(1)<<width)-1; }
static int64_t signed_bits(uint64_t value,unsigned width)
{
    value&=mask(width);
    uint64_t sign=UINT64_C(1)<<(width-1);
    return value&sign ? -1-(int64_t)(~value&mask(width)):(int64_t)value;
}
static void signed_case(int64_t a,int64_t b,unsigned width)
{
    __int128 high=(((__int128)1)<<(width-1))-1,low=-high-1;
    for(int op=XL6_C_INTEGER_ADD;op<=XL6_C_INTEGER_MOD;++op) {
        int64_t value=8383;__int128 expected=0;
        XL6_CheckedCIntegerStatusV1 status=XL6_C_INTEGER_OK;
        switch(op) {
        case XL6_C_INTEGER_ADD:expected=(__int128)a+b;break;
        case XL6_C_INTEGER_SUB:expected=(__int128)a-b;break;
        case XL6_C_INTEGER_MUL:expected=(__int128)a*b;break;
        default:
            if(!b)status=XL6_C_INTEGER_ZERO_DIVISOR;
            else if((__int128)a==low && b==-1)status=XL6_C_INTEGER_OVERFLOW;
            else expected=op==XL6_C_INTEGER_DIV ? (__int128)a/b:(__int128)a%b;
        }
        if(!status && (expected<low || expected>high))status=XL6_C_INTEGER_OVERFLOW;
        CHECK(xl6CheckedCIntegerSignedV1(a,b,width,(XL6_CheckedCIntegerOpV1)op,&value)==status);
        CHECK(status ? value==8383:value==(int64_t)expected);
    }
}
static void unsigned_case(uint64_t a,uint64_t b,unsigned width)
{
    unsigned __int128 high=mask(width);
    for(int op=XL6_C_INTEGER_ADD;op<=XL6_C_INTEGER_MOD;++op) {
        uint64_t value=8383;unsigned __int128 expected=0;
        XL6_CheckedCIntegerStatusV1 status=XL6_C_INTEGER_OK;
        switch(op) {
        case XL6_C_INTEGER_ADD:expected=(unsigned __int128)a+b;break;
        case XL6_C_INTEGER_SUB:
            if(a<b)status=XL6_C_INTEGER_OVERFLOW;else expected=a-b;
            break;
        case XL6_C_INTEGER_MUL:expected=(unsigned __int128)a*b;break;
        default:
            if(!b)status=XL6_C_INTEGER_ZERO_DIVISOR;
            else expected=op==XL6_C_INTEGER_DIV ? a/b:a%b;
        }
        if(!status && expected>high)status=XL6_C_INTEGER_OVERFLOW;
        CHECK(xl6CheckedCIntegerUnsignedV1(a,b,width,(XL6_CheckedCIntegerOpV1)op,&value)==status);
        CHECK(status ? value==8383:value==(uint64_t)expected);
    }
}
static void shift_case(uint64_t bits,uint64_t count,unsigned width)
{
    int64_t a=signed_bits(bits,width);
    uint64_t u=bits&mask(width);
    __int128 high=(((__int128)1)<<(width-1))-1;
    for(unsigned right=0;right<=1;++right) {
        int64_t output=8383;uint64_t unsigned_output=8383;
        __int128 expected=0;unsigned __int128 unsigned_expected=0;
        XL6_CheckedCIntegerStatusV1 ss=XL6_C_INTEGER_OK,us=XL6_C_INTEGER_OK;
        if(count>=width)ss=us=XL6_C_INTEGER_SHIFT_COUNT;
        else {
            __int128 divisor=((__int128)1)<<(unsigned)count;
            if(right)expected=a<0 ? -((-(__int128)a+divisor-1)/divisor):a/divisor;
            else if(a<0)ss=XL6_C_INTEGER_SHIFT_COUNT;
            else {expected=(__int128)a*divisor;if(expected>high)ss=XL6_C_INTEGER_OVERFLOW;}
            unsigned_expected=right ? u>>(unsigned)count:(unsigned __int128)u<<(unsigned)count;
            if(unsigned_expected>mask(width))us=XL6_C_INTEGER_OVERFLOW;
        }
        CHECK(xl6CheckedCIntegerSignedShiftV1(a,count,width,right!=0,&output)==ss);
        CHECK(ss ? output==8383:output==(int64_t)expected);
        CHECK(xl6CheckedCIntegerUnsignedShiftV1(u,count,width,right!=0,&unsigned_output)==us);
        CHECK(us ? unsigned_output==8383:unsigned_output==(uint64_t)unsigned_expected);
    }
}
int main(void)
{
    for(unsigned a=0;a<256;++a) {
        for(unsigned b=0;b<256;++b) {
            signed_case(signed_bits(a,8),signed_bits(b,8),8);
            unsigned_case(a,b,8);
        }
        for(unsigned n=0;n<10;++n)shift_case(a,n,8);
    }
    uint64_t state=UINT64_C(0x9e3779b97f4a7c15);
    const unsigned widths[]={1,7,8,16,32,64};
    for(unsigned k=0;k<sizeof(widths)/sizeof(widths[0]);++k) {
        unsigned width=widths[k];
        uint64_t cases[]={0,1,mask(width),mask(width)>>1,(mask(width)>>1)+1};
        for(unsigned i=0;i<5;++i) {
            for(unsigned j=0;j<5;++j) {
                signed_case(signed_bits(cases[i],width),signed_bits(cases[j],width),width);
                unsigned_case(cases[i],cases[j],width);
            }
            for(unsigned count=0;count<=65;++count)shift_case(cases[i],count,width);
            shift_case(cases[i],UINT64_MAX,width);
            shift_case(cases[i],UINT64_C(0x100000000),width);
        }
        for(unsigned i=0;i<20000;++i) {
            state^=state<<13;state^=state>>7;state^=state<<17;uint64_t a=state&mask(width);
            state^=state<<13;state^=state>>7;state^=state<<17;uint64_t b=state&mask(width);
            signed_case(signed_bits(a,width),signed_bits(b,width),width);
            unsigned_case(a,b,width);shift_case(a,b,width);
        }
    }
    int64_t signed_output=8383;uint64_t unsigned_output=8383;
    CHECK(xl6CheckedCIntegerSignedV1(0,0,0,XL6_C_INTEGER_ADD,&signed_output)==XL6_C_INTEGER_INVALID_DOMAIN);
    CHECK(xl6CheckedCIntegerUnsignedV1(0,0,65,XL6_C_INTEGER_ADD,&unsigned_output)==XL6_C_INTEGER_INVALID_DOMAIN);
    CHECK(xl6CheckedCIntegerSignedV1(INT64_MAX,0,8,XL6_C_INTEGER_ADD,&signed_output)==XL6_C_INTEGER_INVALID_DOMAIN);
    CHECK(xl6CheckedCIntegerUnsignedV1(UINT64_MAX,0,8,XL6_C_INTEGER_ADD,&unsigned_output)==XL6_C_INTEGER_INVALID_DOMAIN);
    CHECK(signed_output==8383 && unsigned_output==8383);
    CHECK(xl6CheckedCIntegerSignedV1(1,1,8,XL6_C_INTEGER_ADD,NULL)==XL6_C_INTEGER_INVALID_DOMAIN);
    printf("Checked C integer arithmetic: %" PRIu64 " exact-oracle assertions passed\n",checks);
    return 0;
}
