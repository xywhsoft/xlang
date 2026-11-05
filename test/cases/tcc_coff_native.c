/* Compile this provider with GCC -O2; consume the actual COFF via TinyCC. */
#include <stdio.h>

__attribute__((selectany)) int coff_shared_value = 17;
int coff_constructor_value;
unsigned char coff_unused_payload[131072] = {7};
unsigned char coff_unused_bss[262144];

static void __attribute__((constructor)) coff_construct(void)
{
    coff_constructor_value = 29;
    puts("COFF_CONSTRUCTOR");
}
static void __attribute__((destructor)) coff_destruct(void)
{
    coff_constructor_value = 0;
    puts("COFF_DESTRUCTOR");
}
int coff_add(int value) { return value + coff_shared_value; }
double coff_scale(double value) { return value * 1.25; }
int (*coff_callback)(int) = coff_add;
int coff_invoke(int (*callback)(int), int value) { return callback(value) + 3; }
typedef struct CoffPair { long long left, right; } CoffPair;
CoffPair coff_pair(long long value)
{
    CoffPair result = {value, value + 17};
    return result;
}

extern int coff_unavailable_dead_dependency(void);
int coff_unreachable(void) { return coff_unavailable_dead_dependency(); }
