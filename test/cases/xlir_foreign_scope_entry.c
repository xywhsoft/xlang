#include <stddef.h>
#include <stdio.h>

typedef struct xvalue xvalue;
extern int xlang_main(xvalue* arg);
extern void xl_script_module_unit(void);
extern int xl_script_has_error(void);
extern void xl_script_print_uncaught(void);

int main(void)
{
    int result = xlang_main(NULL);
    if (result != 0 || xl_script_has_error()) {
        xl_script_print_uncaught();
        return 1;
    }
    xl_script_module_unit();
    if (xl_script_has_error()) {
        xl_script_print_uncaught();
        return 2;
    }
    puts("foreign scope: separately compiled generated C and native host passed");
    return 0;
}
