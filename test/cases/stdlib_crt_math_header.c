#include "test/cases/stdlib_crt_math_header.h"
#include <math.h>
#include <stdint.h>
int crt_math_header_probe(void)
{
 union { double value; uint64_t bits; } d={-0.0};
 union { float value; uint32_t bits; } f={-0.0f};
 d.value=fabs(d.value);f.value=fabsf(f.value);
 if(d.bits || f.bits || signbit(fabsl(-0.0L)))return 1;
 d.bits=UINT64_C(0xfff8000000000123);d.value=fabs(d.value);
 f.bits=UINT32_C(0xffc00123);f.value=fabsf(f.value);
 if(d.bits!=UINT64_C(0x7ff8000000000123) || f.bits!=UINT32_C(0x7fc00123))return 2;
 return fabs(-2.0)==2.0 && fabsf(-2.0f)==2.0f && fabsl(-2.0L)==2.0L ? 0:3;
}
