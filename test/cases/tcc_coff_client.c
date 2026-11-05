#include <stdio.h>
typedef struct CoffPair { long long left, right; } CoffPair;
extern int coff_add(int);
extern double coff_scale(double);
extern int coff_invoke(int (*)(int), int);
extern CoffPair coff_pair(long long);
extern int (*coff_callback)(int);
extern int coff_constructor_value;
extern int coff_duplicate(void);
extern int coff_cpp_first(int);
extern int coff_cpp_second(int);
static int client_callback(int value) { return value * 2; }
int main(void)
{
    CoffPair pair = coff_pair(10000000000LL);
    if (coff_add(25) != 42 || coff_callback(5) != 22 ||
        coff_scale(8.0) != 10.0 || coff_invoke(client_callback, 7) != 17 ||
        pair.left != 10000000000LL || pair.right != 10000000017LL ||
        coff_duplicate() != 18 || coff_constructor_value != 29 ||
        coff_cpp_first(5)!=22 || coff_cpp_second(7)!=24) return 1;
    puts("COFF_MAIN_OK");
    return 0;
}
