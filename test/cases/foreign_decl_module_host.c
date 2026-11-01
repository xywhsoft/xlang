#include <stdio.h>
#include <stddef.h>
typedef struct xvalue xvalue;
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*, int*);
extern int xl_script_module_unit_checked(void);

int main(void)
{
    int code = -1;
    if (!xl_script_module_init(NULL) || !xl_script_main_checked(NULL, &code)) {
        fputs("declared foreign module execution failed\n", stderr);
        return 90;
    }
    if (!xl_script_module_unit_checked()) return 91;
    return code;
}
