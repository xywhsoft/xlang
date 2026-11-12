#include "xlir_c_integer_arithmetic.h"
#include <limits.h>
char fixtureCIntegerMax_char(void) { unsigned bits=sizeof(char)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(CHAR_MIN<0)max>>=1;return (char)max; }
char fixtureCIntegerMin_char(void) { return (CHAR_MIN<0) ? (char)(-((int64_t)fixtureCIntegerMax_char())-1):(char)0; }
int64_t fixtureCIntegerWidth_char(void) { return sizeof(char)*CHAR_BIT; }
bool fixtureCIntegerSigned_char(void) { return CHAR_MIN<0; }
signed char fixtureCIntegerMax_schar(void) { unsigned bits=sizeof(signed char)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (signed char)max; }
signed char fixtureCIntegerMin_schar(void) { return (1) ? (signed char)(-((int64_t)fixtureCIntegerMax_schar())-1):(signed char)0; }
int64_t fixtureCIntegerWidth_schar(void) { return sizeof(signed char)*CHAR_BIT; }
bool fixtureCIntegerSigned_schar(void) { return 1; }
unsigned char fixtureCIntegerMax_uchar(void) { unsigned bits=sizeof(unsigned char)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (unsigned char)max; }
unsigned char fixtureCIntegerMin_uchar(void) { return (0) ? (unsigned char)(-((int64_t)fixtureCIntegerMax_uchar())-1):(unsigned char)0; }
int64_t fixtureCIntegerWidth_uchar(void) { return sizeof(unsigned char)*CHAR_BIT; }
bool fixtureCIntegerSigned_uchar(void) { return 0; }
short fixtureCIntegerMax_short(void) { unsigned bits=sizeof(short)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (short)max; }
short fixtureCIntegerMin_short(void) { return (1) ? (short)(-((int64_t)fixtureCIntegerMax_short())-1):(short)0; }
int64_t fixtureCIntegerWidth_short(void) { return sizeof(short)*CHAR_BIT; }
bool fixtureCIntegerSigned_short(void) { return 1; }
unsigned short fixtureCIntegerMax_ushort(void) { unsigned bits=sizeof(unsigned short)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (unsigned short)max; }
unsigned short fixtureCIntegerMin_ushort(void) { return (0) ? (unsigned short)(-((int64_t)fixtureCIntegerMax_ushort())-1):(unsigned short)0; }
int64_t fixtureCIntegerWidth_ushort(void) { return sizeof(unsigned short)*CHAR_BIT; }
bool fixtureCIntegerSigned_ushort(void) { return 0; }
int fixtureCIntegerMax_int(void) { unsigned bits=sizeof(int)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (int)max; }
int fixtureCIntegerMin_int(void) { return (1) ? (int)(-((int64_t)fixtureCIntegerMax_int())-1):(int)0; }
int64_t fixtureCIntegerWidth_int(void) { return sizeof(int)*CHAR_BIT; }
bool fixtureCIntegerSigned_int(void) { return 1; }
unsigned int fixtureCIntegerMax_uint(void) { unsigned bits=sizeof(unsigned int)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (unsigned int)max; }
unsigned int fixtureCIntegerMin_uint(void) { return (0) ? (unsigned int)(-((int64_t)fixtureCIntegerMax_uint())-1):(unsigned int)0; }
int64_t fixtureCIntegerWidth_uint(void) { return sizeof(unsigned int)*CHAR_BIT; }
bool fixtureCIntegerSigned_uint(void) { return 0; }
long fixtureCIntegerMax_long(void) { unsigned bits=sizeof(long)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (long)max; }
long fixtureCIntegerMin_long(void) { return (1) ? (long)(-((int64_t)fixtureCIntegerMax_long())-1):(long)0; }
int64_t fixtureCIntegerWidth_long(void) { return sizeof(long)*CHAR_BIT; }
bool fixtureCIntegerSigned_long(void) { return 1; }
unsigned long fixtureCIntegerMax_ulong(void) { unsigned bits=sizeof(unsigned long)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (unsigned long)max; }
unsigned long fixtureCIntegerMin_ulong(void) { return (0) ? (unsigned long)(-((int64_t)fixtureCIntegerMax_ulong())-1):(unsigned long)0; }
int64_t fixtureCIntegerWidth_ulong(void) { return sizeof(unsigned long)*CHAR_BIT; }
bool fixtureCIntegerSigned_ulong(void) { return 0; }
long long fixtureCIntegerMax_longlong(void) { unsigned bits=sizeof(long long)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (long long)max; }
long long fixtureCIntegerMin_longlong(void) { return (1) ? (long long)(-((int64_t)fixtureCIntegerMax_longlong())-1):(long long)0; }
int64_t fixtureCIntegerWidth_longlong(void) { return sizeof(long long)*CHAR_BIT; }
bool fixtureCIntegerSigned_longlong(void) { return 1; }
unsigned long long fixtureCIntegerMax_ulonglong(void) { unsigned bits=sizeof(unsigned long long)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (unsigned long long)max; }
unsigned long long fixtureCIntegerMin_ulonglong(void) { return (0) ? (unsigned long long)(-((int64_t)fixtureCIntegerMax_ulonglong())-1):(unsigned long long)0; }
int64_t fixtureCIntegerWidth_ulonglong(void) { return sizeof(unsigned long long)*CHAR_BIT; }
bool fixtureCIntegerSigned_ulonglong(void) { return 0; }
size_t fixtureCIntegerMax_size(void) { unsigned bits=sizeof(size_t)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (size_t)max; }
size_t fixtureCIntegerMin_size(void) { return (0) ? (size_t)(-((int64_t)fixtureCIntegerMax_size())-1):(size_t)0; }
int64_t fixtureCIntegerWidth_size(void) { return sizeof(size_t)*CHAR_BIT; }
bool fixtureCIntegerSigned_size(void) { return 0; }
intptr_t fixtureCIntegerMax_ssize(void) { unsigned bits=sizeof(intptr_t)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (intptr_t)max; }
intptr_t fixtureCIntegerMin_ssize(void) { return (1) ? (intptr_t)(-((int64_t)fixtureCIntegerMax_ssize())-1):(intptr_t)0; }
int64_t fixtureCIntegerWidth_ssize(void) { return sizeof(intptr_t)*CHAR_BIT; }
bool fixtureCIntegerSigned_ssize(void) { return 1; }
ptrdiff_t fixtureCIntegerMax_ptrdiff(void) { unsigned bits=sizeof(ptrdiff_t)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (ptrdiff_t)max; }
ptrdiff_t fixtureCIntegerMin_ptrdiff(void) { return (1) ? (ptrdiff_t)(-((int64_t)fixtureCIntegerMax_ptrdiff())-1):(ptrdiff_t)0; }
int64_t fixtureCIntegerWidth_ptrdiff(void) { return sizeof(ptrdiff_t)*CHAR_BIT; }
bool fixtureCIntegerSigned_ptrdiff(void) { return 1; }
intptr_t fixtureCIntegerMax_intptr(void) { unsigned bits=sizeof(intptr_t)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(1)max>>=1;return (intptr_t)max; }
intptr_t fixtureCIntegerMin_intptr(void) { return (1) ? (intptr_t)(-((int64_t)fixtureCIntegerMax_intptr())-1):(intptr_t)0; }
int64_t fixtureCIntegerWidth_intptr(void) { return sizeof(intptr_t)*CHAR_BIT; }
bool fixtureCIntegerSigned_intptr(void) { return 1; }
uintptr_t fixtureCIntegerMax_uintptr(void) { unsigned bits=sizeof(uintptr_t)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(0)max>>=1;return (uintptr_t)max; }
uintptr_t fixtureCIntegerMin_uintptr(void) { return (0) ? (uintptr_t)(-((int64_t)fixtureCIntegerMax_uintptr())-1):(uintptr_t)0; }
int64_t fixtureCIntegerWidth_uintptr(void) { return sizeof(uintptr_t)*CHAR_BIT; }
bool fixtureCIntegerSigned_uintptr(void) { return 0; }
wchar_t fixtureCIntegerMax_wchar(void) { unsigned bits=sizeof(wchar_t)*CHAR_BIT;uint64_t max=UINT64_MAX >> (64-bits);if(WCHAR_MIN<0)max>>=1;return (wchar_t)max; }
wchar_t fixtureCIntegerMin_wchar(void) { return (WCHAR_MIN<0) ? (wchar_t)(-((int64_t)fixtureCIntegerMax_wchar())-1):(wchar_t)0; }
int64_t fixtureCIntegerWidth_wchar(void) { return sizeof(wchar_t)*CHAR_BIT; }
bool fixtureCIntegerSigned_wchar(void) { return WCHAR_MIN<0; }
