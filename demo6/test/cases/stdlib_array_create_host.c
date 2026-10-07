/* Strict-C executable host for the generated program's public checked ABI.
 * Module retirement remains explicit even when initialization/entry fails. */
#include <assert.h>
#include <stddef.h>
typedef struct xvalue xvalue;
extern xvalue* xrtValueArray(void);
extern void xrtValueRelease(xvalue*);
extern int xl_script_main_checked(xvalue*, int*);
extern int xl_script_module_unit_checked(void);
extern int xl_script_has_error(void);
extern void xl_script_print_uncaught(void);
extern void xl_script_clear_error(void);

int main(void)
{
    int code = 1;
    xvalue* arguments = xrtValueArray();
    int ok = arguments != NULL && xl_script_main_checked(arguments, &code);
    if (!ok || xl_script_has_error()) {
        xl_script_print_uncaught();
        ok = 0;
    }
    xrtValueRelease(arguments);
    if (!xl_script_module_unit_checked() || xl_script_has_error()) {
        xl_script_print_uncaught();
        ok = 0;
    }
    xl_script_clear_error();
    assert(ok && code == 0 && !xl_script_has_error());
    return ok ? code : 1;
}
