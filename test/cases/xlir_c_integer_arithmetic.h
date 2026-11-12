#ifndef XLIR_C_INTEGER_ARITHMETIC_FIXTURE_H
#define XLIR_C_INTEGER_ARITHMETIC_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct CheckedNativeFields { int value; int samples[2]; } CheckedNativeFields;
char fixtureCIntegerMax_char(void);
char fixtureCIntegerMin_char(void);
int64_t fixtureCIntegerWidth_char(void);
bool fixtureCIntegerSigned_char(void);
signed char fixtureCIntegerMax_schar(void);
signed char fixtureCIntegerMin_schar(void);
int64_t fixtureCIntegerWidth_schar(void);
bool fixtureCIntegerSigned_schar(void);
unsigned char fixtureCIntegerMax_uchar(void);
unsigned char fixtureCIntegerMin_uchar(void);
int64_t fixtureCIntegerWidth_uchar(void);
bool fixtureCIntegerSigned_uchar(void);
short fixtureCIntegerMax_short(void);
short fixtureCIntegerMin_short(void);
int64_t fixtureCIntegerWidth_short(void);
bool fixtureCIntegerSigned_short(void);
unsigned short fixtureCIntegerMax_ushort(void);
unsigned short fixtureCIntegerMin_ushort(void);
int64_t fixtureCIntegerWidth_ushort(void);
bool fixtureCIntegerSigned_ushort(void);
int fixtureCIntegerMax_int(void);
int fixtureCIntegerMin_int(void);
int64_t fixtureCIntegerWidth_int(void);
bool fixtureCIntegerSigned_int(void);
unsigned int fixtureCIntegerMax_uint(void);
unsigned int fixtureCIntegerMin_uint(void);
int64_t fixtureCIntegerWidth_uint(void);
bool fixtureCIntegerSigned_uint(void);
long fixtureCIntegerMax_long(void);
long fixtureCIntegerMin_long(void);
int64_t fixtureCIntegerWidth_long(void);
bool fixtureCIntegerSigned_long(void);
unsigned long fixtureCIntegerMax_ulong(void);
unsigned long fixtureCIntegerMin_ulong(void);
int64_t fixtureCIntegerWidth_ulong(void);
bool fixtureCIntegerSigned_ulong(void);
long long fixtureCIntegerMax_longlong(void);
long long fixtureCIntegerMin_longlong(void);
int64_t fixtureCIntegerWidth_longlong(void);
bool fixtureCIntegerSigned_longlong(void);
unsigned long long fixtureCIntegerMax_ulonglong(void);
unsigned long long fixtureCIntegerMin_ulonglong(void);
int64_t fixtureCIntegerWidth_ulonglong(void);
bool fixtureCIntegerSigned_ulonglong(void);
size_t fixtureCIntegerMax_size(void);
size_t fixtureCIntegerMin_size(void);
int64_t fixtureCIntegerWidth_size(void);
bool fixtureCIntegerSigned_size(void);
intptr_t fixtureCIntegerMax_ssize(void);
intptr_t fixtureCIntegerMin_ssize(void);
int64_t fixtureCIntegerWidth_ssize(void);
bool fixtureCIntegerSigned_ssize(void);
ptrdiff_t fixtureCIntegerMax_ptrdiff(void);
ptrdiff_t fixtureCIntegerMin_ptrdiff(void);
int64_t fixtureCIntegerWidth_ptrdiff(void);
bool fixtureCIntegerSigned_ptrdiff(void);
intptr_t fixtureCIntegerMax_intptr(void);
intptr_t fixtureCIntegerMin_intptr(void);
int64_t fixtureCIntegerWidth_intptr(void);
bool fixtureCIntegerSigned_intptr(void);
uintptr_t fixtureCIntegerMax_uintptr(void);
uintptr_t fixtureCIntegerMin_uintptr(void);
int64_t fixtureCIntegerWidth_uintptr(void);
bool fixtureCIntegerSigned_uintptr(void);
wchar_t fixtureCIntegerMax_wchar(void);
wchar_t fixtureCIntegerMin_wchar(void);
int64_t fixtureCIntegerWidth_wchar(void);
bool fixtureCIntegerSigned_wchar(void);
#ifdef __cplusplus
}
#endif
#endif
